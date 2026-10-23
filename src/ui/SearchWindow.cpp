#include "SearchWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>

#include "Ref.h"

#include <cctype>

using namespace sheath;

namespace {

enum {
	kMsgSearch = 'srch',
	kMsgInvoke = 'invk',
};

const int kMaxHits = 2000;

const char* kModes[] = {"All words", "Exact phrase", "Regular expression", "Strong's number"};
const SearchMode kModeValues[] = {SearchMode::AllWords, SearchMode::Phrase, SearchMode::Regex,
	SearchMode::Strongs};

const char* kScopes[] = {"Whole Bible", "Old Testament", "New Testament", "Current book"};

// One result row: "John 3:16  For God so loved..." with the location kept for Invoke.
class HitItem : public BStringItem {
public:
	HitItem(const SearchHit& hit)
		:
		BStringItem(""),
		fHit(hit)
	{
		std::string text = hit.text;
		for (char& c : text) {
			if (c == '\n' || c == '\t')
				c = ' ';
		}
		size_t first = text.find_first_not_of(' ');
		text = first == std::string::npos ? "" : text.substr(first);
		if (text.size() > 160)
			text = text.substr(0, 157) + "...";
		SetText((FormatRef({hit.book, hit.chapter, hit.verse}) + "   " + text).c_str());
	}

	const SearchHit& Hit() const { return fHit; }

private:
	SearchHit fHit;
};

}	// namespace


SearchWindow::SearchWindow(BMessenger reader)
	:
	BWindow(BRect(120, 100, 680, 560), "Search", B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fLibrary(""),
	fReader(reader)
{
	fQuery = new BTextControl("query", nullptr, "", new BMessage(kMsgSearch));
	BPopUpMenu* modes = new BPopUpMenu("mode");
	for (int i = 0; i < 4; i++)
		modes->AddItem(new BMenuItem(kModes[i], nullptr));
	modes->ItemAt(0)->SetMarked(true);
	fModeField = new BMenuField("mode", nullptr, modes);
	BPopUpMenu* scopes = new BPopUpMenu("scope");
	for (int i = 0; i < 4; i++)
		scopes->AddItem(new BMenuItem(kScopes[i], nullptr));
	scopes->ItemAt(0)->SetMarked(true);
	fScopeField = new BMenuField("scope", nullptr, scopes);
	BButton* go = new BButton("go", "Search", new BMessage(kMsgSearch));

	fResults = new BListView("results");
	fResults->SetInvocationMessage(new BMessage(kMsgInvoke));
	BScrollView* scroll = new BScrollView("scroll", fResults, 0, false, true);
	// Without a minimum the window shrinks to the list's preferred size when it fills.
	scroll->SetExplicitMinSize(BSize(be_plain_font->StringWidth("m") * 40, 240));
	fStatus = new BStringView("status", "");

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_HALF_ITEM_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
			.Add(fQuery, 3.0f)
			.Add(go)
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
			.Add(fModeField)
			.Add(fScopeField)
			.AddGlue()
		.End()
		.Add(scroll, 10.0f)
		.Add(fStatus);
	fQuery->MakeFocus(true);
}


void SearchWindow::Start(const std::string& module, const std::string& currentBook,
	const std::string& query, SearchMode mode)
{
	fModule = module;
	fBook = currentBook;
	if (!fLibrary.Find(module))
		fLibrary.Reload();		// installed since this window opened
	SetTitle(("Search " + module).c_str());
	if (!query.empty()) {
		fQuery->SetText(query.c_str());
		for (int i = 0; i < 4; i++) {
			if (kModeValues[i] == mode)
				fModeField->Menu()->ItemAt(i)->SetMarked(true);
		}
		RunSearch();
	}
	if (IsHidden())
		Show();
	else
		Activate();
	fQuery->MakeFocus(true);
}


void SearchWindow::RunSearch()
{
	while (BListItem* item = fResults->RemoveItem((int32)0))
		delete item;
	std::string query = fQuery->Text();
	if (fModule.empty() || query.find_first_not_of(' ') == std::string::npos) {
		fStatus->SetText(fModule.empty() ? "No Bible is open." : "");
		return;
	}
	int mode = fModeField->Menu()->IndexOf(fModeField->Menu()->FindMarked());
	int scopeIndex = fScopeField->Menu()->IndexOf(fScopeField->Menu()->FindMarked());
	std::string scope;
	std::vector<BookInfo> books = fLibrary.Books(fModule);
	if (scopeIndex == 1 || scopeIndex == 2) {
		// The testament's first to last book, in this module's versification.
		std::string first, last;
		for (const BookInfo& b : books) {
			if (b.testament != scopeIndex)
				continue;
			if (first.empty())
				first = b.osis;
			last = b.osis;
		}
		scope = first.empty() ? "" : first + "-" + last;
	} else if (scopeIndex == 3) {
		scope = fBook;
	}
	if (mode == 3 && !query.empty() && std::isdigit((unsigned char)query[0]))
		query = "G" + query;		// a bare number: Greek, as most lookups come from the NT

	fStatus->SetText("Searching" "\xE2\x80\xA6");
	UpdateIfNeeded();
	SearchResult result = fLibrary.Search(fModule, query, kModeValues[mode < 0 ? 0 : mode],
		scope, kMaxHits);
	BList items;
	for (const SearchHit& hit : result.hits)
		items.AddItem(new HitItem(hit));
	fResults->AddList(&items);

	std::string status;
	if (result.total == 0)
		status = "No matches.";
	else if (result.total == 1)
		status = "1 verse.";
	else
		status = std::to_string(result.total) + " verses.";
	if (result.total > (int)result.hits.size())
		status += " Showing the first " + std::to_string(result.hits.size()) + ".";
	fStatus->SetText(status.c_str());
}


void SearchWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSearch:
			RunSearch();
			break;
		case kMsgInvoke: {
			HitItem* item = dynamic_cast<HitItem*>(fResults->ItemAt(fResults->CurrentSelection()));
			if (item == nullptr)
				break;
			BMessage show(kMsgShowVerse);
			show.AddString("book", item->Hit().book.c_str());
			show.AddInt32("chapter", item->Hit().chapter);
			show.AddInt32("verse", item->Hit().verse);
			fReader.SendMessage(&show);
			break;
		}
		default:
			BWindow::MessageReceived(message);
	}
}


bool SearchWindow::QuitRequested()
{
	// Keep the results for the next time; the reader window quits this window when it closes.
	if (!IsHidden())
		Hide();
	return false;
}
