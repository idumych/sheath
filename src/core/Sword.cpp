#include "Sword.h"
#include <sword/swmgr.h>
#include <sword/swmodule.h>
#include <sword/versekey.h>
#include <sword/swkey.h>
#include <sword/swbuf.h>
#include <sword/swconfig.h>
#include <sword/strkey.h>
#include <sword/listkey.h>
#include <sword/swld.h>
#include <sword/rawld.h>
#include <sword/rawld4.h>
#include <sword/zld.h>
#include <sword/swtext.h>
#include <sword/rawtext.h>
#include <sword/ztext.h>
#include <sword/ztext4.h>
#include <sword/swgenbook.h>
#include <sword/rawgenbook.h>
#include <sword/versificationmgr.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <regex>
#include <set>

namespace sheath {

// ---- internal helpers ----

static std::string mapModuleType(const std::string& swordType) {
    if (swordType.find("Bible") != std::string::npos) return "Bible";
    if (swordType.find("Commentary") != std::string::npos) return "Commentary";
    if (swordType.find("Dictionary") != std::string::npos ||
        swordType.find("Lexicon") != std::string::npos) return "Dictionary";
    if (swordType.find("Book") != std::string::npos ||
        swordType.find("Generic") != std::string::npos ||
        swordType.find("Daily") != std::string::npos) return "Book";
    return swordType;
}

static bool hasOption(const sword::ConfigEntMap& cfg, const std::string& opt) {
    return cfg.find(sword::SWBuf(opt.c_str())) != cfg.end();
}

// Map a Strong's key like "G25"/"H430" to a numeric key for the dictionary module
static std::string strongsToKey(const std::string& key) {
    if (key.empty()) return key;
    std::string num = key;
    if (!num.empty() && (num[0] == 'G' || num[0] == 'H')) {
        num = num.substr(1);
    }
    return num;
}

// Check if a key exists in the module
static bool keyExists(sword::SWModule* mod, const std::string& keyText) {
    if (!mod) return false;
    auto* k = mod->createKey();
    k->setText(keyText.c_str());
    mod->setKey(k);
    const char* txt = k->getText();
    bool found = (txt != nullptr && txt[0] != 0);
    delete k;
    return found;
}

struct SwordLibrary::Impl {
    sword::SWMgr* mgr = nullptr;
    mutable std::set<std::string> bibleModules;

    Impl(const std::string& swordPath) {
        const char* pathEnv = swordPath.empty() ? nullptr : swordPath.c_str();
        mgr = new sword::SWMgr(pathEnv, true, nullptr, false);
    }

    ~Impl() {
        delete mgr;
    }

    std::vector<ModuleInfo> getModules() const {
        std::vector<ModuleInfo> result;
        for (auto& [name, mod]: mgr->getModules()) {
            ModuleInfo info;
            info.name = name;
            info.description = mod->getDescription();
            info.type = mapModuleType(mod->getType());
            info.language = mod->getLanguage() ? mod->getLanguage() : "";
            const auto& cfg = mod->getConfig();
            auto itV = cfg.find("Version");
            if (itV != cfg.end()) info.version = itV->second;
            auto itVF = cfg.find("Versification");
            if (itVF != cfg.end()) info.versification = itVF->second;
            info.versification = info.versification.empty() ? "KJV" : info.versification;

            // Bible modules have a Versification config entry
            if (itVF != cfg.end() && info.type != "Bible" && info.type != "Commentary") {
                info.type = "Bible";
            }

            // Detect features from config and global options
            auto itS = cfg.find("StrongsNumbers");
            info.strongs = (itS != cfg.end());
            auto itM = cfg.find("Morph");
            info.morph = (itM != cfg.end());
            auto itF = cfg.find("Footnotes");
            info.footnotes = (itF != cfg.end());
            auto itH = cfg.find("Headings");
            info.headings = (itH != cfg.end());
            auto itR = cfg.find("RedLetter");
            info.redLetter = (itR != cfg.end());

            // Also check for OSIS-based features from GlobalOptionFilters
            for (auto& [k, v]: cfg) {
                if (k == "GlobalOptionFilter") {
                    if (v == "OSISStrongs" || v == "OSISLemma") info.strongs = true;
                    if (v == "OSISMorph" || v == "OSISMorphSegmentation") info.morph = true;
                    if (v == "OSISFootnotes") info.footnotes = true;
                    if (v == "OSISHeadings") info.headings = true;
                    if (v == "OSISRedLetterWords") info.redLetter = true;
                }
            }

            // Track bible modules by name
            if (info.type == "Bible") {
                bibleModules.insert(std::string(name));
            }

            result.push_back(info);
        }
        std::sort(result.begin(), result.end(),
                  [](const ModuleInfo& a, const ModuleInfo& b) { return a.name < b.name; });
        return result;
    }

