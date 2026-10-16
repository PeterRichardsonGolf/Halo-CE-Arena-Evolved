/* AE map layout files: the reader (port/linux/game/ae_layout_rules.c): good files, every refusal, the limits */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_layout_rules.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_layout layout;
static char error[160];

static int parse(char const *text)
{
	return ae_layout_parse(text, (long)strlen(text), &layout, error, sizeof(error));
}

static void test_good_file(void)
{
	char const *text =
		"# H2 Zanzibar\r\n"
		"layout 1 h2_zanzibar.map 58102bdc\n"
		"clear items   # all of the map's\n"
		"remove item 7\n"
		"item rockets at 1.0 24.7 5.4 respawn 120\n"
		"item like 3 at -18.4 -11 0.5 facing 90 team blue\n"
		"item \"weapons\\sniper rifle\\sniper rifle\" at 0 0 0 team neutral respawn 90 facing -45.5\n"
		"remove vehicle turret\n"
		"remove vehicle 2 near 10 -4 2.5 6\n";

	AE_CHECK(parse(text) == 0);
	AE_CHECK(!strcmp(layout.map_file, "h2_zanzibar.map") && layout.map_hash == 0x58102bdcUL);
	AE_CHECK(layout.clear_items && layout.removed_count == 1 && layout.removed[0] == 7);
	AE_CHECK(layout.item_count == 3);
	AE_CHECK(layout.items[0].what == _ae_layout_alias && layout.items[0].alias == _ae_layout_rockets);
	AE_CHECK(layout.items[0].respawn_seconds == 120 && layout.items[0].team == _ae_layout_team_unset);
	AE_CHECK(!layout.items[0].has_facing && layout.items[0].line == 5);
	AE_CHECK(layout.items[1].what == _ae_layout_like && layout.items[1].like == 3);
	AE_CHECK(layout.items[1].has_facing && layout.items[1].facing_degrees == 90.0f);
	AE_CHECK(layout.items[1].team == _ae_layout_team_blue && layout.items[1].respawn_seconds == -1);
	AE_CHECK(layout.items[1].x < -18.3f && layout.items[1].y == -11.0f && layout.items[1].z == 0.5f);
	AE_CHECK(layout.items[2].what == _ae_layout_tag && !strcmp(layout.items[2].tag, "weapons\\sniper rifle\\sniper rifle"));
	AE_CHECK(layout.items[2].team == _ae_layout_team_neutral && layout.items[2].respawn_seconds == 90);
	AE_CHECK(layout.vehicle_rule_count == 2 && layout.vehicle_rules[0].turret && !layout.vehicle_rules[0].near);
	AE_CHECK(!layout.vehicle_rules[1].turret && layout.vehicle_rules[1].palette_index == 2);
	AE_CHECK(layout.vehicle_rules[1].near && layout.vehicle_rules[1].radius == 6.0f && layout.vehicle_rules[1].line == 9);
	AE_CHECK(ae_layout_for_map(&layout, "H2_Zanzibar.map", 0x58102bdcUL));
	AE_CHECK(!ae_layout_for_map(&layout, "h2_zanzibar.map", 0x58102bddUL));
	AE_CHECK(!ae_layout_for_map(&layout, "h2_warlock.map", 0x58102bdcUL));
	AE_CHECK(!strcmp(ae_layout_alias_name(_ae_layout_camo), "camo") && !strcmp(ae_layout_alias_name(99), "?"));
}

/* a tag path: quoted when it has spaces; unquoted with spaces it is refused (its words read as more words) */
static void test_tag_paths(void)
{
	AE_CHECK(parse("layout 1 m.map 00000001\nitem weapons\\x\\x at 0 0 0\n") == 0 &&
		!strcmp(layout.items[0].tag, "weapons\\x\\x"));
	AE_CHECK(parse("layout 1 m.map 00000001\nitem weapons\\sniper rifle\\sniper rifle at 0 0 0\n") == 2);
	AE_CHECK(parse("layout 1 m.map 00000001\nitem \"weapons\\a b at 0 0 0\n") == 2);
	AE_CHECK(parse("layout 1 m.map 00000001\nitem \"weapons\\a b\"at 0 0 0\n") == 2);
}

