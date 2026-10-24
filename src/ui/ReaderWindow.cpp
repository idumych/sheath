#include "ReaderWindow.h"

#include <Application.h>
#include <Alert.h>
#include <Clipboard.h>
#include <FilePanel.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Beep.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <TextControl.h>

#include <cmath>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <set>

#include "ChapterView.h"
#include "EntryWindow.h"
#include "Markup.h"
#include "Ref.h"
#include "InstallWindow.h"
#include "MyBible.h"
#include "SearchWindow.h"

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
	kMsgSearchWindow = 'srwn',
	kMsgInstallWindow = 'inwn',
	kMsgCopyVerse = 'cpyv',
	kMsgToggleBookmark = 'tgbm',
	kMsgOpenBookmark = 'opbm',
	kMsgParallel = 'prll',
	kMsgFont = 'font',
	kMsgSpacing = 'spcg',
	kMsgVersePerLine = 'vpln',
	kMsgImportPanel = 'impp',
	kMsgImportRef = 'impr',
	kMsgImportDone = 'impd',
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


ReaderWindow::ReaderWindow(BRect frame, SwordLibrary* library, const ReaderState& state,
	const std::string& bookmarksPath)
	:
	BWindow(frame, "Sheath", B_DOCUMENT_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fLibrary(library),
	fState(state),
	fBookmarks(bookmarksPath)
{
	if (fState.style.size <= 0)
		fState.style.size = be_plain_font->Size() * 1.35f;
	if (!fState.style.family.empty() && !ChapterView::HasFamily(fState.style.family.c_str()))
		fState.style.family = "";

	BMenuBar* menuBar = new BMenuBar("menu");
	BMenu* app = new BMenu("Sheath");
	app->AddItem(new BMenuItem("About Sheath", new BMessage(B_ABOUT_REQUESTED)));
	app->AddSeparatorItem();
	app->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
	menuBar->AddItem(app);
	fModuleMenu = new BMenu("Bible");
	fModuleMenu->SetRadioMode(true);
	menuBar->AddItem(fModuleMenu);
	BMenu* go = new BMenu("Go");
	go->AddItem(new BMenuItem("Previous chapter", new BMessage(kMsgPrev), B_LEFT_ARROW));
	go->AddItem(new BMenuItem("Next chapter", new BMessage(kMsgNext), B_RIGHT_ARROW));
	go->AddSeparatorItem();
	go->AddItem(new BMenuItem("Search" "\xE2\x80\xA6", new BMessage(kMsgSearchWindow), 'F'));
	menuBar->AddItem(go);
	fBookmarksMenu = new BMenu("Bookmarks");
	menuBar->AddItem(fBookmarksMenu);
	fViewMenu = new BMenu("View");
	fViewMenu->AddItem(new BMenuItem("Larger text", new BMessage(kMsgBigger), '+'));
	fViewMenu->AddItem(new BMenuItem("Smaller text", new BMessage(kMsgSmaller), '-'));
	fFontMenu = new BMenu("Font");
	fFontMenu->SetRadioMode(true);
	int32 families = count_font_families();
	for (int32 i = 0; i < families; i++) {
		font_family family;
		uint32 flags = 0;
		if (get_font_family(i, &family, &flags) != B_OK || (flags & B_IS_FIXED) != 0)
			continue;
		BMessage* msg = new BMessage(kMsgFont);
		msg->AddString("family", family);
		BMenuItem* item = new BMenuItem(family, msg);
		item->SetMarked(fState.style.family == family);
		fFontMenu->AddItem(item);
	}
	fViewMenu->AddItem(fFontMenu);
	fSpacingMenu = new BMenu("Line spacing");
	fSpacingMenu->SetRadioMode(true);
	for (float spacing : {1.0f, 1.2f, 1.4f, 1.5f, 1.6f, 1.8f, 2.0f}) {
		BMessage* msg = new BMessage(kMsgSpacing);
		msg->AddFloat("spacing", spacing);
		char label[16];
		snprintf(label, sizeof(label), "%.1f", spacing);
		BMenuItem* item = new BMenuItem(label, msg);
		item->SetMarked(std::fabs(spacing - fState.style.lineSpacing) < 0.05f);
		fSpacingMenu->AddItem(item);
	}
	fViewMenu->AddItem(fSpacingMenu);
	fVersePerLineItem = new BMenuItem("One verse per line", new BMessage(kMsgVersePerLine));
	fViewMenu->AddItem(fVersePerLineItem);
	fViewMenu->AddSeparatorItem();
	fParallelMenu = new BMenu("Parallel Bible");
	fParallelMenu->SetRadioMode(true);
	fViewMenu->AddItem(fParallelMenu);
	fViewMenu->AddSeparatorItem();
	for (int i = 0; i < 4; i++) {
		BMessage* msg = new BMessage(kMsgToggle);
		msg->AddInt32("option", i);
		fOptionItems[i] = new BMenuItem(kOptionNames[i], msg);
		fViewMenu->AddItem(fOptionItems[i]);
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
	fText2 = new ChapterView("parallel");
	fScroll2 = new BScrollView("scroll2", fText2, 0, false, true, B_NO_BORDER);
	BSplitView* split = new BSplitView(B_HORIZONTAL, 0);
	BLayoutBuilder::Split<>(split).Add(scroll).Add(fScroll2);

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
		.Add(split);
	fScroll2->Hide();

	fSearch = new SearchWindow(BMessenger(this));
	fSearch->Hide();
	fSearch->Show();	// run its looper hidden, so Start() can be called any time

	BuildModuleMenu();
	BuildBookmarksMenu();
	UpdateOptionItems();
	ShowChapter(fState.verse);
	if (fState.verse > 1)
		fText->RestoreVerse(fState.verse);
}


void ReaderWindow::BuildBookmarksMenu()
{
	while (BMenuItem* item = fBookmarksMenu->RemoveItem((int32)0))
		delete item;
	if (fBookmarks.All().empty()) {
		BMenuItem* none = new BMenuItem("Right-click a verse to bookmark it", nullptr);
		none->SetEnabled(false);
		fBookmarksMenu->AddItem(none);
		return;
	}
	// Newest first.
	const std::vector<Bookmark>& all = fBookmarks.All();
	for (auto it = all.rbegin(); it != all.rend(); ++it) {
		std::string label = FormatRef({it->book, it->chapter, it->verse});
		if (!it->note.empty())
			label += " \xE2\x80\x94 " + it->note.substr(0, 60);
		BMessage* msg = new BMessage(kMsgOpenBookmark);
		msg->AddString("book", it->book.c_str());
		msg->AddInt32("chapter", it->chapter);
		msg->AddInt32("verse", it->verse);
		fBookmarksMenu->AddItem(new BMenuItem(label.c_str(), msg));
	}
}


void ReaderWindow::ShowVerseMenu(int verse, BPoint where)
{
	std::string ref = FormatRef({fState.book, fState.chapter, verse});
	BPopUpMenu* menu = new BPopUpMenu("verse", false, false);
	BMessage* copy = new BMessage(kMsgCopyVerse);
	copy->AddInt32("verse", verse);
	menu->AddItem(new BMenuItem(("Copy " + ref).c_str(), copy));
	BMessage* mark = new BMessage(kMsgToggleBookmark);
	mark->AddInt32("verse", verse);
	bool marked = fBookmarks.Find(fState.book, fState.chapter, verse) != nullptr;
	menu->AddItem(new BMenuItem(((marked ? "Remove bookmark " : "Bookmark ") + ref).c_str(), mark));
	menu->SetTargetForItems(this);
	menu->SetAsyncAutoDestruct(true);
	menu->Go(where, true, false, true);
}


void ReaderWindow::CopyVerse(int verse)
{
	std::string text;
	for (const VerseText& v : fVerses) {
		if (v.verse != verse)
			continue;
		for (const Block& b : ParseXhtml(v.xhtml)) {
			if (b.kind == Block::Heading)
				continue;
			for (const sheath::Run& r : b.runs) {
				// Strong's numbers and note markers aren't part of the text.
				if (r.link.kind == LinkKind::None || r.link.kind == LinkKind::CrossRef
						|| r.link.kind == LinkKind::Url)
					text += r.text;
			}
			text += ' ';
		}
	}
	size_t first = text.find_first_not_of(' ');
	size_t last = text.find_last_not_of(' ');
	text = first == std::string::npos ? "" : text.substr(first, last - first + 1);
	text += " (" + FormatRef({fState.book, fState.chapter, verse}) + " " + fState.module + ")";
	if (be_clipboard->Lock()) {
		be_clipboard->Clear();
		be_clipboard->Data()->AddData("text/plain", B_MIME_TYPE, text.c_str(), text.size());
		be_clipboard->Commit();
		be_clipboard->Unlock();
	}
}


void ReaderWindow::ImportMyBible(const entry_ref& ref)
{
	BPath path(&ref);
	const char* home = getenv("HOME");
	std::string userDir = std::string(home ? home : "/boot/home") + "/.sword";
	std::string file = path.Path() ? path.Path() : "";
	BMessenger self(this);
	SetTitle("Importing" "\xE2\x80\xA6");
	// A whole Bible takes a few seconds; don't block the window.
	std::thread([self, file, userDir]() {
		ImportResult result;
		std::string error;
		bool ok = sheath::ImportMyBible(file, userDir, "", &result, &error);
		BMessage done(kMsgImportDone);
		done.AddBool("ok", ok);
		done.AddString("module", result.module.c_str());
		done.AddInt32("verses", result.verses);
		done.AddInt32("skipped", result.skipped);
		done.AddString("error", error.c_str());
		self.SendMessage(&done);
	}).detach();
}


void ReaderWindow::ToggleBookmark(int verse)
{
	if (!fBookmarks.Remove(fState.book, fState.chapter, verse))
		fBookmarks.Set({fState.book, fState.chapter, verse, "", (int64_t)time(nullptr)});
	std::string error;
	if (!fBookmarks.Save(&error))
		fprintf(stderr, "Sheath: %s\n", error.c_str());
	BuildBookmarksMenu();
	int top = fText->VerseAtTop();
	ShowChapter(top);
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
	state.AddFloat("fontSize", s.style.size);
	state.AddString("fontFamily", s.style.family.c_str());
	state.AddFloat("lineSpacing", s.style.lineSpacing);
	state.AddBool("versePerLine", s.style.versePerLine);
	state.AddString("parallel", s.parallel.c_str());
	for (int i = 0; i < 4; i++)
		state.AddBool(kOptionNames[i], *OptionField(s.options, i));
	state.AddRect("frame", Frame());
	// Written here, not posted to the app: when the app is quitting it may not handle
	// another message.
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path, true) == B_OK) {
		path.Append("Sheath_settings");
		BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (file.InitCheck() == B_OK)
			state.Flatten(&file);
	}
	if (fSearch->Lock())
		fSearch->Quit();
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
	while (BMenuItem* item = fParallelMenu->RemoveItem((int32)0))
		delete item;
	bool parallelFound = false;
	BMessage* none = new BMessage(kMsgParallel);
	none->AddString("module", "");
	fParallelMenu->AddItem(new BMenuItem("None", none));
	for (const ModuleInfo& m : bibles) {
		BMessage* msg = new BMessage(kMsgParallel);
		msg->AddString("module", m.name.c_str());
		BMenuItem* item = new BMenuItem(m.name.c_str(), msg);
		item->SetMarked(m.name == fState.parallel);
		parallelFound = parallelFound || m.name == fState.parallel;
		fParallelMenu->AddItem(item);
	}
	if (!parallelFound) {
		fState.parallel = "";
		fParallelMenu->ItemAt(0)->SetMarked(true);
	}

	fModuleMenu->AddSeparatorItem();
	BMenuItem* install = new BMenuItem("Install modules" "\xE2\x80\xA6",
		new BMessage(kMsgInstallWindow));
	fModuleMenu->AddItem(install);
	fModuleMenu->AddItem(new BMenuItem("Import MyBible module" "\xE2\x80\xA6",
		new BMessage(kMsgImportPanel)));
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
		fText->SetMessageText("No Bible is installed yet.\n\n"
			"Choose Bible \xE2\x96\xB8 Install modules\xE2\x80\xA6 to download one, "
			"for example the King James Version (KJV).", fState.style);
		SetTitle("Sheath");
		return;
	}
	fVerses = fLibrary->Chapter(fState.module, fState.book, fState.chapter, fState.options);
	if (fVerses.empty()) {
		fText->SetMessageText("This chapter is empty in this module.", fState.style);
	} else {
		std::set<int> marked;
		for (const Bookmark& b : fBookmarks.All()) {
			if (b.book == fState.book && b.chapter == fState.chapter)
				marked.insert(b.verse);
		}
		fText->SetChapter(fVerses, fState.style, marked);
		if (scrollToVerse > 1)
			fText->ScrollToVerse(scrollToVerse);
	}

	bool parallel = !fState.parallel.empty();
	fText->SetScrollNotify(parallel);
	fText2->SetScrollNotify(parallel);
	if (parallel) {
		fVerses2 = fLibrary->Chapter(fState.parallel, fState.book, fState.chapter, fState.options);
		if (fVerses2.empty())
			fText2->SetMessageText("This chapter is empty in this module.", fState.style);
		else
			fText2->SetChapter(fVerses2, fState.style);
		fText2->SyncToVerse(fText->VerseAtTop());
		if (fScroll2->IsHidden())
			fScroll2->Show();
	} else {
		fVerses2.clear();
		if (!fScroll2->IsHidden())
			fScroll2->Hide();
	}

	std::string title = FormatRef({fState.book, fState.chapter, 0}) + " \xE2\x80\x94 "
		+ fState.module + (parallel ? " | " + fState.parallel : "") + " \xE2\x80\x94 Sheath";
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


