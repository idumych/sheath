#include "Sword.h"

#include <listkey.h>
#include <swmgr.h>
#include <swmodule.h>
#include <markupfiltmgr.h>
#include <versekey.h>
#include <versificationmgr.h>

#include <regex.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace sheath {

namespace {

std::string ConfigEntry(sword::SWModule* mod, const char* key)
{
	const char* v = mod->getConfigEntry(key);
	return v ? v : "";
}

std::string TypeOf(sword::SWModule* mod)
{
	std::string t = mod->getType();
	if (t.find("Bibl") != std::string::npos) return "Bible";
	if (t.find("Commentar") != std::string::npos) return "Commentary";
	if (t.find("Lexicon") != std::string::npos || t.find("Dictionar") != std::string::npos)
		return "Dictionary";
	return "Book";
}

bool HasFilter(sword::SWModule* mod, const char* name)
{
	const sword::ConfigEntMap& cfg = mod->getConfig();
	for (auto it = cfg.lower_bound("GlobalOptionFilter");
			it != cfg.upper_bound("GlobalOptionFilter"); ++it) {
		if (it->second == name)
			return true;
	}
	return false;
}

bool HasFeature(sword::SWModule* mod, const char* name)
{
	const sword::ConfigEntMap& cfg = mod->getConfig();
	for (auto it = cfg.lower_bound("Feature"); it != cfg.upper_bound("Feature"); ++it) {
		if (it->second == name)
			return true;
	}
	return false;
}

bool StartsWithNoCase(const std::string& s, const std::string& prefix)
{
	if (s.size() < prefix.size())
		return false;
	for (size_t i = 0; i < prefix.size(); i++) {
		if (std::toupper((unsigned char)s[i]) != std::toupper((unsigned char)prefix[i]))
			return false;
	}
	return true;
}

}	// namespace


struct SwordLibrary::Impl {
	std::string path;
	sword::SWMgr* mgr = nullptr;

	explicit Impl(const std::string& p) : path(p) { Load(); }
	~Impl() { delete mgr; }

	void Load()
	{
		delete mgr;
		sword::MarkupFilterMgr* filters = new sword::MarkupFilterMgr(sword::FMT_XHTML);
		if (path.empty())
			mgr = new sword::SWMgr(filters);
		else
			mgr = new sword::SWMgr(path.c_str(), true, filters);
	}

	sword::SWModule* Module(const std::string& name) const
	{
		return mgr->getModule(name.c_str());
	}

	void SetOptions(const DisplayOptions& o)
	{
		mgr->setGlobalOption("Strong's Numbers", o.strongs ? "On" : "Off");
		mgr->setGlobalOption("Morphological Tags", o.morph ? "On" : "Off");
		mgr->setGlobalOption("Footnotes", o.footnotes ? "On" : "Off");
		mgr->setGlobalOption("Cross-references", o.crossRefs ? "On" : "Off");
		mgr->setGlobalOption("Headings", o.headings ? "On" : "Off");
		mgr->setGlobalOption("Words of Christ in Red", o.redLetter ? "On" : "Off");
	}

	const sword::VersificationMgr::System* System(sword::SWModule* mod) const
	{
		std::string v11n = ConfigEntry(mod, "Versification");
		if (v11n.empty())
			v11n = "KJV";
		return sword::VersificationMgr::getSystemVersificationMgr()
			->getVersificationSystem(v11n.c_str());
	}

	// The module's own verse key (its versification), positioned at book/chapter/1.
	// nullptr if the module has no verse key or doesn't know the book or chapter.
	sword::VerseKey* ChapterKey(sword::SWModule* mod, const std::string& book, int chapter) const
	{
		sword::VerseKey* vk = dynamic_cast<sword::VerseKey*>(mod->createKey());
		if (vk == nullptr)
			return nullptr;
		vk->setIntros(false);
		vk->setText((book + "." + std::to_string(chapter) + ".1").c_str());
		if (vk->popError() || book != vk->getOSISBookName() || vk->getChapter() != chapter
				|| chapter < 1 || chapter > vk->getChapterMax()) {
			delete vk;
			return nullptr;
		}
		vk->setVerse(1);
		return vk;
	}
};


SwordLibrary::SwordLibrary(const std::string& swordPath) : d(new Impl(swordPath)) {}
SwordLibrary::~SwordLibrary() { delete d; }

void SwordLibrary::Reload() { d->Load(); }


