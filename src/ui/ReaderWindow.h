#pragma once

#include <Entry.h>
#include <Window.h>

#include <map>
#include <string>
#include <vector>

#include "Bookmarks.h"
#include "ChapterView.h"
#include "Sword.h"

class BButton;
class BMenu;
class BFilePanel;
class BMenuField;
class BMenuItem;
class BPopUpMenu;
class HintField;
class PaneHeader;
class SearchWindow;
class VerseMenu;

struct ReaderState {
	std::string module;
	std::string parallel;		// second Bible shown beside the first, "" = none
	std::string book = "Gen";
	int chapter = 1;
	int verse = 0;
	ReadingStyle style;		// style.size 0 = from be_plain_font
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
	void OpenLink(int32 kind, const std::string& target, int verse, bool inParallel);
	void UpdateOptionItems();
	void BuildBookmarksMenu();
	void ShowVerseMenu(int verse, BPoint where);
	void CopyVerse(int verse);
	void ToggleBookmark(int verse);
	void ImportMyBible(const entry_ref& ref);
	void BuildDocumentMenu(const std::vector<sheath::ModuleInfo>& bibles);

	// The verse card (LinBible's tap menu) and its actions.
	void VerseTapped(int verse, ChapterView* view);
	void HideVerseMenu();
	void RefreshAfterBookmarkChange();
	void EditNote(int verse);
	void CompareVerse(int verse);
	void ShowDefinitions(int verse);

	// Back/forward through places jumped to (references, bookmarks, links, search results).
	struct Place { std::string book; int chapter; int verse; };
	void JumpTo(const std::string& book, int chapter, int verse);
	void GoHistory(int delta);
	void UpdateHistoryButtons();

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
	BButton* fBackButton;
	BButton* fForwardButton;
	BButton* fPrevButton;
	BButton* fNextButton;
	BButton* fBookmarksButton;
	BPopUpMenu* fDocMenu;
	BMenuField* fDocField;
	HintField* fRefControl;
	PaneHeader* fHeader;
	PaneHeader* fHeader2;
	ChapterView* fText;
	ChapterView* fText2;
	BView* fPane2;
	VerseMenu* fVerseMenu;
	ChapterView* fTappedView = nullptr;
	std::vector<Place> fBack;
	std::vector<Place> fForward;
	BMenu* fParallelMenu;
	BMenu* fFontMenu;
	BMenu* fSpacingMenu;
	BMenuItem* fVersePerLineItem;
	BMenuItem* fOptionItems[4];
	BFilePanel* fImportPanel = nullptr;
	std::vector<sheath::VerseText> fVerses2;
	SearchWindow* fSearch;
};
