// test_core.cpp — tests for sheath::SwordLibrary, Markup, Ref
// Build: g++ -std=gnu++17 -I.deps/sword/include -I.deps/sword/include/sword -Isrc/core \
//        -L.deps/sword/lib -lsword -lbz2 -lz -o test_core tests/test_core.cpp src/core/Sword.cpp src/core/Markup.cpp src/core/Ref.cpp

#include "Sword.h"
#include "Markup.h"
#include "Ref.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>

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

// ---- helpers ----

static std::string loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

// ---- tests ----

static void test_modules() {
    std::cout << "\n--- modules ---\n";
    const char* spath = getenv("SWORD_PATH");
    std::string swordPath = spath ? spath : "";
    sheath::SwordLibrary sword(swordPath);
    auto mods = sword.Modules();
    
    // Should find KJV
    bool foundKJV = false;
    for (auto& m : mods) {
        if (m.name == "KJV") { foundKJV = true; break; }
    }
    CHECK(foundKJV, "KJV module found");
    
    // Should find StrongsGreek
    bool foundSG = false;
    for (auto& m : mods) {
        if (m.name == "StrongsGreek") { foundSG = true; break; }
    }
    CHECK(foundSG, "StrongsGreek module found");
    
    // KJV should be type "Bible"
    bool kjvBible = false;
    for (auto& m : mods) {
        if (m.name == "KJV" && m.type == "Bible") { kjvBible = true; break; }
    }
    CHECK(kjvBible, "KJV type is \"Bible\"");
    
    // StrongsGreek should be type "Dictionary"
    bool sgDict = false;
    for (auto& m : mods) {
        if (m.name == "StrongsGreek" && m.type == "Dictionary") { sgDict = true; break; }
    }
    CHECK(sgDict, "StrongsGreek type is \"Dictionary\"");
    
    // KJV should have strongs and redLetter features
    bool kjvStrong = false, kjvRed = false;
    for (auto& m : mods) {
        if (m.name == "KJV") {
            if (m.strongs) kjvStrong = true;
            if (m.redLetter) kjvRed = true;
        }
    }
    CHECK(kjvStrong, "KJV has strongs feature");
    CHECK(kjvRed, "KJV has redLetter feature");
}

static void test_books() {
    std::cout << "\n--- books ---\n";
    const char* spath = getenv("SWORD_PATH");
    std::string swordPath = spath ? spath : "";
    sheath::SwordLibrary sword(swordPath);
    auto books = sword.Books("KJV");
    
    // Should have ~39 OT books
    bool hasGenesis = false, hasJohn = false, hasRev = false;
    for (auto& b : books) {
        if (b.name == "Genesis") hasGenesis = true;
        if (b.name == "John") hasJohn = true;
        if (b.name == "Revelation of John") hasRev = true;
    }
    CHECK(hasGenesis, "Genesis in books list");
    CHECK(hasJohn, "John in books list");
    CHECK(hasRev, "Revelation of John in books list");
    
    // Genesis should have 50 chapters
    int genChapters = 0;
    for (auto& b : books) {
        if (b.osis == "Gen") { genChapters = b.chapters; break; }
    }
    std::cout << "    Genesis chapters: " << genChapters << "\n";
    CHECK(genChapters == 50, "Genesis has 50 chapters");
    
    // John should have 21 chapters
    int johnChapters = 0;
    for (auto& b : books) {
        if (b.osis == "John") { johnChapters = b.chapters; break; }
    }
    std::cout << "    John chapters: " << johnChapters << "\n";
    CHECK(johnChapters == 21, "John has 21 chapters");
    
    // Genesis should be OT (testament=1)
    int genTest = 0;
    for (auto& b : books) {
        if (b.osis == "Gen") { genTest = b.testament; break; }
    }
    CHECK(genTest == 1, "Genesis testament = 1 (OT)");
    
    // John should be NT (testament=2)
    int johnTest = 0;
    for (auto& b : books) {
        if (b.osis == "John") { johnTest = b.testament; break; }
    }
    CHECK(johnTest == 2, "John testament = 2 (NT)");
}

