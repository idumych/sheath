// Spec tests for SwordLibrary, ParseXhtml/PlainText and references. Written by Claude; the
// implementation must satisfy them unchanged. Fixture: CrossWire KJV + StrongsGreek (read-only).
#include "check.h"
#include "Markup.h"
#include "Ref.h"
#include "Sword.h"
#include <vector>

using namespace sheath;

static DisplayOptions allOff() {
    DisplayOptions o;
    o.strongs = o.morph = o.footnotes = o.crossRefs = o.headings = o.redLetter = false;
    return o;
}

static const VerseText* findVerse(const std::vector<VerseText>& vs, int v) {
    for (auto& x : vs) if (x.verse == v) return &x;
    return nullptr;
}

// diatheke -f plain prints "<Book> <c>:<v>: <text>\n(KJV)\n"; returns <text>, whitespace collapsed.
static std::string diatheke(const std::string& ref) {
    const char* d = std::getenv("DIATHEKE");
    std::string out = run_cmd(std::string(d ? d : "diatheke") + " -b KJV -o '' -f plain -k '" + ref + "'");
    size_t end = out.rfind("(KJV)");
    if (end != std::string::npos) out = out.substr(0, end);
    size_t colon = out.find(": ");
    if (colon != std::string::npos) out = out.substr(colon + 2);
    return collapse_ws(out);
}

static void test_modules(SwordLibrary& lib) {
    ModuleInfo kjv, sg;
    check_true(lib.Find("KJV", &kjv), "modules: KJV found");
    check_eq(kjv.type, "Bible", "modules: KJV type");
    check_true(kjv.strongs, "modules: KJV has Strong's");
    check_eq(kjv.versification, "KJV", "modules: KJV versification");
    check_true(lib.Find("StrongsGreek", &sg), "modules: StrongsGreek found");
    check_eq(sg.type, "Dictionary", "modules: StrongsGreek type");
    check_true(!lib.Find("NoSuchModule"), "modules: unknown module not found");
    auto all = lib.Modules();
    check_eq(all.size(), 2u, "modules: count");
    check_true(all.size() == 2 && all[0].name == "KJV" && all[1].name == "StrongsGreek", "modules: sorted by name");
}

static void test_books(SwordLibrary& lib) {
    auto b = lib.Books("KJV");
    check_eq(b.size(), 66u, "books: KJV count");
    if (b.size() != 66) return;
    check_eq(b[0].osis, "Gen", "books: first osis");
    check_eq(b[0].name, "Genesis", "books: first name");
    check_eq(b[0].chapters, 50, "books: Genesis chapters");
    check_eq(b[0].testament, 1, "books: Genesis testament");
    check_eq(b[18].osis, "Ps", "books: 19th is Ps");
    check_eq(b[18].chapters, 150, "books: Psalms chapters");
    check_eq(b[42].osis, "John", "books: 43rd is John");
    check_eq(b[42].chapters, 21, "books: John chapters");
    check_eq(b[42].testament, 2, "books: John testament");
    check_eq(b[65].osis, "Rev", "books: last osis");
    check_eq(lib.VerseCount("KJV", "Gen", 1), 31, "versecount: Gen 1");
    check_eq(lib.VerseCount("KJV", "John", 3), 36, "versecount: John 3");
    check_eq(lib.VerseCount("KJV", "Ps", 119), 176, "versecount: Ps 119");
    check_eq(lib.VerseCount("KJV", "Rev", 22), 21, "versecount: Rev 22");
    check_eq(lib.VerseCount("KJV", "John", 99), 0, "versecount: no such chapter");
    check_eq(lib.VerseCount("KJV", "Nope", 1), 0, "versecount: no such book");
}

static void test_chapter_text(SwordLibrary& lib) {
    auto john3 = lib.Chapter("KJV", "John", 3, allOff());
    check_eq(john3.size(), 36u, "chapter: John 3 verse count");
    check_true(!john3.empty() && john3[0].verse == 1, "chapter: first verse is 1");
    check_true(lib.Chapter("KJV", "John", 99, allOff()).empty(), "chapter: no such chapter is empty");

    struct { const char* ref; const char* book; int c, v; } cases[] = {
        {"Gen 1:1", "Gen", 1, 1}, {"Ps 23:1", "Ps", 23, 1}, {"John 3:16", "John", 3, 16},
        {"Rom 8:28", "Rom", 8, 28}, {"Rev 22:21", "Rev", 22, 21},
    };
    for (auto& c : cases) {
        std::string want = diatheke(c.ref);
        check_true(want.size() > 20, std::string("chapter_text: diatheke gave text for ") + c.ref, "[" + want + "]");
        auto ch = lib.Chapter("KJV", c.book, c.c, allOff());
        const VerseText* vt = findVerse(ch, c.v);
        std::string got = vt ? collapse_ws(PlainText(ParseXhtml(vt->xhtml))) : "<missing verse>";
        check_eq(got, want, std::string("chapter_text: ") + c.ref + " equals diatheke");
    }
}

