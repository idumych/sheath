#pragma once

#include <Messenger.h>
#include <Window.h>

#include <string>

#include "Sword.h"

class BListView;
class BMenuField;
class BStringView;
class BTextControl;

// Search in the current Bible. Double-clicking a result (or Enter) sends kMsgShowVerse with
// "book", "chapter", "verse" to the reader window.
class SearchWindow : public BWindow {
public:
	static const uint32 kMsgShowVerse = 'shvr';

	explicit SearchWindow(BMessenger reader);

	// Shows the window searching `module`; with a query, runs it right away.
	void Start(const std::string& module, const std::string& currentBook,
		const std::string& query = "", sheath::SearchMode mode = sheath::SearchMode::AllWords);

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void RunSearch();

	// Its own library: a long search then doesn't hold the reader's library lock.
	sheath::SwordLibrary fLibrary;
	BMessenger fReader;
	std::string fModule;
	std::string fBook;

	BTextControl* fQuery;
	BMenuField* fModeField;
	BMenuField* fScopeField;
	BListView* fResults;
	BStringView* fStatus;
};
