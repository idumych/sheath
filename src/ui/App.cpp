#include <Alert.h>
#include <Application.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Screen.h>

#include "ReaderWindow.h"
#include "Sword.h"

namespace {

const char* kSignature = "application/x-vnd.Sheath";

BPath SettingsPath()
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path, true) == B_OK)
		path.Append("Sheath_settings");
	return path;
}

}	// namespace


class SheathApp : public BApplication {
public:
	SheathApp() : BApplication(kSignature), fLibrary("") {}

	void ReadyToRun() override
	{
		BMessage settings;
		BFile file(SettingsPath().Path(), B_READ_ONLY);
		if (file.InitCheck() == B_OK)
			settings.Unflatten(&file);

		ReaderState state;
		state.module = settings.GetString("module", "");
		state.parallel = settings.GetString("parallel", "");
		state.book = settings.GetString("book", "John");
		state.chapter = settings.GetInt32("chapter", 1);
		state.verse = settings.GetInt32("verse", 0);
		state.fontSize = settings.GetFloat("fontSize", 0);
		state.options.strongs = settings.GetBool("Strong's numbers", false);
		state.options.footnotes = settings.GetBool("Footnotes", true);
		state.options.headings = settings.GetBool("Headings", true);
		state.options.redLetter = settings.GetBool("Words of Christ in red", true);

		BRect frame = settings.GetRect("frame", BRect(80, 60, 720, 700));
		if (!BScreen().Frame().Intersects(frame))
			frame = BRect(80, 60, 720, 700);
		BPath bookmarks;
		find_directory(B_USER_SETTINGS_DIRECTORY, &bookmarks, true);
		bookmarks.Append("Sheath_bookmarks");
		(new ReaderWindow(frame, &fLibrary, state, bookmarks.Path()))->Show();
	}

	void AboutRequested() override
	{
		BAlert* about = new BAlert("About Sheath", "Sheath 0.1.0\n\n"
			"A Bible reader built on the SWORD library from CrossWire. Inspired by AndBible "
			"and LinBible.\n\nGPL-3.0. https://github.com/idumych/sheath", "OK");
		about->Go(nullptr);
	}

private:
	sheath::SwordLibrary fLibrary;
};


int main()
{
	SheathApp app;
	app.Run();
	return 0;
}
