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
		"item like 3 at -18.4 -11 0.5 facing 90 team blue games all\n"
		"item \"weapons\\sniper rifle\\sniper rifle\" at 0 0 0 team neutral respawn 90 facing -45.5\n"
		"remove vehicle turret\n"
		"remove vehicle 2 near 10 -4 2.5 6\n"
		"name item 33 SWORD\n"
		"name item 4 \"ENERGY SWORD\"\n";

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
	AE_CHECK(layout.items[1].games_all && !layout.items[0].games_all);
	AE_CHECK(layout.items[1].x < -18.3f && layout.items[1].y == -11.0f && layout.items[1].z == 0.5f);
	AE_CHECK(layout.items[2].what == _ae_layout_tag && !strcmp(layout.items[2].tag, "weapons\\sniper rifle\\sniper rifle"));
	AE_CHECK(layout.items[2].team == _ae_layout_team_neutral && layout.items[2].respawn_seconds == 90);
	AE_CHECK(layout.vehicle_rule_count == 2 && layout.vehicle_rules[0].turret && !layout.vehicle_rules[0].near);
	AE_CHECK(!layout.vehicle_rules[1].turret && layout.vehicle_rules[1].palette_index == 2);
	AE_CHECK(layout.vehicle_rules[1].near && layout.vehicle_rules[1].radius == 6.0f && layout.vehicle_rules[1].line == 9);
	AE_CHECK(layout.name_count == 2 && layout.names[0].item == 33 && !strcmp(layout.names[0].label, "SWORD"));
	AE_CHECK(layout.names[1].item == 4 && !strcmp(layout.names[1].label, "ENERGY SWORD"));
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
		"layout 1 m.map 00000001\nname item 3\n",
		"layout 1 m.map 00000001\nitem camo at 0 0 0 games\n",
		"layout 1 m.map 00000001\nitem camo at 0 0 0 games ctf\n",
		"layout 1 m.map 00000001\nitem camo at 0 0 0 games all games all\n",
		"layout 1 m.map 00000001\nname item x SWORD\n",
		"layout 1 m.map 00000001\nname item 3 SIXTEEN_LETTERS_\n",
		"layout 1 m.map 00000001\nname item 3 TWO WORDS\n",
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

