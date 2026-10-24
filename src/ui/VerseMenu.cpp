#include "VerseMenu.h"

#include <Button.h>
#include <ControlLook.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Shape.h>
#include <StringView.h>
#include <Window.h>

#include <algorithm>
#include <cstdio>

namespace {

rgb_color CardColor()
{
	rgb_color back = ui_color(B_PANEL_BACKGROUND_COLOR);
	return tint_color(back, B_DARKEN_1_TINT);
}

}	// namespace


// #pragma mark - IconButton


IconButton::IconButton(const char* name, const char* label, Icon icon, BMessage* message)
	:
	BControl(name, label, message, B_WILL_DRAW),
	fIcon(icon)
{
}


void IconButton::SetFilled(bool filled)
{
	if (fFilled == filled)
		return;
	fFilled = filled;
	Invalidate();
}


BSize IconButton::MinSize()
{
	font_height fh;
	GetFontHeight(&fh);
	float labelWidth = Label() != nullptr ? StringWidth(Label()) : 0;
	float icon = 22;
	float width = std::max(icon, labelWidth) + 16;
	float height = icon + (Label() != nullptr && Label()[0] ? fh.ascent + fh.descent + 6 : 0) + 8;
	return BSize(width, height);
}


BSize IconButton::PreferredSize() { return MinSize(); }
BSize IconButton::MaxSize() { return BSize(MinSize().width * 2, MinSize().height); }


void IconButton::DrawIcon(BRect box)
{
	// All icons in a 22 x 22 box, drawn with lines and polygons in the text colour.
	SetPenSize(1.6f);
	float l = box.left, t = box.top;
	switch (fIcon) {
		case kBookmark: {
			BPoint ribbon[] = {BPoint(l + 6, t + 2), BPoint(l + 16, t + 2), BPoint(l + 16, t + 20),
				BPoint(l + 11, t + 15), BPoint(l + 6, t + 20)};
			if (fFilled)
				FillPolygon(ribbon, 5);
			else
				StrokePolygon(ribbon, 5);
			break;
		}
		case kNote: {
			// A sheet with a pencil across it.
			StrokeRect(BRect(l + 3, t + 3, l + 15, t + 19));
			StrokeLine(BPoint(l + 6, t + 8), BPoint(l + 12, t + 8));
			StrokeLine(BPoint(l + 6, t + 12), BPoint(l + 10, t + 12));
			BPoint pencil[] = {BPoint(l + 19, t + 5), BPoint(l + 21, t + 7), BPoint(l + 13, t + 17),
				BPoint(l + 10, t + 18), BPoint(l + 11, t + 15)};
			FillPolygon(pencil, 5);
			break;
		}
		case kCopy:
			StrokeRect(BRect(l + 3, t + 6, l + 14, t + 20));
			StrokeLine(BPoint(l + 7, t + 6), BPoint(l + 7, t + 2));
			StrokeLine(BPoint(l + 7, t + 2), BPoint(l + 18, t + 2));
			StrokeLine(BPoint(l + 18, t + 2), BPoint(l + 18, t + 16));
			StrokeLine(BPoint(l + 18, t + 16), BPoint(l + 14, t + 16));
			break;
		case kCompare: {
			// Two arrows, right then left.
			StrokeLine(BPoint(l + 2, t + 7), BPoint(l + 18, t + 7));
			BPoint right[] = {BPoint(l + 20, t + 7), BPoint(l + 15, t + 3), BPoint(l + 15, t + 11)};
			FillPolygon(right, 3);
			StrokeLine(BPoint(l + 4, t + 15), BPoint(l + 20, t + 15));
			BPoint left[] = {BPoint(l + 2, t + 15), BPoint(l + 7, t + 11), BPoint(l + 7, t + 19)};
			FillPolygon(left, 3);
			break;
		}
		case kClose:
			StrokeLine(BPoint(l + 5, t + 5), BPoint(l + 17, t + 17));
			StrokeLine(BPoint(l + 17, t + 5), BPoint(l + 5, t + 17));
			break;
	}
	SetPenSize(1.0f);
}


