#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace sheath {

struct Bookmark {
	std::string book;		// OSIS id
	int chapter = 0;
	int verse = 0;
	std::string note;		// free text, may contain tabs and newlines
	int64_t created = 0;	// seconds since the epoch
};

// Verse bookmarks with an optional note, kept in a small text file (one bookmark per line,
// tab-separated, \t \n \\ escaped). Missing or unreadable lines are skipped on load.
class BookmarkStore {
public:
	explicit BookmarkStore(const std::string& path);	// loads the file if it exists

	const std::vector<Bookmark>& All() const { return fItems; }	// in the order added
	const Bookmark* Find(const std::string& book, int chapter, int verse) const;
	void Set(const Bookmark& b);	// adds, or replaces the note of the same verse (keeps created)
	bool Remove(const std::string& book, int chapter, int verse);	// false if not there
	// Writes the file (via a temporary file and rename). false + error on failure.
	bool Save(std::string* error) const;

private:
	std::string fPath;
	std::vector<Bookmark> fItems;
};
}