/* the bots' lines: bot jump at A to B, bot avoid P radius r (ae_bots_travel.c reads them) */
static void test_bot_lines(void)
{
	char const *text =
		"layout 1 chillout.map 0a1b2c3d\n"
		"bot jump at 1.5 -2 0.25 to 4.5 -2 1.75   # up onto the ledge\n"
		"BOT JUMP AT 0 0 0 TO 0 0 -12\n"
		"bot avoid 10 20 -1.5 radius 3.5\n"
		"bot avoid 0 0 0 radius 50\n"
		"item camo at 0 0 0\n";
	static char const *const bad[] = {
		"layout 1 m.map 00000001\nbot\n",
		"layout 1 m.map 00000001\nbot run at 0 0 0 to 1 1 1\n",
		"layout 1 m.map 00000001\nbot jump 0 0 0 to 1 1 1\n",                /* no at */
		"layout 1 m.map 00000001\nbot jump at 0 0 0 1 1 1\n",                /* no to */
		"layout 1 m.map 00000001\nbot jump at 0 0 0 to 1 1\n",
		"layout 1 m.map 00000001\nbot jump at 0 0 0 to 1 1 1 now\n",
		"layout 1 m.map 00000001\nbot jump at 0 0 x to 1 1 1\n",
		"layout 1 m.map 00000001\nbot jump at 0 0 0 to 0.2 0.2 0\n",         /* too short */
		"layout 1 m.map 00000001\nbot jump at 0 0 0 to 20.1 0 0\n",          /* too long */
		"layout 1 m.map 00000001\nbot jump at nan 0 0 to 1 1 1\n",
		"layout 1 m.map 00000001\nbot avoid 0 0 0\n",
		"layout 1 m.map 00000001\nbot avoid 0 0 0 radius\n",
		"layout 1 m.map 00000001\nbot avoid 0 0 0 size 3\n",
		"layout 1 m.map 00000001\nbot avoid 0 0 0 radius 0\n",
		"layout 1 m.map 00000001\nbot avoid 0 0 0 radius -2\n",
		"layout 1 m.map 00000001\nbot avoid 0 0 0 radius 50.5\n",
		"layout 1 m.map 00000001\nbot avoid 0 0 0 radius nan\n",
		"layout 1 m.map 00000001\nbot avoid 0 0 radius 3\n",
		"bot avoid 0 0 0 radius 3\nlayout 1 m.map 00000001\n",                /* before the header */
	};
	char *many;
	long length;
	unsigned int i;

	AE_CHECK(parse(text) == 0);
	AE_CHECK(layout.bot_jump_count == 2 && layout.bot_avoid_count == 2 && layout.item_count == 1);
	AE_CHECK(layout.bot_jumps[0].from_x == 1.5f && layout.bot_jumps[0].from_y == -2.0f &&
		layout.bot_jumps[0].from_z == 0.25f);
	AE_CHECK(layout.bot_jumps[0].to_x == 4.5f && layout.bot_jumps[0].to_y == -2.0f && layout.bot_jumps[0].to_z == 1.75f);
	AE_CHECK(layout.bot_jumps[0].line == 2 && layout.bot_jumps[1].line == 3 && layout.bot_jumps[1].to_z == -12.0f);
	AE_CHECK(layout.bot_avoids[0].x == 10.0f && layout.bot_avoids[0].y == 20.0f && layout.bot_avoids[0].z == -1.5f &&
		layout.bot_avoids[0].radius == 3.5f && layout.bot_avoids[0].line == 4);
	AE_CHECK(layout.bot_avoids[1].radius == 50.0f);
	/* (a file of bot lines alone) */
	AE_CHECK(parse("layout 1 m.map 00000001\nbot avoid 1 2 3 radius 1\n") == 0 && layout.bot_avoid_count == 1 &&
		!layout.item_count && !layout.clear_items);
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		int result = parse(bad[i]);

		if (!(result > 0 && error[0] && !layout.bot_jump_count && !layout.bot_avoid_count && !layout.map_file[0]))
			printf("FAIL bot refused #%u (%d: %s)\n", i, result, error);
		AE_CHECK(result > 0 && error[0] && !layout.bot_jump_count && !layout.bot_avoid_count && !layout.map_file[0]);
	}
	AE_CHECK(parse("layout 1 m.map 00000001\nbot avoid 0 0 0 radius 1\nbot jump at 0 0 0 to 0 0 0\n") == 3);
	/* 64 of each: fine; a 65th: refused */
	many = malloc(AE_LAYOUT_MAXIMUM_FILE_SIZE);
	length = sprintf(many, "layout 1 m.map 00000001\n");
	for (i = 0; i < AE_LAYOUT_MAXIMUM_BOT_JUMPS; i++)
		length += sprintf(many + length, "bot jump at %u 0 0 to %u 1 0\nbot avoid %u 0 0 radius 1\n", i, i, i);
	AE_CHECK(ae_layout_parse(many, length, &layout, error, sizeof(error)) == 0 &&
		layout.bot_jump_count == AE_LAYOUT_MAXIMUM_BOT_JUMPS && layout.bot_avoid_count == AE_LAYOUT_MAXIMUM_BOT_AVOIDS);
	AE_CHECK(sprintf(many + length, "bot jump at 0 0 0 to 1 0 0\n") > 0 &&
		ae_layout_parse(many, (long)strlen(many), &layout, error, sizeof(error)) == 2 * AE_LAYOUT_MAXIMUM_BOT_JUMPS + 2);
	AE_CHECK(sprintf(many + length, "bot avoid 0 0 0 radius 1\n") > 0 &&
		ae_layout_parse(many, (long)strlen(many), &layout, error, sizeof(error)) == 2 * AE_LAYOUT_MAXIMUM_BOT_AVOIDS + 2);
	free(many);
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
	test_bot_lines();
	test_limits();
	printf("%s\n", failures ? "FAILED" : "ok");
	return failures != 0;
}
