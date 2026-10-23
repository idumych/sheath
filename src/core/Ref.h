#pragma once
#include <string>
#include <vector>
namespace sheath {
struct VerseRef { std::string book; int chapter = 0; int verse = 0; };  // book = OSIS id, verse 0 = whole chapter
// Parses "John 3:16", "jn 3.16", "1 Cor 13", "Gen 1:1-3" (range end ignored), "John.3.16".
// Uses sword::VerseKey with the given versification. false if not a reference.
bool ParseRef(const std::string& text, VerseRef* out, const std::string& v11n = "KJV");
std::string FormatRef(const VerseRef& r);        // "John 3:16" / "John 3" (English book names from SWORD)
std::vector<VerseRef> ParseOsisRefList(const std::string& osisRefs); // "John.3.16 Rom.5.8-Rom.5.9"
}
