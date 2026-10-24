#include "Installer.h"

#include <installmgr.h>
#include <swmgr.h>
#include <swconfig.h>
#include <filemgr.h>
#include <remotetrans.h>
#include <swmodule.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace sheath {

namespace {

class MyInstallMgr : public sword::InstallMgr {
public:
	explicit MyInstallMgr(const char* privatePath, sword::StatusReporter* sr = nullptr)
		: sword::InstallMgr(privatePath, sr) {}

	bool isUserDisclaimerConfirmed() const override { return true; }

	// SWORD (1.8.1 and 1.9.0) never initializes RemoteTransport::timeoutMillis and only sets it
	// for FTP sources, so HTTP(S) downloads ran with whatever was in memory: 0 or a huge value
	// worked, 48 failed every transfer ("code -9") and others let a stalled connection hang.
	sword::RemoteTransport* createHTTPTransport(const char* host,
		sword::StatusReporter* statusReporter) override
	{
		sword::RemoteTransport* transport = sword::InstallMgr::createHTTPTransport(host,
			statusReporter);
		if (transport != nullptr)
			transport->setTimeoutMillis(kTimeoutMillis);
		return transport;
	}

	static const long kTimeoutMillis = 30000;
};

} // anonymous namespace

struct Installer::Impl {
	std::string userDir, configDir;
	sword::InstallMgr* mgr = nullptr;
	sword::SWMgr* userMgr = nullptr;

	Impl(const std::string& u, const std::string& c) : userDir(u), configDir(c) {
		// SWMgr only treats a directory as a library if mods.d/ and modules/ exist.
		// (createParent makes the directories leading up to the last path component.)
		sword::FileMgr::createParent((userDir + "/mods.d/x").c_str());
		sword::FileMgr::createParent((userDir + "/modules/x").c_str());

		std::string confPath = configDir + "/InstallMgr.conf";
		if (!sword::FileMgr::existsFile(confPath.c_str())) {
			sword::FileMgr::createParent(confPath.c_str());
			sword::SWConfig conf(confPath.c_str());
			conf["General"]["PassiveFTP"] = "true";
			conf["Sources"]["HTTPSSource"] = "CrossWire|crosswire.org|/ftpmirror/pub/sword/raw";
			conf.save();
		}

		mgr = new MyInstallMgr(configDir.c_str(), nullptr);
		userMgr = new sword::SWMgr(userDir.c_str());
	}

	void ReloadUser() {
		delete userMgr;
		userMgr = new sword::SWMgr(userDir.c_str());
	}

	~Impl() {
		delete userMgr;
		delete mgr;
	}
};

Installer::Installer(const std::string& userDir, const std::string& configDir)
		: d(new Impl(userDir, configDir)) {}
Installer::~Installer() { delete d; }

std::vector<std::string> Installer::Sources() const {
	d->mgr->readInstallConf();
	std::vector<std::string> result;
	for (auto& entry : d->mgr->sources)
		result.push_back(entry.first.c_str());
	std::sort(result.begin(), result.end());
	return result;
}

bool Installer::RefreshSource(const std::string& source, std::string* error) {
	auto it = d->mgr->sources.find(source.c_str());
	if (it == d->mgr->sources.end()) {
		if (error) *error = "No source '" + source + "'";
		return false;
	}
	int status = d->mgr->refreshRemoteSource(it->second);
	if (status != 0) {
		if (error) *error = "Couldn't download the module list from " + source + " (code "
			+ std::to_string(status) + ")";
		return false;
	}
	return true;
}

std::vector<RemoteModule> Installer::Available(const std::string& source) const {
	auto it = d->mgr->sources.find(source.c_str());
	if (it == d->mgr->sources.end())
		return {};

	std::vector<RemoteModule> result;

	for (auto& mod : it->second->getMgr()->getModules()) {
		RemoteModule m;
		m.name = mod.second->getName();
		m.description = mod.second->getDescription() ? mod.second->getDescription() : "";
		std::string t = mod.second->getType();
		if (t.find("Bibl") != std::string::npos) m.type = "Bible";
		else if (t.find("Commentar") != std::string::npos) m.type = "Commentary";
		else if (t.find("Lexicon") != std::string::npos || t.find("Dictionar") != std::string::npos)
			m.type = "Dictionary";
		else m.type = "Book";
		m.language = mod.second->getLanguage() ? mod.second->getLanguage() : "";
		const char* v = mod.second->getConfigEntry("Version");
		m.version = v ? v : "";
		m.installedVersion = "";

		auto* um = d->userMgr->getModule(m.name.c_str());
		if (um) {
			const char* iv = um->getConfigEntry("Version");
			m.installedVersion = iv ? iv : "1.0";
		}

		result.push_back(m);
	}
	std::sort(result.begin(), result.end(),
		[](const RemoteModule& a, const RemoteModule& b) { return a.name < b.name; });
	return result;
}

bool Installer::Install(const std::string& source, const std::string& module, std::string* error) {
	auto it = d->mgr->sources.find(source.c_str());
	if (it == d->mgr->sources.end()) {
		if (error) *error = "No source '" + source + "'";
		return false;
	}
	if (!it->second->getMgr()->getModule(module.c_str())) {
		if (error) *error = "No module '" + module + "' in " + source;
		return false;
	}
	int status = d->mgr->installModule(d->userMgr, nullptr, module.c_str(), it->second);
	d->ReloadUser();
	if (status != 0) {
		if (error) *error = "Couldn't install " + module + " (code " + std::to_string(status) + ")";
		return false;
	}
	return true;
}

bool Installer::InstallFromDirectory(const std::string& dir, const std::string& module, std::string* error) {
	sword::SWMgr srcMgr(dir.c_str());
	if (!srcMgr.getModule(module.c_str())) {
		if (error) *error = "No module '" + module + "' in " + dir;
		return false;
	}
	int status = d->mgr->installModule(d->userMgr, dir.c_str(), module.c_str());
	d->ReloadUser();
	if (status != 0) {
		if (error) *error = "Couldn't install " + module + " (code " + std::to_string(status) + ")";
		return false;
	}
	return true;
}

bool Installer::Remove(const std::string& module, std::string* error) {
	if (!d->userMgr->getModule(module.c_str())) {
		if (error) *error = "Module '" + module + "' not installed";
		return false;
	}
	int status = d->mgr->removeModule(d->userMgr, module.c_str());
	d->ReloadUser();
	if (status != 0) {
		if (error) *error = "Couldn't remove " + module + " (code " + std::to_string(status) + ")";
		return false;
	}
	return true;
}

std::vector<std::string> Installer::Installed() const {
	sword::SWMgr userMgr(d->userDir.c_str());
	std::vector<std::string> result;
	for (auto& entry : userMgr.getModules())
		result.push_back(entry.first.c_str());
	std::sort(result.begin(), result.end());
	return result;
}

} // namespace sheath
