#include <stdio.h>
#include <string.h>
#define AE_UPDATER_PICK_TEST
#include "../src/updater.c"
#include "../src/ae_update.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void none(const char *json)
{
	char version[32] = "unchanged";

	CHECK(!updater_pick_release(json, version, sizeof(version)));
	CHECK(!version[0]);
}

/* the version order (updater_newer's): numbers, then no suffix after the
ones with, then the suffixes in order */
static void order(void)
{
	CHECK(updater_version_compare("0.4.3-beta", "0.4.2-beta") > 0);
	CHECK(updater_version_compare("0.4.2-beta", "0.4.3-beta") < 0);
	CHECK(updater_version_compare("0.4.2-beta", "0.4.2-beta") == 0);
	CHECK(updater_version_compare("v0.4.3-beta", "0.4.2-beta") > 0);
	CHECK(updater_version_compare("0.4.10-beta", "0.4.9-beta") > 0);
	CHECK(updater_version_compare("0.5.0-beta", "0.4.12-beta") > 0);
	CHECK(updater_version_compare("1.0.0", "0.9.9-beta") > 0);
	/* (a hotfix of the same three numbers: 0.3.9.1-beta after 0.3.9-beta) */
	CHECK(updater_version_compare("0.3.9.1-beta", "0.3.9-beta") > 0);
	/* (a release without a suffix after its betas) */
	CHECK(updater_version_compare("0.4.3", "0.4.3-beta") > 0);
	CHECK(updater_version_compare("0.4.3-beta", "0.4.3") < 0);
	/* (not a version: nothing is newer) */
	CHECK(updater_version_compare("map-pack-1", "0.4.2-beta") == 0);
	CHECK(updater_version_compare("0.4", "0.4.2-beta") == 0);
	CHECK(updater_version_compare("0.4.3-beta", "dev") == 0);
	/* (AE: a suffix's numbers by their value, not their text) */
	CHECK(updater_version_compare("1.0.0-rc10", "1.0.0-rc9") > 0);
	CHECK(updater_version_compare("1.0.0-rc9", "1.0.0-rc10") < 0);
	CHECK(updater_version_compare("0.3.9.10-beta", "0.3.9.9-beta") > 0);
	CHECK(updater_version_compare("0.3.9.2-beta", "0.3.9.10-beta") < 0);
	CHECK(updater_version_compare("1.0.0-rc010", "1.0.0-rc10") == 0);
	CHECK(updater_version_compare("0.4.3-beta2", "0.4.3-beta") > 0);
}

/* GitHub's answer as it is: an author object and assets with uploaders
between the tag and the draft flag, the newest release first, a map pack's
release, a draft newer than all, notes that quote a tag */
static void shaped_like_github(void)
{
	char version[32];
	const char *releases =
		"[\n  {\n    \"url\": \"https://api.github.com/repos/o/r/releases/3\",\n"
		"    \"author\": {\"login\": \"someone\", \"id\": 1, \"type\": \"User\"},\n"
		"    \"node_id\": \"RE_x\",\n    \"tag_name\": \"v9.9.9-beta\",\n    \"name\": \"draft\",\n"
		"    \"draft\": true,\n    \"prerelease\": true,\n    \"assets\": []\n  },\n"
		"  {\n    \"author\": {\"login\": \"someone\"},\n    \"tag_name\": \"v0.4.3-beta\",\n"
		"    \"target_commitish\": \"main\",\n    \"name\": \"0.4.3-beta\",\n    \"draft\": false,\n"
		"    \"prerelease\": true,\n    \"assets\": [{\"name\": \"updates.json\", \"uploader\": {\"login\": \"someone\"}}],\n"
		"    \"body\": \"notes with a \\\"tag_name\\\": \\\"v99.0.0\\\" quoted in them\"\n  },\n"
		"  {\n    \"tag_name\": \"map-pack-1\",\n    \"draft\": false,\n    \"prerelease\": false\n  },\n"
		"  {\n    \"tag_name\": \"v0.4.2-beta\",\n    \"draft\": false,\n    \"prerelease\": true\n  }\n]\n";

	CHECK(updater_pick_release(releases, version, sizeof(version)));
	CHECK(!strcmp(version, "0.4.3-beta"));
}

