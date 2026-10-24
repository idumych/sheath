#include "EntryWindow.h"

#include <Autolock.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>

#include "ChapterView.h"
#include "Widgets.h"

#include <algorithm>

EntryWindow* EntryWindow::sWindow = nullptr;


EntryWindow::EntryWindow()
	:
	BWindow(BRect(0, 0, 420, 320), "Entry", B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE)
{
	fText = new ChapterView("entry");
	BScrollView* scroll = new BScrollView("scroll", fText, 0, false, true, B_NO_BORDER);
	fFind = new BButton("find", "Find all verses", new BMessage(kMsgFindStrongs));
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(scroll)
		.AddGroup(B_HORIZONTAL)
			.SetInsets(B_USE_HALF_ITEM_INSETS)
			.Add(HorizontalGlue())	// the row takes no height while Find is hidden
			.Add(fFind)
		.End();
	CenterOnScreen();
}


void EntryWindow::Show(const std::string& title, const std::string& xhtml, const ReadingStyle& style,
	const std::string& strongs, BMessenger target)
{
	if (sWindow == nullptr)
		sWindow = new EntryWindow();
	BAutolock lock(sWindow);
	sWindow->SetTitle(title.c_str());
	// Entries read better without the extra line spacing and verse layout.
	ReadingStyle entryStyle = style;
	entryStyle.lineSpacing = std::min(style.lineSpacing, 1.3f);
	sWindow->fText->SetEntry(xhtml, entryStyle, strongs.empty() ? 0 : strongs[0]);
	sWindow->fStrongs = strongs;
	sWindow->fTarget = target;
	// IsHidden(view) asks about the button itself: plain IsHidden() is also true while the
	// window is hidden, before its first Show(), so the button would never be hidden.
	bool hidden = sWindow->fFind->IsHidden(sWindow->fFind);
	if (strongs.empty() && !hidden)
		sWindow->fFind->Hide();
	else if (!strongs.empty() && hidden)
		sWindow->fFind->Show();
	if (sWindow->IsHidden())
		sWindow->BWindow::Show();
	else
		sWindow->Activate();
}


void EntryWindow::MessageReceived(BMessage* message)
{
	if (message->what == ChapterView::kMsgLinkClicked) {
		// A Strong's number inside the entry: the reader opens it (in this window).
		fTarget.SendMessage(message);
		return;
	}
	if (message->what == kMsgFindStrongs) {
		BMessage find(kMsgFindStrongs);
		find.AddString("strongs", fStrongs.c_str());
		fTarget.SendMessage(&find);
		return;
	}
	BWindow::MessageReceived(message);
}


EntryWindow::~EntryWindow()
{
	sWindow = nullptr;
}
