#pragma once
#include "Markup.h"
#include <string>
#include <vector>
#include <map>
namespace sheath {

struct Span {
    size_t start, end;
    unsigned runStyle;
    Link link;
    unsigned blockStyle;
    float blockSize;
};

// Style of a piece of text as the renderer needs it.
struct TextStyle { unsigned style = 0; float size = 1.0f; };   // size: multiple of base font size

class Metrics {   // implemented by the UI with BFont; by a fake in tests
public:
    virtual ~Metrics() {}
    virtual float Width(const std::string& utf8, const TextStyle& s) const = 0;
    virtual float Ascent(const TextStyle& s) const = 0;
    virtual float Descent(const TextStyle& s) const = 0;
};

// One verse (or one entry) as input to the layout.
struct LayoutVerse {
    int verse = 0;                 // 0 = no verse number (dictionary entry, heading-only)
    std::vector<Block> blocks;     // from ParseXhtml
};

struct LayoutOptions {
    float width = 400;             // available width in pixels
    float lineGap = 0.25f;         // extra space between lines, multiple of line height
    float paragraphGap = 0.5f;     // extra space after a Paragraph block, multiple of line height
    bool versePerLine = false;     // each verse starts on a new line
    bool showVerseNumbers = true;  // verse number drawn as kSuperscript|kSmall, size 0.7, before the verse
    float headingSize = 1.25f;     // Heading blocks: kBold at this size, on their own line
};

struct Fragment {                  // a piece of one run placed on one line
    std::string text;
    TextStyle style;
    Link link;
    int verse = 0;                 // verse this fragment belongs to
    bool isVerseNumber = false;
    float x = 0, baseline = 0, width = 0;
    size_t docStart = 0, docEnd = 0;  // byte range in the document's plain text (see below)
};
struct Line { float top = 0, bottom = 0, baseline = 0; std::vector<Fragment> frags; };

class Layout {
public:
    void Build(const std::vector<LayoutVerse>& verses, const LayoutOptions& opts, const Metrics& m);
    const std::vector<Line>& Lines() const;
    float Height() const;                      // bottom of last line, 0 if empty
    const std::string& Text() const;           // the document's plain text (what Copy copies)
    // Hit testing. Returns a byte offset into Text() nearest to (x, y), always on a UTF-8
    // character boundary; points left of a line map to its start, right of it to its end,
    // above the first line to 0, below the last to Text().size().
    size_t OffsetAt(float x, float y, const Metrics& m) const;
    const Fragment* FragmentAt(float x, float y) const;   // nullptr if none under the point
    int VerseAt(float x, float y) const;                  // verse of the fragment under/nearest; 0 if none
    // Rectangles (left, top, right, bottom) covering text range [start, end), one per line touched.
    struct Rect { float l, t, r, b; };
    std::vector<Rect> RangeRects(size_t start, size_t end, const Metrics& m) const;
    // Byte range of a verse in Text() (including its number), {0,0} if absent.
    std::pair<size_t, size_t> VerseRange(int verse) const;
    float VerseTop(int verse) const;           // top of the line where the verse starts; -1 if absent
private:
    std::string m_text;
    std::vector<Span> m_spans;
    std::map<size_t,int> m_verseNumPositions;
    std::map<int,std::pair<size_t,size_t>> m_verseRanges;
    std::vector<Line> m_lines;
    float m_height;
    void AddLine(std::vector<Fragment>&, float, const Metrics&);
    int FindVerse(size_t) const;
};
}
