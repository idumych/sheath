#include "EntryWindow.h"

#include <Autolock.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>

#include "ChapterView.h"

EntryWindow* EntryWindow::sWindow = nullptr;


EntryWindow::EntryWindow()
	:
	BWindow(BRect(0, 0, 420, 320), "Entry", B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE)
{
	fText = new ChapterView("entry");
	BScrollView* scroll = new BScrollView("scroll", fText, 0, false, true, B_NO_BORDER);
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0).Add(scroll);
	CenterOnScreen();
}


void EntryWindow::Show(const std::string& title, const std::string& xhtml, float fontSize)
{
	if (sWindow == nullptr)
		sWindow = new EntryWindow();
	BAutolock lock(sWindow);
	sWindow->SetTitle(title.c_str());
	sWindow->fText->SetEntry(xhtml, fontSize);
	if (sWindow->IsHidden())
		sWindow->BWindow::Show();
	else
		sWindow->Activate();
}


EntryWindow::~EntryWindow()
{
	sWindow = nullptr;
}
