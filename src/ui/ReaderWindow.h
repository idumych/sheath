#pragma once

#include <Window.h>

#include <map>
#include <string>
#include <vector>

#include "Bookmarks.h"
#include "Sword.h"

class BButton;
class BMenu;
class BMenuField;
class BPopUpMenu;
class BTextControl;
class ChapterView;
class SearchWindow;

struct ReaderState {
	std::string module;
	std::string book = "Gen";
	int chapter = 1;
	int verse = 0;
	float fontSize = 0;		// 0 = from be_plain_font
	sheath::DisplayOptions options;
};

class ReaderWindow : public BWindow {
public:
	ReaderWindow(BRect frame, sheath::SwordLibrary* library, const ReaderState& state,
		const std::string& bookmarksPath);

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

	ReaderState State() const;

	// Rebuilds the module menu after modules were installed or removed.
	void LibraryChanged();

private:
	void BuildModuleMenu();
	void BuildBookMenu();
	void BuildChapterMenu();
	void ShowChapter(int scrollToVerse = 0);
	void GoTo(const std::string& book, int chapter, int verse);
	void Step(int delta);
	void OpenLink(int32 kind, const std::string& target, int verse);
	void UpdateOptionItems();
	void BuildBookmarksMenu();
	void ShowVerseMenu(int verse, BPoint where);
	void CopyVerse(int verse);
	void ToggleBookmark(int verse);

	sheath::SwordLibrary* fLibrary;
	ReaderState fState;
	std::vector<sheath::BookInfo> fBooks;
	std::vector<sheath::VerseText> fVerses;

	BMenu* fModuleMenu;
	BMenu* fViewMenu;
	BMenu* fBookmarksMenu;
	sheath::BookmarkStore fBookmarks;
	BPopUpMenu* fBookMenu;
	BPopUpMenu* fChapterMenu;
	BMenuField* fBookField;
	BMenuField* fChapterField;
	BButton* fPrevButton;
	BButton* fNextButton;
	BTextControl* fRefControl;
	ChapterView* fText;
	SearchWindow* fSearch;
};
