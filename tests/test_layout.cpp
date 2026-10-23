// test_layout.cpp — tests for sheath::Layout
// Build: g++ -std=gnu++17 -Isrc/core -o test_layout tests/test_layout.cpp src/core/Layout.cpp src/core/Markup.cpp

#include "Layout.h"
#include "Markup.h"
#include <iostream>
#include <string>
#include <vector>

static int g_pass = 0, g_fail = 0;

#define PASS(msg) do { \
    std::cout << "  PASS: " << (msg) << "\n"; \
    g_pass++; \
} while(0)

#define FAIL(msg) do { \
    std::cout << "  FAIL: " << (msg) << "\n"; \
    g_fail++; \
} while(0)

#define CHECK(cond, msg) do { \
    if (cond) PASS(msg); else FAIL(msg); \
} while(0)

// ---- FakeMetrics: each codepoint = 10px * size, ascent = 8*size, descent = 2*size ----

struct FakeMetrics : sheath::Metrics {
    float Width(const std::string& utf8, const sheath::TextStyle& s) const override {
        int codepoints = 0;
        for (size_t i = 0; i < utf8.size(); ) {
            unsigned char c = static_cast<unsigned char>(utf8[i]);
            if (c < 0x80) i += 1;
            else if (c < 0xE0) i += 2;
            else if (c < 0xF0) i += 3;
            else i += 4;
            codepoints++;
        }
        return codepoints * 10.0f * s.size;
    }
    float Ascent(const sheath::TextStyle& s) const override { return 8.0f * s.size; }
    float Descent(const sheath::TextStyle& s) const override { return 2.0f * s.size; }
};

// ---- Helpers ----

static sheath::LayoutVerse MakeVerse(int verseNum, const std::string& text, unsigned runStyle = 0) {
    sheath::LayoutVerse v;
    v.verse = verseNum;
    sheath::Block b;
    b.kind = sheath::Block::Paragraph;
    sheath::Run r;
    r.text = text;
    r.style = runStyle;
    b.runs.push_back(r);
    v.blocks.push_back(b);
    return v;
}

static sheath::LayoutVerse MakeHeadingVerse(int verseNum, const std::string& text) {
    sheath::LayoutVerse v;
    v.verse = verseNum;
    sheath::Block b;
    b.kind = sheath::Block::Heading;
    sheath::Run r;
    r.text = text;
    b.runs.push_back(r);
    v.blocks.push_back(b);
    return v;
}

// ---- Tests ----

static void test_empty() {
    std::cout << "\n--- empty ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 200;
    layout.Build({}, opts, metrics);
    CHECK(layout.Text().empty(), "Empty verses produce empty text");
    CHECK(layout.Lines().empty(), "Empty verses produce no lines");
    CHECK(layout.Height() == 0, "Empty verses have zero height");
}

static void test_single_line() {
    std::cout << "\n--- single_line ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 400;

    auto verses = std::vector<sheath::LayoutVerse>{
        MakeVerse(1, "Hello world")
    };
    layout.Build(verses, opts, metrics);

    CHECK(layout.Lines().size() == 1, "Single line fits on one line");
    CHECK(layout.Text().find("1 Hello world") != std::string::npos, "Text includes verse number and content");
    CHECK(layout.Height() > 0, "Height > 0 for single line");
}

static void test_wrap() {
    std::cout << "\n--- wrap ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 100;

    auto verses = std::vector<sheath::LayoutVerse>{
        MakeVerse(1, "Hello world")
    };
    layout.Build(verses, opts, metrics);

    CHECK(layout.Lines().size() == 2, "Text wraps into two lines");
}

static void test_long_word() {
    std::cout << "\n--- long_word ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 100;  // 10 codepoints

    auto verses = std::vector<sheath::LayoutVerse>{
        MakeVerse(1, "supercalifragilistic")
    };
    layout.Build(verses, opts, metrics);

    // All characters should be preserved (possibly broken into segments)
    int totalChars = 0;
    for (auto& line : layout.Lines()) {
        for (auto& frag : line.frags) {
            totalChars += static_cast<int>(frag.text.size());
        }
    }
    CHECK(totalChars > 0, "Long word characters preserved (possibly broken)");

    // No fragment should exceed available width
    for (auto& line : layout.Lines()) {
        for (auto& frag : line.frags) {
            CHECK(frag.width <= 100.0f + 0.1f, "No fragment exceeds available width");
        }
    }
}