static void test_links(SwordLibrary& lib) {
    DisplayOptions o = allOff();
    o.strongs = true;
    auto ch = lib.Chapter("KJV", "John", 3, o);
    const VerseText* v16 = findVerse(ch, 16);
    bool g25 = false;
    std::string text;
    if (v16) {
        auto blocks = ParseXhtml(v16->xhtml);
        text = PlainText(blocks);
        for (auto& b : blocks) for (auto& r : b.runs)
            if (r.link.kind == LinkKind::Strongs && r.link.target == "G25") g25 = true;
    }
    check_true(g25, "links: John 3:16 has a Strongs link G25");
    check_true(text.find("loved") != std::string::npos, "links: text with Strong's on still has 'loved'");
    check_true(text.find('<') == std::string::npos && text.find("&lt;") == std::string::npos,
               "links: no markup left in plain text", text);

    DisplayOptions r = allOff();
    r.redLetter = true;
    auto red = lib.Chapter("KJV", "John", 3, r);
    const VerseText* rv = findVerse(red, 16);
    bool hasRed = false;
    if (rv) for (auto& b : ParseXhtml(rv->xhtml)) for (auto& run : b.runs) if (run.style & kRedLetter) hasRed = true;
    check_true(hasRed, "links: red letter on -> John 3:16 has a kRedLetter run");
    const VerseText* off = findVerse(lib.Chapter("KJV", "John", 3, allOff()), 16);
    bool redOff = false;
    if (off) for (auto& b : ParseXhtml(off->xhtml)) for (auto& run : b.runs) if (run.style & kRedLetter) redOff = true;
    check_true(!redOff, "links: red letter off -> no kRedLetter run");
}

static void test_dictionary(SwordLibrary& lib) {
    std::string e = lib.Entry("StrongsGreek", "G25", allOff());
    check_true(e.find("agapao") != std::string::npos, "dict: G25 entry contains 'agapao'", e.substr(0, 80));
    check_eq(lib.Entry("StrongsGreek", "G99999", allOff()), "", "dict: missing key gives empty");
    check_eq(lib.Entry("NoSuchModule", "G25", allOff()), "", "dict: missing module gives empty");
    auto keys = lib.Keys("StrongsGreek", "0002", 5);
    check_eq(keys.size(), 5u, "dict: Keys honours limit");
}

static void test_refs() {
    VerseRef r;
    check_true(ParseRef("jn 3:16", &r) && r.book == "John" && r.chapter == 3 && r.verse == 16, "ref: jn 3:16");
    check_true(ParseRef("John.3.16", &r) && r.book == "John" && r.chapter == 3 && r.verse == 16, "ref: John.3.16");
    check_true(ParseRef("1 Cor 13", &r) && r.book == "1Cor" && r.chapter == 13 && r.verse == 0, "ref: 1 Cor 13");
    check_true(ParseRef("Gen 1:1-3", &r) && r.book == "Gen" && r.chapter == 1 && r.verse == 1, "ref: Gen 1:1-3");
    check_true(ParseRef("Psalm 23", &r) && r.book == "Ps" && r.chapter == 23 && r.verse == 0, "ref: Psalm 23");
    check_true(ParseRef("Revelation 22:21", &r) && r.book == "Rev" && r.chapter == 22 && r.verse == 21, "ref: Revelation 22:21");
    check_true(!ParseRef("xyz", &r), "ref: xyz is not a reference");
    check_true(!ParseRef("", &r), "ref: empty is not a reference");
    check_eq(FormatRef({"John", 3, 16}), "John 3:16", "ref: format verse");
    check_eq(FormatRef({"Ps", 23, 0}), "Psalms 23", "ref: format chapter");
    auto l = ParseOsisRefList("John.3.16 Rom.5.8-Rom.5.9");
    check_eq(l.size(), 2u, "ref: osis list count");
    check_true(l.size() == 2 && l[1].book == "Rom" && l[1].chapter == 5 && l[1].verse == 8, "ref: osis list second");
}

static void test_markup_unit() {
    auto b = ParseXhtml("In the <i>beginning</i> &amp; the   end&#33;");
    check_eq(PlainText(b), "In the beginning & the end!", "markup: entities and whitespace");
    bool italic = false;
    for (auto& blk : b) for (auto& r : blk.runs) if (r.text == "beginning" && (r.style & kItalic)) italic = true;
    check_true(italic, "markup: <i> gives a kItalic run 'beginning'");
    auto m = ParseXhtml("<b>a</b><b>b</b>");
    check_true(m.size() == 1 && m[0].runs.size() == 1 && m[0].runs[0].text == "ab", "markup: equal runs merged");
    check_eq(PlainText(ParseXhtml("<unknown>kept</unknown>")), "kept", "markup: unknown tag keeps text");
    check_true(ParseXhtml("").empty() || PlainText(ParseXhtml("")).empty(), "markup: empty input");
}

int main() {
    SwordLibrary lib(fixture_dir());
    test_modules(lib);
    test_books(lib);
    test_chapter_text(lib);
    test_links(lib);
    test_dictionary(lib);
    test_refs();
    test_markup_unit();
    return check_result();
}
