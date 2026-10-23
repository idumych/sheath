// Spec tests for ImportMyBible (src/core/MyBible.h). Written by Claude. The test builds a small
// MyBible file from the KJV fixture's own text, imports it, and reads it back with SwordLibrary:
// the imported verses must have the same plain text as the KJV, with the markup mapped.
#include "check.h"
#include "Markup.h"
#include "MyBible.h"
#include "Sword.h"

#include <sqlite3.h>
#include <fstream>

using namespace sheath;

static std::string plain(SwordLibrary& lib, const std::string& module, const std::string& book,
	int chapter, int verse, const DisplayOptions& o)
{
	for (const VerseText& v : lib.Chapter(module, book, chapter, o)) {
		if (v.verse == verse)
			return collapse_ws(PlainText(ParseXhtml(v.xhtml)));
	}
	return "<missing>";
}

static const VerseText* find(const std::vector<VerseText>& ch, int verse)
{
	for (const VerseText& v : ch)
		if (v.verse == verse) return &v;
	return nullptr;
}

static bool has_run(const VerseText* v, unsigned style, const std::string& text = "")
{
	if (v == nullptr) return false;
	for (const Block& b : ParseXhtml(v->xhtml))
		for (const Run& r : b.runs)
			if ((r.style & style) && (text.empty() || collapse_ws(r.text) == text)) return true;
	return false;
}

static bool has_link(const VerseText* v, LinkKind kind, const std::string& target)
{
	if (v == nullptr) return false;
	for (const Block& b : ParseXhtml(v->xhtml))
		for (const Run& r : b.runs)
			if (r.link.kind == kind && r.link.target == target) return true;
	return false;
}

static std::string replace_once(std::string s, const std::string& from, const std::string& to)
{
	size_t p = s.find(from);
	if (p != std::string::npos) s.replace(p, from.size(), to);
	return s;
}

struct Row { int book; int chapter; int verse; std::string text; };

static bool make_mybible(const std::string& path, const std::vector<Row>& rows)
{
	std::remove(path.c_str());
	sqlite3* db = nullptr;
	if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) return false;
	const char* schema =
		"CREATE TABLE info (name TEXT, value TEXT);"
		"INSERT INTO info VALUES ('description', 'Test Bible from MyBible');"
		"INSERT INTO info VALUES ('language', 'en');"
		"INSERT INTO info VALUES ('detailed_info', 'ignored');"
		"CREATE TABLE books (book_number NUMERIC, short_name TEXT, long_name TEXT);"
		"CREATE TABLE verses (book_number INTEGER, chapter INTEGER, verse INTEGER, text TEXT);";
	bool ok = sqlite3_exec(db, schema, nullptr, nullptr, nullptr) == SQLITE_OK;
	sqlite3_stmt* st = nullptr;
	ok = ok && sqlite3_prepare_v2(db, "INSERT INTO verses VALUES (?,?,?,?)", -1, &st, nullptr) == SQLITE_OK;
	for (const Row& r : rows) {
		if (!ok) break;
		sqlite3_bind_int(st, 1, r.book);
		sqlite3_bind_int(st, 2, r.chapter);
		sqlite3_bind_int(st, 3, r.verse);
		sqlite3_bind_text(st, 4, r.text.c_str(), -1, SQLITE_TRANSIENT);
		ok = sqlite3_step(st) == SQLITE_DONE;
		sqlite3_reset(st);
	}
	sqlite3_finalize(st);
	sqlite3_close(db);
	return ok;
}