/* which entries of a release's zip are installed (ae_update.h) */
static void entries(void)
{
	/* (the game's files at the top, as before) */
	CHECK(ae_update_entry_allowed("halo"));
	CHECK(ae_update_entry_allowed("halo.exe"));
	CHECK(ae_update_entry_allowed("libSDL3.so.0"));
	CHECK(ae_update_entry_allowed("70-ae-dualsense.rules"));
	CHECK(ae_update_entry_allowed("GPL-3.0.txt"));
	/* (AE's folders: packs, voices/<voice>, sounds) */
	CHECK(ae_update_entry_allowed("packs/ae_battle_rifle.aepack"));
	CHECK(ae_update_entry_allowed("packs/ae_battle_rifle.CREDITS.txt"));
	CHECK(ae_update_entry_allowed("voices/cori/beep_tick.wav"));
	CHECK(ae_update_entry_allowed("sounds/horn.wav"));
	CHECK(ae_update_entry_allowed("Voices/cori/and.wav"));
	/* (a folder's own entry, deeper than a voice's folder, a folder not the
	release's, the game's own folders) */
	CHECK(!ae_update_entry_allowed("packs/"));
	CHECK(!ae_update_entry_allowed("voices/cori/"));
	CHECK(!ae_update_entry_allowed("voices/cori/deeper/and.wav"));
	CHECK(!ae_update_entry_allowed("maps/bloodgulch.map"));
	CHECK(!ae_update_entry_allowed("data/config.toml"));
	CHECK(!ae_update_entry_allowed("private/key"));
	CHECK(!ae_update_entry_allowed("other/file.txt"));
	CHECK(!ae_update_entry_allowed("data"));
	CHECK(!ae_update_entry_allowed("LOGS"));
	CHECK(!ae_update_entry_allowed("maps"));
	/* (names that would leave the folder or mean something else) */
	CHECK(!ae_update_entry_allowed(""));
	CHECK(!ae_update_entry_allowed(NULL));
	CHECK(!ae_update_entry_allowed("/etc/passwd"));
	CHECK(!ae_update_entry_allowed("../halo"));
	CHECK(!ae_update_entry_allowed("packs/../halo"));
	CHECK(!ae_update_entry_allowed("packs/../../halo"));
	CHECK(!ae_update_entry_allowed("packs//x.aepack"));
	CHECK(!ae_update_entry_allowed("packs/.hidden"));
	CHECK(!ae_update_entry_allowed(".halo"));
	CHECK(!ae_update_entry_allowed("voices\\cori\\and.wav"));
	CHECK(!ae_update_entry_allowed("packs\\..\\halo"));
	CHECK(!ae_update_entry_allowed("C:/halo.exe"));
	CHECK(!ae_update_entry_allowed("C:halo.exe"));
	CHECK(!ae_update_entry_allowed("sounds/horn wav"));
}

int main(void)
{
	char version[32];
	const char *releases = "[{\"tag_name\":\"map-pack-1\",\"draft\":false},"
		"{\"tag_name\":\"v0.2.2-beta\",\"draft\":false,\"prerelease\":true},"
		"{\"tag_name\":\"v0.2.1-beta\",\"draft\":false}]";
	const char *draft = "[{\"tag_name\":\"v9.0.0\",\"draft\":true},"
		"{\"tag_name\":\"v0.2.2-beta\",\"draft\":false}]";
	const char *long_tag = "[{\"tag_name\":\"v1234567890123456789012345678901234567890\",\"draft\":false}]";

	CHECK(updater_pick_release(releases, version, sizeof(version)));
	CHECK(!strcmp(version, "0.2.2-beta"));
	CHECK(updater_pick_release(draft, version, sizeof(version)));
	CHECK(!strcmp(version, "0.2.2-beta"));
	none("[{\"tag_name\":\"map-pack-1\",\"draft\":false}]");
	none("");
	none("[{\"tag_name\":\"v0.2");
	none("not json at all");
	none(long_tag);
	none("[]");
	order();
	shaped_like_github();
	entries();
	if (!failures)
		printf("ae_updater_pick_test: ok\n");
	return failures ? 1 : 0;
}
