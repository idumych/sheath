#include "Ref.h"
#include <sword/versekey.h>
#include <sword/swbuf.h>
#include <sword/swkey.h>
#include <sword/versificationmgr.h>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace sheath {

// Default versification system name used internally
static const char* kDefaultV11n = "KJV";

// Try to parse text with VerseKey and extract book/chapter/verse.
// Returns true if parsing succeeded (book > 0 and chapter > 0).
static bool parseWithVerseKey(const std::string& text, int& book, int& chapter, int& verse,
                               const char*& bookNameOut, const std::string& v11n) {
    sword::VerseKey vk;
    vk.setVersificationSystem(v11n.empty() ? kDefaultV11n : v11n.c_str());
    vk.setText(text.c_str());
    
    book = vk.getBook();
    chapter = vk.getChapter();
    verse = vk.getVerse();
    
    if (book <= 0) return false;
    
    bookNameOut = vk.getBookName();
    return true;
}

// Check if a string could be the start of a valid reference by parsing with VerseKey.
// This handles abbreviations that aren't in the KJV system's abbrev table
// (like "Jn" for John) because VerseKey has its own abbreviation tables.
static bool couldBeBookName(const std::string& text, const std::string& v11n) {
    int book = 0, chapter = 0, verse = 0;
    const char* bn = nullptr;
    
    // Try as a standalone book name
    if (parseWithVerseKey(text, book, chapter, verse, bn, v11n)) {
        // Valid book if we got a book number, chapter >= 0, verse == 0
        // (standalone book name shouldn't have chapter/verse)
        if (book > 0 && chapter == 0 && verse == 0) return true;
        // Also accept if it parsed with chapter/verse (e.g. "John 3:16")
        // as long as the book is valid and chapter > 0
        if (book > 0 && chapter > 0) {
            // Verify the book name matches by checking a fresh parse
            std::string firstWord = text.substr(0, text.find_first_of(" \t"));
            sword::VerseKey vk;
            vk.setVersificationSystem(v11n.empty() ? kDefaultV11n : v11n.c_str());
            vk.setBookName(firstWord.c_str());
            if (vk.getBook() > 0) return true;
        }
    }
    
    // Try with a dummy chapter to see if the first word is a valid book
    if (text.find_first_of(" \t") != std::string::npos) {
        std::string firstWord = text.substr(0, text.find_first_of(" \t"));
        sword::VerseKey vk;
        vk.setVersificationSystem(v11n.empty() ? kDefaultV11n : v11n.c_str());
        vk.setBookName(firstWord.c_str());
        if (vk.getBook() > 0) return true;
    }
    
    return false;
}

// Check if text looks like it has a reference component (chapter:verse after a book name)
static bool hasRefComponents(const std::string& text) {
    // Must have at least one colon or dot-separated number
    if (text.find(':') != std::string::npos) return true;
    if (text.find('.') != std::string::npos) {
        // Check that the dot is followed by a number (not just a period at the end)
        size_t dotPos = text.find('.');
        if (dotPos + 1 < text.size() && std::isdigit(text[dotPos + 1])) return true;
    }
    // Check for space followed by a number (chapter)
    size_t spacePos = text.find(' ');
    if (spacePos != std::string::npos && spacePos + 1 < text.size()) {
        if (std::isdigit(text[spacePos + 1])) return true;
        // Multi-word book like "1 Cor 13" - check after second word
        size_t space2 = text.find(' ', spacePos + 1);
        if (space2 != std::string::npos && space2 + 1 < text.size()) {
            if (std::isdigit(text[space2 + 1])) return true;
        }
    }
    return false;
}

