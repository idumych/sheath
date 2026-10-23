#include "Bookmarks.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace sheath {

namespace {

std::string Escape(const std::string& s)
{
	std::string out;
	for (char c : s) {
		switch (c) {
			case '\\': out += "\\\\"; break;
			case '\t': out += "\\t"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			default: out += c;
		}
	}
	return out;
}

std::string Unescape(const std::string& s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] != '\\' || i + 1 == s.size()) {
			out += s[i];
			continue;
		}
		char c = s[++i];
		out += c == 't' ? '\t' : c == 'n' ? '\n' : c == 'r' ? '\r' : c;
	}
	return out;
}

bool ParseInt(const std::string& s, long long* out)
{
	if (s.empty())
		return false;
	char* end = nullptr;
	long long v = std::strtoll(s.c_str(), &end, 10);
	if (*end != '\0')
		return false;
	*out = v;
	return true;
}

}	// namespace


BookmarkStore::BookmarkStore(const std::string& path)
	:
	fPath(path)
{
	std::ifstream in(path, std::ios::binary);
	std::string line;
	while (std::getline(in, line)) {
		// book \t chapter \t verse \t note \t created
		std::vector<std::string> f;
		size_t start = 0;
		for (;;) {
			size_t tab = line.find('\t', start);
			f.push_back(line.substr(start, tab == std::string::npos ? std::string::npos : tab - start));
			if (tab == std::string::npos)
				break;
			start = tab + 1;
		}
		long long chapter, verse, created;
		if (f.size() != 5 || f[0].empty() || !ParseInt(f[1], &chapter) || !ParseInt(f[2], &verse)
				|| !ParseInt(f[4], &created) || chapter < 1 || verse < 0)
			continue;
		Bookmark b;
		b.book = f[0];
		b.chapter = (int)chapter;
		b.verse = (int)verse;
		b.note = Unescape(f[3]);
		b.created = created;
		if (Find(b.book, b.chapter, b.verse) == nullptr)
			fItems.push_back(b);
	}
}


const Bookmark* BookmarkStore::Find(const std::string& book, int chapter, int verse) const
{
	for (const Bookmark& b : fItems) {
		if (b.book == book && b.chapter == chapter && b.verse == verse)
			return &b;
	}
	return nullptr;
}


void BookmarkStore::Set(const Bookmark& bookmark)
{
	for (Bookmark& b : fItems) {
		if (b.book == bookmark.book && b.chapter == bookmark.chapter && b.verse == bookmark.verse) {
			b.note = bookmark.note;
			return;
		}
	}
	fItems.push_back(bookmark);
}


bool BookmarkStore::Remove(const std::string& book, int chapter, int verse)
{
	for (size_t i = 0; i < fItems.size(); i++) {
		const Bookmark& b = fItems[i];
		if (b.book == book && b.chapter == chapter && b.verse == verse) {
			fItems.erase(fItems.begin() + i);
			return true;
		}
	}
	return false;
}


bool BookmarkStore::Save(std::string* error) const
{
	std::string tmp = fPath + ".tmp";
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		if (!out) {
			if (error)
				*error = "Can't write " + tmp + ": " + std::strerror(errno);
			return false;
		}
		for (const Bookmark& b : fItems) {
			out << b.book << '\t' << b.chapter << '\t' << b.verse << '\t' << Escape(b.note) << '\t'
				<< b.created << '\n';
		}
		out.flush();
		if (!out) {
			if (error)
				*error = "Can't write " + tmp;
			std::remove(tmp.c_str());
			return false;
		}
	}
	if (std::rename(tmp.c_str(), fPath.c_str()) != 0) {
		if (error)
			*error = "Can't replace " + fPath + ": " + std::strerror(errno);
		std::remove(tmp.c_str());
		return false;
	}
	return true;
}

}	// namespace sheath
