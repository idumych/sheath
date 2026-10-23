#pragma once

#include <Messenger.h>
#include <Window.h>

#include <string>

class BButton;
class ChapterView;

// A small floating window showing a dictionary entry or a footnote. One is reused for all
// lookups so clicking through Strong's numbers doesn't pile up windows.
class EntryWindow : public BWindow {
public:
	static const uint32 kMsgFindStrongs = 'fnds';

	// strongs: a Strong's number to offer "Find all verses" for ("" = no button). The button
	// sends kMsgFindStrongs with "strongs" to `target`.
	static void Show(const std::string& title, const std::string& xhtml, float fontSize,
		const std::string& strongs = "", BMessenger target = BMessenger());

	void MessageReceived(BMessage* message) override;

	~EntryWindow() override;

private:
	EntryWindow();

	static EntryWindow* sWindow;
	ChapterView* fText;
	BButton* fFind;
	std::string fStrongs;
	BMessenger fTarget;
};
