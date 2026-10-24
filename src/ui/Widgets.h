#pragma once

#include <GroupView.h>
#include <TextView.h>

#include <string>

class BButton;
class BCheckBox;
class BStringView;

// A one-line text field that shows a grey hint ("Go to… (e.g. John 3:16)") while it is empty
// and unfocused, and sends its message when Enter is pressed. Put it in a BScrollView for
// the border.
class HintField : public BTextView {
public:
	HintField(const char* name, const char* hint, BMessage* message);
	~HintField() override;

	void SetTarget(BHandler* target) { fTarget = target; }

	void Draw(BRect updateRect) override;
	void MakeFocus(bool focus) override;
	void KeyDown(const char* bytes, int32 numBytes) override;
	BSize MinSize() override;
	BSize MaxSize() override;
	BSize PreferredSize() override;

private:
	std::string fHint;
	BMessage* fMessage;
	BHandler* fTarget = nullptr;
};


// The light-blue band above a text pane: its title ("ASV – John 1:1") and, for a closable
// pane, a Sync check box and a close button.
class PaneHeader : public BGroupView {
public:
	PaneHeader(const char* name, BMessage* syncMessage, BMessage* closeMessage);

	void SetTitle(const char* title);
	void SetSync(bool sync);
	bool Sync() const;
	void SetTarget(BHandler* target);

	void AttachedToWindow() override;
	void Draw(BRect updateRect) override;

private:
	BStringView* fTitle;
	BCheckBox* fSync;
	BButton* fClose;
};