static void test_utf8_break() {
    std::cout << "\n--- utf8_break ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 100;  // 10 codepoints

    // Accented characters (multi-byte UTF-8)
    auto verses = std::vector<sheath::LayoutVerse>{
        MakeVerse(1, "cafénaïveexposé")
    };
    layout.Build(verses, opts, metrics);

    // Count all UTF-8 codepoints in the layout
    int totalCP = 0;
    for (auto& line : layout.Lines()) {
        for (auto& frag : line.frags) {
            for (size_t i = 0; i < frag.text.size(); ) {
                unsigned char c = static_cast<unsigned char>(frag.text[i]);
                if (c < 0x80) i += 1;
                else if (c < 0xE0) i += 2;
                else if (c < 0xF0) i += 3;
                else i += 4;
                totalCP++;
            }
        }
    }
    CHECK(totalCP == 16, "All UTF-8 codepoints preserved (1 verse # + 15 word chars)");
    CHECK(layout.Text().find("café") != std::string::npos, "café preserved in Text()");
}

static void test_verse_per_line() {
    std::cout << "\n--- verse_per_line ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 400;
    opts.versePerLine = true;

    auto verses = std::vector<sheath::LayoutVerse>{
        MakeVerse(1, "First verse text"),
        MakeVerse(2, "Second verse text"),
        MakeVerse(3, "Third verse text")
    };
    layout.Build(verses, opts, metrics);

    CHECK(layout.Lines().size() == 3, "Each verse starts on its own line");
    CHECK(layout.Text().find("First verse text") != std::string::npos, "All verses present in text");
}

static void test_heading() {
    std::cout << "\n--- heading ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 400;

    auto verses = std::vector<sheath::LayoutVerse>{
        MakeHeadingVerse(0, "Chapter One"),
        MakeVerse(1, "First verse content")
    };
    layout.Build(verses, opts, metrics);

    CHECK(layout.Lines().size() >= 2, "Heading and verse each get lines");

    bool foundHeading = false;
    for (auto& frag : layout.Lines()[0].frags) {
        if (frag.text.find("Chapter") != std::string::npos) {
            foundHeading = true;
            break;
        }
    }
    CHECK(foundHeading, "Heading text appears on first line");
}

static void test_doc_ranges() {
    std::cout << "\n--- doc_ranges ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 400;

    auto verses = std::vector<sheath::LayoutVerse>{
        MakeVerse(1, "Hello world"),
        MakeVerse(2, "Goodbye world")
    };
    layout.Build(verses, opts, metrics);

    const std::string& text = layout.Text();
    size_t helloStart = text.find("Hello");
    CHECK(helloStart != std::string::npos, "Found 'Hello' in text");
    if (helloStart != std::string::npos) {
        auto vr = layout.VerseRange(1);
        CHECK(vr.first != 0 || vr.second != 0, "VerseRange(1) returns non-zero range");
        float vt = layout.VerseTop(1);
        CHECK(vt >= 0, "VerseTop(1) >= 0");
    }
}

static void test_offset_at() {
    std::cout << "\n--- offset_at ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 400;

    auto verses = std::vector<sheath::LayoutVerse>{
        MakeVerse(1, "Hello world")
    };
    layout.Build(verses, opts, metrics);

    size_t off = layout.OffsetAt(0, 9, metrics);
    CHECK(off == 0, "OffsetAt at y=9 returns start (0)");

    off = layout.OffsetAt(0, 9999, metrics);
    CHECK(off == layout.Text().size(), "OffsetAt below text returns text size");

    // FragmentAt
    const sheath::Fragment* frag = layout.FragmentAt(5, 5);
    if (frag) {
        CHECK(frag->verse == 1, "FragmentAt returns fragment of verse 1");
    } else {
        FAIL("FragmentAt returned nullptr");
    }
}

