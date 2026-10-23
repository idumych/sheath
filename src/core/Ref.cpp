#include "Ref.h"

#include <listkey.h>
#include <versekey.h>
#include <versificationmgr.h>

#include <cctype>
#include <memory>

namespace sheath {

namespace {

// True if the text names a verse ("3:16", "3.16", "John.3.16") rather than a whole chapter.
bool NamesVerse(const std::string& text)
{
	size_t lastDigitRun = text.find_last_not_of("0123456789-– ");
	if (lastDigitRun == std::string::npos)
		return false;
	// Look for a ':' or '.' separating chapter and verse after the book name.
	for (size_t i = 1; i + 1 < text.size(); i++) {
		if ((text[i] == ':' || text[i] == '.') && std::isdigit((unsigned char)text[i - 1])
				&& std::isdigit((unsigned char)text[i + 1]))
			return true;
	}
	return false;
}

}	// namespace


bool ParseRef(const std::string& text, VerseRef* out, const std::string& v11n)
{
	if (text.find_first_not_of(" \t") == std::string::npos)
		return false;
	sword::VerseKey vk;
	vk.setVersificationSystem(v11n.c_str());
	vk.setAutoNormalize(true);
	sword::ListKey list = vk.parseVerseList(text.c_str(), nullptr, true);
	if (list.getCount() == 0 || list.popError())
		return false;
	sword::SWKey* first = list.getElement(0);
	sword::VerseKey* key = dynamic_cast<sword::VerseKey*>(first);
	if (key == nullptr)
		return false;
	// A range "Gen 1:1-3" or a whole chapter comes back as a range key; take its start.
	std::unique_ptr<sword::VerseKey> start(new sword::VerseKey(key->getLowerBound()));
	if (start->getBook() == 0 || start->getChapter() == 0)
		return false;
	if (out) {
		out->book = start->getOSISBookName();
		out->chapter = start->getChapter();
		out->verse = NamesVerse(text) ? start->getVerse() : 0;
	}
	return true;
}


std::string FormatRef(const VerseRef& r)
{
	const sword::VersificationMgr::System* sys =
		sword::VersificationMgr::getSystemVersificationMgr()->getVersificationSystem("KJV");
	std::string name = r.book;
	int index = sys ? sys->getBookNumberByOSISName(r.book.c_str()) : 0;	// 1-based
	if (index > 0)
		name = sys->getBook(index - 1)->getLongName();
	std::string s = name + " " + std::to_string(r.chapter);
	if (r.verse > 0)
		s += ":" + std::to_string(r.verse);
	return s;
}


std::vector<VerseRef> ParseOsisRefList(const std::string& osisRefs)
{
	std::vector<VerseRef> result;
	size_t pos = 0;
	while (pos < osisRefs.size()) {
		size_t end = osisRefs.find_first_of(" ;", pos);
		std::string item = osisRefs.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
		pos = end == std::string::npos ? osisRefs.size() : end + 1;
		size_t dash = item.find('-');
		if (dash != std::string::npos)
			item.erase(dash);		// a range: keep its start
		size_t colon = item.find(':');
		if (colon != std::string::npos && item.find('.') > colon)
			item.erase(0, colon + 1);	// "KJV:John.3.16" -> "John.3.16"
		VerseRef r;
		if (!item.empty() && ParseRef(item, &r))
			result.push_back(r);
	}
	return result;
}

}	// namespace sheath
