#pragma once
#include <string>
namespace sheath {

struct OsisFlags { bool strongs = false, redLetter = false, footnotes = false; };

// Converts one verse of MyBible markup into OSIS for a SWORD module. Pure function, no I/O.
// newTestament: bare Strong's numbers get "G" (true) or "H" (false).
// `used` (may be null): set to true for each feature the text used; never reset to false.
//
// Mapping (exactly):
//   <J>x</J>   -> <q who="Jesus" marker="">x</q>          (redLetter)
//   <i>x</i>   -> <transChange type="added">x</transChange>
//   <e>x</e>   -> <hi type="emphasis">x</hi>
//   <f>x</f>   -> <note type="x-footnote">x</note>        (footnotes; x converted too)
//   <n>x</n>, <m>x</m> -> removed together with x
//   <t>x</t>   -> x<lb/>
//   <pb/>      -> <milestone type="x-p"/>
//   <br/>      -> <lb/>
//   word<S>25</S>         -> <w lemma="strong:G25">word</w>     (strongs)
//   word<S>1</S><S>2</S>  -> <w lemma="strong:G1 strong:G2">word</w>
//     "word" is the run of non-space characters directly before the first <S>, stopping at
//     a tag boundary, and without trailing ASCII punctuation (.,;:!?) which stays outside:
//     "God,<S>430</S>" -> <w lemma="strong:H430">God</w>,
//     No word before it (start of text, after a space or a tag) -> <w lemma="..."></w>.
//     A number that already starts with G or H keeps its prefix ("H0430" stays "H0430").
//   Any other tag: dropped, its content kept. Stray closing tags: dropped.
//   Tags still open at the end are closed, so the result is always well-formed.
//   Tag names are case-sensitive as listed (<J> and <S> upper case, the rest lower case).
// Text: "&" starting an entity (&name; or &#123; or &#x1F;) is kept; any other & -> &amp;
//   "<" that doesn't start a tag (e.g. "a < b") -> &lt;   ">" outside a tag -> &gt;
std::string MyBibleToOsis(const std::string& text, bool newTestament, OsisFlags* used);
}
