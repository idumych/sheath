#include "Markup.h"
#include <map>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstring>

namespace sheath {

// ---- HTML entity decoding ----
static std::string decodeEntities(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        if (s[i] == '&' && i + 1 < s.size()) {
            // Find closing ;
            size_t end = s.find(';', i + 1);
            if (end == std::string::npos) {
                out += s[i]; i++; continue;
            }
            std::string entity = s.substr(i + 1, end - i - 1);
            if (entity == "nbsp" || entity == "shy") { out += ' '; i = end + 1; }
            else if (entity == "lt") { out += '<'; i = end + 1; }
            else if (entity == "gt") { out += '>'; i = end + 1; }
            else if (entity == "amp") { out += '&'; i = end + 1; }
            else if (entity == "quot") { out += '"'; i = end + 1; }
            else if (entity == "apos") { out += '\''; i = end + 1; }
            else if (entity == "mdash") { out += '\xE2\x80\x94'; i = end + 1; }
            else if (entity == "ndash") { out += '\xE2\x80\x93'; i = end + 1; }
            else if (entity == "laquo") { out += '\xC2\xAB'; i = end + 1; }
            else if (entity == "raquo") { out += '\xC2\xBB'; i = end + 1; }
            else if (entity == "bull") { out += '\xE2\x80\xA2'; i = end + 1; }
            else if (entity == "hellip") { out += '\xE2\x80\xA6'; i = end + 1; }
            else if (entity == "lsquo") { out += '\xE2\x80\x98'; i = end + 1; }
            else if (entity == "rsquo") { out += '\xE2\x80\x99'; i = end + 1; }
            else if (entity == "ldquo") { out += '\xE2\x80\x9C'; i = end + 1; }
            else if (entity == "rdquo") { out += '\xE2\x80\x9D'; i = end + 1; }
            else if (entity == "copy") { out += '\xC2\xA9'; i = end + 1; }
            else if (entity == "reg") { out += '\xC2\xAE'; i = end + 1; }
            else if (entity == "sup1") { out += '\xC2\xb9'; i = end + 1; }
            else if (entity == "sup2") { out += '\xC2\xb2'; i = end + 1; }
            else if (entity == "sup3") { out += '\xC2\xb3'; i = end + 1; }
            else if (entity == "frac12") { out += '\xC2\xBD'; i = end + 1; }
            else if (entity == "frac14") { out += '\xC2\xBC'; i = end + 1; }
            else if (entity == "frac34") { out += '\xC2\xBE'; i = end + 1; }
            else if (entity.size() >= 2 && entity[0] == '#' && entity.size() <= 7) {
                // Numeric entity
                int start = (entity[0] == '#' && entity[1] == 'x') ? 2 : 1;
                int codepoint = 0;
                try {
                    std::string numStr = entity.substr(start);
                    if (entity[1] == 'x') codepoint = std::stoi(numStr, nullptr, 16);
                    else codepoint = std::stoi(numStr);
                } catch (...) {
                    out += s[i]; i++; continue;
                }
                // Encode as UTF-8
                if (codepoint < 0x80) {
                    out += static_cast<char>(codepoint);
                } else if (codepoint < 0x800) {
                    out += static_cast<char>(0xC0 | (codepoint >> 6));
                    out += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else {
                    out += static_cast<char>(0xE0 | (codepoint >> 12));
                    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                i = end + 1;
            }
            else {
                // Unknown entity - output as-is without & and ;
                out += s.substr(i, end - i + 1);
                i = end + 1;
            }
        } else {
            out += s[i]; i++;
        }
    }
    return out;
}

// ---- Simple XML/XHTML parser ----

struct TagInfo {
    std::string name;
    std::map<std::string, std::string> attrs;
    bool selfClosing = false;
};

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static std::string collapseWhitespace(const std::string& s) {
    std::string out;
    bool inSpace = false;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!inSpace && !out.empty()) {
                out += ' ';
                inSpace = true;
            }
        } else {
            out += c;
            inSpace = false;
        }
    }
    // Trim trailing space
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

