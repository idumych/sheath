// Spec tests for BookmarkStore. Written by Claude.
#include "check.h"
#include "Bookmarks.h"

#include <fstream>

using namespace sheath;

static std::string read_file(const std::string& path)
{
	std::ifstream in(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

int main()
{
	std::string path = scratch_dir() + "/bookmarks.txt";
	std::remove(path.c_str());

	BookmarkStore missing(path);
	check_true(missing.All().empty(), "missing file: empty store");

	BookmarkStore s(path);
	s.Set({"John", 3, 16, "", 1000});
	s.Set({"Ps", 23, 1, "The LORD\tis my shepherd\nline two \\ backslash", 2000});
	s.Set({"Gen", 1, 1, "start", 3000});
	check_eq(s.All().size(), 3u, "set: three bookmarks");
	check_true(s.Find("Ps", 23, 1) != nullptr, "find: Ps 23:1");
	check_true(s.Find("Ps", 23, 2) == nullptr, "find: Ps 23:2 not there");

	s.Set({"John", 3, 16, "updated note", 9999});
	check_eq(s.All().size(), 3u, "set same verse: still three");
	const Bookmark* j = s.Find("John", 3, 16);
	check_true(j && j->note == "updated note" && j->created == 1000,
		"set same verse: note replaced, created kept");
	check_true(s.All()[0].book == "John", "order: as added");

	std::string error;
	check_true(s.Save(&error), "save", error);
	BookmarkStore loaded(path);
	check_eq(loaded.All().size(), 3u, "load: three bookmarks");
	const Bookmark* p = loaded.Find("Ps", 23, 1);
	check_true(p && p->note == "The LORD\tis my shepherd\nline two \\ backslash",
		"load: note with tab, newline and backslash round-trips");
	check_true(p && p->created == 2000, "load: created");
	check_true(loaded.All()[2].book == "Gen" && loaded.All()[2].note == "start", "load: order kept");
	std::string text = read_file(path);
	int lines = 0;
	for (char c : text) lines += c == '\n';
	check_eq(lines, 3, "file: one line per bookmark");

	check_true(loaded.Remove("John", 3, 16), "remove John 3:16");
	check_true(!loaded.Remove("John", 3, 16), "remove again: false");
	check_true(loaded.Save(&error), "save after remove", error);
	check_eq(BookmarkStore(path).All().size(), 2u, "reload after remove: two");

	// A damaged line is skipped, the rest still loads.
	{
		std::ofstream out(path, std::ios::app);
		out << "garbage line without tabs\nRev\tx\t1\t\t5\n";
	}
	BookmarkStore damaged(path);
	check_eq(damaged.All().size(), 2u, "damaged lines skipped");

	BookmarkStore bad(scratch_dir() + "/no/such/dir/bookmarks.txt");
	bad.Set({"Gen", 1, 1, "", 1});
	error.clear();
	check_true(!bad.Save(&error) && !error.empty(), "save into a missing dir fails with a message");
	return check_result();
}
