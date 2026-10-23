#include "MyBible.h"

#include "MyBibleMarkup.h"

#include <filemgr.h>
#include <versekey.h>
#include <zipcomprs.h>
#include <ztext.h>

#include <sqlite3.h>

#include <cctype>
#include <fstream>
#include <map>

namespace sheath {

namespace {

// MyBible's own book numbering -> OSIS. Other numbers (deuterocanonical books) are skipped.
const std::map<int, const char*> kMyBibleBooks = {
	{10, "Gen"}, {20, "Exod"}, {30, "Lev"}, {40, "Num"}, {50, "Deut"}, {60, "Josh"},
	{70, "Judg"}, {80, "Ruth"}, {90, "1Sam"}, {100, "2Sam"}, {110, "1Kgs"}, {120, "2Kgs"},
	{130, "1Chr"}, {140, "2Chr"}, {150, "Ezra"}, {160, "Neh"}, {190, "Esth"}, {220, "Job"},
	{230, "Ps"}, {240, "Prov"}, {250, "Eccl"}, {260, "Song"}, {290, "Isa"}, {300, "Jer"},
	{310, "Lam"}, {330, "Ezek"}, {340, "Dan"}, {350, "Hos"}, {360, "Joel"}, {370, "Amos"},
	{380, "Obad"}, {390, "Jonah"}, {400, "Mic"}, {410, "Nah"}, {420, "Hab"}, {430, "Zeph"},
	{440, "Hag"}, {450, "Zech"}, {460, "Mal"}, {470, "Matt"}, {480, "Mark"}, {490, "Luke"},
	{500, "John"}, {510, "Acts"}, {520, "Jas"}, {530, "1Pet"}, {540, "2Pet"}, {550, "1John"},
	{560, "2John"}, {570, "3John"}, {580, "Jude"}, {590, "Rom"}, {600, "1Cor"}, {610, "2Cor"},
	{620, "Gal"}, {630, "Eph"}, {640, "Phil"}, {650, "Col"}, {660, "1Thess"}, {670, "2Thess"},
	{680, "1Tim"}, {690, "2Tim"}, {700, "Titus"}, {710, "Phlm"}, {720, "Heb"}, {730, "Rev"},
};
const int kFirstNewTestamentBook = 470;

bool Fail(std::string* error, const std::string& message)
{
	if (error)
		*error = message;
	return false;
}

std::string Lower(std::string s)
{
	for (char& c : s)
		c = (char)std::tolower((unsigned char)c);
	return s;
}

// "dir/KJV.SQLite3" -> "KJV"; letters and digits only.
std::string NameFromPath(const std::string& path)
{
	size_t slash = path.find_last_of('/');
	std::string base = path.substr(slash == std::string::npos ? 0 : slash + 1);
	size_t dot = base.find('.');
	if (dot != std::string::npos)
		base.erase(dot);
	std::string name;
	for (char c : base) {
		if (std::isalnum((unsigned char)c))
			name += c;
	}
	return name;
}

// Conf values are one line each.
std::string OneLine(std::string s)
{
	for (char& c : s) {
		if (c == '\n' || c == '\r')
			c = ' ';
	}
	return s;
}

class Database {
public:
	explicit Database(const std::string& path)
	{
		if (sqlite3_open_v2(path.c_str(), &fDb, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
			sqlite3_close(fDb);
			fDb = nullptr;
		}
	}
	~Database() { sqlite3_close(fDb); }

	sqlite3* Handle() const { return fDb; }

	// false when the file isn't SQLite (sqlite only notices on the first query) or lacks it.
	bool HasTable(const char* name) const
	{
		sqlite3_stmt* st = nullptr;
		bool found = false;
		if (fDb && sqlite3_prepare_v2(fDb,
				"SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", -1, &st, nullptr) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
			found = sqlite3_step(st) == SQLITE_ROW;
		}
		sqlite3_finalize(st);
		return found;
	}

	std::string Info(const char* key) const
	{
		sqlite3_stmt* st = nullptr;
		std::string value;
		if (HasTable("info") && sqlite3_prepare_v2(fDb, "SELECT value FROM info WHERE name=?", -1,
				&st, nullptr) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
			if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0))
				value = (const char*)sqlite3_column_text(st, 0);
		}
		sqlite3_finalize(st);
		return value;
	}

private:
	sqlite3* fDb = nullptr;
};

}	// namespace