bool ParseRef(const std::string& text, VerseRef* out, const std::string& v11n) {
    if (text.empty()) return false;

    // Extract the book name part from the start.
    // Handle multi-word book names like "1 Cor" (1 Corinthians).
    size_t pos = 0;
    std::string bookText;
    
    // Try single word first
    size_t firstSpace = text.find_first_of(" \t");
    std::string firstWord = (firstSpace == std::string::npos) ? text : text.substr(0, firstSpace);
    
    if (couldBeBookName(firstWord, v11n)) {
        // Try two words (e.g. "1 Cor" for 1 Corinthians)
        size_t secondSpace = text.find_first_of(" \t", firstSpace + 1);
        std::string twoWords = (secondSpace == std::string::npos) ? firstWord :
                               text.substr(0, secondSpace);
        
        // Try parsing with two words
        int book = 0, chapter = 0, verse = 0;
        const char* bn = nullptr;
        if (parseWithVerseKey(twoWords, book, chapter, verse, bn, v11n)) {
            // Verify the second word is not a chapter number itself
            std::string secondPart = text.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            bool secondPartIsNum = !secondPart.empty() && std::isdigit(static_cast<unsigned char>(secondPart[0]));
            
            if (!secondPartIsNum) {
                bookText = twoWords;
            } else {
                // The second word looks like a number, so use first word only
                bookText = firstWord;
            }
        } else {
            // Two words didn't parse, use first word
            bookText = firstWord;
        }
    } else {
        // First word is not a valid book name
        return false;
    }
    
    // Now parse the full text to get chapter and verse
    int book = 0, chapter = 0, verse = 0;
    const char* bn = nullptr;
    
    if (!parseWithVerseKey(text, book, chapter, verse, bn, v11n)) {
        return false;
    }
    
    // Must have a valid book and at least a chapter
    if (book <= 0 || chapter <= 0) return false;
    
    // Verify the parse actually extracted chapter/verse from the input text,
    // not from VerseKey defaults. If there are no digits in the text after
    // the book name, the chapter/verse couldn't have come from input.
    std::string remainder = text.substr(bookText.size());
    bool hasDigitInRemainder = false;
    for (char c : remainder) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            hasDigitInRemainder = true;
            break;
        }
    }
    if (chapter > 0 && !hasDigitInRemainder) {
        // No digits after book name but got a chapter - likely defaults.
        // Check if there are any number-like patterns (colon/dot notation)
        bool hasRefNotation = text.find(':') != std::string::npos ||
                              text.find('.') != std::string::npos;
        if (!hasRefNotation) {
            return false;
        }
    }
    
    // If the input didn't have a colon or dot notation for verse,
    // and the parsed verse is non-zero, it might have misparsed a number as verse.
    // In that case, set verse to 0 (chapter-only reference).
    bool hasColon = text.find(':') != std::string::npos;
    bool hasDotRef = text.find('.') != std::string::npos;
    
    if (!hasColon && !hasDotRef && chapter > 0) {
        // No colon/dot notation: if the last token is a number, it's a chapter
        // (not a verse), so set verse=0.
        // We verify by re-parsing without the last token and checking that
        // the chapter changes (indicating the number was a chapter spec).
        std::string noLastNum = text;
        size_t lastSpace = noLastNum.find_last_of(" \t");
        if (lastSpace != std::string::npos && lastSpace + 1 < noLastNum.size()) {
            std::string lastToken = noLastNum.substr(lastSpace + 1);
            bool lastIsNum = !lastToken.empty() && std::isdigit(static_cast<unsigned char>(lastToken[0]));
            if (lastIsNum) {
                noLastNum = noLastNum.substr(0, lastSpace);
                int book2 = 0, chapter2 = 0, verse2 = 0;
                const char* bn2 = nullptr;
                if (parseWithVerseKey(noLastNum, book2, chapter2, verse2, bn2, v11n)) {
                    if (book2 == book && chapter2 != chapter) {
                        // The last token changed the chapter, so it was a chapter number
                        verse = 0;
                    } else if (book2 == book && chapter2 == chapter && verse == 1) {
                        // Chapter didn't change but verse=1 (likely default from VerseKey).
                        // Without colon/dot, the number was a chapter specification.
                        verse = 0;
                    }
                }
            }
        }
    }
    
    if (out) {
        out->book = bn ? bn : bookText;
        out->chapter = chapter;
        out->verse = verse;
    }
    return true;
}

std::string FormatRef(const VerseRef& r) {
    if (r.book.empty()) return "";
    if (r.verse == 0) {
        return r.book + " " + std::to_string(r.chapter);
    }
    return r.book + " " + std::to_string(r.chapter) + ":" + std::to_string(r.verse);
}

// Parse a single OSIS ref like "John.3.16" or "Rom.5.8"
// Uses VerseKey's built-in abbreviations to map OSIS-style names
static bool parseSingleOsis(const std::string& osis, VerseRef& out, const std::string& v11n) {
    // Convert OSIS dot notation to VerseKey-parseable format
    // "Gen.1.1" -> "Gen 1:1", "1Co.5.8" -> "1Co 5:8"
    std::string converted;
    bool inChapter = false;
    bool inVerse = false;
    size_t i = 0;

    while (i < osis.size()) {
        if (osis[i] == '.') {
            if (!inChapter) {
                inChapter = true;
                converted += ' ';
            } else {
                inVerse = true;
                converted += ':';
            }
        } else {
            converted += osis[i];
        }
        i++;
    }

    int book = 0, chapter = 0, verse = 0;
    const char* bn = nullptr;
    if (!parseWithVerseKey(converted, book, chapter, verse, bn, v11n)) {
        return false;
    }
    
    out.book = bn ? bn : "";
    out.chapter = chapter;
    out.verse = verse;
    return true;
}

std::vector<VerseRef> ParseOsisRefList(const std::string& osisRefs) {
    std::vector<VerseRef> result;
    if (osisRefs.empty()) return result;

    // Split by whitespace into individual refs
    std::istringstream iss(osisRefs);
    std::string token;

    while (iss >> token) {
        // Check for range: "Rom.5.8-Rom.5.9"
        size_t rangePos = token.find('-');
        if (rangePos != std::string::npos) {
            // Parse start of range
            std::string start = token.substr(0, rangePos);
            VerseRef ref;
            if (parseSingleOsis(start, ref, kDefaultV11n)) {
                result.push_back(ref);
            }
            // Parse end of range (ignored - task says "end ignored")
            continue;
        }

        VerseRef ref;
        if (parseSingleOsis(token, ref, kDefaultV11n)) {
            result.push_back(ref);
        }
    }

    return result;
}

}
