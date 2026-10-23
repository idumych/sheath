#pragma once
#include <map>
#include <string>
#include <vector>
namespace sheath {

struct ModuleInfo {
    std::string name;         // "KJV"
    std::string description;  // conf Description
    std::string type;         // "Bible", "Commentary", "Dictionary" or "Book"
    std::string language;     // conf Lang, e.g. "en"
    std::string version;      // conf Version, "" if none
    std::string versification;// conf Versification, "KJV" if none
    bool strongs = false, morph = false, footnotes = false, headings = false, redLetter = false;
};

struct DisplayOptions {
    bool strongs = false, morph = false, footnotes = true, crossRefs = true,
         headings = true, redLetter = true;
};

struct BookInfo { std::string osis; std::string prefAbbrev; std::string name; int chapters = 0; int testament = 0; };

struct VerseText {
    int verse = 0;
    std::string xhtml;                        // SWORD FMT_XHTML render of the verse
    std::map<std::string, std::string> notes; // footnote id ("1", "2"...) -> body as xhtml
};

enum class SearchMode {
    AllWords,   // every word, any order, within one verse (case-insensitive)
    Phrase,     // the exact phrase (case-insensitive)
    Regex,      // POSIX extended regex on the plain text (case-insensitive)
    Strongs,    // verses tagged with this Strong's number ("G25", "H0430"; exact match)
};
struct SearchHit { std::string book; int chapter = 0; int verse = 0; std::string text; };  // text: plain
struct SearchResult { std::vector<SearchHit> hits; int total = 0; };   // hits capped; total = all

class SwordLibrary {
public:
    // swordPath "" = SWORD's default search (SWORD_PATH, ~/.sword, /usr/share/sword...).
    explicit SwordLibrary(const std::string& swordPath = "");
    ~SwordLibrary();
    void Reload();                                    // rescan after install/remove
    std::vector<ModuleInfo> Modules() const;          // sorted by name
    bool Find(const std::string& module, ModuleInfo* out = nullptr) const;
    std::vector<BookInfo> Books(const std::string& module) const;  // in canonical order
    int VerseCount(const std::string& module, const std::string& bookOsis, int chapter) const;
    // One chapter of a Bible or commentary. Skips empty verses.
    std::vector<VerseText> Chapter(const std::string& module, const std::string& bookOsis,
                                   int chapter, const DisplayOptions& opts);
    // Dictionary/general book entry by key ("G25", "0025", "Love"), rendered as xhtml; "" if missing.
    // For Strong's dictionaries a key "G25"/"H430" is mapped to the module's key form.
    std::string Entry(const std::string& module, const std::string& key, const DisplayOptions& opts);
    std::vector<std::string> Keys(const std::string& module, const std::string& prefix, int limit) const;
    // Searches a Bible or commentary. scope "" = whole module, else a verse list SWORD parses
    // ("John", "Matt-John", "Gen 1-3"). At most `limit` hits are returned (in canonical order).
    SearchResult Search(const std::string& module, const std::string& query, SearchMode mode,
                        const std::string& scope, int limit);
private:
    struct Impl; Impl* d;
};
}