bool ImportMyBible(const std::string& sqlitePath, const std::string& userDir,
	const std::string& moduleName, ImportResult* result, std::string* error)
{
	ImportResult res;
	if (!sword::FileMgr::existsFile(sqlitePath.c_str()))
		return Fail(error, "No such file: " + sqlitePath);
	Database db(sqlitePath);
	if (db.Handle() == nullptr || !db.HasTable("verses"))
		return Fail(error, sqlitePath + " isn't a MyBible Bible module (no verses table)");

	res.module = moduleName.empty() ? NameFromPath(sqlitePath) : moduleName;
	if (res.module.empty())
		return Fail(error, "Can't make a module name from " + sqlitePath);
	std::string lower = Lower(res.module);
	std::string confPath = userDir + "/mods.d/" + lower + ".conf";
	std::string dataRel = "modules/texts/ztext/" + lower + "/";
	std::string dataPath = userDir + "/" + dataRel;
	if (sword::FileMgr::existsFile(confPath.c_str()) || sword::FileMgr::existsDir(dataPath.c_str()))
		return Fail(error, "A module named " + res.module + " is already installed");

	sword::FileMgr::createParent(confPath.c_str());
	sword::FileMgr::createParent((dataPath + "x").c_str());
	const int kBookBlocks = 4;
	if (sword::zText::createModule(dataPath.c_str(), kBookBlocks, "KJV") != 0)
		return Fail(error, "Can't create the module files in " + dataPath);

	OsisFlags used;
	{
		// The module owns the compressor.
		sword::zText module(dataPath.c_str(), 0, 0, kBookBlocks, new sword::ZipCompress(), 0,
			sword::ENC_UNKNOWN, sword::DIRECTION_LTR, sword::FMT_UNKNOWN, 0, "KJV");
		sword::VerseKey key;
		key.setVersificationSystem("KJV");
		key.setAutoNormalize(false);
		key.setIntros(false);

		sqlite3_stmt* st = nullptr;
		if (sqlite3_prepare_v2(db.Handle(), "SELECT book_number, chapter, verse, text FROM verses "
				"ORDER BY book_number, chapter, verse", -1, &st, nullptr) != SQLITE_OK) {
			sword::FileMgr::removeDir(dataPath.c_str());
			return Fail(error, std::string("Can't read the verses: ") + sqlite3_errmsg(db.Handle()));
		}
		while (sqlite3_step(st) == SQLITE_ROW) {
			int book = sqlite3_column_int(st, 0);
			int chapter = sqlite3_column_int(st, 1);
			int verse = sqlite3_column_int(st, 2);
			const unsigned char* raw = sqlite3_column_text(st, 3);
			auto osis = kMyBibleBooks.find(book);
			if (osis == kMyBibleBooks.end() || raw == nullptr) {
				res.skipped++;
				continue;
			}
			// Place the key, then check SWORD kept it where we asked (the versification knows
			// each book's chapter and verse counts).
			key.setText((std::string(osis->second) + " 1:1").c_str());
			if (key.popError() || chapter < 1 || chapter > key.getChapterMax()) {
				res.skipped++;
				continue;
			}
			key.setChapter(chapter);
			if (verse < 1 || verse > key.getVerseMax()) {
				res.skipped++;
				continue;
			}
			key.setVerse(verse);
			std::string text = MyBibleToOsis((const char*)raw, book >= kFirstNewTestamentBook, &used);
			module.setKey(key);
			module.setEntry(text.c_str());
			res.verses++;
		}
		sqlite3_finalize(st);
	}
	if (res.verses == 0) {
		sword::FileMgr::removeDir(dataPath.c_str());
		return Fail(error, "No verses could be imported from " + sqlitePath);
	}

	std::ofstream conf(confPath);
	conf << "[" << res.module << "]\n"
		<< "DataPath=./" << dataRel << "\n"
		<< "ModDrv=zText\nSourceType=OSIS\nEncoding=UTF-8\nCompressType=ZIP\nBlockType=BOOK\n"
		<< "Versification=KJV\n";
	std::string lang = db.Info("language");
	if (!lang.empty())
		conf << "Lang=" << OneLine(lang) << "\n";
	std::string description = db.Info("description");
	conf << "Description=" << OneLine(description.empty() ? res.module : description) << "\n";
	if (used.strongs)
		conf << "GlobalOptionFilter=OSISStrongs\nFeature=StrongsNumbers\n";
	if (used.redLetter)
		conf << "GlobalOptionFilter=OSISRedLetterWords\n";
	if (used.footnotes)
		conf << "GlobalOptionFilter=OSISFootnotes\n";
	// No OSISHeadings: MyBible verses carry no headings (those live in a separate table), and
	// SWORD 1.9's OSISHeadings filter drops "&amp;" from the text.
	conf << "About=Imported from the MyBible module " << OneLine(NameFromPath(sqlitePath))
		<< " by Sheath.\n";
	conf.close();
	if (!conf)
		return Fail(error, "Can't write " + confPath);

	if (result)
		*result = res;
	return true;
}

}	// namespace sheath
