#pragma once
#include <string>
namespace sheath {

struct ImportResult {
	std::string module;      // SWORD module name written, e.g. "KJVMB"
	int verses = 0;          // verses written
	int skipped = 0;         // verses not written (unknown book number, or chapter/verse outside
	                         // the KJV versification)
};

// Converts a MyBible Bible module (an SQLite file, usually "<NAME>.SQLite3") into a SWORD
// module that SwordLibrary(userDir) can read.
//
// Input (MyBible format):
//   table info(name TEXT, value TEXT): rows "description", "language" (e.g. "en"), others ignored
//   table verses(book_number INTEGER, chapter INTEGER, verse INTEGER, text TEXT)
//   book_number: MyBible numbering, 10 = Gen, 20 = Exod ... 460 = Mal, 470 = Matt ... 730 = Rev
//   (see kMyBibleBooks in MyBible.cpp; other numbers, e.g. deuterocanonical books, are skipped)
//   text markup: <J>..</J> words of Christ, <i>..</i> added words, <S>NNN</S> Strong's number
//   for the word before it (Hebrew in the OT, Greek in the NT), <f>..</f> footnote,
//   <n>..</n> and <m>..</m> notes/morphology, <t>..</t> poetry line, <pb/> paragraph,
//   <br/> line break, <e>..</e> emphasis. Any other tag: dropped, its text kept.
//
// Output: a zText module (OSIS markup, KJV versification, BOOK blocks, ZIP compression)
//   userDir/mods.d/<name lower case>.conf
//   userDir/modules/texts/ztext/<name lower case>/
// Markup mapping: <J> -> <q who="Jesus" marker="">, <i> -> <transChange type="added">,
//   word<S>25</S> -> <w lemma="strong:G25">word</w> (several <S> after one word -> one lemma
//   attribute "strong:G1 strong:G2"), <f>x</f> -> <note type="x-footnote">x</note>,
//   <n>/<m> -> removed with their content, <t>x</t> -> x<lb/>, <pb/> -> <milestone type="x-p"/>,
//   <br/> -> <lb/>, <e>x</e> -> <hi type="emphasis">x</hi>. Text is XML-escaped (& < >).
// The conf has Description and Lang from the info table, and GlobalOptionFilter lines for
// OSISStrongs (if any <S>), OSISRedLetterWords (if any <J>), OSISFootnotes (if any <f>).
//
// moduleName "" = derived from the file name ("KJV.SQLite3" -> "KJV"), letters and digits only.
// Fails (false + one-line error) if the file isn't a MyBible Bible, if a module with that name
// already exists in userDir, or if nothing could be written.
bool ImportMyBible(const std::string& sqlitePath, const std::string& userDir,
	const std::string& moduleName, ImportResult* result, std::string* error);
}
