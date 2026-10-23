#include "ChapterView.h"

#include <Cursor.h>
#include <Font.h>
#include <Looper.h>
#include <Message.h>
#include <ScrollBar.h>
#include <Window.h>

#include <cstdlib>

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
class ChapterView::RunBuilder {
public:
	explicit RunBuilder(float size) : fSize(size) {}

	int32 Length() const { return (int32)fText.size(); }

	void Add(const std::string& text, const BFont& font, rgb_color color)
	{
		if (text.empty())
			return;
		Style s{Length(), font, color};
		if (fStyles.empty() || !Same(fStyles.back(), s))
			fStyles.push_back(s);
		fText += text;
	}

	void AddRun(const Run& run, bool inHeading)
	{
		BFont font(be_plain_font);
		float size = fSize;
		uint16 face = 0;
		if (inHeading) {
			face |= B_BOLD_FACE;
			size = fSize * 1.1f;
		}
		if (run.style & kBold)
			face |= B_BOLD_FACE;
		if (run.style & (kItalic | kTransChange))
			face |= B_ITALIC_FACE;
		if (run.style & (kSmall | kSuperscript))
			size = fSize * 0.7f;
		if (run.style & kDivineName)
			size = fSize * 0.85f;
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

	float Size() const { return fSize; }

private:
	struct Style { int32 offset; BFont font; rgb_color color; };

	static bool Same(const Style& a, const Style& b)
	{
		return a.font == b.font && a.color.red == b.color.red && a.color.green == b.color.green
			&& a.color.blue == b.color.blue;
	}

	float fSize;
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


void ChapterView::SetChapter(const std::vector<VerseText>& verses, float fontSize,
	const std::set<int>& bookmarked)
{
	UpdatePalette();
	fLinks.clear();
	fVerses.clear();
	RunBuilder b(fontSize);
	BFont numberFont(be_bold_font);
	numberFont.SetSize(fontSize * 0.65f);

	for (const VerseText& v : verses) {
		std::vector<Block> blocks = ParseXhtml(v.xhtml);
		bool numbered = false;
		for (const Block& block : blocks) {
			if (block.kind == Block::LineBreak) {
				if (b.Length() > 0)
					b.Add("\n", BFont(be_plain_font), kText);
				continue;
			}
			bool heading = block.kind == Block::Heading;
			if (heading) {
				// A heading starts its own paragraph, with a blank line above it.
				if (b.Length() > 0)
					b.Add("\n\n", BFont(be_plain_font), kText);
			} else if (!numbered) {
				if (b.Length() > 0)
					b.Add(" ", BFont(be_plain_font), kText);
				fVerses.push_back({v.verse, b.Length()});
				rgb_color numberColor = bookmarked.count(v.verse) ? kBookmarked : kVerseNumber;
				if (bookmarked.count(v.verse))
					b.Add("\xE2\x97\x86", numberFont, numberColor);	// ◆
				b.Add(std::to_string(v.verse), numberFont, numberColor);
				b.Add("\xC2\xA0", numberFont, numberColor);	// no-break space
				numbered = true;
			}
			bool afterNumber = false;
			for (const Run& run : block.runs) {
				// Consecutive Strong's/morphology numbers would read as one number.
				bool number = run.link.kind == LinkKind::Strongs || run.link.kind == LinkKind::Morph;
				if (number && afterNumber)
					b.Add(" ", numberFont, kText);
				afterNumber = number;
				int32 start = b.Length();
				b.AddRun(run, heading);
				if (run.link.kind != LinkKind::None)
					fLinks.push_back({start, b.Length(), run.link, v.verse});
			}
			if (heading) {
				BFont plain(be_plain_font);
				plain.SetSize(fontSize);
				b.Add("\n", plain, kText);
			}
		}
	}
	b.ApplyTo(this);
	ScrollToOffset(0);
}


void ChapterView::SetEntry(const std::string& xhtml, float fontSize)
{
	UpdatePalette();
	fLinks.clear();
	fVerses.clear();
	RunBuilder b(fontSize);
	for (const Block& block : ParseXhtml(xhtml)) {
		if (b.Length() > 0)
			b.Add("\n", BFont(be_plain_font), kText);
		for (const Run& run : block.runs) {
			int32 start = b.Length();
			b.AddRun(run, block.kind == Block::Heading);
			if (run.link.kind != LinkKind::None)
				fLinks.push_back({start, b.Length(), run.link, 0});
		}
	}
	b.ApplyTo(this);
	ScrollToOffset(0);
}


void ChapterView::SetMessageText(const char* text, float fontSize)
{
	UpdatePalette();
	fLinks.clear();
	fVerses.clear();
	RunBuilder b(fontSize);
	BFont font(be_plain_font);
	font.SetSize(fontSize);
	b.Add(text, font, kHeading);
	b.ApplyTo(this);
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
