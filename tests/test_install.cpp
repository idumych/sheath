// Spec tests for Installer (src/core/Installer.h). Written by Claude; the implementation
// must make these pass without changing them. The network part (CrossWire) is skipped when
// SHEATH_OFFLINE=1.
#include "check.h"
#include "Installer.h"
#include "Sword.h"

#include <algorithm>
#include <sys/stat.h>

using namespace sheath;

static bool exists(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

static bool contains(const std::vector<std::string>& v, const std::string& s)
{
	return std::find(v.begin(), v.end(), s) != v.end();
}

static void test_local(const std::string& user, const std::string& config)
{
	Installer inst(user, config);
	check_true(inst.Installed().empty(), "local: fresh user dir has no modules");

	std::string error;
	bool ok = inst.InstallFromDirectory(fixture_dir(), "KJV", &error);
	check_true(ok, "local: install KJV from the fixture tree", error);
	check_true(exists(user + "/mods.d/kjv.conf"), "local: kjv.conf in userDir/mods.d");
	check_true(contains(inst.Installed(), "KJV") && inst.Installed().size() == 1,
		"local: Installed() == {KJV}");

	SwordLibrary lib(user);
	check_true(lib.Find("KJV"), "local: SwordLibrary(userDir) finds KJV");
	DisplayOptions o;
	auto ch = lib.Chapter("KJV", "John", 3, o);
	check_eq(ch.size(), 36u, "local: installed KJV renders John 3 (36 verses)");

	error.clear();
	check_true(!inst.InstallFromDirectory(fixture_dir(), "NoSuchModule", &error),
		"local: installing a missing module fails");
	check_true(!error.empty(), "local: ... with an error message");

	ok = inst.InstallFromDirectory(fixture_dir(), "StrongsGreek", &error);
	check_true(ok, "local: install StrongsGreek", error);
	auto installed = inst.Installed();
	check_true(installed.size() == 2 && installed[0] == "KJV" && installed[1] == "StrongsGreek",
		"local: Installed() sorted {KJV, StrongsGreek}");

	ok = inst.Remove("KJV", &error);
	check_true(ok, "local: remove KJV", error);
	check_true(!exists(user + "/mods.d/kjv.conf"), "local: kjv.conf gone");
	check_true(!exists(user + "/modules/texts/ztext/kjv"), "local: KJV data dir gone");
	check_true(contains(inst.Installed(), "StrongsGreek") && !contains(inst.Installed(), "KJV"),
		"local: StrongsGreek still installed");
	SwordLibrary after(user);
	check_true(!after.Find("KJV") && after.Find("StrongsGreek"), "local: library sees the removal");

	error.clear();
	check_true(!inst.Remove("KJV", &error) && !error.empty(), "local: removing again fails with a message");
	check_true(exists(fixture_dir() + "/mods.d/kjv.conf"), "local: the source tree is untouched");
}

static void test_remote(const std::string& user, const std::string& config)
{
	Installer inst(user, config);
	auto sources = inst.Sources();
	check_true(contains(sources, "CrossWire"), "remote: CrossWire source configured by default");
	check_true(exists(config + "/InstallMgr.conf"), "remote: InstallMgr.conf written in configDir");

	std::string error;
	bool ok = inst.RefreshSource("CrossWire", &error);
	check_true(ok, "remote: refresh CrossWire", error);
	auto avail = inst.Available("CrossWire");
	check_true(avail.size() > 100, "remote: CrossWire lists more than 100 modules",
		std::to_string(avail.size()));
	bool sorted = std::is_sorted(avail.begin(), avail.end(),
		[](const RemoteModule& a, const RemoteModule& b) { return a.name < b.name; });
	check_true(sorted, "remote: Available() sorted by name");
	const RemoteModule* asv = nullptr;
	for (const RemoteModule& m : avail) {
		if (m.name == "ASV")
			asv = &m;
	}
	check_true(asv != nullptr, "remote: ASV is listed");
	if (asv != nullptr) {
		check_eq(asv->type, std::string("Bible"), "remote: ASV type");
		check_eq(asv->language, std::string("en"), "remote: ASV language");
		check_true(!asv->version.empty(), "remote: ASV has a version");
		check_eq(asv->installedVersion, std::string(""), "remote: ASV not installed yet");
	}

	ok = inst.Install("CrossWire", "ASV", &error);
	check_true(ok, "remote: install ASV", error);
	SwordLibrary lib(user);
	check_true(lib.Find("ASV"), "remote: library finds ASV");
	for (const RemoteModule& m : inst.Available("CrossWire")) {
		if (m.name == "ASV")
			check_eq(m.installedVersion, m.version, "remote: ASV installedVersion == version");
	}
	error.clear();
	check_true(!inst.Install("CrossWire", "NoSuchModule", &error) && !error.empty(),
		"remote: installing an unknown module fails with a message");
	check_true(!inst.RefreshSource("NoSuchSource", &error), "remote: unknown source fails");
}

int main()
{
	std::string tmp = scratch_dir();
	test_local(tmp + "/local-user", tmp + "/local-user/InstallMgr");
	const char* offline = std::getenv("SHEATH_OFFLINE");
	if (offline == nullptr || std::string(offline) != "1")
		test_remote(tmp + "/remote-user", tmp + "/remote-user/InstallMgr");
	return check_result();
}
