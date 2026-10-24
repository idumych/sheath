#include "ChapterView.h"

#include <Clipboard.h>
#include <Cursor.h>
#include <Font.h>
#include <Looper.h>
#include <Message.h>
#include <ScrollBar.h>
#include <Window.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

using namespace sheath;

namespace {

// Text colours, derived from the system's document colours so dark themes work. Updated
// before each render (UpdatePalette); every window computes the same values.
rgb_color kText, kVerseNumber, kRed, kLink, kHeading, kBookmarked;

rgb_color Mix(rgb_color a, rgb_color b, float t)
{
	return make_color((uint8)(a.red + (b.red - a.red) * t), (uint8)(a.green + (b.green - a.green) * t),
		(uint8)(a.blue + (b.blue - a.blue) * t));
}

void UpdatePalette()
{
	rgb_color text = ui_color(B_DOCUMENT_TEXT_COLOR);
	rgb_color back = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	bool dark = back.Brightness() < 128;
	kText = text;
	kVerseNumber = Mix(text, back, 0.5f);
	kHeading = Mix(text, back, 0.2f);
	kRed = dark ? make_color(240, 110, 100) : make_color(180, 20, 20);
	kLink = dark ? make_color(120, 170, 255) : make_color(40, 90, 170);
	kBookmarked = dark ? make_color(255, 170, 60) : make_color(215, 120, 0);
}

}	// namespace


// Collects text and per-run styles, then hands them to the BTextView in one go.
//
// Line spacing: a BTextView line is as tall as the tallest font on it, and the view has no
// line-spacing setting. So after every space the builder adds a zero-width space (U+200B) in
// a font `lineSpacing` times the text size: invisible, no width, so wrapping is unchanged,
// but every line that holds one (all lines with a word boundary) gets taller.
class ChapterView::RunBuilder {
public:
	explicit RunBuilder(const ReadingStyle& style)
		:
		fStyle(style)
	{
		fBase = BaseFont(style);
		fStrut = fBase;
		fStrut.SetSize(style.size * std::max(1.0f, style.lineSpacing));
	}

	static BFont BaseFont(const ReadingStyle& style)
	{
		BFont font(be_plain_font);
		if (!style.family.empty())
			font.SetFamilyAndFace(style.family.c_str(), B_REGULAR_FACE);
		font.SetSize(style.size);
		return font;
	}

	int32 Length() const { return (int32)fText.size(); }
	const BFont& Base() const { return fBase; }

	// Adds text; spaces get the line-spacing strut unless `plain` (verse numbers, gaps).
	void Add(const std::string& text, const BFont& font, rgb_color color, bool plain = false)
	{
		if (text.empty())
			return;
		bool spaced = !plain && fStyle.lineSpacing > 1.01f;
		size_t start = 0;
		while (start < text.size()) {
			size_t space = spaced ? text.find(' ', start) : std::string::npos;
			size_t end = space == std::string::npos ? text.size() : space + 1;
			Append(text.substr(start, end - start), font, color);
			if (space != std::string::npos)
				Append("\xE2\x80\x8B", fStrut, color);
			start = end;
		}
	}

	// A blank line `fraction` of a text line high (the gap between verses or paragraphs).
	void Gap(float fraction)
	{
		BFont gap(fBase);
		gap.SetSize(std::max(1.0f, fStyle.size * fraction));
		Append("\n", gap, kText);
	}

	void AddRun(const Run& run, bool inHeading)
	{
		BFont font(fBase);
		float size = fStyle.size;
		uint16 face = 0;
		if (inHeading) {
			face |= B_BOLD_FACE;
			size = fStyle.size * 1.1f;
		}
		if (run.style & kBold)
			face |= B_BOLD_FACE;
		if (run.style & (kItalic | kTransChange))
			face |= B_ITALIC_FACE;
		if (run.style & (kSmall | kSuperscript))
			size = fStyle.size * 0.7f;
		if (run.style & kDivineName)
			size = fStyle.size * 0.85f;
		font.SetSize(size);
		if (face != 0)
			font.SetFace(face);

		rgb_color color = inHeading ? kHeading : kText;
		if (run.style & kRedLetter)
			color = kRed;
		if (run.link.kind != LinkKind::None)
			color = kLink;
		Add(run.text, font, color);
	}

	void ApplyTo(BTextView* view)
	{
		view->SetText("");
		if (fStyles.empty())
			return;
		text_run_array* runs = BTextView::AllocRunArray((int32)fStyles.size());
		for (size_t i = 0; i < fStyles.size(); i++) {
			runs->runs[i].offset = fStyles[i].offset;
			runs->runs[i].font = fStyles[i].font;
			runs->runs[i].color = fStyles[i].color;
		}
		view->SetText(fText.c_str(), (int32)fText.size(), runs);
		BTextView::FreeRunArray(runs);
	}

private:
	struct Style { int32 offset; BFont font; rgb_color color; };