static TagInfo parseTag(const std::string& tagContent, size_t& pos) {
    TagInfo ti;
    std::string content = tagContent;
    
    // Check if self-closing
    if (!content.empty() && content.back() == '/') {
        ti.selfClosing = true;
        content.pop_back();
    }
    
    // Check if closing tag
    size_t nameStart = 0;
    if (!content.empty() && content[0] == '/') {
        nameStart = 1;
    }
    
    // Extract tag name
    size_t spacePos = content.find_first_of(" \t\n\r/>");
    if (spacePos == std::string::npos) {
        ti.name = content.substr(nameStart);
        return ti;
    }
    ti.name = content.substr(nameStart, spacePos - nameStart);
    
    // Extract attributes
    std::string rest = content.substr(spacePos);
    size_t attrStart = 0;
    while (attrStart < rest.size()) {
        // Skip whitespace
        while (attrStart < rest.size() && (rest[attrStart] == ' ' || rest[attrStart] == '\t' || rest[attrStart] == '\n' || rest[attrStart] == '\r')) {
            attrStart++;
        }
        if (attrStart >= rest.size() || rest[attrStart] == '/' || rest[attrStart] == '>') break;
        
        // Attribute name
        size_t eqPos = rest.find('=', attrStart);
        size_t attrEnd = rest.find_first_of(" \t\n\r/>", attrStart);
        if (eqPos == std::string::npos || eqPos > attrEnd) {
            // Boolean attribute
            ti.attrs[rest.substr(attrStart, attrEnd - attrStart)] = "";
            attrStart = attrEnd;
            continue;
        }
        
        std::string attrName = rest.substr(attrStart, eqPos - attrStart);
        attrStart = eqPos + 1;
        
        // Skip whitespace after =
        while (attrStart < rest.size() && rest[attrStart] == ' ') attrStart++;
        
        // Quoted value
        if (attrStart < rest.size() && (rest[attrStart] == '"' || rest[attrStart] == '\'')) {
            char quote = rest[attrStart];
            attrStart++;
            size_t valEnd = rest.find(quote, attrStart);
            ti.attrs[attrName] = rest.substr(attrStart, valEnd - attrStart);
            attrStart = valEnd + 1;
        } else {
            // Unquoted value
            size_t valEnd = rest.find_first_of(" \t\n\r/>", attrStart);
            ti.attrs[attrName] = rest.substr(attrStart, valEnd - attrStart);
            attrStart = valEnd;
        }
    }
    
    return ti;
}