void IconButton::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	rgb_color back = Parent() != nullptr ? Parent()->ViewColor() : ui_color(B_PANEL_BACKGROUND_COLOR);
	SetHighColor(back);
	FillRect(bounds);
	if (fPressed || fHover) {
		SetHighColor(tint_color(back, fPressed ? B_DARKEN_2_TINT : B_DARKEN_1_TINT));
		FillRoundRect(bounds.InsetByCopy(1, 1), 5, 5);
	}
	rgb_color text = ui_color(B_PANEL_TEXT_COLOR);
	if (!IsEnabled())
		text = tint_color(text, B_LIGHTEN_1_TINT);
	SetHighColor(text);
	SetLowColor(back);

	BRect icon(0, 0, 21, 21);
	icon.OffsetTo(floorf((bounds.Width() - 22) / 2), 4);
	DrawIcon(icon);

	if (Label() != nullptr && Label()[0]) {
		font_height fh;
		GetFontHeight(&fh);
		float x = floorf((bounds.Width() - StringWidth(Label())) / 2);
		DrawString(Label(), BPoint(x, icon.bottom + 6 + fh.ascent));
	}
}


void IconButton::MouseDown(BPoint where)
{
	if (!IsEnabled())
		return;
	fPressed = true;
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	Invalidate();
}


void IconButton::MouseUp(BPoint where)
{
	bool clicked = fPressed && Bounds().Contains(where);
	fPressed = false;
	Invalidate();
	if (clicked)
		Invoke();
}


void IconButton::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	bool hover = transit == B_ENTERED_VIEW || transit == B_INSIDE_VIEW;
	if (hover != fHover) {
		fHover = hover;
		Invalidate();
	}
}


// #pragma mark - VerseMenu


VerseMenu::VerseMenu()
	:
	BView("verse menu", B_WILL_DRAW | B_FRAME_EVENTS)
{
	fTitle = new BStringView("title", "");
	BFont bold(be_bold_font);
	fTitle->SetFont(&bold);
	IconButton* close = new IconButton("close", "", IconButton::kClose,
		new BMessage(kMsgClose));
	fBookmark = new IconButton("bookmark", "Bookmark", IconButton::kBookmark,
		new BMessage(kMsgBookmark));
	IconButton* note = new IconButton("note", "Note", IconButton::kNote, new BMessage(kMsgNote));
	IconButton* copy = new IconButton("copy", "Copy", IconButton::kCopy, new BMessage(kMsgCopy));
	IconButton* compare = new IconButton("compare", "Compare", IconButton::kCompare,
		new BMessage(kMsgCompare));
	fDefinitions = new BButton("definitions", "Definition & Morphology",
		new BMessage(kMsgDefinitions));
	fControls[0] = close;
	fControls[1] = fBookmark;
	fControls[2] = note;
	fControls[3] = copy;
	fControls[4] = compare;
	fControls[5] = fDefinitions;

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_HALF_ITEM_SPACING)
		.SetInsets(B_USE_ITEM_INSETS, B_USE_HALF_ITEM_INSETS, B_USE_ITEM_INSETS,
			B_USE_ITEM_INSETS)
		.AddGroup(B_HORIZONTAL)
			.Add(fTitle)
			.AddGlue()
			.Add(close)
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_ITEM_SPACING)
			.AddGlue()
			.Add(fBookmark)
			.Add(note)
			.Add(copy)
			.Add(compare)
			.AddGlue()
		.End()
		.Add(fDefinitions);
}


void VerseMenu::AttachedToWindow()
{
	SetViewColor(CardColor());
	BView::AttachedToWindow();
	for (BControl* control : fControls)
		control->SetTarget(Window());
}


void VerseMenu::Draw(BRect updateRect)
{
	// A card with a rounded top edge over the panel colour, as in LinBible.
	SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	FillRect(Bounds());
	SetHighColor(CardColor());
	BRect card = Bounds();
	card.InsetBy(4, 0);
	card.top += 4;
	card.bottom += 8;
	FillRoundRect(card, 8, 8);
	SetHighColor(tint_color(CardColor(), B_DARKEN_2_TINT));
	StrokeRoundRect(card, 8, 8);
}


void VerseMenu::ShowFor(int verse, const char* reference, bool bookmarked)
{
	fVerse = verse;
	fTitle->SetText(reference);
	fBookmark->SetLabel(bookmarked ? "Bookmarked" : "Bookmark");
	fBookmark->SetFilled(bookmarked);
	SetVerseOnMessages();
	if (IsHidden())
		Show();
	InvalidateLayout();
}


void VerseMenu::SetVerseOnMessages()
{
	for (BControl* control : fControls) {
		BMessage* msg = control->Message();
		if (msg == nullptr)
			continue;
		msg->RemoveName("verse");
		msg->AddInt32("verse", fVerse);
	}
}
