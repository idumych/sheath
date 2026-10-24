#pragma once

#include <TextView.h>

#include <set>
#include <string>
#include <vector>

#include "Markup.h"
#include "Sword.h"

// How chapters are drawn; set from the View menu.
struct ReadingStyle {
	std::string family;		// font family, "" = the system's plain font
	float size = 16;
	float lineSpacing = 1.5f;	// 1.0 = the font's own line height
	bool versePerLine = true;	// each verse starts a new line, with a small gap
};

// Read-only, selectable chapter text. Verses are laid out as styled runs in a BTextView;
// clicking a link (Strong's number, footnote, cross-reference) sends kMsgLinkClicked to the
// window with "kind" (int32 LinkKind), "target" and "verse". A secondary click sends
// kMsgVerseMenu with "verse" and "where" (screen point).
class ChapterView : public BTextView {
public:
	static const uint32 kMsgLinkClicked = 'lnkc';
	static const uint32 kMsgVerseMenu = 'vrsm';
	static const uint32 kMsgScrolled = 'scrl';	// "verse" now at the top, "view" (pointer)

	explicit ChapterView(const char* name);

	// bookmarked: verse numbers drawn in the accent colour.
	void SetChapter(const std::vector<sheath::VerseText>& verses, const ReadingStyle& style,
		const std::set<int>& bookmarked = std::set<int>());
	// A dictionary entry. strongsPrefix 'G'/'H': link "see GREEK for 5689" etc. (0 = don't).
	void SetEntry(const std::string& xhtml, const ReadingStyle& style, char strongsPrefix = 0);
	void SetMessageText(const char* text, const ReadingStyle& style);	// "no modules" etc.

	static bool HasFamily(const char* family);

	void ScrollToVerse(int verse);
	void SyncToVerse(int verse);		// ScrollToVerse without sending kMsgScrolled
	// Scrolls to the verse once the view has its final size (the saved position at launch:
	// before the window is shown, line breaks are for the wrong width).
	void RestoreVerse(int verse);
	void FrameResized(float width, float height) override;
	void SetScrollNotify(bool notify) { fNotify = notify; }

	using BTextView::ScrollTo;
	void ScrollTo(BPoint where) override;
	int VerseAtTop() const;
	int VerseAt(BPoint where) const;		// 0 if none

	void Copy(BClipboard* clipboard) override;
	void MouseDown(BPoint where) override;
	void MouseMoved(BPoint where, uint32 transit, const BMessage* drag) override;

private:
	struct LinkRange { int32 start, end; sheath::Link link; int verse; };
	struct VerseStart { int verse; int32 offset; };

	class RunBuilder;

	const LinkRange* LinkAt(BPoint where) const;
	void Render(RunBuilder& builder);

	std::vector<LinkRange> fLinks;
	std::vector<VerseStart> fVerses;
	bool fNotify = false;
	bool fSyncing = false;
	int fLastTopVerse = 0;
	int fRestoreVerse = 0;
};