// Collect all text content from child elements, ignoring the tag itself
static std::string collectText(const std::string& html, size_t& pos,
                                const std::string& closeTag) {
    std::string text;
    while (pos < html.size()) {
        if (html[pos] == '<') {
            // Check for closing tag
            size_t closeStart = pos;
            size_t closeEnd = html.find('>', pos);
            if (closeEnd == std::string::npos) break;
            std::string closeContent = html.substr(closeStart + 1, closeEnd - closeStart - 1);
            // Strip leading /
            if (!closeContent.empty() && closeContent[0] == '/') closeContent = closeContent.substr(1);
            // Strip attributes
            size_t sp = closeContent.find_first_of(" \t\n\r");
            std::string closeName = (sp != std::string::npos) ? closeContent.substr(0, sp) : closeContent;
            if (closeName == closeTag) {
                pos = closeEnd + 1;
                return text;
            }
            // Opening tag - find its end and recurse
            size_t tagEnd = html.find('>', pos);
            if (tagEnd == std::string::npos) { pos++; continue; }
            std::string tagContent = html.substr(pos + 1, tagEnd - pos - 1);
            
            // Check if self-closing
            bool selfClose = false;
            if (!tagContent.empty() && tagContent.back() == '/') {
                selfClose = true;
            }
            
            // Determine tag name
            std::string tagName;
            size_t nameStart = (tagContent[0] == '/') ? 1 : 0;
            size_t ns = tagContent.find_first_of(" \t\n\r/>", nameStart);
            tagName = tagContent.substr(nameStart, ns - nameStart);
            
            // Known inline formatting tags - process specially
            if (tagName == "milestone" || tagName == "note" || tagName == "ref" ||
                tagName == "chapter" || tagName == "div" || tagName == "head" ||
                tagName == "h" || tagName == "title" || tagName == "headitem" ||
                tagName == "hi" || tagName == "label" || tagName == "noteRef" ||
                tagName == "xref" || tagName == "indexEntry" || tagName == "indexItem" ||
                tagName == "noteNote") {
                // Skip self-closing tags entirely
                if (selfClose || tagName == "milestone" || tagName == "noteRef" ||
                    tagName == "xref" || tagName == "ref" || tagName == "chapter" ||
                    tagName == "note" || tagName == "noteNote" || tagName == "title") {
                    // Find the closing tag for non-self-closing
                    if (!selfClose && tagName != "milestone") {
                        pos = tagEnd + 1;
                        // Skip content up to closing tag
                        std::string closed = "/" + tagName;
                        // Simple skip - find the closing tag
                        std::string closingTagStr = "<" + closed;
                        size_t ctag = html.find(closingTagStr, pos);
                        if (ctag != std::string::npos) {
                            size_t ctend = html.find('>', ctag);
                            if (ctend != std::string::npos) {
                                pos = ctend + 1;
                            } else {
                                pos = html.size();
                            }
                        } else {
                            pos = html.size();
                        }
                    } else {
                        pos = tagEnd + 1;
                    }
                    continue;
                }
            }
            
            // For inline tags with content, recurse to get their text
            // <w>, <divineName>, <transChange>, <q>, <sup>, <em>, <b>, <italic>, etc.
            if (tagName == "w" || tagName == "divineName" || tagName == "transChange" ||
                tagName == "q" || tagName == "sup" || tagName == "sub" ||
                tagName == "italic" || tagName == "emphasis" || tagName == "bold" ||
                tagName == "b" || tagName == "i" || tagName == "u" ||
                tagName == "span" || tagName == "ob" || tagName == "oe" ||
                tagName == "wg" || tagName == "lex" || tagName == "orth" ||
                tagName == "pron" || tagName == "def" || tagName == "entryFree" ||
                tagName == "lb" || tagName == "lb/" || tagName == "p" ||
                tagName == "lg" || tagName == "l" || tagName == "tr" ||
                tagName == "td" || tagName == "th") {
                // Recurse into tag content
                pos = tagEnd + 1;
                std::string inner = collectText(html, pos, tagName);
                text += inner;
                continue;
            }
            
            // For other tags (like <chapter>, <div>, <head>, etc.) that we want to skip
            if (selfClose) {
                pos = tagEnd + 1;
                continue;
            }
            
            // Skip the tag content
            pos = tagEnd + 1;
            // Find closing tag
            std::string closed = "/" + tagName;
            size_t ctag = html.find("<" + closed, pos);
            if (ctag != std::string::npos) {
                size_t ctend = html.find('>', ctag);
                if (ctend != std::string::npos) {
                    pos = ctend + 1;
                } else {
                    pos = html.size();
                }
            } else {
                pos = html.size();
            }
        } else {
            text += html[pos];
            pos++;
        }
    }
    return text;
}

// ---- Main parser ----

// Parse attributes from a tag string and return (strongs_target, morph_target, href, style_flags)
static std::pair<std::string, std::string> parseStrongMorph(const std::string& tagContent) {
    std::string strongs;
    std::string morph;
    
    // Find standalone strong: attribute (not inside another attribute's value)
    size_t pos = 0;
    while ((pos = tagContent.find("strong:", pos)) != std::string::npos) {
        // Ensure "strong:" is a standalone attribute
        bool standalone = (pos == 0 || std::isspace(static_cast<unsigned char>(tagContent[pos - 1])));
        if (standalone) {
            size_t valStart = pos + 7;
            size_t valEnd = tagContent.find_first_of(" \t\n\r/>", valStart);
            if (valEnd == std::string::npos) valEnd = tagContent.size();
            std::string val = tagContent.substr(valStart, valEnd - valStart);
            if (!val.empty() && val[0] != ':') {
                strongs = val;
                break;
            }
        }
        pos++;
    }
    
    // Find morph attribute (like morph="robinson:V-PAI-3S")
    pos = tagContent.find("morph=");
    if (pos != std::string::npos) {
        pos += 6;
        if (pos < tagContent.size() && (tagContent[pos] == '"' || tagContent[pos] == '\'')) {
            char quote = tagContent[pos];
            pos++;
            size_t valEnd = tagContent.find(quote, pos);
            if (valEnd != std::string::npos) {
                morph = tagContent.substr(pos, valEnd - pos);
            }
        }
    }
    
    return {strongs, morph};
}