static void test_refusals(void)
{
	static char const *const bad[] = {
		"",                                                   /* no layout line */
		"# only a comment\n",
		"layout 2 m.map 00000001\n",                          /* another version */
		"layout 1 m.map 1234567\n",                           /* 7 digits */
		"layout 1 m.map 1234567g\n",
		"layout 1 m.map\n",
		"item rockets at 0 0 0\n",                            /* before the header */
		"layout 1 m.map 00000001\nclear\n",
		"layout 1 m.map 00000001\nclear items now\n",
		"layout 1 m.map 00000001\nremove item -1\n",
		"layout 1 m.map 00000001\nremove item x\n",
		"layout 1 m.map 00000001\nitem rockets 0 0 0\n",      /* no at */
		"layout 1 m.map 00000001\nitem rockets at 0 0\n",
		"layout 1 m.map 00000001\nitem rockets at 0 0 z\n",
		"layout 1 m.map 00000001\nitem rockets at 0 0 0 respawn 0\n",
		"layout 1 m.map 00000001\nitem rockets at 0 0 0 respawn 30 respawn 60\n",
		"layout 1 m.map 00000001\nitem rockets at 0 0 0 team green\n",
		"layout 1 m.map 00000001\nitem rockets at 0 0 0 facing\n",
		"layout 1 m.map 00000001\nitem rocket at 0 0 0\n",    /* not an alias, no path */
		"layout 1 m.map 00000001\nitem like at 0 0 0\n",
		"layout 1 m.map 00000001\nitem at 0 0 0\n",
		"layout 1 m.map 00000001\nremove vehicle\n",
		"layout 1 m.map 00000001\nremove vehicle tank\n",
		"layout 1 m.map 00000001\nremove vehicle turret near 0 0 0\n",
		"layout 1 m.map 00000001\nremove vehicle turret near 0 0 0 0\n",
		"layout 1 m.map 00000001\nspawn rockets\n",
		"layout 1 m.map 00000001\nitem rockets at 1e9 0 0\n",
		"layout 1 m.map 00000001\nitem rockets at nan 0 0\n",
	};
	unsigned int i;

	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		int result = parse(bad[i]);

		if (!(result > 0 && error[0] && layout.item_count == 0 && !layout.map_file[0]))
			printf("FAIL refused #%u (%d: %s)\n", i, result, error);
		AE_CHECK(result > 0 && error[0] && layout.item_count == 0 && !layout.map_file[0]);
	}
	/* the line at fault */
	AE_CHECK(parse("layout 1 m.map 00000001\n\n# x\nitem rockets at 0 0\n") == 4);
}

static void test_limits(void)
{
	char *text = malloc(AE_LAYOUT_MAXIMUM_FILE_SIZE + 64);
	long length = 0;
	int i;

	/* 256 items: fine; 257: refused */
	length = sprintf(text, "layout 1 m.map 00000001\n");
	for (i = 0; i < AE_LAYOUT_MAXIMUM_ITEMS; i++)
		length += sprintf(text + length, "item camo at %d 0 0\n", i);
	AE_CHECK(ae_layout_parse(text, length, &layout, error, sizeof(error)) == 0 &&
		layout.item_count == AE_LAYOUT_MAXIMUM_ITEMS);
	length += sprintf(text + length, "item camo at 1 0 0\n");
	AE_CHECK(ae_layout_parse(text, length, &layout, error, sizeof(error)) == AE_LAYOUT_MAXIMUM_ITEMS + 2);
	/* a 201-character line */
	length = sprintf(text, "layout 1 m.map 00000001\n# ");
	for (i = 0; i < 199; i++)
		text[length++] = 'x';
	text[length++] = '\n';
	AE_CHECK(ae_layout_parse(text, length, &layout, error, sizeof(error)) == 2);
	/* 513 lines */
	length = sprintf(text, "layout 1 m.map 00000001\n");
	for (i = 0; i < AE_LAYOUT_MAXIMUM_LINES; i++)
		text[length++] = '\n';
	AE_CHECK(ae_layout_parse(text, length, &layout, error, sizeof(error)) == AE_LAYOUT_MAXIMUM_LINES + 1);
	/* bigger than 64 KB, and a NUL inside a line; text not terminated is read only to length */
	AE_CHECK(ae_layout_parse(text, AE_LAYOUT_MAXIMUM_FILE_SIZE + 1, &layout, error, sizeof(error)) == 1);
	memcpy(text, "layout 1 m.map 00000001\nclear\0items\n", 37);
	AE_CHECK(ae_layout_parse(text, 37, &layout, error, sizeof(error)) == 2);
	memcpy(text, "layout 1 m.map 00000001\nclear itemsXXXX", 39);
	AE_CHECK(ae_layout_parse(text, 35, &layout, error, sizeof(error)) == 0 && layout.clear_items);
	AE_CHECK(ae_layout_parse(NULL, 0, &layout, error, sizeof(error)) == 1);
	free(text);
}

int main(void)
{
	test_good_file();
	test_tag_paths();
	test_refusals();
	test_limits();
	printf("%s\n", failures ? "FAILED" : "ok");
	return failures != 0;
}
