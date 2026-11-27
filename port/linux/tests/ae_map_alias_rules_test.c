#include <stdio.h>
#include <string.h>
#include "ae_map_alias_rules.h"
#include "ae_social_playlists.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define EQ(a, b) do { long x_ = (long)(a), y_ = (long)(b); if (x_ != y_) { printf("FAIL %s:%d %s = %ld, want %ld\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

static char text[131072];

int main(void)
{
	static struct ae_social_playlist lists[AE_SOCIAL_PLAYLISTS];
	char const *old_name, *identity;
	FILE *file;
	size_t length;
	int index, entry, count;

	/* 19 Social Map Pack maps and 10 Pro Edition maps */
	EQ(ae_map_alias_count(), 29);
	for (index = 0; index < ae_map_alias_count(); index++)
	{
		CHECK(ae_map_alias_get(index, &old_name, &identity));
		CHECK(strstr(old_name, "@smp") || strstr(old_name, "@pro"));
		CHECK(strstr(identity, "@ws") != NULL);
		/* (the loader's level names hold 63 characters) */
		CHECK(strlen(identity) < 64);
		CHECK(!strcmp(ae_map_alias_old_name(identity), old_name));
		CHECK(!strcmp(ae_map_alias_new_name(old_name), identity));
	}
	CHECK(!strcmp(ae_map_alias_old_name("WARREN_MCC debug flycam pm@ws2900072240"), "warren@smp"));
	CHECK(!strcmp(ae_map_alias_new_name("pat_wizard@pro"), "pat_wizard@ws2899666869"));
	CHECK(!ae_map_alias_old_name("1@ws3477809506") && !ae_map_alias_old_name(NULL) && !ae_map_alias_new_name("warren"));
	{
		char file[16];

		CHECK(ae_map_alias_old_file("WARREN_MCC debug flycam pm@ws2900072240", file, sizeof(file)) && !strcmp(file, "warren"));
		CHECK(ae_map_alias_old_file("pat_wizard@ws2899666869", file, sizeof(file)) && !strcmp(file, "pat_wizard"));
		CHECK(!ae_map_alias_old_file("pat_wizard@ws2899666869", file, 10));
		CHECK(!ae_map_alias_old_file("1@ws3477809506", file, sizeof(file)) && !ae_map_alias_old_file(NULL, file, sizeof(file)));
	}
	EQ(ae_map_alias_get(29, &old_name, &identity), 0);
	/* every Workshop entry of the shipped playlists is a table identity and its stand-in is the table's old name */
	file = fopen(AE_SOCIAL_TXT, "rb");
	if (!file)
		return failures != 0;
	length = fread(text, 1, sizeof(text) - 1, file);
	text[length] = 0;
	fclose(file);
	count = ae_social_playlists_parse(text, lists, AE_SOCIAL_PLAYLISTS);
	for (index = 0; index < count; index++)
		for (entry = 0; entry < lists[index].count; entry++)
			if (lists[index].entry[entry].stand_for)
				CHECK(!strcmp(ae_map_alias_new_name(lists[index].entry[entry].map),
					lists[index].entry[lists[index].entry[entry].stand_for - 1].map));
	printf(failures ? "ae_map_alias_rules_test: %d failed\n" : "ae_map_alias_rules_test: ok\n", failures);
	return failures != 0;
}