static unsigned parseInlineStyle(const std::string& tagContent) {
    unsigned style = 0;
    
    // Check for type="added" in transChange
    size_t typePos = tagContent.find("type=");
    if (typePos != std::string::npos) {
        typePos += 5;
        if (typePos < tagContent.size()) {
            char quote = 0;
            if (tagContent[typePos] == '"' || tagContent[typePos] == '\'') {
                quote = tagContent[typePos];
                typePos++;
            }
            size_t valEnd = (quote) ? tagContent.find(quote, typePos) : 
                                   tagContent.find_first_of(" \t\n\r/>", typePos);
            if (valEnd != std::string::npos) {
                std::string typeVal = tagContent.substr(typePos, valEnd - typePos);
                if (typeVal == "added" || typeVal == "added") {
                    style |= kTransChange;
                }
            }
        }
    }
    
    // Check for rend attribute
    typePos = tagContent.find("rend=");
    if (typePos != std::string::npos) {
        typePos += 5;
        if (typePos < tagContent.size() && (tagContent[typePos] == '"' || tagContent[typePos] == '\'')) {
            char quote = tagContent[typePos];
            typePos++;
            size_t valEnd = tagContent.find(quote, typePos);
            if (valEnd != std::string::npos) {
                std::string rendVal = tagContent.substr(typePos, valEnd - typePos);
                if (rendVal == "italic" || rendVal == "weak") style |= kItalic;
                if (rendVal == "bold") style |= kBold;
            }
        }
    }
    
    // Check for type attribute on milestone (skip these)
    
    return style;
}

