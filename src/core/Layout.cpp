#include "Layout.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <vector>

namespace sheath {

namespace {

static size_t UTF8Advance(const std::string& s, size_t pos) {
    if (pos >= s.size()) return pos;
    unsigned char c = static_cast<unsigned char>(s[pos]);
    if (c < 0x80) return pos + 1;
    if (c < 0xC0) return pos;
    if (c < 0xE0) return pos + 2;
    if (c < 0xF0) return pos + 3;
    return pos + 4;
}

static size_t UTF8EndOfCodepoint(const std::string& s, size_t start) {
    return UTF8Advance(s, start);
}

// Find the span covering position p (pointer; nullptr if not found)
static const Span* FindSpanAt(const std::vector<Span>& spans, size_t p) {
    for (auto& sp : spans) {
        if (p >= sp.start && p < sp.end) return &sp;
    }
    return nullptr;
}

} // anonymous

void Layout::Build(const std::vector<LayoutVerse>& verses, const LayoutOptions& opts, const Metrics& m) {
    // -- Phase 1: build Text(), spans, verse info --
    m_text.clear();
    m_spans.clear();
    m_verseNumPositions.clear();
    m_verseRanges.clear();

    for (size_t vi = 0; vi < verses.size(); vi++) {
        const auto& verse = verses[vi];
        size_t verseStart = m_text.size();

        // Verse number
        if (opts.showVerseNumbers && verse.verse > 0) {
            std::string numStr = std::to_string(verse.verse);
            size_t numPos = m_text.size();
            m_text += numStr + " ";
            m_verseNumPositions[numPos] = verse.verse;
        } else {
            m_verseNumPositions[m_text.size()] = verse.verse;
        }

        // Verse blocks
        for (auto& block : verse.blocks) {
            unsigned blockStyle = (block.kind == Block::Heading) ? kBold : 0;
            float blockSize = (block.kind == Block::Heading) ? opts.headingSize : 1.0f;

            for (auto& run : block.runs) {
                Span sp;
                sp.start = m_text.size();
                sp.end = m_text.size() + run.text.size();
                sp.runStyle = run.style;
                sp.link = run.link;
                sp.blockStyle = blockStyle;
                sp.blockSize = blockSize;
                m_spans.push_back(sp);
                m_text += run.text;
            }
            // "Heading/Paragraph block ends add \n"
            if (block.kind == Block::Paragraph || block.kind == Block::Heading ||
                block.kind == Block::LineBreak) {
                m_text += "\n";
            }
        }

        m_verseRanges[verse.verse] = {verseStart, m_text.size()};

        // Verse separator
        if (vi + 1 < verses.size()) {
            if (opts.versePerLine ||
                (!verses[vi + 1].blocks.empty() &&
                 verses[vi + 1].blocks[0].kind == Block::Heading)) {
                m_text += "\n";
            } else {
                m_text += " ";
            }
        }
    }

    // -- Phase 2: layout into lines --
    m_lines.clear();
    float top = 0;
    size_t pos = 0;
    const float avail = opts.width;
    const float baseLineMetric = m.Ascent({0, 1.0f}) + m.Descent({0, 1.0f});

    while (pos < m_text.size()) {
        // Skip newlines (paragraph boundaries)
        if (m_text[pos] == '\n') {
            if (!m_lines.empty()) {
                top = m_lines.back().bottom + opts.lineGap * baseLineMetric;
            }
            pos++;
            continue;
        }

        // Find paragraph end
        size_t paraEnd = m_text.find('\n', pos);
        if (paraEnd == std::string::npos) paraEnd = m_text.size();

        // Word-wrap this paragraph into lines
        std::vector<Fragment> frags;
        float x = 0;

        while (pos < paraEnd) {
            // Skip spaces
            if (m_text[pos] == ' ') {
                pos++;
                continue;
            }

            // Find word end
            size_t wordStart = pos;
            while (pos < paraEnd && m_text[pos] != ' ') pos++;
            size_t wordEnd = pos;

            // Get style from spans
            TextStyle style = {0, 1.0f};
            const Span* span = FindSpanAt(m_spans, wordStart);
            if (span) {
                style.style = span->blockStyle | span->runStyle;
                style.size = span->blockSize;
            }

            // Check for verse number
            auto vnIt = m_verseNumPositions.find(wordStart);
            bool isVN = (vnIt != m_verseNumPositions.end());
            if (isVN) {
                style.style = kSuperscript | kSmall;
                style.size = 0.7f;
            }

            // Place the word (or its segments) onto the line(s)
            size_t segStart = wordStart;
            while (segStart < wordEnd) {
                size_t segEnd = wordEnd;

                // While the segment doesn't fit, flush line or break word
                while (true) {
                    float segW = m.Width(m_text.substr(segStart, segEnd - segStart), style);
                    bool fits = (frags.empty() && segW <= avail + 0.001f) ||
                                (!frags.empty() && x + segW <= avail + 0.001f);
                    if (fits) break;

                    if (!frags.empty()) {
                        AddLine(frags, top, m);
                        top = m_lines.back().bottom + opts.lineGap * baseLineMetric;
                        frags.clear();
                        x = 0;
                        continue;
                    }

                    // Word too long for empty line — break at UTF-8 boundary
                    size_t nbp = UTF8EndOfCodepoint(m_text, segStart);
                    if (nbp <= segStart || nbp >= segEnd) {
                        break;
                    }
                    segEnd = nbp;
                }

                // Emit fragment
                std::string fragText = m_text.substr(segStart, segEnd - segStart);
                float fragW = m.Width(fragText, style);

                Fragment f;
                f.text = fragText;
                f.style = style;
                f.link = isVN ? Link{} : (span ? span->link : Link{});
                f.verse = isVN ? vnIt->second : FindVerse(segStart);
                f.isVerseNumber = isVN;
                f.x = x;
                f.width = fragW;
                f.docStart = segStart;
                f.docEnd = segEnd;
                frags.push_back(f);
                x += fragW;

                if (segEnd >= wordEnd) break;
                segStart = segEnd;
            }
        }

        // Flush line
        if (!frags.empty()) {
            AddLine(frags, top, m);
            top = m_lines.back().bottom + opts.lineGap * baseLineMetric;
        }

        pos = paraEnd;
    }

    if (m_lines.empty()) {
        m_height = 0;
    } else {
        m_height = m_lines.back().bottom;
    }
}

void Layout::AddLine(std::vector<Fragment>& frags, float top, const Metrics& m) {
    Line line;
    line.top = top;

    float maxA = 0, maxD = 0;
    for (auto& f : frags) {
        float a = m.Ascent(f.style);
        float d = m.Descent(f.style);
        if (a > maxA) maxA = a;
        if (d > maxD) maxD = d;
    }

    line.baseline = top + maxA;
    line.bottom = line.baseline + maxD;

    for (auto& f : frags) {
        if (f.style.style & kSuperscript) {
            f.baseline = line.baseline - 0.4f * m.Ascent(f.style);
        } else {
            f.baseline = line.baseline;
        }
    }

    line.frags = std::move(frags);
    m_lines.push_back(line);
}

const std::vector<Line>& Layout::Lines() const { return m_lines; }
float Layout::Height() const { return m_height; }
const std::string& Layout::Text() const { return m_text; }

size_t Layout::OffsetAt(float x, float y, const Metrics& m) const {
    if (m_lines.empty()) return 0;
    if (y <= m_lines.front().top) return 0;
    if (y >= m_lines.back().bottom) return m_text.size();

    // Find line
    const Line* target = nullptr;
    for (auto& line : m_lines) {
        if (y >= line.top && y <= line.bottom) {
            target = &line;
            break;
        }
    }
    if (!target) {
        // Between lines: pick nearest
        const Line* best = &m_lines.front();
        float bestDist = std::numeric_limits<float>::max();
        for (auto& line : m_lines) {
            float center = (line.top + line.bottom) * 0.5f;
            float dist = std::abs(y - center);
            if (dist < bestDist) { bestDist = dist; best = &line; }
        }
        target = best;
    }

    if (target->frags.empty()) return target->top < y ? 0 : m_text.size();

    // Left of line
    if (x < target->frags.front().x) return target->frags.front().docStart;
    // Right of line
    if (x >= target->frags.back().x + target->frags.back().width)
        return target->frags.back().docEnd;

    // Find nearest fragment
    for (auto& frag : target->frags) {
        float fragCenter = frag.x + frag.width * 0.5f;
        if (x <= fragCenter) return frag.docStart;
    }
    return target->frags.back().docEnd;
}

const Fragment* Layout::FragmentAt(float x, float y) const {
    for (auto& line : m_lines) {
        if (y >= line.top && y <= line.bottom) {
            for (auto& frag : line.frags) {
                if (x >= frag.x && x < frag.x + frag.width) return &frag;
            }
        }
    }
    return nullptr;
}

int Layout::VerseAt(float x, float y) const {
    const Fragment* frag = FragmentAt(x, y);
    if (frag) return frag->verse;
    // Nearest fragment
    float bestDist = std::numeric_limits<float>::max();
    int bestVerse = 0;
    for (auto& line : m_lines) {
        for (auto& frag : line.frags) {
            float cx = frag.x + frag.width * 0.5f;
            float dist = std::sqrt((x - cx) * (x - cx) + (y - line.baseline) * (y - line.baseline));
            if (dist < bestDist) { bestDist = dist; bestVerse = frag.verse; }
        }
    }
    return bestVerse;
}

std::vector<Layout::Rect> Layout::RangeRects(size_t start, size_t end, const Metrics& m) const {
    std::vector<Rect> result;
    if (m_lines.empty() || start >= end) return result;

    for (auto& line : m_lines) {
        const Fragment* firstOvl = nullptr;
        const Fragment* lastOvl = nullptr;

        for (auto& frag : line.frags) {
            // Fragment overlaps range if frag.docEnd > start && frag.docStart < end
            if (frag.docEnd > start && frag.docStart < end) {
                if (!firstOvl) firstOvl = &frag;
                lastOvl = &frag;
            }
        }

        if (firstOvl) {
            Rect r;
            r.l = firstOvl->x;
            r.r = lastOvl->x + lastOvl->width;
            r.t = line.top;
            r.b = line.bottom;
            result.push_back(r);
        }
    }
    return result;
}

std::pair<size_t, size_t> Layout::VerseRange(int verse) const {
    auto it = m_verseRanges.find(verse);
    return (it != m_verseRanges.end()) ? it->second : std::make_pair<size_t, size_t>(0, 0);
}

float Layout::VerseTop(int verse) const {
    for (auto& line : m_lines) {
        for (auto& frag : line.frags) {
            if (frag.verse == verse) return line.top;
        }
    }
    return -1;
}

// -- Private helpers --

int Layout::FindVerse(size_t pos) const {
    // Find which verse this position belongs to by scanning verse ranges
    for (auto& p : m_verseRanges) {
        if (pos >= p.second.first && pos < p.second.second) return p.first;
    }
    return 0;
}

}
