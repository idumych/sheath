// Spec tests for sheath::Layout. Written by Claude; the implementation must satisfy them
// unchanged. Fake metrics: width = 10 * size per UTF-8 code point, ascent 8 * size,
// descent 2 * size. Default LayoutOptions unless stated: lineGap 0.25 -> 0.25 * (8 + 2) = 2.5 px.
// Rules these numbers encode (also documented in Layout.h):
//  - the space between a verse number and its text has size 1.0 and the text's style (10 px);
//  - Paragraph blocks flow inline; a LineBreak block starts a new line (plus paragraphGap);
//    a Heading block gets its own line(s) before the verse number;
//  - a verse number is drawn kSuperscript|kSmall at size 0.7, raised by 0.4 * its ascent.
#include "check.h"
#include "Layout.h"
#include "Markup.h"
#include "Sword.h"
#include <algorithm>
#include <vector>

using namespace sheath;

class FakeMetrics : public Metrics {
public:
    float Width(const std::string& s, const TextStyle& st) const override {
        int cps = 0;
        for (unsigned char c : s) if ((c & 0xC0) != 0x80) cps++;
        return 10.0f * st.size * cps;
    }
    float Ascent(const TextStyle& st) const override { return 8.0f * st.size; }
    float Descent(const TextStyle& st) const override { return 2.0f * st.size; }
};
static FakeMetrics M;

static Block para(const std::string& text) { Block b; b.kind = Block::Paragraph; b.runs.push_back({text, 0, {}}); return b; }
static Block heading(const std::string& text) { Block b; b.kind = Block::Heading; b.runs.push_back({text, 0, {}}); return b; }
static LayoutVerse verse(int n, std::vector<Block> blocks) { LayoutVerse v; v.verse = n; v.blocks = std::move(blocks); return v; }
static LayoutOptions width(float w) { LayoutOptions o; o.width = w; return o; }

static void check_doc_ranges(const Layout& L, const std::string& name) {
    bool ok = true; std::string bad;
    for (auto& line : L.Lines()) for (auto& f : line.frags)
        if (f.docEnd < f.docStart || f.docEnd > L.Text().size() ||
            L.Text().substr(f.docStart, f.docEnd - f.docStart) != f.text) { ok = false; bad = f.text; }
    check_true(ok, "doc_ranges: " + name, "fragment [" + bad + "] does not match Text()");
}
static const Fragment* fragContaining(const Layout& L, const std::string& s) {
    for (auto& line : L.Lines()) for (auto& f : line.frags) if (f.text.find(s) != std::string::npos) return &f;
    return nullptr;
}

static void test_empty() {
    Layout L; L.Build({}, width(400), M);
    check_eq(L.Lines().size(), 0u, "empty: no lines");
    check_near(L.Height(), 0, "empty: height 0");
    check_eq(L.Text(), "", "empty: text");
}

static void test_single_line() {   // A
    Layout L; L.Build({verse(1, {para("In the beginning")})}, width(400), M);
    check_eq(L.Text(), "1 In the beginning", "single_line: Text()");
    check_eq(L.Lines().size(), 1u, "single_line: one line");
    if (L.Lines().size() != 1 || L.Lines()[0].frags.empty()) return;
    const Line& ln = L.Lines()[0];
    const Fragment& num = ln.frags[0];
    check_eq(num.text, "1", "single_line: first frag is the number");
    check_true(num.isVerseNumber, "single_line: number isVerseNumber");
    check_true((num.style.style & kSuperscript) && (num.style.style & kSmall), "single_line: number style");
    check_near(num.style.size, 0.7, "single_line: number size 0.7");
    check_near(num.x, 0, "single_line: number x");
    check_near(num.width, 7, "single_line: number width (1 * 10 * 0.7)");
    check_near(num.baseline, 5.76, "single_line: number baseline (8 - 0.4 * 5.6)");
    const Fragment* in = fragContaining(L, "In");
    check_true(in != nullptr, "single_line: a frag contains 'In'");
    if (in) check_near(in->x + M.Width(in->text.substr(0, in->text.find("In")), in->style), 17, "single_line: 'In' at x 17 (7 + 10)");
    check_near(ln.top, 0, "single_line: top");
    check_near(ln.baseline, 8, "single_line: baseline");
    check_near(ln.bottom, 10, "single_line: bottom");
    check_near(L.Height(), 10, "single_line: height");
    check_doc_ranges(L, "single_line");
}

static Layout layoutB() { Layout L; L.Build({verse(0, {para("aaaa bbbb cccc")})}, width(100), M); return L; }

