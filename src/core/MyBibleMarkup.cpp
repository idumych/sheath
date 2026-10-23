#include "MyBibleMarkup.h"
#include <string>
#include <vector>

namespace sheath {

namespace {

// Check if position starts a valid entity (&name;, &#digits;, &#xhex;).
bool IsEntityStart(const std::string& text, size_t pos)
{
	if (pos + 1 >= text.size() || text[pos] != '&')
		return false;
	size_t i = pos + 1;
	if (i >= text.size())
		return false;
	if (text[i] == '#') {
		++i;
		if (i < text.size() && (text[i] == 'x' || text[i] == 'X'))
			++i;
		bool hasDigits = false;
		while (i < text.size() && text[i] != ';') {
			if (text[i] >= '0' && text[i] <= '9')
				hasDigits = true;
			++i;
		}
		return hasDigits && i < text.size() && text[i] == ';';
	}
	if (text[i] < 'A' || text[i] > 'z')
		return false;
	while (i < text.size() && text[i] != ';' && text[i] != ' '
		&& text[i] != '<' && text[i] != '>')
		++i;
	return i < text.size() && text[i] == ';';
}

// Map known tag names to their OSIS opening. Returns empty for unknown.
std::string ConvertTag(const std::string& tagName)
{
	if (tagName == "J")      return "<q who=\"Jesus\" marker=\"\">";
	if (tagName == "i")      return "<transChange type=\"added\">";
	if (tagName == "e")      return "<hi type=\"emphasis\">";
	if (tagName == "f")      return "<note type=\"x-footnote\">";
	if (tagName == "pb")     return "<milestone type=\"x-p\"/>";
	if (tagName == "br")     return "<lb/>";
	// "t" and "n"/"m" are handled specially below, not here.
	return {};
}

// Map known tag names to their OSIS closing. Returns empty if no closing tag.
std::string GetCloseTag(const std::string& tagName)
{
	if (tagName == "J")      return "</q>";
	if (tagName == "i")      return "</transChange>";
	if (tagName == "e")      return "</hi>";
	if (tagName == "f")      return "</note>";
	if (tagName == "t")      return "<lb/>";
	return {};  // unknown tag — closing dropped
}

} // namespace

std::string MyBibleToOsis(const std::string& text, bool newTestament, OsisFlags* used)
{
	std::string out;
	out.reserve(text.size());
	std::vector<std::string> openStack;
	int skipDepth = 0;  // nesting depth for <n>/<m> content removal
	auto emit = [&](const std::string& s) {
		if (skipDepth == 0)
			out += s;
	};
	auto emitc = [&](char c) {
		if (skipDepth == 0)
			out += c;
	};

	size_t pos = 0;
	size_t n = text.size();

	while (pos < n) {
		// --- Ampersand ---
		if (text[pos] == '&') {
			if (IsEntityStart(text, pos)) {
				size_t eStart = pos;
				while (pos < n && text[pos] != ';')
					++pos;
				if (pos < n)
					emit(text.substr(eStart, pos - eStart + 1));
				else
					emit(text.substr(eStart));
				++pos;
				continue;
			}
			emit("&amp;");
			++pos;
			continue;
		}

		// --- Angle bracket: tag or escape ---
		if (text[pos] == '<') {
			// Check if this starts a tag: '<' followed by optional '/' then a letter.
			bool isTag = false;
			if (pos + 2 < n) {
				size_t c1 = pos + 1;
				if (text[c1] == '/') {
					if (pos + 2 < n &&
						((text[pos + 2] >= 'A' && text[pos + 2] <= 'Z') ||
						 (text[pos + 2] >= 'a' && text[pos + 2] <= 'z')))
						isTag = true;
				} else if (((text[c1] >= 'A' && text[c1] <= 'Z') ||
					(text[c1] >= 'a' && text[c1] <= 'z')))
					isTag = true;
			}

			if (!isTag) {
				emit("&lt;");
				++pos;
				continue;
			}

			// Find matching '>' for this tag
			size_t tagStart = pos;
			size_t gtPos = pos + 1;
			while (gtPos < n && text[gtPos] != '>')
				++gtPos;
			if (gtPos >= n) {
				emit(text.substr(tagStart));
				break;
			}
			std::string tagContent = text.substr(tagStart + 1,
				gtPos - tagStart - 1);

			// --- Self-closing tag (ends with '/') ---
			if (!tagContent.empty() && tagContent.back() == '/') {
				std::string tagName = tagContent.substr(0,
					tagContent.size() - 1);
				if (tagName != "S") {
					std::string converted = ConvertTag(tagName);
					if (!converted.empty())
						emit(converted);
					else
						emit(text.substr(tagStart,
							gtPos - tagStart + 1));
				}
				pos = gtPos + 1;
				continue;
			}

			// --- S tag (opening or closing) ---
			size_t sIdx = 0;
			if (!tagContent.empty() && tagContent[0] == '/')
				sIdx = 1;
			bool isSTag = (!tagContent.empty() && sIdx < tagContent.size()
				&& tagContent[sIdx] == 'S');

			if (isSTag) {
				// Closing </S> — drop it (S tags don't go on stack)
				if (tagContent[0] == '/') {
					pos = gtPos + 1;
					continue;
				}

				// Opening <S> — scan ahead to find </S> and collect numbers
				std::vector<std::string> numbers;

				// Find matching </S> for this first <S>
				size_t scanPos = gtPos + 1;
				bool foundClose = false;
				while (scanPos + 3 < n) {
					if (text.substr(scanPos, 4) == "</S>") {
						foundClose = true;
						std::string num = text.substr(gtPos + 1,
							scanPos - (gtPos + 1));
						if (!num.empty()) {
							if (!(num.size() >= 1 &&
								(num[0] == 'G' || num[0] == 'H'))) {
								num.insert(num.begin(),
									newTestament ? 'G' : 'H');
							}
							numbers.push_back("strong:" + num);
						}
						scanPos += 4;
						break;
					}
					++scanPos;
				}
				if (!foundClose) {
					pos = gtPos + 1;
					continue;
				}

				// Look ahead for more consecutive <S> tags
				while (scanPos + 2 < n) {
					if (text[scanPos] != '<' ||
						text[scanPos + 1] != 'S' ||
						text[scanPos + 2] != '>')
						break;
					size_t innerPos = scanPos + 3;
					bool found2 = false;
					while (innerPos + 3 < n) {
						if (text.substr(innerPos, 4) == "</S>") {
							found2 = true;
							std::string num = text.substr(
								scanPos + 3,
								innerPos - (scanPos + 3));
							if (!num.empty()) {
								if (!(num.size() >= 1 &&
									(num[0] == 'G' || num[0] == 'H'))) {
									num.insert(num.begin(),
										newTestament ? 'G' : 'H');
								}
								numbers.push_back("strong:" + num);
							}
							scanPos = innerPos + 4;
							break;
						}
						++innerPos;
					}
					if (!found2)
						break;
				}

				// Find the word before the first <S>
				if (!numbers.empty()) {
					size_t wordEnd = out.size();
					size_t scan = wordEnd;
					// Scan back over non-space bytes, stop at '>' or space
					while (scan > 0 && out[scan - 1] != ' ' && out[scan - 1] != '>') {
						--scan;
					}
					size_t wordStart = scan;

					if (wordEnd > wordStart) {
						// Trim trailing ASCII punctuation
						std::string punct;
						while (wordEnd > wordStart) {
							char last = out[wordEnd - 1];
							if (last == ',' || last == '.' ||
								last == ';' || last == '!' ||
								last == ':' || last == '?' ||
								last == '"' || last == '\'')
							{
								--wordEnd;
								punct += last;
							} else {
								break;
							}
						}
						std::string word = out.substr(wordStart,
							wordEnd - wordStart);
						out.erase(wordStart);

						std::string lemma = "lemma=\"";
						for (size_t ni = 0; ni < numbers.size(); ++ni) {
							if (ni > 0)
								lemma += ' ';
							lemma += numbers[ni];
						}
						lemma += "\">";
						out += "<w ";
						out += lemma;
						out += word;
						out += "</w>";
						out += punct;
						if (used)
							used->strongs = true;
					} else {
						// No word before it — empty <w>
						std::string lemma = "lemma=\"";
						for (size_t ni = 0; ni < numbers.size(); ++ni) {
							if (ni > 0)
								lemma += ' ';
							lemma += numbers[ni];
						}
						lemma += "\">";
						out += "<w ";
						out += lemma;
						out += "</w>";
						if (used)
							used->strongs = true;
					}
				}

				pos = scanPos;
				continue;
			}

			// --- Closing tag (starts with '/') ---
			if (!tagContent.empty() && tagContent[0] == '/') {
				size_t nameStart = 0;
				while (nameStart < tagContent.size()
					&& tagContent[nameStart] == '/')
					++nameStart;
				std::string tagName = tagContent.substr(nameStart);

				// Special handling for </n> and </m> — remove content
				if (tagName == "n" || tagName == "m") {
					for (int i = static_cast<int>(openStack.size()) - 1;
						i >= 0; --i)
					{
						if (openStack[static_cast<size_t>(i)] == tagName) {
							--skipDepth;
							openStack.pop_back();
							break;
						}
					}
					pos = gtPos + 1;
					continue;
				}

				// General closing tag — find in stack, emit correct closing
				for (int i = static_cast<int>(openStack.size()) - 1;
					i >= 0; --i)
				{
					if (openStack[static_cast<size_t>(i)] == tagName) {
						std::string close = GetCloseTag(tagName);
						if (!close.empty())
							emit(close);
						openStack.pop_back();
						break;
					}
				}
				pos = gtPos + 1;
				continue;
			}

			// --- Opening tag ---
			std::string tagName = tagContent;

			// Special handling for <n> and <m> — remove content
			if (tagName == "n" || tagName == "m") {
				++skipDepth;
				openStack.push_back(tagName);
				pos = gtPos + 1;
				continue;
			}

			// Special handling for <t> — line break marker, push to stack
			if (tagName == "t") {
				openStack.push_back(tagName);
				pos = gtPos + 1;
				continue;
			}

			std::string converted = ConvertTag(tagName);

			if (!converted.empty()) {
				emit(converted);
				openStack.push_back(tagName);
				// Set flags
				if (tagName == "J" && used)
					used->redLetter = true;
				if (tagName == "f" && used)
					used->footnotes = true;
				pos = gtPos + 1;
				continue;
			}

			// Unknown tag — drop the tag, keep content
			pos = gtPos + 1;
		} else {
			// --- Plain character ---
			if (text[pos] == '>') {
				emit("&gt;");
				++pos;
				continue;
			}
			emitc(text[pos]);
			++pos;
		}
	}

	// Close any tags still open at end of text
	for (int i = static_cast<int>(openStack.size()) - 1; i >= 0; --i) {
		const std::string& t = openStack[static_cast<size_t>(i)];
		if (t == "n" || t == "m") {
			--skipDepth;
		} else {
			std::string close = GetCloseTag(t);
			if (!close.empty())
				emit(close);
		}
	}

	return out;
}

} // namespace sheath