    bool findModule(const std::string& name, ModuleInfo* out) const {
        auto mods = getModules();
        for (auto& m : mods) {
            if (m.name == name) {
                if (out) *out = m;
                return true;
            }
        }
        return false;
    }

    // Get the versification system for a module
    const sword::VersificationMgr::System* getVersificationForModule(const std::string& moduleName) const {
        auto* mod = mgr->getModule(moduleName.c_str());
        if (!mod) return nullptr;
        std::string v11n = mod->getConfigEntry("Versification");
        if (v11n.empty()) v11n = "KJV";
        auto* sys = sword::VersificationMgr::getSystemVersificationMgr()->getVersificationSystem(v11n.c_str());
        return sys;
    }

    // Find the book OSIS name by matching either OSIS name or preferred abbreviation
    std::string findBookOsisName(const std::string& sysName, const std::string& bookOsis) const {
        auto* sys = getVersificationForModule(sysName);
        if (!sys) return bookOsis; // fallback to caller's input
        for (int i = 0; i < sys->getBookCount(); i++) {
            const auto* book = sys->getBook(i);
            if (!book) continue;
            if (strcmp(book->getOSISName(), bookOsis.c_str()) == 0) return bookOsis;
            if (strcmp(book->getPreferredAbbreviation(), bookOsis.c_str()) == 0) return bookOsis;
        }
        return ""; // not found
    }

    std::vector<BookInfo> getBooks(const std::string& moduleName) const {
        auto* sys = getVersificationForModule(moduleName);
        if (!sys) return {};

        int bookCount = sys->getBookCount();
        std::vector<BookInfo> books;
        books.reserve(bookCount);

        for (int i = 0; i < bookCount; i++) {
            const auto* book = sys->getBook(i);
            if (!book) continue;
            BookInfo bi;
            bi.osis = book->getOSISName();
            bi.prefAbbrev = book->getPreferredAbbreviation();
            bi.name = book->getLongName();
            bi.chapters = book->getChapterMax();
            // Determine testament: index < BMAX[0] => OT (1), else NT (2)
            const int* bmax = sys->getBMAX();
            bi.testament = (i < bmax[0]) ? 1 : 2;
            books.push_back(bi);
        }
        return books;
    }

