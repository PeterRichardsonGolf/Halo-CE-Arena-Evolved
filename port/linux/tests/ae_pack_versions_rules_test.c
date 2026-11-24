/* ae_pack_versions_rules.c: the host's copy's folder and path, a version folder's name, which copies go */
#include <stdio.h>
#include <string.h>

#include "ae_pack_versions_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char text[96];
	unsigned long version = 0;
	unsigned long versions[5] = { 0x11111111UL, 0x22222222UL, 0x33333333UL, 0x44444444UL, 0x55555555UL };
	unsigned long long used[5] = { 50, 10, 40, 20, 30 };
	int remove[5];

	/* (the folder and the path: maps_shared\<pack>\<version 8 hex>\<file>.map) */
	CHECK(ae_pack_versions_path(text, (int)sizeof(text), "smp", "fissurefall", 0xd4a0eaddUL));
	CHECK(!strcmp(text, "d:\\maps_shared\\smp\\d4a0eadd\\fissurefall.map"));
	CHECK(ae_pack_versions_folder(text, (int)sizeof(text), "smp", 0x00000abcUL));
	CHECK(!strcmp(text, "d:\\maps_shared\\smp\\00000abc\\"));
	/* (no version, a name that is not plain, a text that does not fit) */
	CHECK(!ae_pack_versions_folder(text, (int)sizeof(text), "smp", 0) && !text[0]);
	CHECK(!ae_pack_versions_path(text, (int)sizeof(text), "..", "x", 1));
	CHECK(!ae_pack_versions_path(text, (int)sizeof(text), "smp", "a\\b", 1));
	CHECK(!ae_pack_versions_path(text, (int)sizeof(text), "smp", "a@b", 1));
	CHECK(!ae_pack_versions_path(text, (int)sizeof(text), "", "x", 1));
	CHECK(!ae_pack_versions_path(text, 30, "smp", "fissurefall", 1) && !text[0]);

	/* (a version folder: 8 hex digits, not 0) */
	CHECK(ae_pack_versions_parse_folder("d4A0eadd", &version) && version == 0xd4a0eaddUL);
	CHECK(!ae_pack_versions_parse_folder("d4a0ead", &version));
	CHECK(!ae_pack_versions_parse_folder("d4a0eadd0", &version));
	CHECK(!ae_pack_versions_parse_folder("00000000", &version));
	CHECK(!ae_pack_versions_parse_folder("d4a0eadg", &version));
	CHECK(!ae_pack_versions_parse_folder(NULL, &version));

	/* (3 kept of 5: the two used longest ago go, oldest first) */
	CHECK(ae_pack_versions_evict(5, versions, used, 0, AE_PACK_VERSIONS_KEPT, remove) == 2);
	CHECK(remove[0] == 1 && remove[1] == 3);
	/* (never the one the game plays, though it is the oldest) */
	CHECK(ae_pack_versions_evict(5, versions, used, 0x22222222UL, 3, remove) == 2);
	CHECK(remove[0] == 3 && remove[1] == 4);
	/* (3 or fewer: none go) */
	CHECK(ae_pack_versions_evict(3, versions, used, 0, 3, remove) == 0);
	CHECK(ae_pack_versions_evict(0, versions, used, 0, 3, remove) == 0);

	printf("%s (%d failures)\n", failures ? "FAILED" : "ok", failures);
	return failures != 0;
}