	void Append(const std::string& text, const BFont& font, rgb_color color)
	{
		Style s{Length(), font, color};
		if (fStyles.empty() || !Same(fStyles.back(), s))
			fStyles.push_back(s);
		fText += text;
	}

	static bool Same(const Style& a, const Style& b)
	{
		return a.font == b.font && a.color.red == b.color.red && a.color.green == b.color.green
			&& a.color.blue == b.color.blue;
	}

	ReadingStyle fStyle;
	BFont fBase;
	BFont fStrut;
	std::string fText;
	std::vector<Style> fStyles;
};


ChapterView::ChapterView(const char* name)
	:
	BTextView(name)
{
	MakeEditable(false);
	MakeSelectable(true);
	SetStylable(true);
	SetWordWrap(true);
	SetInsets(16, 12, 16, 12);
}


bool ChapterView::HasFamily(const char* family)
{
	int32 count = count_font_families();
	for (int32 i = 0; i < count; i++) {
		font_family name;
		if (get_font_family(i, &name) == B_OK && strcmp(name, family) == 0)
			return true;
	}
	return false;
}


void ChapterView::SetChapter(const std::vector<VerseText>& verses, const ReadingStyle& style,
	const std::set<int>& bookmarked)
{
	UpdatePalette();
	fLinks.clear();
	fVerses.clear();
	RunBuilder b(style);
	BFont numberFont(b.Base());
	numberFont.SetSize(style.size * 0.68f);

	for (const VerseText& v : verses) {
		std::vector<Block> blocks = ParseXhtml(v.xhtml);
		bool numbered = false;
		for (const Block& block : blocks) {
			if (block.kind == Block::LineBreak) {
				if (b.Length() > 0)
					b.Add("\n", b.Base(), kText, true);
				continue;
			}
			bool heading = block.kind == Block::Heading;
			if (heading) {
				// A heading starts its own paragraph, with a gap above it.
				if (b.Length() > 0) {
					b.Add("\n", b.Base(), kText, true);
					b.Gap(0.6f);
				}
			} else if (!numbered) {
				if (b.Length() > 0) {
					if (style.versePerLine) {
						b.Add("\n", b.Base(), kText, true);
						b.Gap(0.45f);
					} else {
						b.Add(" ", b.Base(), kText);
					}
				}
				fVerses.push_back({v.verse, b.Length()});
				rgb_color numberColor = bookmarked.count(v.verse) ? kBookmarked : kVerseNumber;
				if (bookmarked.count(v.verse))
					b.Add("\xE2\x97\x86", numberFont, numberColor, true);	// ◆
				b.Add(std::to_string(v.verse), numberFont, numberColor, true);
				b.Add("\xE2\x80\xAF", numberFont, numberColor, true);	// narrow no-break space
				numbered = true;
			}
			bool afterNumber = false;
			for (const Run& run : block.runs) {
				// Consecutive Strong's/morphology numbers would read as one number.
				bool number = run.link.kind == LinkKind::Strongs || run.link.kind == LinkKind::Morph;
				if (number && afterNumber)
					b.Add(" ", numberFont, kText, true);
				afterNumber = number;
				int32 start = b.Length();
				b.AddRun(run, heading);
				if (run.link.kind != LinkKind::None)
					fLinks.push_back({start, b.Length(), run.link, v.verse});
			}
			if (heading)
				b.Add("\n", b.Base(), kText, true);
		}
	}
	Render(b);
}


void ChapterView::SetEntry(const std::string& xhtml, const ReadingStyle& style, char strongsPrefix)
{
	UpdatePalette();
	fLinks.clear();
	fVerses.clear();
	RunBuilder b(style);
	std::vector<Block> blocks = ParseXhtml(xhtml);
	if (strongsPrefix != 0)
		LinkStrongsReferences(blocks, strongsPrefix);
	for (const Block& block : blocks) {
		if (b.Length() > 0)
			b.Add("\n", b.Base(), kText, true);
		for (const Run& run : block.runs) {
			int32 start = b.Length();
			b.AddRun(run, block.kind == Block::Heading);
			if (run.link.kind != LinkKind::None)
				fLinks.push_back({start, b.Length(), run.link, 0});
		}
	}
	Render(b);
}


void ChapterView::SetMessageText(const char* text, const ReadingStyle& style)
{
	UpdatePalette();
	fLinks.clear();
	fVerses.clear();
	RunBuilder b(style);
	b.Add(text, b.Base(), kHeading);
	Render(b);
}


void ChapterView::Render(RunBuilder& builder)
{
	// Replacing the text scrolls to the top; that isn't the user scrolling, so the parallel
	// view must not follow it.
	fSyncing = true;
	builder.ApplyTo(this);
	ScrollToOffset(0);
	fLastTopVerse = VerseAtTop();
	fSyncing = false;
}