static void test_wrap() {   // B
    Layout L = layoutB();
    check_eq(L.Text(), "aaaa bbbb cccc", "wrap: Text()");
    check_eq(L.Lines().size(), 2u, "wrap: two lines");
    if (L.Lines().size() != 2) return;
    const Line& l1 = L.Lines()[0]; const Line& l2 = L.Lines()[1];
    float right = 0; for (auto& f : l1.frags) right = std::max(right, f.x + f.width);
    check_near(right, 90, "wrap: line 1 right edge 90 ('aaaa bbbb')");
    check_true(!l2.frags.empty() && l2.frags[0].text == "cccc", "wrap: line 2 starts with 'cccc'");
    if (!l2.frags.empty()) {
        check_near(l2.frags[0].x, 0, "wrap: line 2 x 0");
        check_eq(l2.frags[0].docStart, 10u, "wrap: line 2 docStart 10");
    }
    check_near(l2.top, 12.5, "wrap: line 2 top (10 + 2.5)");
    check_near(l2.baseline, 20.5, "wrap: line 2 baseline");
    check_near(l2.bottom, 22.5, "wrap: line 2 bottom");
    check_near(L.Height(), 22.5, "wrap: height");
    check_doc_ranges(L, "wrap");
}

static void test_long_word() {   // C
    Layout L; L.Build({verse(0, {para(std::string(25, 'x'))})}, width(100), M);
    check_eq(L.Lines().size(), 3u, "long_word: three lines");
    if (L.Lines().size() != 3) return;
    float w[3] = {100, 100, 50}; size_t s[3] = {0, 10, 20};
    for (int i = 0; i < 3; i++) {
        float lw = 0; for (auto& f : L.Lines()[i].frags) lw += f.width;
        check_near(lw, w[i], "long_word: line " + std::to_string(i + 1) + " width");
        check_true(!L.Lines()[i].frags.empty() && L.Lines()[i].frags[0].docStart == s[i],
                   "long_word: line " + std::to_string(i + 1) + " docStart " + std::to_string(s[i]));
    }
    check_doc_ranges(L, "long_word");
}

static void test_utf8_break() {   // D
    std::string e; for (int i = 0; i < 15; i++) e += "\xC3\xA9";
    Layout L; L.Build({verse(0, {para(e)})}, width(100), M);
    check_eq(L.Lines().size(), 2u, "utf8_break: two lines");
    if (L.Lines().size() == 2 && !L.Lines()[1].frags.empty())
        check_eq(L.Lines()[1].frags[0].docStart, 20u, "utf8_break: line 2 starts at byte 20");
    bool even = true;
    for (auto& ln : L.Lines()) for (auto& f : ln.frags) if (f.docStart % 2 || f.docEnd % 2) even = false;
    check_true(even, "utf8_break: all boundaries on even bytes");
    check_doc_ranges(L, "utf8_break");
}

static void test_range_rects() {   // E
    Layout L = layoutB();
    auto r = L.RangeRects(5, 14, M);
    check_eq(r.size(), 2u, "range_rects: two rects");
    if (r.size() == 2) {
        check_near(r[0].l, 50, "range_rects: r1 left"); check_near(r[0].t, 0, "range_rects: r1 top");
        check_near(r[0].r, 90, "range_rects: r1 right"); check_near(r[0].b, 10, "range_rects: r1 bottom");
        check_near(r[1].l, 0, "range_rects: r2 left"); check_near(r[1].t, 12.5, "range_rects: r2 top");
        check_near(r[1].r, 40, "range_rects: r2 right"); check_near(r[1].b, 22.5, "range_rects: r2 bottom");
    }
    check_eq(L.RangeRects(3, 3, M).size(), 0u, "range_rects: empty range");
}

static void test_offset_at() {   // F
    Layout L = layoutB();
    size_t o = L.OffsetAt(25, 5, M);
    check_true(o == 2 || o == 3, "offset_at: middle of 3rd char -> 2 or 3", std::to_string(o));
    check_eq(L.OffsetAt(-5, 15, M), 10u, "offset_at: left of line 2 -> 10");
    check_eq(L.OffsetAt(50, -5, M), 0u, "offset_at: above -> 0");
    check_eq(L.OffsetAt(50, 100, M), 14u, "offset_at: below -> end");
    check_eq(L.OffsetAt(500, 5, M), 9u, "offset_at: right of line 1 -> 9");
}

static void test_verse_per_line() {   // G
    LayoutOptions o = width(400); o.versePerLine = true;
    Layout L; L.Build({verse(1, {para("a")}), verse(2, {para("b")}), verse(3, {para("c")})}, o, M);
    check_eq(L.Text(), "1 a\n2 b\n3 c", "verse_per_line: Text()");
    check_eq(L.Lines().size(), 3u, "verse_per_line: three lines");
    bool nums = L.Lines().size() == 3;
    for (auto& ln : L.Lines()) if (ln.frags.empty() || !ln.frags[0].isVerseNumber) nums = false;
    check_true(nums, "verse_per_line: each line starts with a number");
    auto vr = L.VerseRange(2);
    check_true(vr.first == 4 && vr.second == 7, "verse_per_line: VerseRange(2) == {4,7}",
               std::to_string(vr.first) + "," + std::to_string(vr.second));
    check_near(L.VerseTop(2), 12.5, "verse_per_line: VerseTop(2)");
    check_near(L.VerseTop(9), -1, "verse_per_line: VerseTop(9) absent");
    check_eq(L.VerseAt(20, 15), 2, "verse_per_line: VerseAt(20,15)");
    auto none = L.VerseRange(9);
    check_true(none.first == 0 && none.second == 0, "verse_per_line: VerseRange(9) == {0,0}");
    check_doc_ranges(L, "verse_per_line");
}

