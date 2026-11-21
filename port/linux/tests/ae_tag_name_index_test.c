#include <stdio.h>
#include <string.h>
#include "ae_tag_name_index.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

struct tag { long group; char const *name; };

/* (group tags as numbers: the units build with -Werror, multi-character constants warn) */
#define DELA 0x44654c61L
#define BITM 0x6269746dL
#define WEAP 0x77656170L
#define VEHI 0x76656869L
#define SND 0x736e6421L

static void get(void *context, long index, long *group, char const **name)
{
	struct tag const *tags = context;

	*group = tags[index].group;
	*name = tags[index].name;
}

/* the walk the index stands in for (cache_files.c's tag_loaded before 0.9.4) */
static int lower(int c) { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }
static int same(char const *a, char const *b)
{
	while (*a && lower((unsigned char)*a) == lower((unsigned char)*b)) { a++; b++; }
	return lower((unsigned char)*a) == lower((unsigned char)*b);
}
static long walk(struct tag const *tags, long count, long group, char const *name)
{
	long i;

	for (i = 0; i < count; i++)
		if (tags[i].group == group && tags[i].name && same(tags[i].name, name))
			return i;
	return -1;
}

static struct tag big[AE_TAG_NAME_INDEX_MAXIMUM_TAGS + 1];
static char names[AE_TAG_NAME_INDEX_MAXIMUM_TAGS][24];

int main(void)
{
	struct tag tags[] = {
		{ DELA, "ui\\shell\\main_menu\\main_menu" },
		{ BITM, "ui\\shell\\main_menu\\main_menu" },     /* the same name, another group */
		{ DELA, "pc\\main_menu\\LOBBY" },
		{ DELA, "pc\\main_menu\\lobby" },                /* the same name again: the first is the one */
		{ WEAP, NULL },                                  /* no name: never found */
		{ WEAP, "weapons\\pistol\\pistol" },
	};
	long count = (long)(sizeof(tags) / sizeof(tags[0]));
	unsigned long builds;
	long i;

	CHECK(ae_tag_name_index_find(DELA, "ui\\shell\\main_menu\\main_menu", tags, count, 1, get, tags) == 0);
	CHECK(ae_tag_name_index_find(BITM, "UI\\Shell\\Main_Menu\\Main_Menu", tags, count, 1, get, tags) == 1);
	CHECK(ae_tag_name_index_find(DELA, "pc\\main_menu\\lobby", tags, count, 1, get, tags) == 2);
	CHECK(ae_tag_name_index_find(DELA, "pc\\main_menu\\lobb", tags, count, 1, get, tags) == -1);
	CHECK(ae_tag_name_index_find(DELA, "pc\\main_menu\\lobbyy", tags, count, 1, get, tags) == -1);
	CHECK(ae_tag_name_index_find(WEAP, "weapons\\pistol\\pistol", tags, count, 1, get, tags) == 5);
	CHECK(ae_tag_name_index_find(WEAP, "", tags, count, 1, get, tags) == -1);
	CHECK(ae_tag_name_index_find(WEAP, NULL, tags, count, 1, get, tags) == -1);
	CHECK(ae_tag_name_index_find(VEHI, "weapons\\pistol\\pistol", tags, count, 1, get, tags) == -1);

	/* built once for the same table, count and generation; again when any of them moves */
	builds = ae_tag_name_index_builds();
	CHECK(ae_tag_name_index_find(WEAP, "weapons\\pistol\\pistol", tags, count, 1, get, tags) == 5);
	CHECK(ae_tag_name_index_builds() == builds);
	tags[5].name = "weapons\\magnum\\magnum";
	CHECK(ae_tag_name_index_find(WEAP, "weapons\\magnum\\magnum", tags, count, 2, get, tags) == 5);
	CHECK(ae_tag_name_index_find(WEAP, "weapons\\pistol\\pistol", tags, count, 2, get, tags) == -1);
	CHECK(ae_tag_name_index_builds() == builds + 1);
	CHECK(ae_tag_name_index_find(WEAP, "weapons\\magnum\\magnum", tags, count - 1, 2, get, tags) == -1);
	CHECK(ae_tag_name_index_builds() == builds + 2);

	/* a full table: the same answers as the walk, for every tag and for names not there */
	for (i = 0; i < AE_TAG_NAME_INDEX_MAXIMUM_TAGS; i++)
	{
		snprintf(names[i], sizeof(names[i]), "Tag\\%ld", i % 50000);
		big[i].group = i % 7 ? BITM : SND;
		big[i].name = names[i];
	}
	for (i = 0; i < AE_TAG_NAME_INDEX_MAXIMUM_TAGS; i += 97)
	{
		CHECK(ae_tag_name_index_find(big[i].group, big[i].name, big, AE_TAG_NAME_INDEX_MAXIMUM_TAGS, 3, get, big) ==
			walk(big, AE_TAG_NAME_INDEX_MAXIMUM_TAGS, big[i].group, big[i].name));
	}
	CHECK(ae_tag_name_index_find(BITM, "tag\\50000", big, AE_TAG_NAME_INDEX_MAXIMUM_TAGS, 3, get, big) == -1);
	/* (more tags than the index holds: the caller walks) */
	big[AE_TAG_NAME_INDEX_MAXIMUM_TAGS] = big[0];
	CHECK(ae_tag_name_index_find(BITM, "tag\\1", big, AE_TAG_NAME_INDEX_MAXIMUM_TAGS + 1, 3, get, big) ==
		AE_TAG_NAME_INDEX_UNUSABLE);

	if (failures)
		printf("%d failure(s)\n", failures);
	else
		printf("ae_tag_name_index: all checks passed\n");
	return failures != 0;
}