void ChapterView::Copy(BClipboard* clipboard)
{
	// The line-spacing struts are zero-width spaces; keep them out of copied text.
	int32 start, end;
	GetSelection(&start, &end);
	if (start == end)
		return;
	std::string text(Text() + start, end - start);
	std::string clean;
	for (size_t i = 0; i < text.size(); i++) {
		if (text.compare(i, 3, "\xE2\x80\x8B") == 0) {
			i += 2;
			continue;
		}
		clean += text[i];
	}
	if (clipboard->Lock()) {
		clipboard->Clear();
		clipboard->Data()->AddData("text/plain", B_MIME_TYPE, clean.c_str(), clean.size());
		clipboard->Commit();
		clipboard->Unlock();
	}
}


void ChapterView::ScrollToVerse(int verse)
{
	for (const VerseStart& v : fVerses) {
		if (v.verse == verse) {
			// Scroll so the line holding the verse number is the first one shown.
			BPoint p = PointAt(OffsetAt(LineAt(v.offset)));
			ScrollTo(0, p.y > 4 ? p.y - 4 : 0);
			return;
		}
	}
}


void ChapterView::RestoreVerse(int verse)
{
	fRestoreVerse = verse;
	ScrollToVerse(verse);
}


void ChapterView::FrameResized(float width, float height)
{
	BTextView::FrameResized(width, height);
	if (fRestoreVerse > 0 && Window() != nullptr && !Window()->IsHidden()) {
		ScrollToVerse(fRestoreVerse);
		fRestoreVerse = 0;
	}
}


void ChapterView::SyncToVerse(int verse)
{
	fSyncing = true;
	ScrollToVerse(verse);
	fLastTopVerse = VerseAtTop();
	fSyncing = false;
}


void ChapterView::ScrollTo(BPoint where)
{
	BTextView::ScrollTo(where);
	if (!fNotify || fSyncing || Window() == nullptr || fVerses.empty())
		return;
	int verse = VerseAtTop();
	if (verse == fLastTopVerse)
		return;
	fLastTopVerse = verse;
	BMessage msg(kMsgScrolled);
	msg.AddInt32("verse", verse);
	msg.AddPointer("view", this);
	Window()->PostMessage(&msg);
}


int ChapterView::VerseAtTop() const
{
	int32 top = OffsetAt(Bounds().LeftTop() + BPoint(0, 4));
	int verse = fVerses.empty() ? 0 : fVerses.front().verse;
	for (const VerseStart& v : fVerses) {
		if (v.offset > top)
			break;
		verse = v.verse;
	}
	return verse;
}


int ChapterView::VerseAt(BPoint where) const
{
	int32 offset = OffsetAt(where);
	int verse = 0;
	for (const VerseStart& v : fVerses) {
		if (v.offset > offset)
			break;
		verse = v.verse;
	}
	return verse;
}


const ChapterView::LinkRange* ChapterView::LinkAt(BPoint where) const
{
	if (fLinks.empty())
		return nullptr;
	int32 offset = OffsetAt(where);
	// OffsetAt snaps to the nearest character; make sure the point is really over its glyph.
	BPoint glyph = PointAt(offset);
	if (where.x < glyph.x - 20 || where.y < glyph.y || where.y > glyph.y + LineHeight(LineAt(offset)))
		return nullptr;
	for (const LinkRange& l : fLinks) {
		if (offset >= l.start && offset < l.end)
			return &l;
	}
	return nullptr;
}


void ChapterView::MouseDown(BPoint where)
{
	int32 buttons = 0;
	if (Window()->CurrentMessage() != nullptr)
		Window()->CurrentMessage()->FindInt32("buttons", &buttons);
	if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0 && VerseAt(where) > 0) {
		BMessage msg(kMsgVerseMenu);
		msg.AddInt32("verse", VerseAt(where));
		msg.AddPoint("where", ConvertToScreen(where));
		Window()->PostMessage(&msg);
		return;
	}
	const LinkRange* link = (buttons & B_PRIMARY_MOUSE_BUTTON) ? LinkAt(where) : nullptr;
	if (link == nullptr) {
		BTextView::MouseDown(where);
		return;
	}
	BMessage msg(kMsgLinkClicked);
	msg.AddInt32("kind", (int32)link->link.kind);
	msg.AddString("target", link->link.target.c_str());
	msg.AddInt32("verse", link->verse);
	msg.AddPointer("view", this);
	Window()->PostMessage(&msg);
}


void ChapterView::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	BTextView::MouseMoved(where, transit, drag);
	if (drag == nullptr && transit != B_EXITED_VIEW && LinkAt(where) != nullptr) {
		BCursor hand(B_CURSOR_ID_FOLLOW_LINK);
		SetViewCursor(&hand);
	}
}
