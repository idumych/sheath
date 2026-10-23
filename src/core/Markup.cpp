#include "Markup.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <regex>

// A small reader for the XHTML that SWORD's FMT_XHTML filters produce. It isn't a general HTML
// parser: it knows SWORD's vocabulary (passagestudy.jsp links, wordsOfJesus/divineName/
// transChange classes, h1-h6 headings, <br/>) and keeps the text of everything else.

namespace sheath {

namespace {

void AppendUtf8(std::string& out, unsigned long cp)
{
	if (cp < 0x80) {
		out += (char)cp;
	} else if (cp < 0x800) {
		out += (char)(0xC0 | (cp >> 6));
		out += (char)(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		out += (char)(0xE0 | (cp >> 12));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	} else if (cp < 0x110000) {
		out += (char)(0xF0 | (cp >> 18));
		out += (char)(0x80 | ((cp >> 12) & 0x3F));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	}
}

// Decodes the entity starting at s[i] == '&'. Returns the length consumed, 0 if not an entity.
size_t DecodeEntity(const std::string& s, size_t i, std::string& out)
{
	size_t semi = s.find(';', i);
	if (semi == std::string::npos || semi - i > 10)
		return 0;
	std::string name = s.substr(i + 1, semi - i - 1);
	static const struct { const char* name; unsigned long cp; } kNamed[] = {
		{"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''},
		{"nbsp", 0xA0}, {"mdash", 0x2014}, {"ndash", 0x2013}, {"lsquo", 0x2018},
		{"rsquo", 0x2019}, {"ldquo", 0x201C}, {"rdquo", 0x201D}, {"hellip", 0x2026},
		{"para", 0xB6},
	};
	if (!name.empty() && name[0] == '#') {
		bool hex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
		const char* digits = name.c_str() + (hex ? 2 : 1);
		char* end = nullptr;
		unsigned long cp = std::strtoul(digits, &end, hex ? 16 : 10);
		if (end == digits || *end != '\0')
			return 0;
		AppendUtf8(out, cp);
		return semi - i + 1;
	}
	for (const auto& e : kNamed) {
		if (name == e.name) {
			AppendUtf8(out, e.cp);
			return semi - i + 1;
		}
	}
	return 0;
}

std::string DecodeEntities(const std::string& s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '&') {
			size_t n = DecodeEntity(s, i, out);
			if (n > 0) {
				i += n - 1;
				continue;
			}
		}
		out += s[i];
	}
	return out;
}

std::string UrlDecode(const std::string& s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '+') {
			out += ' ';
		} else if (s[i] == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1])
				&& std::isxdigit((unsigned char)s[i + 2])) {
			out += (char)std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
			i += 2;
		} else {
			out += s[i];
		}
	}
	return out;
}

// Value of a query parameter in a passagestudy.jsp?action=...&type=...&value=... link.
std::string QueryParam(const std::string& href, const std::string& key)
{
	size_t q = href.find('?');
	if (q == std::string::npos)
		return "";
	size_t pos = q + 1;
	while (pos < href.size()) {
		size_t amp = href.find('&', pos);
		std::string pair = href.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
		size_t eq = pair.find('=');
		if (eq != std::string::npos && pair.substr(0, eq) == key)
			return UrlDecode(pair.substr(eq + 1));
		if (amp == std::string::npos)
			break;
		pos = amp + 1;
	}
	return "";
}

// Some SWORD builds write Strong's numbers with locale digits (Arabic-Indic, U+0660..0669).
std::string AsciiDigits(const std::string& s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		unsigned char c = s[i];
		unsigned char next = i + 1 < s.size() ? s[i + 1] : 0;
		if (c == 0xD9 && next >= 0xA0 && next <= 0xA9) {
			out += (char)('0' + next - 0xA0);
			i++;
		} else {
			out += (char)c;
		}
	}
	return out;
}

Link LinkFromHref(const std::string& rawHref)
{
	std::string href = DecodeEntities(rawHref);
	Link link;
	std::string action = QueryParam(href, "action");
	std::string value = AsciiDigits(QueryParam(href, "value"));
	std::string type = QueryParam(href, "type");
	if (action == "showStrongs") {
		link.kind = LinkKind::Strongs;
		if (!value.empty() && std::isdigit((unsigned char)value[0]))
			value = (type == "Hebrew" ? "H" : "G") + value;
		link.target = value;
	} else if (action == "showMorph") {
		link.kind = LinkKind::Morph;
		link.target = type.empty() ? value : type + ":" + value;
	} else if (action == "showNote") {
		link.kind = type == "x" ? LinkKind::CrossRef : LinkKind::Footnote;
		link.target = value;
	} else if (action == "showRef") {
		link.kind = LinkKind::CrossRef;
		link.target = value;
	} else if (!href.empty()) {
		link.kind = LinkKind::Url;
		link.target = href;
	}
	return link;
}

struct Tag {
	std::string name;	// lower case, without '/'
	bool closing = false, selfClosing = false;
	std::string cls, href;
};