static void test_verse_at_range_top() {
    std::cout << "\n--- verse_at_range_top ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 400;

    auto verses = std::vector<sheath::LayoutVerse>{
        MakeVerse(1, "First verse"),
        MakeVerse(2, "Second verse"),
        MakeVerse(3, "Third verse")
    };
    layout.Build(verses, opts, metrics);

    // VerseTop for each verse
    CHECK(layout.VerseTop(1) >= 0, "VerseTop(1) >= 0");
    CHECK(layout.VerseTop(2) >= 0, "VerseTop(2) >= 0");
    CHECK(layout.VerseTop(999) == -1, "VerseTop(999) == -1 (non-existent)");

    // VerseRange for each verse
    auto vr1 = layout.VerseRange(1);
    CHECK(vr1.first != 0 || vr1.second != 0, "VerseRange(1) non-zero");

    auto vr999 = layout.VerseRange(999);
    CHECK(vr999.first == 0 && vr999.second == 0, "VerseRange(999) == {0,0}");
}

static void test_range_rects() {
    std::cout << "\n--- range_rects ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 400;

    auto verses = std::vector<sheath::LayoutVerse>{
        MakeVerse(1, "Hello world"),
        MakeVerse(2, "Goodbye world")
    };
    layout.Build(verses, opts, metrics);

    const std::string& text = layout.Text();
    size_t helloStart = text.find("Hello");
    size_t helloEnd = helloStart + 5;

    auto rects = layout.RangeRects(helloStart, helloEnd, metrics);
    CHECK(!rects.empty(), "RangeRects returns at least one rect for 'Hello'");
    if (rects.size() == 1) {
        CHECK(rects[0].l >= 0, "Range rect left >= 0");
        CHECK(rects[0].r > rects[0].l, "Range rect right > left");
        CHECK(rects[0].t >= 0, "Range rect top >= 0");
        CHECK(rects[0].b > rects[0].t, "Range rect bottom > top");
    }
}

static void test_link_kept() {
    std::cout << "\n--- link_kept ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 400;

    auto v = sheath::LayoutVerse{1, {}};
    sheath::Block b;
    b.kind = sheath::Block::Paragraph;
    sheath::Run r;
    r.text = "strongs_word";
    r.link = sheath::Link{sheath::LinkKind::Strongs, "G25"};
    b.runs.push_back(r);
    v.blocks.push_back(b);

    layout.Build({v}, opts, metrics);

    bool foundLink = false;
    for (auto& line : layout.Lines()) {
        for (auto& frag : line.frags) {
            if (frag.link.kind == sheath::LinkKind::Strongs && frag.link.target == "G25") {
                foundLink = true;
            }
        }
    }
    CHECK(foundLink, "Strong's link preserved in layout fragment");
    CHECK(layout.Text().find("strongs_word") != std::string::npos, "Link text present in layout");
}

static void test_real_text() {
    std::cout << "\n--- real_text ---\n";
    sheath::Layout layout;
    FakeMetrics metrics;
    sheath::LayoutOptions opts;
    opts.width = 400;

    // Parse a simple XHTML block
    std::string xhtml = "<p>The <b>quick</b> brown fox</p>";
    auto blocks = sheath::ParseXhtml(xhtml);

    auto v = sheath::LayoutVerse{1, std::move(blocks)};
    layout.Build({v}, opts, metrics);

    CHECK(!layout.Lines().empty(), "Real XHTML parses into lines");
    CHECK(layout.Text().find("quick") != std::string::npos, "Bold word 'quick' in text");
    CHECK(layout.Text().find("brown") != std::string::npos, "Text 'brown' in text");
}

// ---- main ----

int main() {
    std::cout << "=== Sheath Layout Tests ===\n\n";

    test_empty();
    test_single_line();
    test_wrap();
    test_long_word();
    test_utf8_break();
    test_verse_per_line();
    test_heading();
    test_doc_ranges();
    test_offset_at();
    test_verse_at_range_top();
    test_range_rects();
    test_link_kept();
    test_real_text();

    std::cout << "\n=== Results ===\n";
    std::cout << "TOTAL " << (g_pass + g_fail) << " PASS " << g_pass << " FAIL " << g_fail << "\n";

    return g_fail > 0 ? 1 : 0;
}
