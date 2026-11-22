/* AE map layout files: the reader (port/linux/game/ae_layout_rules.c): good files, every refusal, the limits */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_layout_rules.h"
#include "ae_weapon_slots.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_layout layout;
static char error[160];

static int parse(char const *text)
{
	return ae_layout_parse(text, (long)strlen(text), &layout, error, sizeof(error));
}

static int parse_n(char const *text, long length)
{
	return ae_layout_parse(text, length, &layout, error, sizeof(error));
}

static void test_good_file(void)
{
	char const *text =
		"# H2 Zanzibar\r\n"
		"layout 1 h2_zanzibar.map 58102bdc\n"
		"clear items   # all of the map's\n"
		"remove item 7\n"
		"item rockets at 1.0 24.7 5.4 respawn 120 float\n"
		"item like 3 at -18.4 -11 0.5 facing 90 drop team blue games all\n"
		"item \"weapons\\sniper rifle\\sniper rifle\" at 0 0 0 team neutral respawn 90 facing -45.5\n"
		"remove vehicle turret\n"
		"remove vehicle 2 near 10 -4 2.5 6\n"
		"remove scenery \"protected 54\"\n"
		"remove scenery 3 near 1 -2 3 4.5\n"
		"remove object machine coil near 0 0 0 2\n"
		"remove object Equipment 7\n"
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
	AE_CHECK(layout.items[1].drop && !layout.items[0].drop && !layout.items[2].drop);
	AE_CHECK(layout.items[0].float_here && !layout.items[1].float_here && !layout.items[2].float_here);
	AE_CHECK(layout.items[1].x < -18.3f && layout.items[1].y == -11.0f && layout.items[1].z == 0.5f);
	AE_CHECK(layout.items[2].what == _ae_layout_tag && !strcmp(layout.items[2].tag, "weapons\\sniper rifle\\sniper rifle"));
	AE_CHECK(layout.items[2].team == _ae_layout_team_neutral && layout.items[2].respawn_seconds == 90);
	AE_CHECK(layout.vehicle_rule_count == 2 && layout.vehicle_rules[0].turret && !layout.vehicle_rules[0].near);
	AE_CHECK(!layout.vehicle_rules[1].turret && layout.vehicle_rules[1].palette_index == 2);
	AE_CHECK(layout.vehicle_rules[1].near && layout.vehicle_rules[1].radius == 6.0f && layout.vehicle_rules[1].line == 9);
	AE_CHECK(layout.object_rule_count == 4);
	AE_CHECK(layout.object_rules[0].type == _ae_layout_object_scenery && layout.object_rules[0].by_name &&
		!strcmp(layout.object_rules[0].name, "protected 54") && !layout.object_rules[0].near && layout.object_rules[0].line == 10);
	AE_CHECK(layout.object_rules[1].type == _ae_layout_object_scenery && !layout.object_rules[1].by_name &&
		layout.object_rules[1].palette_index == 3 && layout.object_rules[1].near && layout.object_rules[1].y == -2.0f &&
		layout.object_rules[1].radius == 4.5f);
	AE_CHECK(layout.object_rules[2].type == _ae_layout_object_machine && !strcmp(layout.object_rules[2].name, "coil") &&
		layout.object_rules[2].near && layout.object_rules[2].radius == 2.0f);
	AE_CHECK(layout.object_rules[3].type == _ae_layout_object_equipment && layout.object_rules[3].palette_index == 7);
	AE_CHECK(!strcmp(ae_layout_object_type_name(_ae_layout_object_sound_scenery), "sound_scenery") &&
		!strcmp(ae_layout_object_type_name(99), "?"));
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
		"layout 1 m.map 00000001\nremove scenery\n",
		"layout 1 m.map 00000001\nremove scenery coil near 1 2 3\n",
		"layout 1 m.map 00000001\nremove scenery coil near 1 2 3 0\n",
		"layout 1 m.map 00000001\nremove object\n",
		"layout 1 m.map 00000001\nremove object tank 3\n",
		"layout 1 m.map 00000001\nremove object machine\n",
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

/* jump <percent>: the map's jump speed, 50 to 200 percent, once */
static void test_jump_line(void)
{
	static char const *const bad[] = {
		"layout 1 m.map 00000001\njump\n",
		"layout 1 m.map 00000001\njump 49\n",
		"layout 1 m.map 00000001\njump 201\n",
		"layout 1 m.map 00000001\njump 1.15\n",
		"layout 1 m.map 00000001\njump 115 now\n",
		"layout 1 m.map 00000001\njump 115\njump 120\n",
	};
	unsigned int i;

	AE_CHECK(parse("layout 1 h2d_salicreek.map 0a1b2c3d\njump 115   # H2's ledges\nitem camo at 0 0 0\n") == 0);
	AE_CHECK(layout.jump_percent == 115 && layout.item_count == 1);
	AE_CHECK(parse("layout 1 m.map 00000001\nitem camo at 0 0 0\n") == 0 && layout.jump_percent == 0);
	AE_CHECK(parse("layout 1 m.map 00000001\nJUMP 50\n") == 0 && layout.jump_percent == 50);
	AE_CHECK(parse("layout 1 m.map 00000001\njump 200\n") == 0 && layout.jump_percent == 200);
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
		AE_CHECK(parse(bad[i]) != 0 && layout.jump_percent == 0);
}

/* zone at <x y z> [radius r]: the round modes' danger zone centres, up to 8 */
static void test_zone_lines(void)
{
	static char const *const bad[] = {
		"layout 1 m.map 00000001\nzone 0 0 0\n",
		"layout 1 m.map 00000001\nzone at 0 0\n",
		"layout 1 m.map 00000001\nzone at 0 0 0 radius\n",
		"layout 1 m.map 00000001\nzone at 0 0 0 radius 0\n",
		"layout 1 m.map 00000001\nzone at 0 0 0 radius 501\n",
		"layout 1 m.map 00000001\nzone at 0 0 0 size 5\n",
		"layout 1 m.map 00000001\nzone at 0 0 0\nzone at 0 0 0\nzone at 0 0 0\nzone at 0 0 0\nzone at 0 0 0\n"
			"zone at 0 0 0\nzone at 0 0 0\nzone at 0 0 0\nzone at 0 0 0\n",
	};
	unsigned int i;

	AE_CHECK(parse("layout 1 m.map 00000001\nzone at 1.5 -20 0.25   # the middle\nZONE AT 3 4 5 RADIUS 40\n") == 0);
	AE_CHECK(layout.zone_count == 2 && layout.zones[0].x == 1.5f && layout.zones[0].y == -20.0f);
	AE_CHECK(layout.zones[0].radius == 0.0f && layout.zones[0].line == 2);
	AE_CHECK(layout.zones[1].radius == 40.0f && layout.zones[1].z == 5.0f);
	AE_CHECK(parse("layout 1 m.map 00000001\nitem camo at 0 0 0\n") == 0 && layout.zone_count == 0);
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
		AE_CHECK(parse(bad[i]) != 0 && layout.zone_count == 0);
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

/* spawn and flag lines (queue 141: CTF flags and spawns for maps that lack them, as the H2 collection's Terminal) */
static void test_spawn_and_flag_lines(void)
{
	char const *text =
		"layout 1 sidewinder.map 0a1b2c3d\n"
		"clear spawns\n"
		"remove spawn 3\n"
		"spawn at 1 2 3\n"
		"spawn at -4.5 6 0.25 facing 90 team blue games ctf slayer\n"
		"SPAWN AT 0 0 0 games all team red\n"
		"clear flags\n"
		"remove flag 0\n"
		"flag ctf at 10 20 1 team red\n"
		"flag ctf at -10 -20 1 facing 180 team blue\n"
		"flag hill at 0 0 0 team 3\n"
		"flag teleport_from at 1 1 1 team 2\n"
		"flag ball at 5 5 5\n";
	static char const *const bad[] = {
		"layout 1 m.map 00000001\nspawn 0 0 0\n",                         /* no at */
		"layout 1 m.map 00000001\nspawn at 0 0\n",
		"layout 1 m.map 00000001\nspawn at 0 0 0 team green\n",
		"layout 1 m.map 00000001\nspawn at 0 0 0 team red team blue\n",   /* twice */
		"layout 1 m.map 00000001\nspawn at 0 0 0 games\n",
		"layout 1 m.map 00000001\nspawn at 0 0 0 games juggernaut\n",
		"layout 1 m.map 00000001\nspawn at 0 0 0 games ctf ctf ctf ctf ctf\n", /* more than 4 */
		"layout 1 m.map 00000001\nspawn at 0 0 0 facing\n",
		"layout 1 m.map 00000001\nspawn at nan 0 0\n",
		"layout 1 m.map 00000001\nremove spawn\n",
		"layout 1 m.map 00000001\nremove spawn x\n",
		"layout 1 m.map 00000001\nclear spawns now\n",
		"layout 1 m.map 00000001\nflag at 0 0 0\n",
		"layout 1 m.map 00000001\nflag bank at 0 0 0\n",                 /* the vegas bank: no line */
		"layout 1 m.map 00000001\nflag ctf 0 0 0\n",
		"layout 1 m.map 00000001\nflag ctf at 0 0 0 team 16\n",
		"layout 1 m.map 00000001\nflag ctf at 0 0 0 team\n",
		"layout 1 m.map 00000001\nflag ctf at 0 0 0 team red now\n",
		"layout 1 m.map 00000001\nremove flag -1\n",
	};
	unsigned int i;

	AE_CHECK(parse(text) == 0);
	AE_CHECK(layout.clear_spawns && layout.clear_flags);
	AE_CHECK(layout.removed_spawn_count == 1 && layout.removed_spawns[0] == 3);
	AE_CHECK(layout.removed_flag_count == 1 && layout.removed_flags[0] == 0);
	AE_CHECK(layout.spawn_count == 3 && layout.flag_count == 5);
	AE_CHECK(layout.spawns[0].x == 1.0f && layout.spawns[0].z == 3.0f && layout.spawns[0].team == 0 &&
		!layout.spawns[0].has_facing && layout.spawns[0].games[0] == AE_LAYOUT_GAMES_ALL && layout.spawns[0].games[1] == 0);
	AE_CHECK(layout.spawns[1].has_facing && layout.spawns[1].facing_degrees == 90.0f && layout.spawns[1].team == 1 &&
		layout.spawns[1].games[0] == 1 && layout.spawns[1].games[1] == 2 && layout.spawns[1].games[2] == 0);
	AE_CHECK(layout.spawns[2].games[0] == AE_LAYOUT_GAMES_ALL && layout.spawns[2].team == 0 && layout.spawns[2].line == 6);
	AE_CHECK(layout.flags[0].type == _ae_layout_flag_ctf && layout.flags[0].team == 0 && layout.flags[0].y == 20.0f);
	AE_CHECK(layout.flags[1].team == 1 && layout.flags[1].has_facing && layout.flags[1].facing_degrees == 180.0f);
	AE_CHECK(layout.flags[2].type == _ae_layout_flag_hill && layout.flags[2].team == 3);
	AE_CHECK(layout.flags[3].type == _ae_layout_flag_teleport_from && layout.flags[3].team == 2);
	AE_CHECK(layout.flags[4].type == _ae_layout_flag_ball && layout.flags[4].team == 0 && layout.flags[4].line == 13);
	AE_CHECK(strcmp(ae_layout_flag_type_name(_ae_layout_flag_hill), "hill") == 0);
	AE_CHECK(strcmp(ae_layout_flag_type_name(5), "?") == 0);
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		if (parse(bad[i]) != 2)
			printf("FAIL: not refused at line 2: %s", bad[i]);
		AE_CHECK(parse(bad[i]) == 2);
	}
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

static void test_vehicle_lines(void)
{
	char const *text =
		"layout 1 h2d_coagulation d7e4cd2e\n"
		"vehicle warthog at 39.3 -154.1 1.0\n"
		"vehicle Rocket_Warthog at 1 2 3 facing 90 team blue games ctf slayer\n"
		"vehicle banshee at 0 0 10 games king team neutral\n"
		"vehicle 3 at -4.5 6 0.25 facing -45 team red\n";
	short none[4] = { 0, 0, 0, 0 }, ctf_slayer[4] = { 1, 2, 0, 0 }, all[4] = { AE_LAYOUT_GAMES_ALL, 0, 0, 0 };

	AE_CHECK(parse(text) == 0);
	AE_CHECK(layout.vehicle_count == 4 && layout.vehicle_rule_count == 0);
	AE_CHECK(layout.vehicles[0].kind == _ae_layout_vehicle_warthog && !layout.vehicles[0].has_facing);
	AE_CHECK(layout.vehicles[0].team == _ae_layout_team_unset && layout.vehicles[0].games[0] == AE_LAYOUT_GAMES_ALL);
	AE_CHECK(layout.vehicles[0].x > 39.2f && layout.vehicles[0].y < -154.0f && layout.vehicles[0].line == 2);
	AE_CHECK(layout.vehicles[1].kind == _ae_layout_vehicle_rocket_warthog && layout.vehicles[1].has_facing);
	AE_CHECK(layout.vehicles[1].facing_degrees == 90.0f && layout.vehicles[1].team == _ae_layout_team_blue);
	AE_CHECK(layout.vehicles[1].games[0] == 1 && layout.vehicles[1].games[1] == 2 && layout.vehicles[1].games[2] == 0);
	AE_CHECK(layout.vehicles[2].kind == _ae_layout_vehicle_banshee && layout.vehicles[2].games[0] == 4);
	AE_CHECK(layout.vehicles[2].team == _ae_layout_team_neutral);
	AE_CHECK(layout.vehicles[3].kind == _ae_layout_vehicle_palette && layout.vehicles[3].palette_index == 3);
	AE_CHECK(layout.vehicles[3].team == _ae_layout_team_red && layout.vehicles[3].facing_degrees == -45.0f);
	/* (refused: an unknown kind, no point, a word twice, an unknown game, no games, more than 4) */
	AE_CHECK(parse("layout 1 m 00000000\nvehicle hog at 1 2 3\n") == 2);
	AE_CHECK(parse("layout 1 m 00000000\nvehicle ghost\n") == 2);
	AE_CHECK(parse("layout 1 m 00000000\nvehicle ghost at 1 2\n") == 2);
	AE_CHECK(parse("layout 1 m 00000000\nvehicle ghost at 1 2 3 team red team blue\n") == 2);
	AE_CHECK(parse("layout 1 m 00000000\nvehicle ghost at 1 2 3 team green\n") == 2);
	AE_CHECK(parse("layout 1 m 00000000\nvehicle ghost at 1 2 3 games juggernaut\n") == 2);
	AE_CHECK(parse("layout 1 m 00000000\nvehicle ghost at 1 2 3 games\n") == 2);
	AE_CHECK(parse("layout 1 m 00000000\nvehicle ghost at 1 2 3 games ctf slayer king oddball race\n") == 2);
	AE_CHECK(parse("layout 1 m 00000000\nvehicle -1 at 1 2 3\n") == 2);
	/* (the limit) */
	{
		static char many[4096];
		int index, used = snprintf(many, sizeof(many), "layout 1 m 00000000\n");

		for (index = 0; index <= AE_LAYOUT_MAXIMUM_VEHICLES; index++)
			used += snprintf(many + used, sizeof(many) - (size_t)used, "vehicle ghost at %d 0 0\n", index);
		AE_CHECK(parse(many) == AE_LAYOUT_MAXIMUM_VEHICLES + 2);
	}
	/* (kinds by tag name, as the vehicle sets tell them; game types) */
	AE_CHECK(ae_layout_vehicle_kind_of_name("vehicles\\rwarthog\\rwarthog") == _ae_layout_vehicle_rocket_warthog);
	AE_CHECK(ae_layout_vehicle_kind_of_name("vehicles\\warthog\\mp_warthog") == _ae_layout_vehicle_warthog);
	AE_CHECK(ae_layout_vehicle_kind_of_name("vehicles\\ghost\\ghost_mp") == _ae_layout_vehicle_ghost);
	AE_CHECK(ae_layout_vehicle_kind_of_name("vehicles\\scorpion\\scorpion_mp") == _ae_layout_vehicle_scorpion);
	AE_CHECK(ae_layout_vehicle_kind_of_name("vehicles\\banshee\\banshee_mp") == _ae_layout_vehicle_banshee);
	AE_CHECK(ae_layout_vehicle_kind_of_name("vehicles\\c gun turret\\c gun turret_mp") == _ae_layout_vehicle_turret);
	AE_CHECK(ae_layout_vehicle_kind_of_name("protected 12") == NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS);
	AE_CHECK(ae_layout_vehicle_kind_of_name(NULL) == NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS);
	AE_CHECK(!strcmp(ae_layout_vehicle_kind_name(_ae_layout_vehicle_rocket_warthog), "rocket_warthog"));
	AE_CHECK(ae_layout_games_take(all, 5) && ae_layout_games_take(ctf_slayer, 1) && ae_layout_games_take(ctf_slayer, 2));
	AE_CHECK(!ae_layout_games_take(ctf_slayer, 4) && !ae_layout_games_take(none, 1) && !ae_layout_games_take(none, 0));
}

/* weapon packs' weapons by name (ae_weapon_slots.h): their slot kept, the map's fallback chosen when applied */
static void test_pack_weapons(void)
{
	char const *text =
		"layout 1 h2d_turf 0badf00d\n"
		"item smg at 1 2 3 respawn 30\n"
		"item BR at 4 5 6 facing 90\n"
		"item silenced_smg at 7 8 9 drop\n"
		"item carbine at 0 0 0\n"
		"item pistol at 1 1 1\n"
		"item silenced_ar at 2 2 2\n";

	AE_CHECK(parse(text) == 0);
	AE_CHECK(layout.item_count == 6);
	AE_CHECK(layout.items[0].what == _ae_layout_pack && layout.items[0].pack_slot == AE_WEAPON_SLOT_SMG);
	AE_CHECK(layout.items[0].respawn_seconds == 30);
	AE_CHECK(layout.items[1].what == _ae_layout_pack && layout.items[1].pack_slot == AE_WEAPON_SLOT_BATTLE_RIFLE);
	AE_CHECK(layout.items[1].has_facing);
	AE_CHECK(layout.items[2].what == _ae_layout_pack && layout.items[2].pack_slot == AE_WEAPON_SLOT_SMG_SILENCED);
	AE_CHECK(layout.items[2].drop);
	AE_CHECK(layout.items[3].what == _ae_layout_pack && layout.items[3].pack_slot == AE_WEAPON_SLOT_CARBINE);
	/* (the stock alias stays the stock weapon) */
	AE_CHECK(layout.items[4].what == _ae_layout_alias && layout.items[4].alias == _ae_layout_pistol);
	AE_CHECK(layout.items[5].what == _ae_layout_pack && layout.items[5].pack_slot == AE_WEAPON_SLOT_AR_SILENCED);
	/* (not a pack weapon's name, and no tag path: refused) */
	AE_CHECK(parse("layout 1 h2d_turf 0badf00d\nitem smgg at 1 2 3\n") != 0);
	AE_CHECK(!layout.items[0].pack_like);

	/* (0.9.4) a pack weapon with its own fallback: like <n>, the map's item n (a stand-in of another kind) */
	AE_CHECK(parse("layout 1 h2d_gemini 61bc7743\nitem brute_shot like 5 at -288.35 -18.21 -42.76 respawn 30\n"
		"item brute_shot at 1 2 3\n") == 0);
	AE_CHECK(layout.item_count == 2);
	AE_CHECK(layout.items[0].what == _ae_layout_pack && layout.items[0].pack_slot == AE_WEAPON_SLOT_BRUTE_SHOT);
	AE_CHECK(layout.items[0].pack_like && layout.items[0].like == 5 && layout.items[0].respawn_seconds == 30);
	AE_CHECK(layout.items[0].x == -288.35f && layout.items[0].z == -42.76f);
	AE_CHECK(layout.items[1].what == _ae_layout_pack && !layout.items[1].pack_like);
	/* (like with no number, or a word: refused) */
	AE_CHECK(parse("layout 1 h2d_gemini 61bc7743\nitem brute_shot like at 1 2 3\n") != 0);
	AE_CHECK(parse("layout 1 h2d_gemini 61bc7743\nitem brute_shot like five at 1 2 3\n") != 0);
	AE_CHECK(parse("layout 1 h2d_gemini 61bc7743\nitem brute_shot like\n") != 0);
}

/* a bundle of layouts (several files one after another): a map's section by its header's name */
static void test_spawner_lines(void)
{
	char const *text =
		"# a comment first\n"
		"layout 1 h2d_a 0000abcd   # the map\n"
		"item smg at 1 2 3\n"
		"spawner 4 kind pad tier power logic static respawn 120\n"
		"  channel SNIPE symmetrical\r\n"
		"auto off\n"
		"snd spawner 9 split\n"
		"spawner at 1 2 3 kind rack class mid tier base\n";
	char out[512];
	long length;

	AE_CHECK(ae_layout_parse(text, (long)strlen(text), &layout, NULL, 0) == 0);
	AE_CHECK(layout.spawner_line_count == 4 && layout.item_count == 1);
	length = ae_layout_spawner_text(text, (long)strlen(text), out, sizeof(out));
	AE_CHECK(length == (long)strlen(out));
	AE_CHECK(!strcmp(out, "spawners 1 h2d_a 0000abcd   # the map\n"
		"spawner 4 kind pad tier power logic static respawn 120\n"
		"  channel SNIPE symmetrical\n"
		"auto off\n"
		"spawner at 1 2 3 kind rack class mid tier base\n"));
	AE_CHECK(ae_layout_spawner_text(text, (long)strlen(text), out, 40) == -1);
	AE_CHECK(ae_layout_spawner_text("item smg at 1 2 3\n", 18, out, sizeof(out)) == -1);
	AE_CHECK(ae_layout_spawner_text("layout 1 x 00000001\n", 20, out, sizeof(out)) == 22 && !strcmp(out, "spawners 1 x 00000001\n"));
	AE_CHECK(ae_layout_spawner_word("SPAWNER") && !ae_layout_spawner_word("spawn") && !ae_layout_spawner_word(NULL));
}

static void test_bundle(void)
{
	char const *bundle =
		"# comment before the first section\n"
		"layout 1 h2d_a 00000001\n"
		"item smg at 1 2 3\n"
		"layout 1 H2D_B.map 00000002\r\n"
		"item carbine at 4 5 6\n"
		"  layout 1 h2d_c 00000003\n"
		"item BR at 7 8 9\n"
		"layout 1 h2d_d 00000004\n";
	long start = -1, length = -1;
	long size = (long)strlen(bundle);

	AE_CHECK(ae_layout_bundle_find(bundle, size, "h2d_a", &start, &length));
	AE_CHECK(!strncmp(bundle + start, "layout 1 h2d_a", 14) && length == (long)strlen("layout 1 h2d_a 00000001\nitem smg at 1 2 3\n"));
	AE_CHECK(parse_n(bundle + start, length) == 0 && layout.item_count == 1 && layout.map_hash == 1);
	/* (case aside, with or without the extension; a crlf header) */
	AE_CHECK(ae_layout_bundle_find(bundle, size, "h2d_b", &start, &length));
	AE_CHECK(!strncmp(bundle + start, "layout 1 H2D_B.map 00000002\r\n", 29));
	/* (an indented line is not a header: the b section runs on past it, to the d section) */
	AE_CHECK(ae_layout_bundle_find(bundle, size, "H2D_B.MAP", &start, &length));
	AE_CHECK(parse_n(bundle + start, length) != 0);
	/* (the last section, with nothing after its header) */
	AE_CHECK(ae_layout_bundle_find(bundle, size, "h2d_d", &start, &length) && start + length == size);
	AE_CHECK(!ae_layout_bundle_find(bundle, size, "h2d_c", &start, &length));
	AE_CHECK(!ae_layout_bundle_find(bundle, size, "h2d_", &start, &length));
	AE_CHECK(!ae_layout_bundle_find(bundle, size, "", &start, &length));
	AE_CHECK(!ae_layout_bundle_find(NULL, 0, "h2d_a", &start, &length));
}

/* the layouts the game ships (port/assets/ae/layouts.txt, read from the repository root): each reads whole, for the
map file and hash its header names */
static void test_shipped_layouts(void)
{
	FILE *file = fopen("port/assets/ae/layouts.txt", "rb");
	static char text[200000];
	long size, start = 0, length = 0;

	if (!file)
		return;
	size = (long)fread(text, 1, sizeof(text) - 1, file);
	fclose(file);
	AE_CHECK(size > 0);
	AE_CHECK(ae_layout_bundle_find(text, size, "h2d_foundation", &start, &length));
	AE_CHECK(parse_n(text + start, length) == 0);
	AE_CHECK(ae_layout_for_map(&layout, "h2d_foundation", 0xf3f9d694UL));
	AE_CHECK(layout.item_count == 11 && layout.removed_count == 21 && !layout.clear_items);
	/* (a Workshop map's section: its whole identity is the file name) */
	AE_CHECK(ae_layout_bundle_find(text, size, "h2-ascension@ws3421353354", &start, &length));
	AE_CHECK(parse_n(text + start, length) == 0);
	AE_CHECK(ae_layout_for_map(&layout, "h2-ascension@ws3421353354", 0x29741628UL));
	AE_CHECK(layout.item_count == 5 && layout.removed_count == 8 && !layout.clear_items);
	/* (AE's needler on the map's needler spot, the map's item 7 its fallback) */
	AE_CHECK(layout.items[4].what == _ae_layout_pack && layout.items[4].pack_slot == AE_WEAPON_SLOT_NEEDLER &&
		layout.items[4].pack_like && layout.items[4].like == 7);
	/* (the community comp maps' sections: a comp section alone, every one of its items the COMP set's) */
	{
		static struct { char const *file; unsigned long hash; short removed, items; } const comp[] =
		{
			{ "h1pb_prisoner", 0x424d0b7cUL, 15, 5 },
			{ "h1pb_wizard", 0x3b6612a1UL, 18, 5 },
			{ "h1pb_chillout", 0xed622e44UL, 7, 4 },
			{ "h1pb_hangemhigh", 0xbb9fefa6UL, 16, 6 },
		};
		unsigned index;
		short item;

		for (index = 0; index < sizeof(comp) / sizeof(comp[0]); index++)
		{
			AE_CHECK(ae_layout_bundle_find(text, size, comp[index].file, &start, &length));
			AE_CHECK(parse_n(text + start, length) == 0 && ae_layout_for_map(&layout, comp[index].file, comp[index].hash));
			AE_CHECK(layout.comp_section && !layout.removed_count && layout.comp_removed_count == comp[index].removed &&
				layout.item_count == comp[index].items);
			for (item = 0; item < layout.item_count; item++)
				AE_CHECK(layout.items[item].comp && layout.items[item].games_all);
		}
	}
}

/* snd lines: read only for a SEARCH AND DESTROY game, left out unread in any other */
static void test_snd_lines(void)
{
	char const *text =
		"layout 1 h2d_salicreek f1955e18\n"
		"remove item 7\n"
		"snd remove flag 2\n"
		"snd remove flag 3\n"
		"snd remove scenery \"teleporter shield\"\n"
		"snd nonsense that would be refused\n";

	AE_CHECK(parse(text) == 0);
	AE_CHECK(layout.removed_count == 1 && layout.removed_flag_count == 0 && layout.object_rule_count == 0);
	AE_CHECK(ae_layout_parse_for(text, (long)strlen(text), 1, &layout, error, sizeof(error)) == 6);
	text =
		"layout 1 h2d_salicreek f1955e18\n"
		"snd remove flag 2\n"
		"snd remove flag 3\n"
		"snd remove scenery \"teleporter shield\"\n";
	AE_CHECK(ae_layout_parse_for(text, (long)strlen(text), 1, &layout, error, sizeof(error)) == 0);
	AE_CHECK(layout.removed_flag_count == 2 && layout.removed_flags[0] == 2 && layout.removed_flags[1] == 3 &&
		layout.object_rule_count == 1);
	AE_CHECK(parse("layout 1 a.map 00000000\nsnd\n") == 2);
	AE_CHECK(parse("snd layout 1 a.map 00000000\n") == 1);
}

/* comp ... end comp: AE's comp positions, in play only under the COMP weapon set; item and remove item lines only */
static void test_comp_section(void)
{
	char const *text =
		"layout 1 h1pb_prisoner 01234567\n"
		"remove item 2\n"
		"item battle_rifle like 3 at 1 2 3\n"
		"comp\n"
		"remove item 4\n"
		"remove item 5\n"
		"item sniper at 4 5 6 respawn 120\n"
		"end comp\n"
		"item smg like 7 at 7 8 9\n";

	AE_CHECK(parse(text) == 0);
	AE_CHECK(layout.comp_section && layout.removed_count == 1 && layout.removed[0] == 2);
	AE_CHECK(layout.comp_removed_count == 2 && layout.comp_removed[0] == 4 && layout.comp_removed[1] == 5);
	AE_CHECK(layout.item_count == 3 && !layout.items[0].comp && layout.items[1].comp && !layout.items[2].comp);
	AE_CHECK(layout.items[1].respawn_seconds == 120);
	AE_CHECK(parse("layout 1 a.map 00000000\nitem smg like 1 at 0 0 0\n") == 0 && !layout.comp_section &&
		!layout.comp_removed_count);
	/* (refused: a section not ended, one inside another, a second one, an end without one, another kind of line) */
	AE_CHECK(parse("layout 1 a.map 00000000\ncomp\nremove item 1\n") == 3);
	AE_CHECK(parse("layout 1 a.map 00000000\ncomp\ncomp\n") == 3);
	AE_CHECK(parse("layout 1 a.map 00000000\ncomp\nend comp\ncomp\nend comp\n") == 4);
	AE_CHECK(parse("layout 1 a.map 00000000\nend comp\n") == 2);
	AE_CHECK(parse("layout 1 a.map 00000000\ncomp\nclear items\nend comp\n") == 3);
	AE_CHECK(parse("layout 1 a.map 00000000\ncomp\nspawn at 0 0 0\nend comp\n") == 3);
	AE_CHECK(parse("layout 1 a.map 00000000\ncomp now\nend comp\n") == 2);
	AE_CHECK(parse("layout 1 a.map 00000000\ncomp\nend\n") == 3);
	AE_CHECK(parse("layout 1 a.map 00000000\nsnd comp\nsnd end comp\n") == 2);
	AE_CHECK(parse("layout 1 a.map 00000000\ncomp\nsnd end comp\n") == 3);
	/* (in play: a comp item only under COMP, a map item it removes in every game but COMP's) */
	AE_CHECK(ae_layout_item_in_play(AE_LAYOUT_ITEM_ALWAYS, 0) && ae_layout_item_in_play(AE_LAYOUT_ITEM_ALWAYS, 1));
	AE_CHECK(!ae_layout_item_in_play(AE_LAYOUT_ITEM_COMP_ONLY, 0) && ae_layout_item_in_play(AE_LAYOUT_ITEM_COMP_ONLY, 1));
	AE_CHECK(ae_layout_item_in_play(AE_LAYOUT_ITEM_NOT_COMP, 0) && !ae_layout_item_in_play(AE_LAYOUT_ITEM_NOT_COMP, 1));
}

int main(void)
{
	test_good_file();
	test_tag_paths();
	test_refusals();
	test_bot_lines();
	test_jump_line();
	test_zone_lines();
	test_spawn_and_flag_lines();
	test_limits();
	test_vehicle_lines();
	test_pack_weapons();
	test_snd_lines();
	test_bundle();
	test_spawner_lines();
	test_shipped_layouts();
	test_comp_section();
	printf("%s\n", failures ? "FAILED" : "ok");
	return failures != 0;
}