    int verseCount(const std::string& moduleName, const std::string& bookOsis, int chapter) const {
        auto* mod = mgr->getModule(moduleName.c_str());
        if (!mod) return 0;

        auto* sys = getVersificationForModule(moduleName);
        if (!sys) return 0;

        // Find the book by iterating (getBookNumberByOSISName is unreliable)
        const sword::VersificationMgr::Book *book = nullptr;
        for (int i = 0; i < sys->getBookCount(); i++) {
            const sword::VersificationMgr::Book *b = sys->getBook(i);
            if (!b) continue;
            if (strcmp(b->getOSISName(), bookOsis.c_str()) == 0) {
                book = b;
                break;
            }
        }
        if (!book) return 0;

        int chapterMax = book->getChapterMax();
        if (chapter > chapterMax) return 0;

        // Use the versification system's verse max for this chapter
        int maxVerses = book->getVerseMax(chapter);
        if (maxVerses < 1) maxVerses = 1;

        sword::VerseKey vk;
        vk.setVersificationSystem(sys->getName());
        vk.setBookName(book->getOSISName());
        vk.setChapter(chapter);

        // Count verses that have actual content
        int count = 0;
        for (int v = 1; v <= maxVerses; v++) {
            vk.setVerse(v);
            mod->setKey(&vk);
            mod->setPosition(0);
            sword::SWBuf rendered = mod->renderText();
            if (rendered.c_str()[0] != '\0') {
                count++;
            }
        }
        return count;
    }
};

// ---- public API ----

SwordLibrary::SwordLibrary(const std::string& swordPath) : d(new Impl(swordPath)) {}
SwordLibrary::~SwordLibrary() { delete d; }

void SwordLibrary::Reload() {
    delete d;
    d = new Impl("");
}

std::vector<ModuleInfo> SwordLibrary::Modules() const {
    return d->getModules();
}

bool SwordLibrary::Find(const std::string& module, ModuleInfo* out) const {
    return d->findModule(module, out);
}

std::vector<BookInfo> SwordLibrary::Books(const std::string& moduleName) const {
    return d->getBooks(moduleName);
}

int SwordLibrary::VerseCount(const std::string& module, const std::string& bookOsis, int chapter) const {
    return d->verseCount(module, bookOsis, chapter);
}

std::vector<VerseText> SwordLibrary::Chapter(const std::string& moduleName,
                                               const std::string& bookOsis,
                                               int chapter,
                                               const DisplayOptions& opts) {
    auto* mod = d->mgr->getModule(moduleName.c_str());
    if (!mod) return {};

    // Set global options
    d->mgr->setGlobalOption("Strong's Numbers", opts.strongs ? "On" : "Off");
    d->mgr->setGlobalOption("Morphological Tags", opts.morph ? "On" : "Off");
    d->mgr->setGlobalOption("Footnotes", opts.footnotes ? "On" : "Off");
    d->mgr->setGlobalOption("Cross-references", opts.crossRefs ? "On" : "Off");
    d->mgr->setGlobalOption("Headings", opts.headings ? "On" : "Off");
    d->mgr->setGlobalOption("Words of Christ in Red", opts.redLetter ? "On" : "Off");

    // Get the versification system for this module
    auto* sys = d->getVersificationForModule(moduleName);
    if (!sys) return {};

    // Find the book by iterating (getBookNumberByOSISName is unreliable)
    const sword::VersificationMgr::Book *book = nullptr;
    for (int i = 0; i < sys->getBookCount(); i++) {
        const sword::VersificationMgr::Book *b = sys->getBook(i);
        if (!b) continue;
        if (strcmp(b->getOSISName(), bookOsis.c_str()) == 0) {
            book = b;
            break;
        }
    }
    if (!book) return {};

    int chapterMax = book->getChapterMax();
    if (chapter > chapterMax) return {};

    // Use the versification system's verse max for this chapter
    int maxVerses = book->getVerseMax(chapter);
    if (maxVerses < 1) maxVerses = 1;

    // Create a VerseKey using the module's versification system
    sword::VerseKey vk;
    vk.setVersificationSystem(sys->getName());
    vk.setBookName(book->getOSISName());
    vk.setChapter(chapter);

    std::vector<VerseText> result;

    for (int v = 1; v <= maxVerses; v++) {
        vk.setVerse(v);
        mod->setKey(&vk);
        mod->setPosition(0);

        sword::SWBuf rendered = mod->renderText();
        if (rendered.c_str()[0] == '\0') {
            continue;
        }

        VerseText vt;
        vt.verse = vk.getVerse();
        vt.xhtml = std::string(rendered.c_str());

        if (opts.footnotes) {
            std::string xhtml(rendered.c_str());
            std::regex noteRegex("<note n=\"(\\d+)\">", std::regex::ECMAScript | std::regex::icase);
            for (std::sregex_iterator it(xhtml.begin(), xhtml.end(), noteRegex), end_it{}; it != end_it; ++it) {
                std::smatch match = *it;
                size_t noteStart = match.position();
                size_t noteEnd = xhtml.find("</note>", noteStart);
                if (noteEnd != std::string::npos) {
                    vt.notes[match[1].str()] = xhtml.substr(noteStart, noteEnd - noteStart);
                }
            }
            std::regex noteRemove("<note[^>]*>.*?</note>", std::regex::ECMAScript | std::regex::icase);
            xhtml = std::regex_replace(xhtml, noteRemove, "");
            vt.xhtml = xhtml;
        }

        result.push_back(vt);
    }

    return result;
}

std::string SwordLibrary::Entry(const std::string& moduleName,
                                  const std::string& key,
                                  const DisplayOptions&) {
    auto* mod = d->mgr->getModule(moduleName.c_str());
    if (!mod) return "";

    // For Strong's dictionary modules, map "G25"/"H430" to numeric key "25"/"430"
    std::string actualKey = key;
    if (!key.empty() && (key[0] == 'G' || key[0] == 'H')) {
        actualKey = strongsToKey(key);
    }

    sword::StrKey sk(actualKey.c_str());
    mod->setKey(&sk);
    sword::SWBuf rendered = mod->renderText();
    std::string result = std::string(rendered.c_str());
    if (!result.empty()) {
        // For dictionary modules (rawtext-based), check that the entry
        // contains meaningful content. Auto-generated placeholders for
        // non-existent keys contain "@@@" patterns and lack real dictionary data.
        if (result.find("@@@@") != std::string::npos && result.find("=====") == std::string::npos) {
            return "";
        }
        return result;
    }

    // Try VerseKey fallback only for Bible/commentary modules (keys that look like refs)
    // Check if this is a Bible-type module
    std::string modType = mapModuleType(mod->getType());
    if (modType == "Bible" || modType == "Commentary") {
        // Only try VerseKey if the key looks like a reference
        bool looksLikeRef = key.find(':') != std::string::npos ||
                            key.find('.') != std::string::npos;
        if (looksLikeRef) {
            sword::VerseKey vk(actualKey.c_str());
            mod->setKey(&vk);
            const char* txt = mod->getKey()->getText();
            if (txt != nullptr && txt[0] != 0) {
                sword::SWBuf rendered = mod->renderText();
                if (rendered.c_str()[0] != '\0') {
                    return std::string(rendered.c_str());
                }
            }
        }
    }

    return "";
}

std::vector<std::string> SwordLibrary::Keys(const std::string& moduleName,
                                              const std::string& prefix,
                                              int limit) const {
    auto* mod = d->mgr->getModule(moduleName.c_str());
    if (!mod) return {};

    std::vector<std::string> result;

    // For dictionary modules, use a StrKey and iterate
    std::string moduleType = mapModuleType(mod->getType());
    if (moduleType == "Dictionary") {
        // Try iterating via StrKey
        sword::StrKey sk("");
        mod->setKey(&sk);
        for (int i = 1; static_cast<int>(result.size()) < limit; i++) {
            std::string num = std::to_string(i);
            sword::StrKey sk2(num.c_str());
            mod->setKey(&sk2);
            const char* txt = mod->getKey()->getText();
            if (txt == nullptr || txt[0] == 0) continue;
            sword::SWBuf rendered = mod->renderText();
            if (rendered.c_str()[0] == '\0') continue;
            // Check if key matches prefix
            std::string displayKey;
            // Try to determine prefix from module name
            if (moduleName.find("Greek") != std::string::npos || moduleName.find("Greek") != std::string::npos) {
                displayKey = "G" + num;
            } else if (moduleName.find("Hebrew") != std::string::npos) {
                displayKey = "H" + num;
            } else {
                displayKey = num;
            }
            if (displayKey.find(prefix) == 0) {
                result.push_back(displayKey);
            }
        }
        return result;
    }

    // For Bible modules, use VersificationMgr to return book names
    auto* sys = d->getVersificationForModule(moduleName);
    if (sys) {
        int bookCount = sys->getBookCount();
        for (int i = 0; i < bookCount && static_cast<int>(result.size()) < limit; i++) {
            const auto* book = sys->getBook(i);
            if (book) {
                std::string osisName = book->getOSISName();
                if (osisName.find(prefix) == 0) {
                    result.push_back(osisName);
                }
            }
        }
    }

    return result;
}

}
