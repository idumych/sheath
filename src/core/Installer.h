#pragma once
#include <string>
#include <vector>
namespace sheath {

struct RemoteModule {
	std::string name;             // "KJV"
	std::string description;      // conf Description
	std::string type;             // "Bible", "Commentary", "Dictionary" or "Book" (as ModuleInfo)
	std::string language;         // conf Lang
	std::string version;          // conf Version, "" if none
	std::string installedVersion; // version installed in the user dir, "" if not installed
};

// Installs and removes SWORD modules with sword::InstallMgr.
//   userDir:    the SWORD library modules go into (mods.d/ + modules/), e.g. ~/.sword.
//               Created if missing. SwordLibrary(userDir) sees what was installed.
//   configDir:  InstallMgr's own state (InstallMgr.conf, downloaded source lists),
//               e.g. userDir + "/InstallMgr". Created if missing.
// The user has agreed to download from the network by using the installer, so the
// InstallMgr disclaimer is confirmed. Every method returns false/empty on failure and,
// where there is an error parameter, sets it to a one-line human readable message.
class Installer {
public:
	Installer(const std::string& userDir, const std::string& configDir);
	~Installer();

	// Configured remote source captions, sorted. On first use (no InstallMgr.conf yet) the
	// CrossWire source is configured: caption "CrossWire", HTTPS, crosswire.org,
	// /ftpmirror/pub/sword/raw.
	std::vector<std::string> Sources() const;

	// Downloads a source's module list (network).
	bool RefreshSource(const std::string& source, std::string* error);

	// Modules in a source's downloaded list, sorted by name. Empty before RefreshSource.
	std::vector<RemoteModule> Available(const std::string& source) const;

	// Installs one module from a remote source (network) into userDir.
	bool Install(const std::string& source, const std::string& module, std::string* error);

	// Installs one module from a local SWORD tree (a directory with mods.d/ and modules/).
	bool InstallFromDirectory(const std::string& dir, const std::string& module, std::string* error);

	// Removes an installed module (its conf and data) from userDir.
	bool Remove(const std::string& module, std::string* error);

	// Names of the modules installed in userDir, sorted.
	std::vector<std::string> Installed() const;

private:
	struct Impl;
	Impl* d;
};
}