std::string Attr(const std::string& tag, const char* name)
{
	std::string key = std::string(name) + "=";
	size_t p = 0;
	while ((p = tag.find(key, p)) != std::string::npos) {
		bool wordStart = p > 0 && std::isspace((unsigned char)tag[p - 1]);
		p += key.size();
		if (!wordStart)
			continue;
		if (p >= tag.size())
			return "";
		char quote = tag[p];
		if (quote == '"' || quote == '\'') {
			size_t end = tag.find(quote, p + 1);
			return tag.substr(p + 1, end == std::string::npos ? std::string::npos : end - p - 1);
		}
		size_t end = tag.find_first_of(" \t\r\n/>", p);
		return tag.substr(p, end == std::string::npos ? std::string::npos : end - p);
	}
	return "";
}

Tag ParseTag(const std::string& body)
{
	Tag t;
	size_t i = 0;
	if (i < body.size() && body[i] == '/') {
		t.closing = true;
		i++;
	}
	while (i < body.size() && !std::isspace((unsigned char)body[i]) && body[i] != '/')
		t.name += (char)std::tolower((unsigned char)body[i++]);
	t.selfClosing = !body.empty() && body.back() == '/';
	t.cls = Attr(body, "class");
	t.href = Attr(body, "href");
	return t;
}

bool HasClass(const std::string& cls, const char* name)
{
	size_t n = std::strlen(name), p = 0;
	while ((p = cls.find(name, p)) != std::string::npos) {
		bool startOk = p == 0 || std::isspace((unsigned char)cls[p - 1]);
		bool endOk = p + n == cls.size() || std::isspace((unsigned char)cls[p + n]);
		if (startOk && endOk)
			return true;
		p += n;
	}
	return false;
}

bool IsHeading(const std::string& name)
{
	return name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6';
}

bool IsBlockTag(const std::string& name)
{
	return name == "p" || name == "div" || name == "li" || name == "blockquote";
}

class Builder {
public:
	std::vector<Block> blocks;

	// linkId: which <a> element the text is in (0 = none). Text of one element joins into one
	// run; two elements stay two runs even with the same target (G3588 G3588).
	void Text(const std::string& text, unsigned style, const Link& link, int linkId)
	{
		for (size_t i = 0; i < text.size(); i++) {
			char c = text[i];
			if (std::isspace((unsigned char)c)) {
				pendingSpace = true;
				continue;
			}
			// The pilcrow is OSIS paragraph markup; plain renderings drop it.
			if ((unsigned char)c == 0xC2 && i + 1 < text.size()
					&& (unsigned char)text[i + 1] == 0xB6) {
				i++;
				continue;
			}
			Block& b = Current();
			// A space between runs gets only what both sides share: "the <i>word</i> and"
			// keeps the space out of the italic run.
			if (pendingSpace && !b.runs.empty()) {
				const Run& prev = b.runs.back();
				bool sameLink = prev.link.kind == link.kind && prev.link.target == link.target
					&& fLastLinkId == linkId;
				Append(b, " ", prev.style & style, sameLink ? link : Link(), sameLink ? linkId : 0);
			}
			pendingSpace = false;
			Append(b, std::string(1, c), style, link, linkId);
		}
	}

	// Starts a block of the given kind; an empty current block is reused.
	void Start(Block::Kind kind)
	{
		pendingSpace = false;
		if (!blocks.empty() && blocks.back().runs.empty() && blocks.back().kind != Block::LineBreak) {
			blocks.back().kind = kind;
			return;
		}
		blocks.push_back(Block());
		blocks.back().kind = kind;
	}

	void LineBreak()
	{
		pendingSpace = false;
		if (!blocks.empty() && blocks.back().runs.empty() && blocks.back().kind != Block::LineBreak)
			blocks.pop_back();
		if (blocks.empty() || blocks.back().kind == Block::LineBreak)
			return;
		Block br;
		br.kind = Block::LineBreak;
		blocks.push_back(br);
	}

	void Finish()
	{
		while (!blocks.empty() && blocks.back().runs.empty())
			blocks.pop_back();
	}

private:
	bool pendingSpace = false;

	Block& Current()
	{
		if (blocks.empty() || blocks.back().kind == Block::LineBreak)
			blocks.push_back(Block());
		return blocks.back();
	}

	void Append(Block& b, const std::string& s, unsigned style, const Link& link, int linkId)
	{
		if (!b.runs.empty()) {
			Run& last = b.runs.back();
			if (last.style == style && last.link.kind == link.kind
					&& last.link.target == link.target && fLastLinkId == linkId) {
				last.text += s;
				return;
			}
		}
		b.runs.push_back(Run{s, style, link});
		fLastLinkId = linkId;
	}

	int fLastLinkId = 0;
};

struct Frame { std::string name; unsigned style; Link link; int linkId; bool heading; bool decor; };

}	// namespace