std::vector<Block> ParseXhtml(const std::string& xhtml) {
    std::vector<Block> blocks;
    Block currentBlock;
    currentBlock.kind = Block::Paragraph;
    std::string currentText;
    unsigned currentStyle = 0;
    Link currentLink;
    
    auto flushRun = [&]() {
        std::string text = trim(currentText);
        if (text.empty()) {
            // Still flush if we have a link (for Strong's links on otherwise-empty words)
            if (currentLink.kind != LinkKind::None) {
                Run r;
                r.text = ""; // Empty text but with a link
                r.style = currentStyle;
                r.link = currentLink;
                currentBlock.runs.push_back(r);
            }
        } else {
            text = decodeEntities(text);
            text = collapseWhitespace(text);
            if (!text.empty()) {
                Run r;
                r.text = std::move(text);
                r.style = currentStyle;
                r.link = currentLink;
                currentBlock.runs.push_back(r);
            }
        }
    };
    
    auto flushBlock = [&]() {
        if (!currentBlock.runs.empty()) {
            // Merge adjacent runs with same style+link
            std::vector<Run> merged;
            for (auto& r : currentBlock.runs) {
                if (!merged.empty() && merged.back().style == r.style &&
                    merged.back().link.kind == r.link.kind &&
                    merged.back().link.target == r.link.target) {
                    merged.back().text += r.text;
                } else {
                    merged.push_back(r);
                }
            }
            currentBlock.runs = std::move(merged);
            blocks.push_back(std::move(currentBlock));
        }
    };
    
    // Process the XHTML character by character
    size_t pos = 0;
    while (pos < xhtml.size()) {
        if (xhtml[pos] == '<') {
            // Find end of tag
            size_t tagEnd = xhtml.find('>', pos);
            if (tagEnd == std::string::npos) break;
            
            std::string tagContent = xhtml.substr(pos + 1, tagEnd - pos - 1);
            bool selfClosing = false;
            if (!tagContent.empty() && tagContent.back() == '/') {
                selfClosing = true;
            }
            
            // Check if closing tag
            std::string tagName;
            size_t nameStart = 0;
            if (!tagContent.empty() && tagContent[0] == '/') {
                tagName = tagContent.substr(1);
                nameStart = 1;
            } else {
                tagName = tagContent.substr(0);
                nameStart = 0;
            }
            size_t sp = tagName.find_first_of(" \t\n\r/>");
            tagName = (sp != std::string::npos) ? tagName.substr(0, sp) : tagName;
            
            // Flush current text run before processing tag
            flushRun();
            currentText.clear();
            currentStyle = 0;
            currentLink.kind = LinkKind::None;
            
            if (tagName == "p" || tagName == "div" || tagName == "head") {
                // Block element - flush current block and start new one
                flushBlock();
                currentBlock.kind = Block::Paragraph;
                if (tagName == "head") {
                    currentBlock.kind = Block::Heading;
                }
                pos = tagEnd + 1;
                // Skip to closing tag for non-self-closing block elements
                if (!selfClosing) {
                    std::string closingTagStr = "</" + tagName;
                    size_t ctag = xhtml.find(closingTagStr, pos);
                    if (ctag != std::string::npos) {
                        size_t ctend = xhtml.find('>', ctag);
                        if (ctend != std::string::npos) {
                            // Parse content between tags
                            std::string content = xhtml.substr(pos, ctag - pos);
                            // Recursively parse the content
                            auto subBlocks = ParseXhtml(content);
                            for (auto& sb : subBlocks) {
                                // Append sub-block content to current block
                                for (auto& sr : sb.runs) {
                                    currentBlock.runs.push_back(sr);
                                }
                            }
                            if (!subBlocks.empty() && !subBlocks.back().runs.empty()) {
                                currentBlock.kind = subBlocks.back().kind;
                            }
                            pos = ctend + 1;
                        } else {
                            pos = xhtml.size();
                        }
                    } else {
                        pos = xhtml.size();
                    }
                } else {
                    pos = tagEnd + 1;
                }
            }
            else if (tagName == "br" || tagName == "lb") {
                // Line break
                flushBlock();
                Block brBlock;
                brBlock.kind = Block::LineBreak;
                blocks.push_back(brBlock);
                pos = tagEnd + 1;
            }
            else if (tagName == "w") {
                // Word element - may contain Strong's numbers and morph tags
                // Check for strong:, savlm, lemma=, morph= attributes
                std::string strongs, morph;
                
                // Check for standalone strong: attribute (not inside another attribute's value)
                {
                    size_t spos = 0;
                    while ((spos = tagContent.find("strong:", spos)) != std::string::npos) {
                        // Ensure "strong:" is a standalone attribute, not inside a quoted value
                        // It must be preceded by whitespace or be at position 0
                        bool standalone = (spos == 0 || std::isspace(static_cast<unsigned char>(tagContent[spos - 1])));
                        if (standalone) {
                            size_t valStart = spos + 7;
                            size_t valEnd = tagContent.find_first_of(" \t\n\r/>", valStart);
                            if (valEnd == std::string::npos) valEnd = tagContent.size();
                            std::string val = tagContent.substr(valStart, valEnd - valStart);
                            if (!val.empty() && val[0] != ':') {
                                strongs = val;
                            }
                            break; // Take the first standalone strong: attribute
                        }
                        spos++;
                    }
                }
                
                // Check savlm attribute (e.g., savlm="strong:H07225")
                if (strongs.empty()) {
                    size_t spos = tagContent.find("savlm=");
                    if (spos != std::string::npos) {
                        spos += 6;
                        if (spos < tagContent.size() && (tagContent[spos] == '"' || tagContent[spos] == '\'')) {
                            char quote = tagContent[spos];
                            spos++;
                            size_t valEnd = tagContent.find(quote, spos);
                            if (valEnd != std::string::npos) {
                                std::string val = tagContent.substr(spos, valEnd - spos);
                                // Extract strong:X from "strong:H07225"
                                size_t sp = val.find("strong:");
                                if (sp != std::string::npos) {
                                    size_t vs = sp + 7;
                                    size_t ve = val.find_first_of(" \"'", vs);
                                    if (ve == std::string::npos) ve = val.size();
                                    strongs = val.substr(vs, ve - vs);
                                } else {
                                    strongs = val;
                                }
                            }
                        }
                    }
                }
                
                // Check lemma attribute (e.g., lemma="strong:H07225")
                if (strongs.empty()) {
                    size_t spos = tagContent.find("lemma=");
                    if (spos != std::string::npos) {
                        spos += 6;
                        if (spos < tagContent.size() && (tagContent[spos] == '"' || tagContent[spos] == '\'')) {
                            char quote = tagContent[spos];
                            spos++;
                            size_t valEnd = tagContent.find(quote, spos);
                            if (valEnd != std::string::npos) {
                                std::string val = tagContent.substr(spos, valEnd - spos);
                                size_t sp = val.find("strong:");
                                if (sp != std::string::npos) {
                                    size_t vs = sp + 7;
                                    size_t ve = val.find_first_of(" \"'", vs);
                                    if (ve == std::string::npos) ve = val.size();
                                    strongs = val.substr(vs, ve - vs);
                                } else {
                                    strongs = val;
                                }
                            }
                        }
                    }
                }
                
                // Parse morph attribute (like morph="robinson:V-PAI-3S")
                {
                    size_t spos = tagContent.find("morph=");
                    if (spos != std::string::npos) {
                        spos += 6;
                        if (spos < tagContent.size() && (tagContent[spos] == '"' || tagContent[spos] == '\'')) {
                            char quote = tagContent[spos];
                            spos++;
                            size_t valEnd = tagContent.find(quote, spos);
                            if (valEnd != std::string::npos) {
                                morph = tagContent.substr(spos, valEnd - spos);
                            }
                        }
                    }
                }
                
                unsigned style = parseInlineStyle(tagContent);
                
                // Parse href for cross-reference links
                size_t hrefPos = tagContent.find("href=");
                if (hrefPos != std::string::npos) {
                    hrefPos += 6;
                    if (hrefPos < xhtml.size() && xhtml[hrefPos] == '"') {
                        hrefPos++;
                        size_t hrefEnd = xhtml.find('"', hrefPos);
                        if (hrefEnd != std::string::npos) {
                            std::string href = xhtml.substr(hrefPos, hrefEnd - hrefPos);
                            // Determine link kind from href
                            if (href.find("Strong") != std::string::npos || 
                                href.find("strong") != std::string::npos) {
                                currentLink.kind = LinkKind::Strongs;
                                currentLink.target = href;
                            } else if (href.find("CrossReference") != std::string::npos) {
                                currentLink.kind = LinkKind::CrossRef;
                                currentLink.target = href;
                            } else {
                                currentLink.kind = LinkKind::Url;
                                currentLink.target = href;
                            }
                            pos = hrefEnd + 1; // Skip past the attribute
                        }
                    }
                }
                
                // Set strongs/morph link if present
                if (!strongs.empty()) {
                    currentLink.kind = LinkKind::Strongs;
                    currentLink.target = strongs;
                } else if (!morph.empty()) {
                    currentLink.kind = LinkKind::Morph;
                    currentLink.target = morph;
                }
                
                currentStyle = style;
                pos = tagEnd + 1;
                // Get text content
                if (!selfClosing) {
                    std::string closedTag = "/w>";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        currentText = xhtml.substr(pos, ctag - pos);
                        pos = ctag + 3;
                    } else {
                        currentText = "";
                        pos = xhtml.size();
                    }
                } else {
                    currentText = "";
                }
            }
            else if (tagName == "divineName") {
                currentStyle |= kDivineName;
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/divineName>";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        currentText = xhtml.substr(pos, ctag - pos);
                        pos = ctag + 11;
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
            else if (tagName == "transChange") {
                currentStyle |= kTransChange;
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/transChange>";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        currentText = xhtml.substr(pos, ctag - pos);
                        pos = ctag + 13;
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
            else if (tagName == "note") {
                // Footnote - extract note id from n attribute
                size_t nPos = tagContent.find("n=");
                if (nPos != std::string::npos) {
                    nPos += 2;
                    char quote = 0;
                    if (nPos < tagContent.size() && (tagContent[nPos] == '"' || tagContent[nPos] == '\'')) {
                        quote = tagContent[nPos];
                        nPos++;
                        size_t valEnd = tagContent.find(quote, nPos);
                        if (valEnd != std::string::npos) {
                            std::string noteId = tagContent.substr(nPos, valEnd - nPos);
                            currentLink.kind = LinkKind::Footnote;
                            currentLink.target = noteId;
                        }
                    }
                }
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/note>";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        currentText = xhtml.substr(pos, ctag - pos);
                        pos = ctag + 5;
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
            else if (tagName == "ref") {
                // Cross-reference link
                size_t targetPos = tagContent.find("target=");
                if (targetPos != std::string::npos) {
                    targetPos += 7;
                    if (targetPos < tagContent.size() && (tagContent[targetPos] == '"' || tagContent[targetPos] == '\'')) {
                        char quote = tagContent[targetPos];
                        targetPos++;
                        size_t valEnd = tagContent.find(quote, targetPos);
                        if (valEnd != std::string::npos) {
                            std::string target = tagContent.substr(targetPos, valEnd - targetPos);
                            currentLink.kind = LinkKind::CrossRef;
                            currentLink.target = target;
                        }
                    }
                }
                pos = tagEnd + 1;
            }
            else if (tagName == "milestone") {
                // Milestone (paragraph marker, etc.) - treat as line break
                // Check type attribute
                std::string typeVal;
                size_t tp = tagContent.find("type=");
                if (tp != std::string::npos) {
                    tp += 5;
                    if (tp < tagContent.size() && (tagContent[tp] == '"' || tagContent[tp] == '\'')) {
                        char q = tagContent[tp];
                        tp++;
                        size_t ve = tagContent.find(q, tp);
                        if (ve != std::string::npos) typeVal = tagContent.substr(tp, ve - tp);
                    }
                }
                if (typeVal == "x-p" || typeVal == "para") {
                    flushBlock();
                    Block mb;
                    mb.kind = Block::Paragraph; // Start new paragraph
                    blocks.push_back(std::move(mb));
                }
                pos = tagEnd + 1;
            }
            else if (tagName == "q") {
                // Quoted speech - just treat as inline, no special handling needed
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/q>";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        // Parse the inner content
                        auto subBlocks = ParseXhtml(xhtml.substr(pos, ctag - pos));
                        for (auto& sb : subBlocks) {
                            for (auto& sr : sb.runs) {
                                currentBlock.runs.push_back(sr);
                            }
                        }
                        pos = ctag + 3;
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
            else if (tagName == "sup") {
                currentStyle |= kSuperscript;
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/sup>";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        currentText = xhtml.substr(pos, ctag - pos);
                        pos = ctag + 5;
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
            else if (tagName == "em" || tagName == "italic") {
                currentStyle |= kItalic;
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/" + tagName + ">";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        currentText = xhtml.substr(pos, ctag - pos);
                        pos = ctag + 3 + tagName.size();
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
            else if (tagName == "b" || tagName == "bold") {
                currentStyle |= kBold;
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/" + tagName + ">";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        currentText = xhtml.substr(pos, ctag - pos);
                        pos = ctag + 3 + tagName.size();
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
            else if (tagName == "small") {
                currentStyle |= kSmall;
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/small>";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        currentText = xhtml.substr(pos, ctag - pos);
                        pos = ctag + 7;
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
            else if (tagName == "xref" || tagName == "crossReference") {
                // Cross-reference element
                size_t targetPos = tagContent.find("target=");
                if (targetPos != std::string::npos) {
                    targetPos += 7;
                    if (targetPos < tagContent.size() && (tagContent[targetPos] == '"' || tagContent[targetPos] == '\'')) {
                        char quote = tagContent[targetPos];
                        targetPos++;
                        size_t valEnd = tagContent.find(quote, targetPos);
                        if (valEnd != std::string::npos) {
                            std::string target = tagContent.substr(targetPos, valEnd - targetPos);
                            currentLink.kind = LinkKind::CrossRef;
                            currentLink.target = target;
                        }
                    }
                }
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/" + tagName + ">";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        currentText = xhtml.substr(pos, ctag - pos);
                        pos = ctag + 3 + tagName.size();
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
            else if (tagName == "chapter") {
                // Chapter end marker - skip
                pos = tagEnd + 1;
            }
            else if (tagName == "entryFree" || tagName == "def" || tagName == "orth" ||
                     tagName == "pron" || tagName == "lex" || tagName == "wg" ||
                     tagName == "lb" || tagName == "entry" || tagName == "headword" ||
                     tagName == "sense" || tagName == "def" || tagName == "see" ||
                     tagName == "seealso") {
                // Dictionary content tags
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/" + tagName + ">";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        // Parse inner content
                        auto subBlocks = ParseXhtml(xhtml.substr(pos, ctag - pos));
                        for (auto& sb : subBlocks) {
                            for (auto& sr : sb.runs) {
                                currentBlock.runs.push_back(sr);
                            }
                        }
                        pos = ctag + 3 + tagName.size();
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
            else {
                // Unknown tag - keep its text content, drop the tag
                pos = tagEnd + 1;
                if (!selfClosing) {
                    std::string closedTag = "/" + tagName + ">";
                    size_t ctag = xhtml.find(closedTag, pos);
                    if (ctag != std::string::npos) {
                        currentText = xhtml.substr(pos, ctag - pos);
                        pos = ctag + 3 + tagName.size();
                    } else {
                        pos = xhtml.size();
                    }
                }
            }
        } else {
            currentText += xhtml[pos];
            pos++;
        }
    }
    
    // Flush final text
    flushRun();
    flushBlock();
    
    return blocks;
}

std::string PlainText(const std::vector<Block>& blocks) {
    std::string result;
    bool first = true;
    for (const auto& block : blocks) {
        if (!first) result += '\n';
        first = false;
        if (block.kind == Block::LineBreak) {
            result += '\n';
        } else {
            for (const auto& run : block.runs) {
                result += run.text;
            }
        }
    }
    return result;
}

}
