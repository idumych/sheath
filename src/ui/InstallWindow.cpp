#include "InstallWindow.h"

#include <Alert.h>
#include <Button.h>
#include <File.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>

#include <algorithm>
#include <cctype>
#include <set>

using namespace sheath;

namespace {

enum {
	kMsgRefresh = 'rfsh',
	kMsgInstall = 'inst',
	kMsgRemove = 'remv',
	kMsgFilter = 'filt',
	kMsgSelect = 'slct',
	kMsgJobDone = 'jobd',
};

const char* kTypes[] = {"All types", "Bible", "Commentary", "Dictionary", "Book"};

std::string Lower(std::string s)
{
	for (char& c : s)
		c = (char)std::tolower((unsigned char)c);
	return s;
}

class ModuleItem : public BStringItem {
public:
	ModuleItem(const RemoteModule& m)
		:
		BStringItem(""),
		fModule(m)
	{
		std::string text = m.name + " \xE2\x80\x94 " + m.description + "   (" + m.language;
		if (!m.version.empty())
			text += ", " + m.version;
		text += ")";
		if (!m.installedVersion.empty()) {
			text += m.installedVersion == m.version || m.version.empty()
				? "   \xE2\x9C\x93 installed" : "   \xE2\x86\x91 update available";
		}
		SetText(text.c_str());
	}

	const RemoteModule& Module() const { return fModule; }

private:
	RemoteModule fModule;
};

// The user agrees once to download from the internet; remembered in the settings folder.
bool NetworkConfirmed()
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return false;
	path.Append("Sheath_network_ok");
	if (BEntry(path.Path()).Exists())
		return true;
	BAlert* alert = new BAlert("Download modules",
		"Sheath downloads module lists and modules from CrossWire (crosswire.org) over the "
		"internet. Some modules have licence terms; you can read them in each module's "
		"description.\n\nContinue?", "Cancel", "Download", nullptr, B_WIDTH_AS_USUAL,
		B_INFO_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	if (alert->Go() != 1)
		return false;
	BFile(path.Path(), B_WRITE_ONLY | B_CREATE_FILE);
	return true;
}

}	// namespace


