#pragma once

#include <TextView.h>

#include <set>
#include <string>
#include <vector>

#include "Markup.h"
#include "Sword.h"

// Read-only, selectable chapter text. Verses are laid out as styled runs in a BTextView;
// clicking a link (Strong's number, footnote, cross-reference) sends kMsgLinkClicked to the
// window with "kind" (int32 LinkKind), "target" and "verse". A secondary click sends
// kMsgVerseMenu with "verse" and "where" (screen point).
class ChapterView : public BTextView {
public:
	static const uint32 kMsgLinkClicked = 'lnkc';
	static const uint32 kMsgVerseMenu = 'vrsm';

	explicit ChapterView(const char* name);

	// bookmarked: verse numbers drawn in the accent colour.
	void SetChapter(const std::vector<sheath::VerseText>& verses, float fontSize,
		const std::set<int>& bookmarked = std::set<int>());
	void SetEntry(const std::string& xhtml, float fontSize);	// a dictionary entry
	void SetMessageText(const char* text, float fontSize);	// "no modules installed" etc.

	void ScrollToVerse(int verse);
	int VerseAtTop() const;
	int VerseAt(BPoint where) const;		// 0 if none

	void MouseDown(BPoint where) override;
	void MouseMoved(BPoint where, uint32 transit, const BMessage* drag) override;

private:
	struct LinkRange { int32 start, end; sheath::Link link; int verse; };
	struct VerseStart { int verse; int32 offset; };

	class RunBuilder;

	const LinkRange* LinkAt(BPoint where) const;

	std::vector<LinkRange> fLinks;
	std::vector<VerseStart> fVerses;
};
