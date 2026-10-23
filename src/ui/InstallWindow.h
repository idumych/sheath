#pragma once

#include <Messenger.h>
#include <Window.h>

#include <string>
#include <thread>
#include <vector>

#include "Installer.h"

class BButton;
class BListView;
class BMenuField;
class BStringView;
class BTextControl;

// Download, install and remove SWORD modules. Network work runs on a worker thread, one job at
// a time; when modules were installed or removed the window sends kMsgLibraryChanged to the
// reader.
class InstallWindow : public BWindow {
public:
	static const uint32 kMsgLibraryChanged = 'libc';

	InstallWindow(const std::string& userDir, BMessenger reader);
	~InstallWindow() override;

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	enum Job { kRefresh, kInstall, kRemove };

	void StartJob(Job job, const std::string& module = "");
	void JobDone(BMessage* message);
	void FillList();
	void UpdateButtons();
	std::string SelectedModule() const;
	std::string Source() const;

	sheath::Installer fInstaller;
	BMessenger fReader;
	std::thread fWorker;
	bool fBusy = false;
	std::vector<sheath::RemoteModule> fModules;
	std::string fLanguage = "en";		// preferred; "" = all languages

	BMenuField* fSourceField;
	BMenuField* fLanguageField;
	BMenuField* fTypeField;
	BTextControl* fFilter;
	BListView* fList;
	BButton* fRefresh;
	BButton* fInstall;
	BButton* fRemove;
	BStringView* fStatus;
};