static void test_versecount() {
    std::cout << "\n--- versecount ---\n";
    const char* spath = getenv("SWORD_PATH");
    std::string swordPath = spath ? spath : "";
    sheath::SwordLibrary sword(swordPath);
    
    // Genesis 1 has 31 verses
    int gc = sword.VerseCount("KJV", "Gen", 1);
    std::cout << "    Genesis 1 verses: " << gc << "\n";
    CHECK(gc == 31, "Genesis 1 has 31 verses");
    
    // John 3 has 36 verses
    int jc = sword.VerseCount("KJV", "John", 3);
    std::cout << "    John 3 verses: " << jc << "\n";
    CHECK(jc == 36, "John 3 has 36 verses");
    
    // Revelation 22 has 21 verses
    int rc = sword.VerseCount("KJV", "Rev", 22);
    std::cout << "    Rev 22 verses: " << rc << "\n";
    CHECK(rc == 21, "Revelation 22 has 21 verses");
}

static void test_chapter_text() {
    std::cout << "\n--- chapter_text ---\n";
    const char* spath = getenv("SWORD_PATH");
    std::string swordPath = spath ? spath : "";
    sheath::SwordLibrary sword(swordPath);
    
    sheath::DisplayOptions opts;
    opts.strongs = true;
    opts.redLetter = true;
    opts.footnotes = true;
    opts.headings = false;  // KJV has no paragraphs
    opts.morph = false;
    opts.crossRefs = false;
    
    auto verses = sword.Chapter("KJV", "John", 3, opts);
    
    // Should have multiple verses
    std::cout << "    John 3 verse count: " << verses.size() << "\n";
    CHECK(verses.size() > 0, "John 3 has verses");
    
    // First verse should be verse 1
    CHECK(verses.size() > 0 && verses[0].verse == 1, "First verse is verse 1");
    
    // Last verse should be verse 36
    if (verses.size() > 0) {
        std::cout << "    Last verse: " << verses.back().verse << "\n";
        CHECK(verses.back().verse == 36, "Last verse is verse 36");
    }
    
    // John 3:16 should contain "God" and "world"
    bool hasGodWorld = false;
    for (auto& v : verses) {
        if (v.verse == 16) {
            if (v.xhtml.find("God") != std::string::npos &&
                v.xhtml.find("world") != std::string::npos) {
                hasGodWorld = true;
            }
            break;
        }
    }
    CHECK(hasGodWorld, "John 3:16 contains expected text");
}

static void test_strongs_links() {
    std::cout << "\n--- strongs_links ---\n";
    const char* spath = getenv("SWORD_PATH");
    std::string swordPath = spath ? spath : "";
    sheath::SwordLibrary sword(swordPath);
    
    sheath::DisplayOptions opts;
    opts.strongs = true;
    opts.redLetter = true;
    opts.footnotes = true;
    opts.headings = false;
    opts.morph = false;
    opts.crossRefs = false;
    
    auto verses = sword.Chapter("KJV", "John", 3, opts);
    
    // Find John 3:16 and check for Strong's links
    bool hasG25 = false;
    bool hasG3588 = false;
    for (auto& v : verses) {
        if (v.verse == 16) {
            auto blocks = sheath::ParseXhtml(v.xhtml);
            for (auto& b : blocks) {
                for (auto& r : b.runs) {
                    if (r.link.kind == sheath::LinkKind::Strongs) {
                        if (r.link.target == "G25") hasG25 = true;
                        if (r.link.target == "G3588") hasG3588 = true;
                    }
                }
            }
            break;
        }
    }
    CHECK(hasG25, "John 3:16 'loved' has strong:G25 link");
    CHECK(hasG3588, "John 3:16 has strong:G3588 (Greek article) link");
}