void ReaderWindow::OpenLink(int32 kind, const std::string& target, int verse, bool inParallel)
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
			EntryWindow::Show(title, entry, fState.style, target, BMessenger(this));
			break;
		}
		case LinkKind::Footnote: {
			for (const VerseText& v : inParallel ? fVerses2 : fVerses) {
				auto note = v.notes.find(target);
				if (v.verse == verse && note != v.notes.end()) {
					std::string title = "Note on " + FormatRef({fState.book, fState.chapter, verse});
					EntryWindow::Show(title, note->second, fState.style);
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
	for (int i = 0; i < 4; i++)
		fOptionItems[i]->SetMarked(*OptionField(fState.options, i));
	fVersePerLineItem->SetMarked(fState.style.versePerLine);
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
			fState.style.size += message->what == kMsgBigger ? 2 : -2;
			if (fState.style.size < 8)
				fState.style.size = 8;
			ShowChapter(verse);
			break;
		}
		case kMsgFont:
			fState.style.family = message->GetString("family", "");
			ShowChapter(fText->VerseAtTop());
			break;
		case kMsgSpacing:
			fState.style.lineSpacing = message->GetFloat("spacing", 1.5f);
			ShowChapter(fText->VerseAtTop());
			break;
		case kMsgVersePerLine:
			fState.style.versePerLine = !fState.style.versePerLine;
			UpdateOptionItems();
			ShowChapter(fText->VerseAtTop());
			break;
		case kMsgToggle: {
			int i = message->GetInt32("option", 0);
			bool* field = OptionField(fState.options, i);
			*field = !*field;
			UpdateOptionItems();
			ShowChapter(fText->VerseAtTop());
			break;
		}
		case kMsgInstallWindow: {
			const char* home = getenv("HOME");
			std::string userDir = std::string(home ? home : "/boot/home") + "/.sword";
			(new InstallWindow(userDir, BMessenger(this)))->Show();
			break;
		}
		case InstallWindow::kMsgLibraryChanged:
			fLibrary->Reload();
			LibraryChanged();
			break;
		case kMsgSearchWindow:
			if (fSearch->Lock()) {
				fSearch->Start(fState.module, fState.book);
				fSearch->Unlock();
			}
			break;
		case EntryWindow::kMsgFindStrongs:
			if (fSearch->Lock()) {
				fSearch->Start(fState.module, fState.book, message->GetString("strongs", ""),
					SearchMode::Strongs);
				fSearch->Unlock();
			}
			break;
		case SearchWindow::kMsgShowVerse:
			GoTo(message->GetString("book", "Gen"), message->GetInt32("chapter", 1),
				message->GetInt32("verse", 0));
			Activate();
			break;
		case kMsgImportPanel:
			if (fImportPanel == nullptr) {
				fImportPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this), nullptr,
					B_FILE_NODE, false, new BMessage(kMsgImportRef));
				fImportPanel->Window()->SetTitle("Import a MyBible module (.SQLite3)");
			}
			fImportPanel->Show();
			break;
		case kMsgImportRef: {
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK)
				ImportMyBible(ref);
			break;
		}
		case kMsgImportDone: {
			std::string module = message->GetString("module", "");
			std::string text;
			if (message->GetBool("ok", false)) {
				text = "Imported " + std::to_string(message->GetInt32("verses", 0)) + " verses as "
					+ module + ".";
				int32 skipped = message->GetInt32("skipped", 0);
				if (skipped > 0) {
					text += "\n\n" + std::to_string(skipped) + " verses were left out (books or "
						"verses outside the KJV canon, such as the deuterocanonical books).";
				}
				fLibrary->Reload();
				fState.module = module;
				LibraryChanged();
			} else {
				text = "Couldn't import the module.\n\n" + std::string(message->GetString("error", ""));
				ShowChapter(fText->VerseAtTop());
			}
			(new BAlert("Import", text.c_str(), "OK"))->Go(nullptr);
			break;
		}
		case kMsgParallel:
			fState.parallel = message->GetString("module", "");
			ShowChapter(fText->VerseAtTop());
			break;
		case ChapterView::kMsgScrolled: {
			ChapterView* source = nullptr;
			message->FindPointer("view", (void**)&source);
			ChapterView* other = source == fText ? fText2 : fText;
			if (!fState.parallel.empty())
				other->SyncToVerse(message->GetInt32("verse", 1));
			break;
		}
		case ChapterView::kMsgVerseMenu:
			ShowVerseMenu(message->GetInt32("verse", 0), message->GetPoint("where", BPoint()));
			break;
		case kMsgCopyVerse:
			CopyVerse(message->GetInt32("verse", 0));
			break;
		case kMsgToggleBookmark:
			ToggleBookmark(message->GetInt32("verse", 0));
			break;
		case kMsgOpenBookmark:
			GoTo(message->GetString("book", "Gen"), message->GetInt32("chapter", 1),
				message->GetInt32("verse", 0));
			break;
		case ChapterView::kMsgLinkClicked:
		{
			void* view = nullptr;
			message->FindPointer("view", &view);
			OpenLink(message->GetInt32("kind", 0), message->GetString("target", ""),
				message->GetInt32("verse", 0), view == fText2);
		}
			break;
		default:
			BWindow::MessageReceived(message);
	}
}
