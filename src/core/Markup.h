#pragma once
#include <string>
#include <vector>
namespace sheath {
enum RunStyle : unsigned {
    kItalic = 1, kBold = 2, kRedLetter = 4, kSmall = 8, kSuperscript = 16,
    kDivineName = 32,   // small caps LORD
    kTransChange = 64,  // KJV added words (rendered italic)
};
enum class LinkKind { None, Strongs, Morph, Footnote, CrossRef, Url };
struct Link { LinkKind kind = LinkKind::None; std::string target; };
// Strongs target "G25"/"H430"; Morph target "robinson:V-PAI-3S"; Footnote target = note id;
// CrossRef target = an OSIS ref list like "John.3.16 Rom.5.8"; Url = href.
struct Run { std::string text; unsigned style = 0; Link link; };   // text is UTF-8, entities decoded
struct Block { enum Kind { Paragraph, Heading, LineBreak } kind = Paragraph; std::vector<Run> runs; };
std::vector<Block> ParseXhtml(const std::string& xhtml);
std::string PlainText(const std::vector<Block>& blocks);
// In a Strong's dictionary entry, links the numbers in "see GREEK for 5689", "see HEBREW for
// 0430", "compare 5368" and "from 1537" (Strongs links; the last two use defaultPrefix, the
// dictionary's own language, 'G' or 'H'). Only the number becomes the link.
void LinkStrongsReferences(std::vector<Block>& blocks, char defaultPrefix);   // runs joined, blocks separated by "\n"
}