static void test_redletter() {
    std::cout << "\n--- redletter ---\n";
    const char* spath = getenv("SWORD_PATH");
    std::string swordPath = spath ? spath : "";
    sheath::SwordLibrary sword(swordPath);
    
    sheath::DisplayOptions opts;
    opts.redLetter = true;
    
    auto verses = sword.Chapter("KJV", "John", 3, opts);
    
    // John 3 is Jesus' teaching, so it should be red-letter
    // The KJV module wraps Jesus' words in <q who="Jesus">
    bool hasJesusQuote = false;
    for (auto& v : verses) {
        if (v.verse == 16) {
            if (v.xhtml.find("who=\"Jesus\"") != std::string::npos ||
                v.xhtml.find("who='Jesus'") != std::string::npos) {
                hasJesusQuote = true;
            }
            break;
        }
    }
    CHECK(hasJesusQuote, "John 3:16 has Jesus quote marker");
}

static void test_dict_entry() {
    std::cout << "\n--- dict_entry ---\n";
    const char* spath = getenv("SWORD_PATH");
    std::string swordPath = spath ? spath : "";
    sheath::SwordLibrary sword(swordPath);
    
    // G25 = agapao
    std::string entry = sword.Entry("StrongsGreek", "G25", sheath::DisplayOptions{});
    std::cout << "    G25 entry length: " << entry.length() << "\n";
    CHECK(!entry.empty(), "StrongsGreek G25 entry exists");
    
    if (!entry.empty()) {
        // Should contain the Greek word or transliteration
        bool hasAgapao = entry.find("agapao") != std::string::npos ||
                         entry.find("ἀγαπάω") != std::string::npos;
        CHECK(hasAgapao, "G25 entry contains 'agapao' or ἀγαπάω");
        
        // Should contain "love" definition
        bool hasLove = entry.find("love") != std::string::npos;
        CHECK(hasLove, "G25 entry contains 'love' definition");
    }
    
    // H430 = yehovah (Hebrew Strong's)
    std::string hebrew = sword.Entry("StrongsHebrew", "H430", sheath::DisplayOptions{});
    std::cout << "    H430 entry length: " << hebrew.length() << "\n";
    CHECK(!hebrew.empty(), "StrongsHebrew H430 entry exists");
}

static void test_parse_ref() {
    std::cout << "\n--- parse_ref ---\n";
    
    // Simple reference
    sheath::VerseRef ref;
    bool ok = sheath::ParseRef("John 3:16", &ref);
    CHECK(ok, "Parse 'John 3:16'");
    if (ok) {
        std::cout << "    book='" << ref.book << "' chap=" << ref.chapter << " vs=" << ref.verse << "\n";
        CHECK(ref.book == "John", "John 3:16 book = 'John'");
        CHECK(ref.chapter == 3, "John 3:16 chapter = 3");
        CHECK(ref.verse == 16, "John 3:16 verse = 16");
    }
    
    // Abbreviated book name
    ok = sheath::ParseRef("Jn 3.16", &ref);
    CHECK(ok, "Parse 'Jn 3.16'");
    if (ok) {
        CHECK(ref.chapter == 3, "Jn 3.16 chapter = 3");
        CHECK(ref.verse == 16, "Jn 3.16 verse = 16");
    }
    
    // Chapter only
    ok = sheath::ParseRef("1 Cor 13", &ref);
    CHECK(ok, "Parse '1 Cor 13'");
    if (ok) {
        std::cout << "    '1 Cor 13' book='" << ref.book << "' chap=" << ref.chapter << "\n";
        CHECK(ref.chapter == 13, "1 Cor 13 chapter = 13");
        CHECK(ref.verse == 0, "1 Cor 13 verse = 0 (chapter)");
    }
    
    // OSIS dot notation
    ok = sheath::ParseRef("Gen.1.1", &ref);
    CHECK(ok, "Parse 'Gen.1.1'");
    if (ok) {
        CHECK(ref.chapter == 1, "Gen.1.1 chapter = 1");
        CHECK(ref.verse == 1, "Gen.1.1 verse = 1");
    }
    
    // Invalid reference
    ok = sheath::ParseRef("not a reference", &ref);
    CHECK(!ok, "Invalid reference returns false");
    
    // Empty string
    ok = sheath::ParseRef("", &ref);
    CHECK(!ok, "Empty string returns false");
}