std::vector<Block> ParseXhtml(const std::string& xhtml)
{
	Builder out;
	std::vector<Frame> stack;
	unsigned style = 0;
	Link link;
	bool decor = false;		// inside SWORD's "<em class=strongs>&lt;<a>N</a>&gt;</em>"
	int linkId = 0, links = 0;
	std::string text;

	auto flush = [&]() {
		if (text.empty())
			return;
		if (decor && link.kind == LinkKind::None) {
			std::string kept;
			for (char ch : text) {
				if (ch != '<' && ch != '>')
					kept += ch;
			}
			text = kept;
		}
		if (style & kDivineName) {
			for (char& c : text)
				c = (char)std::toupper((unsigned char)c);
		}
		out.Text(text, style, link, linkId);
		text.clear();
	};

	for (size_t i = 0; i < xhtml.size(); i++) {
		char c = xhtml[i];
		if (c == '&') {
			size_t n = DecodeEntity(xhtml, i, text);
			if (n > 0)
				i += n - 1;
			else
				text += c;
			continue;
		}
		if (c != '<') {
			text += c;
			continue;
		}
		if (xhtml.compare(i, 4, "<!--") == 0) {
			size_t end = xhtml.find("-->", i);
			i = end == std::string::npos ? xhtml.size() : end + 2;
			continue;
		}
		size_t end = xhtml.find('>', i);
		if (end == std::string::npos) {
			text += xhtml.substr(i);
			break;
		}
		Tag tag = ParseTag(xhtml.substr(i + 1, end - i - 1));
		i = end;
		if (tag.name.empty() || tag.name[0] == '!' || tag.name[0] == '?')
			continue;
		flush();

		if (tag.name == "br") {
			out.LineBreak();
			continue;
		}
		if (tag.name == "head" || tag.name == "style" || tag.name == "script") {
			if (!tag.closing && !tag.selfClosing) {
				size_t close = xhtml.find("</" + tag.name, i);
				size_t gt = close == std::string::npos ? close : xhtml.find('>', close);
				i = gt == std::string::npos ? xhtml.size() : gt;
			}
			continue;
		}

		if (tag.closing) {
			for (size_t k = stack.size(); k > 0; k--) {
				if (stack[k - 1].name != tag.name)
					continue;
				bool heading = stack[k - 1].heading;
				style = stack[k - 1].style;
				link = stack[k - 1].link;
				linkId = stack[k - 1].linkId;
				decor = stack[k - 1].decor;
				stack.resize(k - 1);
				if (heading || IsBlockTag(tag.name))
					out.Start(Block::Paragraph);
				break;
			}
			continue;
		}
		if (tag.selfClosing)
			continue;

		bool heading = IsHeading(tag.name) || HasClass(tag.cls, "title")
			|| HasClass(tag.cls, "heading");
		stack.push_back(Frame{tag.name, style, link, linkId, heading, decor});
		if (heading)
			out.Start(Block::Heading);
		else if (IsBlockTag(tag.name))
			out.Start(Block::Paragraph);

		if (tag.name == "i" || tag.name == "em" || tag.name == "cite")
			style |= kItalic;
		else if (tag.name == "b" || tag.name == "strong")
			style |= kBold;
		else if (tag.name == "small")
			style |= kSmall;
		else if (tag.name == "sup")
			style |= kSuperscript;
		else if (tag.name == "a") {
			link = LinkFromHref(tag.href);
			linkId = ++links;
		}
		if (HasClass(tag.cls, "strongs") || HasClass(tag.cls, "morph"))
			decor = true;
		if (HasClass(tag.cls, "wordsOfJesus"))
			style |= kRedLetter;
		if (HasClass(tag.cls, "divineName"))
			style |= kDivineName;
		if (HasClass(tag.cls, "transChange"))
			style |= kTransChange;
	}
	flush();
	out.Finish();
	return out.blocks;
}


void LinkStrongsReferences(std::vector<Block>& blocks, char defaultPrefix)
{
	static const std::regex kRef("(see GREEK for|see HEBREW for|[Cc]ompare|from) ([0-9]{1,5})\\b");
	for (Block& block : blocks) {
		std::vector<Run> out;
		for (const Run& run : block.runs) {
			if (run.link.kind != LinkKind::None) {
				out.push_back(run);
				continue;
			}
			size_t done = 0;
			for (std::sregex_iterator it(run.text.begin(), run.text.end(), kRef), end; it != end; ++it) {
				const std::smatch& m = *it;
				size_t numberStart = m.position(2);
				char prefix = m.str(1) == "see GREEK for" ? 'G'
					: m.str(1) == "see HEBREW for" ? 'H' : defaultPrefix;
				if (numberStart > done)
					out.push_back(Run{run.text.substr(done, numberStart - done), run.style, Link()});
				Link link;
				link.kind = LinkKind::Strongs;
				link.target = std::string(1, prefix) + m.str(2);
				out.push_back(Run{m.str(2), run.style, link});
				done = numberStart + m.length(2);
			}
			if (done < run.text.size())
				out.push_back(Run{run.text.substr(done), run.style, Link()});
		}
		block.runs = out;
	}
}


std::string PlainText(const std::vector<Block>& blocks)
{
	std::string out;
	for (size_t i = 0; i < blocks.size(); i++) {
		if (i > 0)
			out += '\n';
		for (const Run& r : blocks[i].runs)
			out += r.text;
	}
	return out;
}

}	// namespace sheath