int main()
{
	SwordLibrary kjv(fixture_dir());
	DisplayOptions off;
	off.strongs = off.morph = off.footnotes = off.crossRefs = off.headings = off.redLetter = false;

	std::string j316 = plain(kjv, "KJV", "John", 3, 16, off);
	std::string j317 = plain(kjv, "KJV", "John", 3, 17, off);
	std::string ps231 = plain(kjv, "KJV", "Ps", 23, 1, off);
	std::string rom828 = plain(kjv, "KJV", "Rom", 8, 28, off);
	std::string gen11 = plain(kjv, "KJV", "Gen", 1, 1, off);
	std::string phil413 = plain(kjv, "KJV", "Phil", 4, 13, off);
	std::string jas11 = plain(kjv, "KJV", "Jas", 1, 1, off);
	std::string rev2221 = plain(kjv, "KJV", "Rev", 22, 21, off);
	check_true(j316.find("loved") != std::string::npos, "fixture: KJV John 3:16 available", j316);

	std::vector<Row> rows = {
		{500, 3, 16, "<J>" + replace_once(j316, "loved", "loved<S>25</S>") + "</J>"},
		{500, 3, 17, j317},
		{230, 23, 1, replace_once(ps231, " is ", " <i>is</i> ")},
		{590, 8, 28, rom828},
		{10, 1, 1, replace_once(replace_once(gen11, "beginning", "beginning<S>7225</S>"),
			"God", "God<S>430</S>")},
		{500, 1, 1, "In the beginning was the Word<f>[1] Or, reason</f>, and the Word was with God."},
		{640, 4, 13, "<pb/>" + phil413},
		{520, 1, 1, jas11 + "<n>a note</n><m>V-PAI-3S</m>"},
		{730, 22, 21, rev2221},
		{500, 2, 1, "Jesus & Mary <e>went</e> to Cana<br/>in Galilee."},
		{165, 1, 1, "A deuterocanonical verse"},     // unknown book: skipped
		{500, 99, 1, "Chapter out of range"},         // skipped
	};
	std::string tmp = scratch_dir();
	std::string file = tmp + "/MBTEST.SQLite3";
	check_true(make_mybible(file, rows), "fixture: MyBible file written");

	std::string user = tmp + "/user";
	ImportResult res;
	std::string error;
	bool ok = ImportMyBible(file, user, "", &res, &error);
	check_true(ok, "import: succeeds", error);
	check_eq(res.module, std::string("MBTEST"), "import: name from the file name");
	check_eq(res.verses, 10, "import: verses written");
	check_eq(res.skipped, 2, "import: verses skipped");

	SwordLibrary lib(user);
	ModuleInfo info;
	check_true(lib.Find("MBTEST", &info), "library: finds MBTEST");
	check_eq(info.type, std::string("Bible"), "library: type Bible");
	check_eq(info.description, std::string("Test Bible from MyBible"), "library: description");
	check_eq(info.language, std::string("en"), "library: language");
	check_true(info.strongs && info.redLetter && info.footnotes, "library: Strong's, red letter, footnotes");

	check_eq(plain(lib, "MBTEST", "John", 3, 16, off), j316, "text: John 3:16 equals KJV");
	check_eq(plain(lib, "MBTEST", "John", 3, 17, off), j317, "text: John 3:17 equals KJV");
	check_eq(plain(lib, "MBTEST", "Ps", 23, 1, off), ps231, "text: Ps 23:1 equals KJV");
	check_eq(plain(lib, "MBTEST", "Rom", 8, 28, off), rom828, "text: Rom 8:28 (book 590) equals KJV");
	check_eq(plain(lib, "MBTEST", "Gen", 1, 1, off), gen11, "text: Gen 1:1 equals KJV");
	check_eq(plain(lib, "MBTEST", "Phil", 4, 13, off), phil413, "text: Phil 4:13 (book 640) equals KJV");
	check_eq(plain(lib, "MBTEST", "Jas", 1, 1, off), jas11, "text: Jas 1:1 (book 520), notes removed");
	check_eq(plain(lib, "MBTEST", "Rev", 22, 21, off), rev2221, "text: Rev 22:21 (book 730) equals KJV");
	check_eq(plain(lib, "MBTEST", "John", 2, 1, off), std::string("Jesus & Mary went to Cana in Galilee."),
		"text: & escaped, <e> and <br/> mapped");
	check_eq(plain(lib, "MBTEST", "John", 1, 1, off),
		std::string("In the beginning was the Word, and the Word was with God."),
		"text: footnote not in the text");
	check_eq(lib.Chapter("MBTEST", "John", 4, off).size(), 0u, "text: John 4 empty (not in the file)");

	DisplayOptions o = off;
	o.strongs = true;
	auto john3 = lib.Chapter("MBTEST", "John", 3, o);
	check_true(has_link(find(john3, 16), LinkKind::Strongs, "G25"), "strongs: John 3:16 has G25");
	auto gen1 = lib.Chapter("MBTEST", "Gen", 1, o);
	check_true(has_link(find(gen1, 1), LinkKind::Strongs, "H7225")
		&& has_link(find(gen1, 1), LinkKind::Strongs, "H430"), "strongs: Gen 1:1 has H7225 and H430");

	DisplayOptions red = off;
	red.redLetter = true;
	auto john3r = lib.Chapter("MBTEST", "John", 3, red);
	check_true(has_run(find(john3r, 16), kRedLetter), "red letter: John 3:16");
	check_true(!has_run(find(john3r, 17), kRedLetter), "red letter: not John 3:17");
	auto ps23 = lib.Chapter("MBTEST", "Ps", 23, off);
	check_true(has_run(find(ps23, 1), kTransChange, "is"), "added words: Ps 23:1 'is'");

	DisplayOptions notes = off;
	notes.footnotes = true;
	const VerseText* j11 = find(lib.Chapter("MBTEST", "John", 1, notes), 1);
	bool noteFound = false;
	if (j11) for (auto& n : j11->notes) noteFound = noteFound || n.second.find("Or, reason") != std::string::npos;
	check_true(noteFound, "footnote: John 1:1 note body 'Or, reason'");

	error.clear();
	check_true(!ImportMyBible(file, user, "", &res, &error) && !error.empty(),
		"again: same name fails with a message");
	check_true(ImportMyBible(file, user, "Second", &res, &error) && res.module == "Second",
		"again: under another name works", error);
	check_true(SwordLibrary(user).Find("Second"), "again: library finds Second");

	std::string notdb = tmp + "/notes.SQLite3";
	{ std::ofstream(notdb) << "this is not a database"; }
	error.clear();
	check_true(!ImportMyBible(notdb, user, "", &res, &error) && !error.empty(), "bad: not SQLite");
	std::string empty = tmp + "/Empty.SQLite3";
	{
		sqlite3* db = nullptr;
		sqlite3_open(empty.c_str(), &db);
		sqlite3_exec(db, "CREATE TABLE x (a TEXT);", nullptr, nullptr, nullptr);
		sqlite3_close(db);
	}
	error.clear();
	check_true(!ImportMyBible(empty, user, "", &res, &error) && !error.empty(), "bad: no verses table");
	check_true(!ImportMyBible(tmp + "/missing.SQLite3", user, "", &res, &error), "bad: missing file");
	return check_result();
}