std::vector<ModuleInfo> SwordLibrary::Modules() const
{
	std::vector<ModuleInfo> result;
	for (auto& entry : d->mgr->getModules()) {
		sword::SWModule* mod = entry.second;
		ModuleInfo info;
		info.name = mod->getName();
		info.description = mod->getDescription() ? mod->getDescription() : "";
		info.type = TypeOf(mod);
		info.language = mod->getLanguage() ? mod->getLanguage() : "";
		info.version = ConfigEntry(mod, "Version");
		info.versification = ConfigEntry(mod, "Versification");
		if (info.versification.empty())
			info.versification = "KJV";
		info.strongs = HasFilter(mod, "OSISStrongs") || HasFilter(mod, "GBFStrongs")
			|| HasFilter(mod, "ThMLStrongs") || HasFeature(mod, "StrongsNumbers");
		info.morph = HasFilter(mod, "OSISMorph") || HasFilter(mod, "GBFMorph")
			|| HasFilter(mod, "ThMLMorph");
		info.footnotes = HasFilter(mod, "OSISFootnotes") || HasFilter(mod, "GBFFootnotes")
			|| HasFilter(mod, "ThMLFootnotes");
		info.headings = HasFilter(mod, "OSISHeadings") || HasFilter(mod, "ThMLHeadings");
		info.redLetter = HasFilter(mod, "OSISRedLetterWords") || HasFilter(mod, "GBFRedLetterWords");
		result.push_back(info);
	}
	std::sort(result.begin(), result.end(),
		[](const ModuleInfo& a, const ModuleInfo& b) { return a.name < b.name; });
	return result;
}


bool SwordLibrary::Find(const std::string& module, ModuleInfo* out) const
{
	for (const ModuleInfo& m : Modules()) {
		if (m.name == module) {
			if (out)
				*out = m;
			return true;
		}
	}
	return false;
}


std::vector<BookInfo> SwordLibrary::Books(const std::string& module) const
{
	std::vector<BookInfo> books;
	sword::SWModule* mod = d->Module(module);
	const sword::VersificationMgr::System* sys = mod ? d->System(mod) : nullptr;
	if (sys == nullptr)
		return books;
	int otBooks = sys->getBMAX()[0];
	for (int i = 0; i < sys->getBookCount(); i++) {
		const sword::VersificationMgr::Book* b = sys->getBook(i);
		BookInfo info;
		info.osis = b->getOSISName();
		info.prefAbbrev = b->getPreferredAbbreviation();
		info.name = b->getLongName();
		info.chapters = b->getChapterMax();
		info.testament = i < otBooks ? 1 : 2;
		books.push_back(info);
	}
	return books;
}


int SwordLibrary::VerseCount(const std::string& module, const std::string& book, int chapter) const
{
	sword::SWModule* mod = d->Module(module);
	if (mod == nullptr)
		return 0;
	sword::VerseKey* vk = d->ChapterKey(mod, book, chapter);
	if (vk == nullptr)
		return 0;
	int n = vk->getVerseMax();
	delete vk;
	return n;
}


std::vector<VerseText> SwordLibrary::Chapter(const std::string& module, const std::string& book,
	int chapter, const DisplayOptions& opts)
{
	std::vector<VerseText> result;
	sword::SWModule* mod = d->Module(module);
	if (mod == nullptr)
		return result;
	sword::VerseKey* vk = d->ChapterKey(mod, book, chapter);
	if (vk == nullptr)
		return result;
	d->SetOptions(opts);

	int max = vk->getVerseMax();
	for (int v = 1; v <= max; v++) {
		vk->setVerse(v);
		mod->setKey(vk);
		std::string xhtml = mod->renderText().c_str();
		if (xhtml.find_first_not_of(" \t\r\n") == std::string::npos)
			continue;

		VerseText text;
		text.verse = v;
		text.xhtml = xhtml;
		if (opts.footnotes) {
			// Footnote bodies aren't in the rendered text, only a marker linking to the id.
			sword::AttributeList& notes = mod->getEntryAttributes()["Footnote"];
			for (auto& note : notes) {
				auto body = note.second.find("body");
				if (body != note.second.end())
					text.notes[note.first.c_str()] = mod->renderText(body->second.c_str()).c_str();
			}
		}
		result.push_back(text);
	}
	delete vk;
	return result;
}


