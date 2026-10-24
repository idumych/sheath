#include "Widgets.h"

#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <Looper.h>
#include <Message.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <Window.h>

#include <algorithm>

BLayoutItem* HorizontalGlue()
{
	return new BSpaceLayoutItem(BSize(0, 0), BSize(B_SIZE_UNLIMITED, 0), BSize(0, 0),
		BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_USE_FULL_HEIGHT));
}

// #pragma mark - HintField


HintField::HintField(const char* name, const char* hint, BMessage* message)
	:
	BTextView(name),
	fHint(hint),
	fMessage(message)
{
	SetWordWrap(false);
	SetStylable(false);
	DisallowChar('\n');
	SetInsets(5, 3, 5, 3);
}


HintField::~HintField()
{
	delete fMessage;
}


BSize HintField::MinSize()
{
	font_height fh;
	GetFontHeight(&fh);
	return BSize(StringWidth("Go to\xE2\x80\xA6 (e.g. John 3:16)") * 0.8f,
		ceilf(fh.ascent + fh.descent + fh.leading) + 6);
}


BSize HintField::PreferredSize()
{
	BSize min = MinSize();
	return BSize(StringWidth(fHint.c_str()) + 16, min.height);
}


BSize HintField::MaxSize()
{
	return BSize(PreferredSize().width * 1.4f, MinSize().height);
}


void HintField::Draw(BRect updateRect)
{
	BTextView::Draw(updateRect);
	if (TextLength() > 0 || IsFocus())
		return;
	rgb_color text = ui_color(B_DOCUMENT_TEXT_COLOR);
	rgb_color back = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	SetHighColor((text.red + back.red) / 2, (text.green + back.green) / 2,
		(text.blue + back.blue) / 2);
	font_height fh;
	GetFontHeight(&fh);
	DrawString(fHint.c_str(), BPoint(6, 3 + fh.ascent));
}


void HintField::MakeFocus(bool focus)
{
	BTextView::MakeFocus(focus);
	Invalidate();
}


void HintField::KeyDown(const char* bytes, int32 numBytes)
{
	if (numBytes == 1 && (bytes[0] == B_ENTER || bytes[0] == B_RETURN)) {
		if (fMessage != nullptr && Window() != nullptr) {
			BMessage msg(*fMessage);
			msg.AddString("text", Text());
			Window()->PostMessage(&msg, fTarget != nullptr ? fTarget : Window());
		}
		return;
	}
	if (numBytes == 1 && bytes[0] == B_ESCAPE) {
		SetText("");
		return;
	}
	BTextView::KeyDown(bytes, numBytes);
}


// #pragma mark - PaneHeader


PaneHeader::PaneHeader(const char* name, BMessage* syncMessage, BMessage* closeMessage)
	:
	BGroupView(name, B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
{
	SetFlags(Flags() | B_WILL_DRAW);
	fTitle = new BStringView("title", "");
	BFont bold(be_bold_font);
	bold.SetSize(be_plain_font->Size());
	fTitle->SetFont(&bold);
	fSync = syncMessage != nullptr ? new BCheckBox("sync", "Sync", syncMessage) : nullptr;
	fClose = closeMessage != nullptr ? new BButton("close", "\xE2\x9C\x95", closeMessage) : nullptr;
	if (fClose != nullptr) {
		fClose->SetFlat(true);
		fClose->SetExplicitMaxSize(BSize(fClose->MinSize().height, B_SIZE_UNSET));
	}

	BLayoutBuilder::Group<>(this)
		.SetInsets(B_USE_HALF_ITEM_INSETS, 2, B_USE_HALF_ITEM_INSETS, 2)
		.Add(fTitle)
		.AddGlue();
	if (fSync != nullptr)
		GroupLayout()->AddView(fSync);
	if (fClose != nullptr)
		GroupLayout()->AddView(fClose);
}


void PaneHeader::AttachedToWindow()
{
	// A light tint of the selection colour, like LinBible's pane title band.
	rgb_color select = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
	rgb_color back = ui_color(B_PANEL_BACKGROUND_COLOR);
	rgb_color band = make_color((select.red + 3 * back.red) / 4, (select.green + 3 * back.green) / 4,
		(select.blue + 3 * back.blue) / 4);
	SetViewColor(band);
	fTitle->SetViewColor(band);
	fTitle->SetLowColor(band);
	if (fSync != nullptr)
		fSync->SetViewColor(band);
	BGroupView::AttachedToWindow();
}


void PaneHeader::Draw(BRect updateRect)
{
	BGroupView::Draw(updateRect);
	SetHighColor(tint_color(ViewColor(), B_DARKEN_2_TINT));
	StrokeLine(Bounds().LeftBottom(), Bounds().RightBottom());
}


void PaneHeader::SetTitle(const char* title)
{
	fTitle->SetText(title);
}


void PaneHeader::SetSync(bool sync)
{
	if (fSync != nullptr)
		fSync->SetValue(sync ? B_CONTROL_ON : B_CONTROL_OFF);
}


bool PaneHeader::Sync() const
{
	return fSync != nullptr && fSync->Value() == B_CONTROL_ON;
}


void PaneHeader::SetTarget(BHandler* target)
{
	if (fSync != nullptr)
		fSync->SetTarget(target);
	if (fClose != nullptr)
		fClose->SetTarget(target);
}