static void test_format_ref() {
    std::cout << "\n--- format_ref ---\n";
    
    sheath::VerseRef r1{"John", 3, 16};
    std::string fmt1 = sheath::FormatRef(r1);
    std::cout << "    FormatRef 'John 3:16' => '" << fmt1 << "'\n";
    CHECK(fmt1 == "John 3:16", "FormatRef verse");
    
    sheath::VerseRef r2{"John", 3, 0};
    std::string fmt2 = sheath::FormatRef(r2);
    std::cout << "    FormatRef 'John 3' => '" << fmt2 << "'\n";
    CHECK(fmt2 == "John 3", "FormatRef chapter");
}

static void test_osis_ref_list() {
    std::cout << "\n--- osis_ref_list ---\n";
    
    auto refs = sheath::ParseOsisRefList("John.3.16 Rom.5.8");
    std::cout << "    Parsed " << refs.size() << " OSIS refs\n";
    CHECK(refs.size() == 2, "Parse two OSIS refs");
    if (refs.size() >= 2) {
        CHECK(refs[0].chapter == 3 && refs[0].verse == 16, "First ref: John 3:16");
        CHECK(refs[1].chapter == 5 && refs[1].verse == 8, "Second ref: Rom 5:8");
    }
    
    // Range (end ignored)
    refs = sheath::ParseOsisRefList("Rom.5.8-Rom.5.9");
    std::cout << "    Range refs: " << refs.size() << "\n";
    CHECK(refs.size() == 1, "Range produces one ref (end ignored)");
    if (refs.size() >= 1) {
        CHECK(refs[0].chapter == 5 && refs[0].verse == 8, "Range start: Rom 5:8");
    }
}

static void test_markup_samples() {
    std::cout << "\n--- markup_samples ---\n";
    
    // Load sample files
    std::string xhtml1 = loadFile("tests/samples/kjv_john3_16.xhtml");
    std::string xhtml2 = loadFile("tests/samples/kjv_gen1_1.xhtml");
    
    CHECK(!xhtml1.empty(), "Loaded kjv_john3_16.xhtml");
    CHECK(!xhtml2.empty(), "Loaded kjv_gen1_1.xhtml");
    
    if (!xhtml1.empty()) {
        auto blocks = sheath::ParseXhtml(xhtml1);
        CHECK(!blocks.empty(), "John 3:16 parses into blocks");
        
        // Collect all runs and check for Strong's links
        int strongCount = 0;
        bool hasG25 = false;
        bool hasGod = false, hasLoved = false, hasWorld = false;
        
        for (auto& b : blocks) {
            for (auto& r : b.runs) {
                if (r.link.kind == sheath::LinkKind::Strongs) {
                    strongCount++;
                    if (r.link.target == "G25") hasG25 = true;
                }
                if (r.text.find("God") != std::string::npos) hasGod = true;
                if (r.text.find("loved") != std::string::npos) hasLoved = true;
                if (r.text.find("world") != std::string::npos) hasWorld = true;
            }
        }
        
        std::string plain = sheath::PlainText(blocks);
        
        std::cout << "    Strong's links: " << strongCount << "\n";
        CHECK(strongCount > 0, "John 3:16 has Strong's links");
        CHECK(hasG25, "John 3:16 has G25 link (agapao → 'loved')");
        CHECK(hasGod, "John 3:16 parsed text contains 'God'");
        CHECK(hasLoved, "John 3:16 parsed text contains 'loved'");
        CHECK(hasWorld, "John 3:16 parsed text contains 'world'");
    }
    
    if (!xhtml2.empty()) {
        auto blocks = sheath::ParseXhtml(xhtml2);
        CHECK(!blocks.empty(), "Genesis 1:1 parses into blocks");
        
        std::string plain = sheath::PlainText(blocks);
        CHECK(plain.find("In the beginning") != std::string::npos,
              "Genesis 1:1 plain text has 'In the beginning'");
        CHECK(plain.find("God") != std::string::npos,
              "Genesis 1:1 plain text has 'God'");
        CHECK(plain.find("created") != std::string::npos,
              "Genesis 1:1 plain text has 'created'");
        
        // Hebrew Strong's numbers (H07225, H0430)
        bool hasH07225 = false, hasH0430 = false;
        for (auto& b : blocks) {
            for (auto& r : b.runs) {
                if (r.link.kind == sheath::LinkKind::Strongs) {
                    if (r.link.target == "H07225") hasH07225 = true;
                    if (r.link.target == "H0430") hasH0430 = true;
                }
            }
        }
        CHECK(hasH07225, "Genesis 1:1 has H07225 link (bereshit)");
        CHECK(hasH0430, "Genesis 1:1 has H0430 link (Elohim)");
    }
}

