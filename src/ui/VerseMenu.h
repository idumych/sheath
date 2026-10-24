#pragma once

#include <Control.h>
#include <View.h>

class BButton;
class BStringView;

// The card that appears at the bottom of the reader when a verse is tapped (as in LinBible
// and AndBible): the reference, a row of actions and "Definition & Morphology". Each action
// sends its message, with "verse", to the window.
class VerseMenu : public BView {
public:
	static const uint32 kMsgBookmark = 'vmbk';
	static const uint32 kMsgNote = 'vmnt';
	static const uint32 kMsgCopy = 'vmcp';
	static const uint32 kMsgCompare = 'vmcm';
	static const uint32 kMsgDefinitions = 'vmdf';
	static const uint32 kMsgClose = 'vmcl';

	VerseMenu();

	// Shows the card for a verse; bookmarked changes the first action's label.
	void ShowFor(int verse, const char* reference, bool bookmarked);
	int Verse() const { return fVerse; }

	void Draw(BRect updateRect) override;
	void AttachedToWindow() override;

private:
	void SetVerseOnMessages();

	int fVerse = 0;
	BStringView* fTitle;
	class IconButton* fBookmark;
	BControl* fControls[6];
	BButton* fDefinitions;
};


// A borderless button with a small vector icon over its label.
class IconButton : public BControl {
public:
	enum Icon { kBookmark, kNote, kCopy, kCompare, kClose };

	IconButton(const char* name, const char* label, Icon icon, BMessage* message);

	void SetFilled(bool filled);		// a filled bookmark for "already bookmarked"

	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	void MouseUp(BPoint where) override;
	void MouseMoved(BPoint where, uint32 transit, const BMessage* drag) override;
	BSize MinSize() override;
	BSize PreferredSize() override;
	BSize MaxSize() override;

private:
	void DrawIcon(BRect box);

	Icon fIcon;
	bool fFilled = false;
	bool fPressed = false;
	bool fHover = false;
};