static void test_flow() {   // verses flow on one line without versePerLine
    Layout L; L.Build({verse(1, {para("a")}), verse(2, {para("b")})}, width(400), M);
    check_eq(L.Text(), "1 a 2 b", "flow: Text()");
    check_eq(L.Lines().size(), 1u, "flow: one line");
    check_doc_ranges(L, "flow");
}

static void test_heading() {   // H
    Layout L; L.Build({verse(1, {heading("Title"), para("text")})}, width(400), M);
    check_eq(L.Text(), "Title\n1 text", "heading: Text()");
    check_eq(L.Lines().size(), 2u, "heading: two lines");
    if (L.Lines().size() != 2 || L.Lines()[0].frags.empty() || L.Lines()[1].frags.empty()) return;
    const Fragment& t = L.Lines()[0].frags[0];
    check_eq(L.Lines()[0].frags.size(), 1u, "heading: line 1 has only the title");
    check_eq(t.text, "Title", "heading: title text");
    check_true(t.style.style & kBold, "heading: bold");
    check_near(t.style.size, 1.25, "heading: size 1.25");
    check_near(t.width, 62.5, "heading: width");
    check_near(L.Lines()[0].bottom, 12.5, "heading: line 1 bottom (10 + 2.5)");
    check_near(L.Lines()[1].top, 15, "heading: line 2 top (12.5 + 2.5)");
    check_true(L.Lines()[1].frags[0].isVerseNumber && L.Lines()[1].frags[0].text == "1", "heading: line 2 starts with number");
    check_doc_ranges(L, "heading");
}

static void test_line_break() {
    Block br; br.kind = Block::LineBreak;
    Layout L; L.Build({verse(0, {para("a"), br, para("b")})}, width(400), M);
    check_eq(L.Lines().size(), 2u, "line_break: two lines");
    // line 1 bottom 10, + lineGap 2.5 + paragraphGap 0.5 * 10 = 5 -> top 17.5
    if (L.Lines().size() == 2) check_near(L.Lines()[1].top, 17.5, "line_break: line 2 top");
    check_doc_ranges(L, "line_break");
}

static void test_link_kept() {   // J
    Block b; b.kind = Block::Paragraph;
    b.runs.push_back({"see ", 0, {}});
    b.runs.push_back({"Strongs", 0, {LinkKind::Strongs, "G25"}});
    b.runs.push_back({" here", 0, {}});
    Layout L; L.Build({verse(0, {b})}, width(60), M);
    bool any = false, ok = true;
    for (auto& ln : L.Lines()) for (auto& f : ln.frags) {
        bool inLink = f.docStart >= 4 && f.docEnd <= 11;
        if (inLink && !f.text.empty()) { any = true; if (f.link.kind != LinkKind::Strongs || f.link.target != "G25") ok = false; }
        if (!inLink && f.link.kind != LinkKind::None) ok = false;
    }
    check_true(any && ok, "link_kept: link survives wrapping, only on its own text");
    check_doc_ranges(L, "link_kept");
}

static void test_real_text() {   // L
    SwordLibrary lib(fixture_dir());
    DisplayOptions o; o.strongs = o.morph = o.footnotes = o.crossRefs = o.headings = o.redLetter = false;
    std::vector<LayoutVerse> vs;
    for (auto& v : lib.Chapter("KJV", "John", 3, o)) vs.push_back(verse(v.verse, ParseXhtml(v.xhtml)));
    Layout L; L.Build(vs, width(300), M);
    check_true(L.Lines().size() > 36, "real_text: more than 36 lines", std::to_string(L.Lines().size()));
    check_doc_ranges(L, "real_text");
    auto r = L.VerseRange(16);
    std::string v16 = L.Text().substr(r.first, r.second - r.first);
    check_true(v16.rfind("16 For God so loved the world", 0) == 0, "real_text: verse 16 range", v16.substr(0, 40));
}

int main() {
    test_empty(); test_single_line(); test_wrap(); test_long_word(); test_utf8_break();
    test_range_rects(); test_offset_at(); test_verse_per_line(); test_flow(); test_heading();
    test_line_break(); test_link_kept(); test_real_text();
    return check_result();
}
