// Spec tests for SwordLibrary::Search. Written by Claude. Expected results come from diatheke
// (the SWORD command-line front end) run at test time, except Strong's search: diatheke's
// attribute search matches substrings (G25 also finds G2532), so that one is checked against
// the Strong's links in the rendered chapters instead.
#include "check.h"
#include "Markup.h"
#include "Ref.h"
#include "Sword.h"

#include <set>

using namespace sheath;

// "Entries containing "x"-- John 1:14John 1:18 ; John 3:16 ;  -- 6 matches total (KJV)"
// -> the refs and the total. diatheke omits the separator after the first ref.
struct DiathekeResult { std::vector<std::string> refs; int total = -1; };

static DiathekeResult diatheke_search(const std::string& type, const std::string& query,
	const std::string& range = "")
{
	const char* d = std::getenv("DIATHEKE");
	std::string cmd = std::string(d ? d : "diatheke") + " -b KJV -s " + type
		+ (range.empty() ? "" : " -r '" + range + "'") + " -k '" + query + "'";
	std::string out = run_cmd(cmd);
	DiathekeResult r;
	size_t start = out.find("\"-- ");
	size_t end = out.rfind(" -- ");
	if (start == std::string::npos || end == std::string::npos)
		return r;
	r.total = std::atoi(out.c_str() + end + 4);
	std::string list = out.substr(start + 4, end - start - 4);
	// Split on " ; " and on the missing separator: a digit followed directly by a letter.
	std::string cur;
	for (size_t i = 0; i < list.size(); i++) {
		if (list.compare(i, 3, " ; ") == 0) {
			if (!cur.empty()) r.refs.push_back(cur);
			cur.clear();
			i += 2;
			continue;
		}
		if (i > 0 && std::isdigit((unsigned char)list[i - 1]) && std::isalpha((unsigned char)list[i])
				&& !cur.empty()) {
			r.refs.push_back(cur);
			cur.clear();
		}
		cur += list[i];
	}
	while (!cur.empty() && cur.back() == ' ') cur.pop_back();
	if (!cur.empty()) r.refs.push_back(cur);
	return r;
}

static std::vector<std::string> refs_of(const SearchResult& r)
{
	std::vector<std::string> out;
	for (const SearchHit& h : r.hits)
		out.push_back(FormatRef({h.book, h.chapter, h.verse}));
	return out;
}

static std::string join(const std::vector<std::string>& v)
{
	std::string s;
	for (const std::string& x : v) s += (s.empty() ? "" : " | ") + x;
	return s;
}

static void compare(SwordLibrary& lib, SearchMode mode, const std::string& type,
	const std::string& query, const std::string& scope, const std::string& name)
{
	DiathekeResult want = diatheke_search(type, query, scope);
	check_true(want.total > 0, name + ": diatheke found matches", std::to_string(want.total));
	SearchResult got = lib.Search("KJV", query, mode, scope, 100000);
	check_eq(got.total, want.total, name + ": total equals diatheke");
	check_eq(join(refs_of(got)), join(want.refs), name + ": refs equal diatheke");
}

int main()
{
	SwordLibrary lib(fixture_dir());

	compare(lib, SearchMode::Phrase, "phrase", "only begotten", "", "phrase");
	compare(lib, SearchMode::Phrase, "phrase", "ONLY Begotten", "", "phrase, case-insensitive");
	compare(lib, SearchMode::AllWords, "multiword", "love world", "", "all words");
	compare(lib, SearchMode::Regex, "regex", "Jesus wept", "", "regex");
	compare(lib, SearchMode::Phrase, "phrase", "only begotten", "John", "phrase in scope John");
	compare(lib, SearchMode::AllWords, "multiword", "faith hope", "Rom-Heb", "all words in Rom-Heb");

	// Limit: hits capped, total still complete.
	SearchResult limited = lib.Search("KJV", "love world", SearchMode::AllWords, "", 5);
	check_eq(limited.hits.size(), 5u, "limit: 5 hits returned");
	check_true(limited.total > 5, "limit: total counts all matches", std::to_string(limited.total));

	// Hit text is the plain verse text, as diatheke prints it.
	SearchResult wept = lib.Search("KJV", "Jesus wept", SearchMode::Phrase, "", 10);
	check_true(wept.hits.size() == 1 && wept.hits[0].book == "John" && wept.hits[0].chapter == 11
		&& wept.hits[0].verse == 35, "hit: Jesus wept is John 11:35");
	const char* d = std::getenv("DIATHEKE");
	std::string plain = run_cmd(std::string(d ? d : "diatheke") + " -b KJV -o '' -f plain -k 'John 11:35'");
	size_t colon = plain.find(": ");
	size_t paren = plain.rfind("(KJV)");
	std::string want = colon == std::string::npos || paren == std::string::npos ? "<diatheke failed>"
		: collapse_ws(plain.substr(colon + 2, paren - colon - 2));
	check_eq(wept.hits.empty() ? std::string("<none>") : collapse_ws(wept.hits[0].text), want,
		"hit: text equals diatheke plain text");

	// Strong's: every verse in John whose rendered text has a G25 link, and no others.
	DisplayOptions o;
	o.strongs = true;
	std::set<std::string> rendered;
	for (int c = 1; c <= 21; c++) {
		for (const VerseText& v : lib.Chapter("KJV", "John", c, o)) {
			for (const Block& b : ParseXhtml(v.xhtml))
				for (const Run& r : b.runs)
					if (r.link.kind == LinkKind::Strongs && r.link.target == "G25")
						rendered.insert(FormatRef({"John", c, v.verse}));
		}
	}
	SearchResult g25 = lib.Search("KJV", "G25", SearchMode::Strongs, "John", 100000);
	std::set<std::string> found;
	for (const std::string& r : refs_of(g25)) found.insert(r);
	check_true(!rendered.empty(), "strongs: John has verses with G25", std::to_string(rendered.size()));
	check_true(found == rendered, "strongs: G25 in John matches the rendered links",
		std::to_string(found.size()) + " vs " + std::to_string(rendered.size()));
	check_eq(g25.total, (int)rendered.size(), "strongs: total");
	SearchResult h430 = lib.Search("KJV", "H0430", SearchMode::Strongs, "Gen 1", 1000);
	check_true(h430.total > 20, "strongs: H0430 (God) in Gen 1", std::to_string(h430.total));

	// Bad input.
	check_eq(lib.Search("NoSuchModule", "love", SearchMode::Phrase, "", 10).total, 0, "bad: missing module");
	check_eq(lib.Search("KJV", "", SearchMode::Phrase, "", 10).total, 0, "bad: empty query");
	check_eq(lib.Search("KJV", "zzqxv", SearchMode::Phrase, "", 10).total, 0, "bad: no match");
	return check_result();
}
