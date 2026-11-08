/* ae_extras_rules.c: where the macOS download keeps packs/, voices/ and sounds/, and which files are copied */
#include <stdio.h>
#include <string.h>

#include "ae_extras_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char resources[256], beside[256];

	/* the application in the folder the download was unpacked into */
	CHECK(ae_extras_app_places("/Users/a/Downloads/arena-evolved-macos-release/ArenaEvolved.app/Contents/MacOS/halo",
		resources, sizeof(resources), beside, sizeof(beside)));
	CHECK(!strcmp(resources, "/Users/a/Downloads/arena-evolved-macos-release/ArenaEvolved.app/Contents/Resources"));
	CHECK(!strcmp(beside, "/Users/a/Downloads/arena-evolved-macos-release"));
	/* in Applications, and at the root of a volume */
	CHECK(ae_extras_app_places("/Applications/ArenaEvolved.app/Contents/MacOS/halo", resources, sizeof(resources),
		beside, sizeof(beside)));
	CHECK(!strcmp(resources, "/Applications/ArenaEvolved.app/Contents/Resources"));
	CHECK(!strcmp(beside, "/Applications"));
	CHECK(ae_extras_app_places("/ArenaEvolved.app/Contents/MacOS/halo", resources, sizeof(resources), beside,
		sizeof(beside)));
	CHECK(!strcmp(beside, "/"));
	/* a relative path */
	CHECK(ae_extras_app_places("ArenaEvolved.app/Contents/MacOS/halo", resources, sizeof(resources), beside,
		sizeof(beside)));
	CHECK(!strcmp(beside, "."));
	/* a folder of the same name above the application: the application's own is the last */
	CHECK(ae_extras_app_places("/x/Old.app/Contents/MacOS/y/ArenaEvolved.app/Contents/MacOS/halo", resources,
		sizeof(resources), beside, sizeof(beside)));
	CHECK(!strcmp(beside, "/x/Old.app/Contents/MacOS/y"));
	/* not an application's executable */
	CHECK(!ae_extras_app_places("/opt/ae/halo", resources, sizeof(resources), beside, sizeof(beside)));
	CHECK(!resources[0] && !beside[0]);
	CHECK(!ae_extras_app_places("/x/A.app/Contents/MacOS/", resources, sizeof(resources), beside, sizeof(beside)));
	CHECK(!ae_extras_app_places(NULL, resources, sizeof(resources), beside, sizeof(beside)));
	/* too small a buffer: nothing, and empty */
	CHECK(!ae_extras_app_places("/Applications/ArenaEvolved.app/Contents/MacOS/halo", resources, 10, beside,
		sizeof(beside)));
	CHECK(!resources[0] && !beside[0]);
	CHECK(!ae_extras_app_places("/Applications/ArenaEvolved.app/Contents/MacOS/halo", resources, sizeof(resources),
		beside, 5));
	CHECK(!resources[0] && !beside[0]);

	CHECK(ae_extras_translocated("/private/var/folders/ab/xyz/AppTranslocation/1-2-3/d/ArenaEvolved.app/Contents/MacOS/halo"));
	CHECK(!ae_extras_translocated("/Applications/ArenaEvolved.app/Contents/MacOS/halo"));
	CHECK(!ae_extras_translocated(NULL));

	CHECK(ae_extras_download_name_matches("arena-evolved-0.7.5-beta-macos"));
	CHECK(ae_extras_download_name_matches("Arena Evolved"));
	CHECK(ae_extras_download_name_matches("ArenaEvolved 2"));
	CHECK(ae_extras_download_name_matches("ARENA_EVOLVED"));
	CHECK(!ae_extras_download_name_matches("arena"));
	CHECK(!ae_extras_download_name_matches("arena-shooter"));
	CHECK(!ae_extras_download_name_matches("Documents"));
	CHECK(!ae_extras_download_name_matches(""));
	CHECK(!ae_extras_download_name_matches(NULL));

	CHECK(ae_extras_name_allowed("ae_battle_rifle.aepack"));
	CHECK(ae_extras_name_allowed("horn.wav"));
	CHECK(!ae_extras_name_allowed(".DS_Store"));
	CHECK(!ae_extras_name_allowed(""));
	CHECK(!ae_extras_name_allowed("a/b"));
	CHECK(!ae_extras_name_allowed("a\\b"));
	CHECK(!ae_extras_name_allowed(NULL));

	/* missing: copied; older: copied; same or newer in the data root: kept */
	CHECK(ae_extras_should_copy(0, 100, 0));
	CHECK(ae_extras_should_copy(1, 200, 100));
	CHECK(!ae_extras_should_copy(1, 100, 100));
	CHECK(!ae_extras_should_copy(1, 100, 200));

	CHECK(AE_EXTRAS_FOLDER_COUNT == 3 && !strcmp(ae_extras_folders[0], "packs") && !strcmp(ae_extras_folders[1], "voices") &&
		!strcmp(ae_extras_folders[2], "sounds"));

	if (failures)
		return 1;
	printf("ae_extras_rules: ok\n");
	return 0;
}
