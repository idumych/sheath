#pragma once

#include <Window.h>

#include <string>

class ChapterView;

// A small floating window showing a dictionary entry or a footnote. One is reused for all
// lookups so clicking through Strong's numbers doesn't pile up windows.
class EntryWindow : public BWindow {
public:
	static void Show(const std::string& title, const std::string& xhtml, float fontSize);

	~EntryWindow() override;

private:
	EntryWindow();

	static EntryWindow* sWindow;
	ChapterView* fText;
};
