#include "ReaderWindow.h"

#include <Application.h>
#include <Beep.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <TextControl.h>

#include <cstdlib>

#include "ChapterView.h"
#include "EntryWindow.h"
#include "Markup.h"
#include "Ref.h"

using namespace sheath;

namespace {

enum {
	kMsgModule = 'modl',
	kMsgBook = 'book',
	kMsgChapter = 'chap',
	kMsgPrev = 'prev',
	kMsgNext = 'next',
	kMsgGoRef = 'goto',
	kMsgBigger = 'bigr',
	kMsgSmaller = 'smlr',
	kMsgToggle = 'togl',
};

const char* kOptionNames[] = {"Strong's numbers", "Footnotes", "Headings", "Words of Christ in red"};

bool* OptionField(DisplayOptions& o, int i)
{
	switch (i) {
		case 0: return &o.strongs;
		case 1: return &o.footnotes;
		case 2: return &o.headings;
		default: return &o.redLetter;
	}
}

}	// namespace


ReaderWindow::ReaderWindow(BRect frame, SwordLibrary* library, const ReaderState& state)
	:
	BWindow(frame, "Sheath", B_DOCUMENT_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fLibrary(library),
	fState(state)
{
	if (fState.fontSize <= 0)
		fState.fontSize = be_plain_font->Size() * 1.35f;

	BMenuBar* menuBar = new BMenuBar("menu");
	BMenu* app = new BMenu("Sheath");
	app->AddItem(new BMenuItem("About Sheath", new BMessage(B_ABOUT_REQUESTED)));
	app->AddSeparatorItem();
	app->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
	menuBar->AddItem(app);
	fModuleMenu = new BMenu("Bible");
	fModuleMenu->SetRadioMode(true);
	menuBar->AddItem(fModuleMenu);
	fViewMenu = new BMenu("View");
	fViewMenu->AddItem(new BMenuItem("Larger text", new BMessage(kMsgBigger), '+'));
	fViewMenu->AddItem(new BMenuItem("Smaller text", new BMessage(kMsgSmaller), '-'));
	fViewMenu->AddSeparatorItem();
	for (int i = 0; i < 4; i++) {
		BMessage* msg = new BMessage(kMsgToggle);
		msg->AddInt32("option", i);
		fViewMenu->AddItem(new BMenuItem(kOptionNames[i], msg));
	}
	menuBar->AddItem(fViewMenu);

	fBookMenu = new BPopUpMenu("Book");
	fBookField = new BMenuField("book", nullptr, fBookMenu);
	fChapterMenu = new BPopUpMenu("1");
	fChapterField = new BMenuField("chapter", nullptr, fChapterMenu);
	fPrevButton = new BButton("prev", "\xE2\x97\x80", new BMessage(kMsgPrev));	// ◀
	fNextButton = new BButton("next", "\xE2\x96\xB6", new BMessage(kMsgNext));	// ▶
	fRefControl = new BTextControl("ref", nullptr, "", new BMessage(kMsgGoRef));
	fRefControl->SetToolTip("Go to a reference, e.g. John 3:16");
	fText = new ChapterView("text");
	BScrollView* scroll = new BScrollView("scroll", fText, 0, false, true, B_NO_BORDER);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.AddGroup(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
			.SetInsets(B_USE_HALF_ITEM_INSETS)
			.Add(fPrevButton)
			.Add(fNextButton)
			.Add(fBookField)
			.Add(fChapterField)
			.AddGlue()
			.Add(fRefControl, 2.0f)
		.End()
		.Add(scroll);

	BuildModuleMenu();
	UpdateOptionItems();
	ShowChapter(fState.verse);
}


ReaderState ReaderWindow::State() const
{
	ReaderState s = fState;
	s.verse = fText->VerseAtTop();
	return s;
}


bool ReaderWindow::QuitRequested()
{
	BMessage state('stat');
	ReaderState s = State();
	state.AddString("module", s.module.c_str());
	state.AddString("book", s.book.c_str());
	state.AddInt32("chapter", s.chapter);
	state.AddInt32("verse", s.verse);
	state.AddFloat("fontSize", s.fontSize);
	for (int i = 0; i < 4; i++)
		state.AddBool(kOptionNames[i], *OptionField(s.options, i));
	state.AddRect("frame", Frame());
	be_app->PostMessage(&state);
	return true;
}


void ReaderWindow::LibraryChanged()
{
	BuildModuleMenu();
	ShowChapter();
}


void ReaderWindow::BuildModuleMenu()
{
	while (BMenuItem* item = fModuleMenu->RemoveItem((int32)0))
		delete item;
	std::vector<ModuleInfo> bibles;
	for (const ModuleInfo& m : fLibrary->Modules()) {
		if (m.type == "Bible")
			bibles.push_back(m);
	}
	bool current = false;
	for (const ModuleInfo& m : bibles)
		current = current || m.name == fState.module;
	if (!current)
		fState.module = bibles.empty() ? "" : bibles.front().name;

	for (const ModuleInfo& m : bibles) {
		BMessage* msg = new BMessage(kMsgModule);
		msg->AddString("module", m.name.c_str());
		std::string label = m.name + " \xE2\x80\x94 " + m.description;
		BMenuItem* item = new BMenuItem(label.c_str(), msg);
		item->SetMarked(m.name == fState.module);
		fModuleMenu->AddItem(item);
	}
	if (bibles.empty()) {
		BMenuItem* none = new BMenuItem("No Bibles installed", nullptr);
		none->SetEnabled(false);
		fModuleMenu->AddItem(none);
	}
	BuildBookMenu();
}


void ReaderWindow::BuildBookMenu()
{
	while (BMenuItem* item = fBookMenu->RemoveItem((int32)0))
		delete item;
	fBooks = fState.module.empty() ? std::vector<BookInfo>() : fLibrary->Books(fState.module);
	BMenu* testament[2] = {new BMenu("Old Testament"), new BMenu("New Testament")};
	bool known = false;
	for (const BookInfo& b : fBooks) {
		BMessage* msg = new BMessage(kMsgBook);
		msg->AddString("book", b.osis.c_str());
		testament[b.testament == 2 ? 1 : 0]->AddItem(new BMenuItem(b.name.c_str(), msg));
		known = known || b.osis == fState.book;
	}
	for (BMenu* m : testament) {
		if (m->CountItems() > 0)
			fBookMenu->AddItem(m);
		else
			delete m;
	}
	if (!known && !fBooks.empty()) {
		fState.book = fBooks.front().osis;
		fState.chapter = 1;
	}
	BuildChapterMenu();
}


void ReaderWindow::BuildChapterMenu()
{
	while (BMenuItem* item = fChapterMenu->RemoveItem((int32)0))
		delete item;
	int chapters = 0;
	std::string bookName = fState.book;
	for (const BookInfo& b : fBooks) {
		if (b.osis == fState.book) {
			chapters = b.chapters;
			bookName = b.name;
		}
	}
	if (fState.chapter < 1 || fState.chapter > chapters)
		fState.chapter = 1;
	for (int c = 1; c <= chapters; c++) {
		BMessage* msg = new BMessage(kMsgChapter);
		msg->AddInt32("chapter", c);
		fChapterMenu->AddItem(new BMenuItem(std::to_string(c).c_str(), msg));
	}
	fBookField->MenuItem()->SetLabel(fBooks.empty() ? "Book" : bookName.c_str());
	fChapterField->MenuItem()->SetLabel(std::to_string(fState.chapter).c_str());
}


void ReaderWindow::ShowChapter(int scrollToVerse)
{
	if (fState.module.empty()) {
		fVerses.clear();
		fText->SetMessageText("No Bible modules are installed.\n\n"
			"Install SWORD modules (for example KJV) into ~/.sword or the system SWORD "
			"directory, then restart Sheath.", fState.fontSize);
		SetTitle("Sheath");
		return;
	}
	fVerses = fLibrary->Chapter(fState.module, fState.book, fState.chapter, fState.options);
	if (fVerses.empty()) {
		fText->SetMessageText("This chapter is empty in this module.", fState.fontSize);
	} else {
		fText->SetChapter(fVerses, fState.fontSize);
		if (scrollToVerse > 1)
			fText->ScrollToVerse(scrollToVerse);
	}
	std::string title = FormatRef({fState.book, fState.chapter, 0}) + " \xE2\x80\x94 "
		+ fState.module + " \xE2\x80\x94 Sheath";
	SetTitle(title.c_str());
	BuildChapterMenu();
}


void ReaderWindow::GoTo(const std::string& book, int chapter, int verse)
{
	fState.book = book;
	fState.chapter = chapter;
	ShowChapter(verse);
}


void ReaderWindow::Step(int delta)
{
	for (size_t i = 0; i < fBooks.size(); i++) {
		if (fBooks[i].osis != fState.book)
			continue;
		int chapter = fState.chapter + delta;
		if (chapter < 1 && i > 0)
			GoTo(fBooks[i - 1].osis, fBooks[i - 1].chapters, 0);
		else if (chapter > fBooks[i].chapters && i + 1 < fBooks.size())
			GoTo(fBooks[i + 1].osis, 1, 0);
		else if (chapter >= 1 && chapter <= fBooks[i].chapters)
			GoTo(fState.book, chapter, 0);
		return;
	}
}


void ReaderWindow::OpenLink(int32 kind, const std::string& target, int verse)
{
	switch ((LinkKind)kind) {
		case LinkKind::Strongs: {
			const char* module = target[0] == 'H' ? "StrongsHebrew" : "StrongsGreek";
			std::string entry = fLibrary->Entry(module, target, fState.options);
			std::string title = "Strong's " + target;
			if (entry.empty()) {
				entry = std::string("The ") + module + " dictionary isn't installed, or has no "
					"entry " + target + ".";
			}
			EntryWindow::Show(title, entry, fState.fontSize);
			break;
		}
		case LinkKind::Footnote: {
			for (const VerseText& v : fVerses) {
				auto note = v.notes.find(target);
				if (v.verse == verse && note != v.notes.end()) {
					std::string title = "Note on " + FormatRef({fState.book, fState.chapter, verse});
					EntryWindow::Show(title, note->second, fState.fontSize);
				}
			}
			break;
		}
		case LinkKind::CrossRef: {
			std::vector<VerseRef> refs = ParseOsisRefList(target);
			if (!refs.empty())
				GoTo(refs[0].book, refs[0].chapter, refs[0].verse);
			break;
		}
		default:
			break;
	}
}


void ReaderWindow::UpdateOptionItems()
{
	for (int i = 0; i < 4; i++) {
		BMenuItem* item = fViewMenu->ItemAt(3 + i);
		if (item != nullptr)
			item->SetMarked(*OptionField(fState.options, i));
	}
}


void ReaderWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgModule: {
			int verse = fText->VerseAtTop();
			fState.module = message->GetString("module", "");
			BuildBookMenu();
			ShowChapter(verse);
			break;
		}
		case kMsgBook:
			GoTo(message->GetString("book", "Gen"), 1, 0);
			break;
		case kMsgChapter:
			GoTo(fState.book, message->GetInt32("chapter", 1), 0);
			break;
		case kMsgPrev:
			Step(-1);
			break;
		case kMsgNext:
			Step(1);
			break;
		case kMsgGoRef: {
			VerseRef ref;
			ModuleInfo info;
			std::string v11n = fLibrary->Find(fState.module, &info) ? info.versification : "KJV";
			if (ParseRef(fRefControl->Text(), &ref, v11n)) {
				GoTo(ref.book, ref.chapter, ref.verse);
				fRefControl->SetText("");
			} else {
				beep();
			}
			break;
		}
		case kMsgBigger:
		case kMsgSmaller: {
			int verse = fText->VerseAtTop();
			fState.fontSize += message->what == kMsgBigger ? 2 : -2;
			if (fState.fontSize < 8)
				fState.fontSize = 8;
			ShowChapter(verse);
			break;
		}
		case kMsgToggle: {
			int i = message->GetInt32("option", 0);
			bool* field = OptionField(fState.options, i);
			*field = !*field;
			UpdateOptionItems();
			ShowChapter(fText->VerseAtTop());
			break;
		}
		case ChapterView::kMsgLinkClicked:
			OpenLink(message->GetInt32("kind", 0), message->GetString("target", ""),
				message->GetInt32("verse", 0));
			break;
		default:
			BWindow::MessageReceived(message);
	}
}
