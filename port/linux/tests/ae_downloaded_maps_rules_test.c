/* ae_downloaded_maps_rules.c: the table of downloaded maps' names: past the old 512, a full table that says so and never
overwrites a name, the list read in pieces of any size, names compared as the game names maps. */
#include <stdio.h>
#include <string.h>

#include "ae_downloaded_maps_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char name[64];
	int i;
	struct ae_downloaded_maps_reader reader;
	char const *text = "alpha@ce\r\nBeta@CE\n\n" "gamma@ce";

	ae_downloaded_maps_reset();
	CHECK(!ae_downloaded_maps_has("alpha@ce") && !ae_downloaded_maps_has("") && !ae_downloaded_maps_has(NULL));
	CHECK(ae_downloaded_maps_add("alpha@ce") && ae_downloaded_maps_add("ALPHA@ce") && ae_downloaded_maps_count() == 1);
	CHECK(ae_downloaded_maps_has("Alpha@CE") && ae_downloaded_maps_has("custom_maps\\alpha@ce") && !ae_downloaded_maps_has("alpha"));
	CHECK(ae_downloaded_maps_add("") && ae_downloaded_maps_count() == 1);

	/* past the old table's 512 names, none lost */
	ae_downloaded_maps_reset();
	for (i = 0; i < 20000; i++)
	{
		snprintf(name, sizeof(name), "map%d@ce", i);
		CHECK(ae_downloaded_maps_add(name));
	}
	CHECK(ae_downloaded_maps_count() == 20000 && !ae_downloaded_maps_overflowed());
	CHECK(ae_downloaded_maps_has("map0@ce") && ae_downloaded_maps_has("map511@ce") && ae_downloaded_maps_has("map512@ce") &&
		ae_downloaded_maps_has("map19999@ce") && !ae_downloaded_maps_has("map20000@ce"));

	/* the table's limit: refused with a flag, then everything is a downloaded map; nothing was overwritten */
	for (i = 20000; i < AE_DOWNLOADED_MAPS_MAXIMUM; i++)
	{
		snprintf(name, sizeof(name), "map%d@ce", i);
		CHECK(ae_downloaded_maps_add(name));
	}
	CHECK(ae_downloaded_maps_count() == AE_DOWNLOADED_MAPS_MAXIMUM && !ae_downloaded_maps_overflowed());
	CHECK(ae_downloaded_maps_add("map0@ce"));
	CHECK(!ae_downloaded_maps_add("onetoomany@ce") && ae_downloaded_maps_overflowed());
	CHECK(ae_downloaded_maps_has("onetoomany@ce") && ae_downloaded_maps_has("never_seen@ce") && ae_downloaded_maps_count() == AE_DOWNLOADED_MAPS_MAXIMUM);
	ae_downloaded_maps_reset();
	CHECK(!ae_downloaded_maps_overflowed() && !ae_downloaded_maps_has("map0@ce"));

	/* the list in pieces of 1, 3 and all, a long line skipped, a last line with no line feed */
	{
		int piece, at;
		long sizes[3] = { 1, 3, 1000 };

		for (piece = 0; piece < 3; piece++)
		{
			ae_downloaded_maps_reset();
			ae_downloaded_maps_reader_start(&reader);
			for (at = 0; at < (int)strlen(text); at += (int)sizes[piece])
			{
				long left = (long)strlen(text) - at;

				ae_downloaded_maps_reader_feed(&reader, text + at, left < sizes[piece] ? left : sizes[piece]);
			}
			ae_downloaded_maps_reader_end(&reader);
			CHECK(ae_downloaded_maps_count() == 3 && ae_downloaded_maps_has("gamma@ce") && ae_downloaded_maps_has("beta@ce"));
		}
		ae_downloaded_maps_reset();
		ae_downloaded_maps_reader_start(&reader);
		memset(name, 'x', 63);
		name[63] = 0;
		ae_downloaded_maps_reader_feed(&reader, name, 63);
		ae_downloaded_maps_reader_feed(&reader, "y\nok@ce\n", 8);
		ae_downloaded_maps_reader_feed(&reader, name, 63);
		ae_downloaded_maps_reader_end(&reader);
		CHECK(ae_downloaded_maps_count() == 2 && ae_downloaded_maps_has("ok@ce") && ae_downloaded_maps_has(name));
	}

	/* a map opened under another name than the one asked for: the archive marked the file's name, the game asks for the
	other (a hashed name's base, a version's file), and the scripts' rules still hold, both ways */
	ae_downloaded_maps_reset();
	ae_downloaded_maps_add("trap@ce");
	CHECK(!ae_downloaded_maps_has("trap_1a2b3c4d@ce"));
	CHECK(ae_downloaded_maps_has_either("trap_1a2b3c4d@ce", "d:\\custom_maps\\trap.map"));
	CHECK(ae_downloaded_maps_has_either("trap_1a2b3c4d@ce", "/home/x/custom_maps/TRAP.MAP"));
	CHECK(!ae_downloaded_maps_has_either("other@ce", "d:\\custom_maps\\mine.map") && !ae_downloaded_maps_has_either("other@ce", NULL));
	ae_downloaded_maps_reset();
	ae_downloaded_maps_add("trap_1a2b3c4d@ce");
	CHECK(!ae_downloaded_maps_has("trap@ce"));
	CHECK(ae_downloaded_maps_has_either("trap@ce", "d:\\custom_maps\\trap_1a2b3c4d.map"));
	{
		char file_name[64];

		CHECK(ae_downloaded_maps_file_name("a/b\\c.Map", file_name, sizeof(file_name)) && !strcmp(file_name, "c@ce"));
		CHECK(ae_downloaded_maps_file_name("noext", file_name, sizeof(file_name)) && !strcmp(file_name, "noext@ce"));
		CHECK(!ae_downloaded_maps_file_name("", file_name, sizeof(file_name)) && !ae_downloaded_maps_file_name(NULL, file_name, sizeof(file_name)));
	}
	CHECK(ae_downloaded_maps_alias_name("LONGNA~1") && ae_downloaded_maps_alias_name("a~") && !ae_downloaded_maps_alias_name("longname_ctf") &&
		!ae_downloaded_maps_alias_name("") && !ae_downloaded_maps_alias_name(NULL));
	ae_downloaded_maps_reset();

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