InstallWindow::InstallWindow(const std::string& userDir, BMessenger reader)
	:
	BWindow(BRect(140, 90, 760, 590), "Modules", B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fInstaller(userDir, userDir + "/InstallMgr"),
	fReader(reader)
{
	BPopUpMenu* sources = new BPopUpMenu("source");
	for (const std::string& s : fInstaller.Sources())
		sources->AddItem(new BMenuItem(s.c_str(), new BMessage(kMsgFilter)));
	if (sources->CountItems() > 0)
		sources->ItemAt(0)->SetMarked(true);
	fSourceField = new BMenuField("source", "Source:", sources);

	BPopUpMenu* languages = new BPopUpMenu("language");
	fLanguageField = new BMenuField("language", nullptr, languages);
	BPopUpMenu* types = new BPopUpMenu("type");
	for (const char* t : kTypes)
		types->AddItem(new BMenuItem(t, new BMessage(kMsgFilter)));
	types->ItemAt(1)->SetMarked(true);
	fTypeField = new BMenuField("type", nullptr, types);
	fFilter = new BTextControl("filter", nullptr, "", nullptr);
	fFilter->SetModificationMessage(new BMessage(kMsgFilter));
	fFilter->SetToolTip("Filter by name or description");

	fList = new BListView("modules");
	fList->SetSelectionMessage(new BMessage(kMsgSelect));
	fList->SetInvocationMessage(new BMessage(kMsgInstall));
	BScrollView* scroll = new BScrollView("scroll", fList, 0, false, true);

	fRefresh = new BButton("refresh", "Refresh list", new BMessage(kMsgRefresh));
	fInstall = new BButton("install", "Install", new BMessage(kMsgInstall));
	fRemove = new BButton("remove", "Remove", new BMessage(kMsgRemove));
	fStatus = new BStringView("status", "");

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_HALF_ITEM_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
			.Add(fSourceField)
			.Add(fRefresh)
			.AddGlue()
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
			.Add(fLanguageField)
			.Add(fTypeField)
			.Add(fFilter, 2.0f)
		.End()
		.Add(scroll, 10.0f)
		.AddGroup(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
			.Add(fStatus, 3.0f)
			.Add(fRemove)
			.Add(fInstall)
		.End();

	fModules = fInstaller.Available(Source());
	FillList();
	if (fModules.empty())
		fStatus->SetText("Press \"Refresh list\" to download the list of modules.");
}


InstallWindow::~InstallWindow()
{
	if (fWorker.joinable())
		fWorker.join();
}


std::string InstallWindow::Source() const
{
	BMenuItem* item = fSourceField->Menu()->FindMarked();
	return item ? item->Label() : "";
}


std::string InstallWindow::SelectedModule() const
{
	ModuleItem* item = dynamic_cast<ModuleItem*>(fList->ItemAt(fList->CurrentSelection()));
	return item ? item->Module().name : "";
}


void InstallWindow::FillList()
{
	// Languages present in the list; keep the user's choice if it's still there.
	BMenu* langMenu = fLanguageField->Menu();
	std::string chosen = langMenu->FindMarked() ? langMenu->FindMarked()->Label() : "en";
	std::set<std::string> languages;
	for (const RemoteModule& m : fModules)
		languages.insert(m.language.empty() ? "?" : m.language);
	while (BMenuItem* item = langMenu->RemoveItem((int32)0))
		delete item;
	langMenu->AddItem(new BMenuItem("All languages", new BMessage(kMsgFilter)));
	for (const std::string& l : languages)
		langMenu->AddItem(new BMenuItem(l.c_str(), new BMessage(kMsgFilter)));
	BMenuItem* mark = langMenu->FindItem(chosen.c_str());
	(mark ? mark : langMenu->ItemAt(0))->SetMarked(true);

	std::string lang = langMenu->FindMarked()->Label();
	BMenuItem* typeItem = fTypeField->Menu()->FindMarked();
	std::string type = typeItem ? typeItem->Label() : kTypes[0];
	std::string filter = Lower(fFilter->Text());
	std::string selected = SelectedModule();

	while (BListItem* item = fList->RemoveItem((int32)0))
		delete item;
	BList items;
	int selectIndex = -1;
	for (const RemoteModule& m : fModules) {
		if (lang != "All languages" && (m.language.empty() ? "?" : m.language) != lang)
			continue;
		if (type != kTypes[0] && m.type != type)
			continue;
		if (!filter.empty() && Lower(m.name).find(filter) == std::string::npos
				&& Lower(m.description).find(filter) == std::string::npos)
			continue;
		if (m.name == selected)
			selectIndex = items.CountItems();
		items.AddItem(new ModuleItem(m));
	}
	fList->AddList(&items);
	if (selectIndex >= 0)
		fList->Select(selectIndex);
	UpdateButtons();
}


void InstallWindow::UpdateButtons()
{
	ModuleItem* item = dynamic_cast<ModuleItem*>(fList->ItemAt(fList->CurrentSelection()));
	bool installed = item && !item->Module().installedVersion.empty();
	bool update = installed && !item->Module().version.empty()
		&& item->Module().version != item->Module().installedVersion;
	fRefresh->SetEnabled(!fBusy);
	fInstall->SetEnabled(!fBusy && item && (!installed || update));
	fInstall->SetLabel(update ? "Update" : "Install");
	fRemove->SetEnabled(!fBusy && installed);
	fSourceField->SetEnabled(!fBusy);
}


void InstallWindow::StartJob(Job job, const std::string& module)
{
	if (fBusy || (job != kRemove && !NetworkConfirmed()))
		return;
	if (fWorker.joinable())
		fWorker.join();
	fBusy = true;
	UpdateButtons();
	std::string source = Source();
	const char* what = job == kRefresh ? "Downloading the module list"
		: job == kInstall ? "Installing " : "Removing ";
	fStatus->SetText((std::string(what) + module + "\xE2\x80\xA6").c_str());

	BMessenger self(this);
	fWorker = std::thread([this, self, job, source, module]() {
		std::string error;
		bool ok = false;
		if (job == kRefresh)
			ok = fInstaller.RefreshSource(source, &error);
		else if (job == kInstall)
			ok = fInstaller.Install(source, module, &error);
		else
			ok = fInstaller.Remove(module, &error);
		BMessage done(kMsgJobDone);
		done.AddInt32("job", job);
		done.AddBool("ok", ok);
		done.AddString("module", module.c_str());
		done.AddString("error", error.c_str());
		self.SendMessage(&done);
	});
}


void InstallWindow::JobDone(BMessage* message)
{
	fBusy = false;
	int32 job = message->GetInt32("job", kRefresh);
	bool ok = message->GetBool("ok", false);
	std::string module = message->GetString("module", "");
	std::string error = message->GetString("error", "");
	fModules = fInstaller.Available(Source());
	FillList();

	std::string status;
	if (!ok)
		status = error.empty() ? "Failed." : error;
	else if (job == kRefresh)
		status = std::to_string(fModules.size()) + " modules available.";
	else
		status = module + (job == kInstall ? " installed." : " removed.");
	fStatus->SetText(status.c_str());
	if (ok && job != kRefresh)
		fReader.SendMessage(kMsgLibraryChanged);
}


void InstallWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgRefresh:
			StartJob(kRefresh);
			break;
		case kMsgInstall:
			if (!SelectedModule().empty())
				StartJob(kInstall, SelectedModule());
			break;
		case kMsgRemove:
			if (!SelectedModule().empty())
				StartJob(kRemove, SelectedModule());
			break;
		case kMsgFilter:
			if (!fBusy) {
				fModules = fInstaller.Available(Source());
				FillList();
			}
			break;
		case kMsgSelect:
			UpdateButtons();
			break;
		case kMsgJobDone:
			JobDone(message);
			break;
		default:
			BWindow::MessageReceived(message);
	}
}


bool InstallWindow::QuitRequested()
{
	if (fBusy) {
		fStatus->SetText("Please wait until the download finishes.");
		return false;
	}
	return true;
}