std::string SwordLibrary::Entry(const std::string& module, const std::string& key,
	const DisplayOptions& opts)
{
	sword::SWModule* mod = d->Module(module);
	if (mod == nullptr || key.empty())
		return "";
	d->SetOptions(opts);

	// Strong's dictionaries key their entries by number ("00025"); SWORD pads it itself.
	bool strongs = HasFeature(mod, "GreekDef") || HasFeature(mod, "HebrewDef");
	std::string want = key;
	if (strongs && (key[0] == 'G' || key[0] == 'H' || key[0] == 'g' || key[0] == 'h'))
		want = key.substr(1);

	mod->setKey(want.c_str());
	if (mod->popError())
		return "";
	// A lexicon jumps to the nearest key; only an exact landing is a hit. (Lexicon drivers
	// only know the key they landed on after reading the entry.)
	std::string body = mod->renderText().c_str();
	std::string landed = mod->getKeyText();
	bool exact = strongs ? std::atol(landed.c_str()) == std::atol(want.c_str())
			&& want.find_first_not_of("0123456789") == std::string::npos
		: landed.size() == want.size() && StartsWithNoCase(landed, want);
	return exact ? body : "";
}


std::vector<std::string> SwordLibrary::Keys(const std::string& module, const std::string& prefix,
	int limit) const
{
	std::vector<std::string> result;
	sword::SWModule* mod = d->Module(module);
	if (mod == nullptr || limit <= 0)
		return result;

	if (TypeOf(mod) == "Bible" || TypeOf(mod) == "Commentary") {
		for (const BookInfo& b : Books(module)) {
			if ((int)result.size() >= limit)
				break;
			if (StartsWithNoCase(b.osis, prefix) || StartsWithNoCase(b.name, prefix))
				result.push_back(b.osis);
		}
		return result;
	}

	// Lexicons and books: position near the prefix, walk forward while keys match. The driver
	// may land before the prefix (Strong's padding turns "0002" into "00002"), so skip ahead
	// a bounded number of keys first.
	mod->setKey(prefix.c_str());
	mod->popError();
	int skip = 0;
	while ((int)result.size() < limit) {
		mod->getRawEntry();
		std::string k = mod->getKeyText();
		if (!StartsWithNoCase(k, prefix)) {
			bool before = strcasecmp(k.c_str(), prefix.c_str()) < 0;
			if (!result.empty() || !before || ++skip > 5000)
				break;
			mod->increment();
			if (mod->popError())
				break;
			continue;
		}
		if (result.empty() || result.back() != k)
			result.push_back(k);
		mod->increment();
		if (mod->popError())
			break;
	}
	return result;
}


SearchResult SwordLibrary::Search(const std::string& module, const std::string& query,
	SearchMode mode, const std::string& scope, int limit)
{
	SearchResult result;
	sword::SWModule* mod = d->Module(module);
	if (mod == nullptr || query.find_first_not_of(" \t") == std::string::npos)
		return result;
	sword::VerseKey* key = dynamic_cast<sword::VerseKey*>(mod->createKey());
	if (key == nullptr)
		return result;

	std::string term = query;
	int type = -1;
	int flags = REG_ICASE;
	switch (mode) {
		case SearchMode::Phrase: type = -1; break;
		case SearchMode::AllWords: type = -2; break;
		case SearchMode::Regex: type = 0; flags |= REG_EXTENDED; break;
		case SearchMode::Strongs:
			// Entry attributes Word/<n>/Lemma[.k] hold one Strong's number each; "Lemma." also
			// matches the numbered components of words tagged with several numbers.
			type = -3;
			flags |= SEARCHFLAG_MATCHWHOLEENTRY;
			term = "Word//Lemma./" + query + "/";
			break;
	}
	// Strong's attributes only exist while the Strong's filter is on.
	DisplayOptions options;
	options.strongs = mode == SearchMode::Strongs;
	d->SetOptions(options);

	sword::ListKey scopeList;
	sword::ListKey hits;
	if (scope.empty()) {
		hits = mod->search(term.c_str(), type, flags);
	} else {
		scopeList = key->parseVerseList(scope.c_str(), "", true);
		if (scopeList.getCount() == 0) {
			delete key;
			return result;
		}
		hits = mod->search(term.c_str(), type, flags, &scopeList);
	}

	result.total = hits.getCount();
	for (hits = sword::TOP; !hits.popError() && (int)result.hits.size() < limit; hits++) {
		key->setText(hits.getText());
		SearchHit hit;
		hit.book = key->getOSISBookName();
		hit.chapter = key->getChapter();
		hit.verse = key->getVerse();
		mod->setKey(key);
		hit.text = mod->stripText();
		result.hits.push_back(hit);
	}
	delete key;
	return result;
}

}	// namespace sheath
