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

const rgb_color kText = {20, 20, 20, 255};
const rgb_color kVerseNumber = {130, 130, 130, 255};
const rgb_color kRed = {180, 20, 20, 255};
const rgb_color kLink = {40, 90, 170, 255};
const rgb_color kHeading = {60, 60, 60, 255};

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


void ChapterView::SetChapter(const std::vector<VerseText>& verses, float fontSize)
{
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
				b.Add(std::to_string(v.verse), numberFont, kVerseNumber);
				b.Add("\xC2\xA0", numberFont, kVerseNumber);	// no-break space
				numbered = true;
			}
			for (const Run& run : block.runs) {
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
			BPoint p = PointAt(v.offset);
			ScrollTo(0, p.y > 12 ? p.y - 12 : 0);
			return;
		}
	}
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
	const LinkRange* link = (buttons & B_PRIMARY_MOUSE_BUTTON) ? LinkAt(where) : nullptr;
	if (link == nullptr) {
		BTextView::MouseDown(where);
		return;
	}
	BMessage msg(kMsgLinkClicked);
	msg.AddInt32("kind", (int32)link->link.kind);
	msg.AddString("target", link->link.target.c_str());
	msg.AddInt32("verse", link->verse);
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