static void test_markup_edge_cases() {
    std::cout << "\n--- markup_edge_cases ---\n";
    
    // HTML entities
    std::string xhtml = "<p>It&#39;s &amp; &lt;bold&gt;</p>";
    auto blocks = sheath::ParseXhtml(xhtml);
    std::string plain = sheath::PlainText(blocks);
    CHECK(plain.find("It's") != std::string::npos, "HTML entity &#39; decoded to apostrophe");
    CHECK(plain.find("&") != std::string::npos, "HTML entity &amp; decoded to &");
    
    // Empty input
    blocks = sheath::ParseXhtml("");
    CHECK(blocks.empty(), "Empty XHTML returns empty blocks");
    
    // Self-closing milestone
    std::string xhtml2 = "<p><milestone type=\"x-p\"/>Hello</p>";
    blocks = sheath::ParseXhtml(xhtml2);
    CHECK(!blocks.empty(), "Self-closing milestone doesn't crash");
    
    // Nested tags
    std::string xhtml3 = "<p><em>hello</em> <b>world</b></p>";
    blocks = sheath::ParseXhtml(xhtml3);
    plain = sheath::PlainText(blocks);
    CHECK(plain.find("hello") != std::string::npos, "Nested <em> text extracted");
    CHECK(plain.find("world") != std::string::npos, "Nested <b> text extracted");
}

static void test_entry_empty() {
    std::cout << "\n--- entry_empty ---\n";
    const char* spath = getenv("SWORD_PATH");
    std::string swordPath = spath ? spath : "";
    sheath::SwordLibrary sword(swordPath);
    
    // Non-existent module
    std::string result = sword.Entry("NonExistent", "G1", sheath::DisplayOptions{});
    CHECK(result == "", "Non-existent module returns empty string");
    
    // Non-existent key
    result = sword.Entry("StrongsGreek", "99999", sheath::DisplayOptions{});
    CHECK(result == "", "Non-existent key returns empty string");
}

// ---- main ----

int main() {
    std::cout << "=== Sheath Core Tests ===\n\n";
    
    test_modules();
    test_books();
    test_versecount();
    test_chapter_text();
    test_strongs_links();
    test_redletter();
    test_dict_entry();
    test_parse_ref();
    test_format_ref();
    test_osis_ref_list();
    test_markup_samples();
    test_markup_edge_cases();
    test_entry_empty();
    
    std::cout << "\n=== Results ===\n";
    std::cout << "TOTAL " << (g_pass + g_fail) << " PASS " << g_pass << " FAIL " << g_fail << "\n";
    
    return g_fail > 0 ? 1 : 0;
}
