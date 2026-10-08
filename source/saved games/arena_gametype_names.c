/*
ARENA_GAMETYPE_NAMES.C

port: Halo CE: Arena Evolved, the seeded gametypes' names: each one's stored
name (game_variant's, at most 11 characters: what every machine, listing and
saved file has) and the longer display name AE machines show in their lists,
lobby and scoreboard, its set (AE, AE COMP, Halo 1: NHE's, TRAINING), its
place in the set, and a description for a panel.

The table holds every gametype of the final sets, seeded or not yet (one not
seeded is simply never on disk). Old stored names (aliases) report their
successor's set, so a kept old file sorts beside it, and show their own
stored name. Lookups are by stored name, letters' case aside, so a player's
edited copy of a seeded gametype gets its display name too (a renamed one
does not).

Pure: no file system, no engine state (tools/arena_gametype_names_check.c
checks it on its own).
*/

/* ---------- headers */

#include "cseries/cseries.h"
#include "saved games/arena_gametypes.h"
#include "game/game_engine.h"

/* ---------- constants */

enum
{
	/* a stored name's characters (game_variant's 12 with the NUL) */
	ARENA_STORED_NAME_MAXIMUM = 11,
	/* a display name's */
	ARENA_DISPLAY_NAME_MAXIMUM = 31,
};

/* ---------- structures */

struct arena_gametype_name
{
	char const *stored_name;
	char const *display_name;
	short set;
	short game_engine;
	boolean team;
	boolean special;
	char const *description;
};

struct arena_gametype_alias
{
	char const *stored_name;
	/* (the table's stored name it became) */
	char const *successor;
};

/* ---------- globals */

/* the sets in the lists' order, each in its own order */
static struct arena_gametype_name const arena_gametype_names[] =
{
	/* AE casual */
	{ "AE FFA SLAY", "FFA AE SLAYER", _arena_gametype_set_ae, game_engine_slayer, FALSE, FALSE,
		"Free for all slayer on AE's rules: Halo 2 health, no fall damage, no spread, a pre-game countdown, "
		"power item timers and the motion tracker." },
	{ "AE TEAM SLY", "TEAM AE SLAYER", _arena_gametype_set_ae, game_engine_slayer, TRUE, FALSE,
		"Team slayer on AE's rules, with power item timers and the motion tracker." },
	{ "AE CTF", "TEAM AE CTF", _arena_gametype_set_ae, game_engine_ctf, TRUE, FALSE,
		"Capture the flag on AE's rules, with power item timers and the motion tracker." },
	{ "AE KING", "TEAM AE KING", _arena_gametype_set_ae, game_engine_king, TRUE, FALSE,
		"Team King of the Hill on AE's rules, with power item timers and the motion tracker." },
	{ "AE FFA BALL", "FFA AE ODDBALL", _arena_gametype_set_ae, game_engine_oddball, FALSE, FALSE,
		"Free for all Oddball on AE's rules, with power item timers and the motion tracker." },
	{ "AE TEAM OB", "TEAM AE ODDBALL", _arena_gametype_set_ae, game_engine_oddball, TRUE, FALSE,
		"Team Oddball on AE's rules, with power item timers and the motion tracker." },
	{ "AE 2V2 SLY", "TEAM AE 2V2 SLAYER", _arena_gametype_set_ae, game_engine_slayer, TRUE, FALSE,
		"Team slayer for two against two (split screen too), to 25 kills, on AE's rules." },
	{ "AE 2V2 CTF", "TEAM AE 2V2 CTF", _arena_gametype_set_ae, game_engine_ctf, TRUE, FALSE,
		"Capture the flag for two against two (split screen too), on AE's rules." },
	{ "AE 2V2 KING", "TEAM AE 2V2 KING", _arena_gametype_set_ae, game_engine_king, TRUE, FALSE,
		"Team King of the Hill for two against two (split screen too), on AE's rules." },
	{ "AE 2V2 BALL", "TEAM AE 2V2 ODDBALL", _arena_gametype_set_ae, game_engine_oddball, TRUE, FALSE,
		"Team Oddball for two against two (split screen too), on AE's rules." },
	{ "AE PRACTICE", "FFA AE PRACTICE", _arena_gametype_set_ae, game_engine_slayer, FALSE, FALSE,
		"Free for all practice: every weapon and powerup respawns every 30 seconds; 500 kills to win, no time limit." },
	/* AE casual's special team modes */
	{ "AE SNIPERS", "TEAM AE SNIPERS", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with sniper rifles and pistols, sniper ammo on the map, no motion tracker." },
	{ "AE SHOTSNIP", "TEAM AE SHOTTY SNIPERS", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with a shotgun and a sniper rifle each and no weapons on the map, no motion tracker." },
	{ "AE SWAT", "TEAM AE SWAT", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with pistols, no shields and classic health: a headshot kills. No motion tracker." },
	{ "AE ROCKETS", "TEAM AE ROCKETS", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with rocket launchers." },
	{ "AE SHOTGUNS", "TEAM AE SHOTGUNS", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with shotguns and pistols." },
	{ "AE HEAVIES", "TEAM AE HEAVIES", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with rocket launchers, assault rifles, the heavy weapons and the map's vehicles, to 75 kills." },
	/* AE casual on Halo 1: NHE's maps' own modes */
	{ "AE VANILLA", "TEAM AE VANILLA", _arena_gametype_set_ae, game_engine_slayer, TRUE, FALSE,
		"Team slayer on AE's rules. On Halo 1: NHE's maps, NHE's VANILLA mode (no timers, no countdown)." },
	{ "AE POWERUPS", "TEAM AE POWERUPS", _arena_gametype_set_ae, game_engine_slayer, TRUE, FALSE,
		"Team slayer on AE's rules. On Halo 1: NHE's maps, NHE & POWERUPS." },

	/* AE COMP */
	{ "AE COMP FFA", "FFA AE COMP SLAYER", _arena_gametype_set_ae_comp, game_engine_slayer, FALSE, FALSE,
		"Competitive free for all: 5 second respawns, no motion tracker, power item timers only in line of sight, "
		"25 kills." },
	{ "AE COMP TS", "TEAM AE COMP SLAYER", _arena_gametype_set_ae_comp, game_engine_slayer, TRUE, FALSE,
		"Competitive team slayer: 5 second respawns, no motion tracker, power item timers only in line of sight, "
		"50 kills." },
	{ "AE COMP CTF", "TEAM AE COMP CTF", _arena_gametype_set_ae_comp, game_engine_ctf, TRUE, FALSE,
		"Competitive capture the flag: 3 captures, 5 second respawns, no motion tracker; the flags show only in "
		"line of sight." },
	{ "AE COMP KOH", "TEAM AE COMP KING", _arena_gametype_set_ae_comp, game_engine_king, TRUE, FALSE,
		"Competitive team King of the Hill: 5 minutes in the hill, 5 second respawns; the hill shows only in line "
		"of sight." },
	{ "AE COMP OB", "TEAM AE COMP ODDBALL", _arena_gametype_set_ae_comp, game_engine_oddball, TRUE, FALSE,
		"Competitive team Oddball: 5 minutes with the ball, 5 second respawns; the ball shows only in line of sight." },

	/* Halo 1: NHE's 23 gametypes, in its own order (010 to 230) */
	{ "TS 50", "TEAM NHE SLAYER 50", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"NHE's TS 50: 50 kills, 5 second respawns, no motion tracker. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "TS 100", "TEAM NHE SLAYER 100", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"NHE's TS 100: 100 kills, 5 second respawns, a 10 second suicide penalty, no motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "TS ON-OFF", "TEAM NHE SLAYER ON-OFF", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"NHE's TS ON-OFF: 50 kills, 5 second respawns, no motion tracker. On Halo 1: NHE's maps, TIMER ONLY." },
	{ "TS TRAINING", "TEAM NHE TRAINING", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"NHE's TS TRAINING: 50 kills, 5 second respawns, no motion tracker. On Halo 1: NHE's maps, TRAINING." },
	{ "TS PRACTICE", "TEAM NHE PRACTICE", _arena_gametype_set_nhe, game_engine_race, TRUE, FALSE,
		"NHE's TS PRACTICE: instant respawns, infinite grenades, every weapon and powerup every 30 seconds. "
		"On Halo 1: NHE's maps, TRAINING." },
	{ "TS SNIPERS", "TEAM NHE SNIPERS", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"NHE's TS SNIPERS: invisible players with sniper rifles, 50 kills, 5 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "FFA 50 NR", "FFA NHE SLAYER 50", _arena_gametype_set_nhe, game_engine_slayer, FALSE, FALSE,
		"NHE's FFA 50: 50 kills, 5 second respawns, no motion tracker. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "FFA 50 R", "FFA NHE SLAYER 50 RADAR", _arena_gametype_set_nhe, game_engine_slayer, FALSE, FALSE,
		"NHE's FFA 50 with the motion tracker: 50 kills, 5 second respawns. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "1 V 1 NR", "FFA NHE 1V1", _arena_gametype_set_nhe, game_engine_slayer, FALSE, FALSE,
		"NHE's 1 V 1: 15 kills, 5 second respawns, no motion tracker. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "1 V 1 R", "FFA NHE 1V1 RADAR", _arena_gametype_set_nhe, game_engine_slayer, FALSE, FALSE,
		"NHE's 1 V 1 with the motion tracker: 15 kills, 5 second respawns. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "KOTH 5M 7S", "TEAM NHE KING 7.5S", _arena_gametype_set_nhe, game_engine_king, TRUE, FALSE,
		"NHE's King of the Hill: 5 minutes in a moving hill, 7.5 second respawns, a 10 second suicide penalty. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "KOTH 5M 10S", "TEAM NHE KING 10S", _arena_gametype_set_nhe, game_engine_king, TRUE, FALSE,
		"NHE's King of the Hill: 5 minutes in a moving hill, 10 second respawns. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "BALL 5M 7S", "TEAM NHE ODDBALL 7.5S", _arena_gametype_set_nhe, game_engine_oddball, TRUE, FALSE,
		"NHE's Oddball: 5 minutes with the ball (slow while you carry it), 7.5 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "BALL 5M 10S", "TEAM NHE ODDBALL 10S", _arena_gametype_set_nhe, game_engine_oddball, TRUE, FALSE,
		"NHE's Oddball: 5 minutes with the ball (slow while you carry it), 10 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 3C 7S", "TEAM NHE CTF 3 7.5S", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF: 3 captures, your flag at home to score, 7.5 second respawns, no motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 3C 7S R", "TEAM NHE CTF 3 7.5S RADAR", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF with the motion tracker: 3 captures, your flag at home to score, 7.5 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 3C 10S", "TEAM NHE CTF 3 10S", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF: 3 captures, your flag at home to score, 10 second respawns, no motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 3 10S R", "TEAM NHE CTF 3 10S RADAR", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF with the motion tracker: 3 captures, your flag at home to score, 10 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 5C 7S", "TEAM NHE CTF 5 7.5S", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF: 5 captures, your flag at home to score, 7.5 second respawns, no motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 5C 7S R", "TEAM NHE CTF 5 7.5S RADAR", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF with the motion tracker: 5 captures, your flag at home to score, 7.5 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 5C 10S", "TEAM NHE CTF 5 10S", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF: 5 captures, your flag at home to score, 10 second respawns, no motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 5 10S R", "TEAM NHE CTF 5 10S RADAR", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF with the motion tracker: 5 captures, your flag at home to score, 10 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF WIZARD", "TEAM NHE CTF WIZARD", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF WIZARD: pistols, 5 captures, 10 second respawns, the motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },

	/* TRAINING */
	{ "AE TRAINING", "FFA AE TRAINING", _arena_gametype_set_training, game_engine_slayer, FALSE, FALSE,
		"Free for all training: every item's spawn and the players' spawns marked, with timers; 500 kills to win, "
		"no time limit." },
};

/* old stored names: the seeds of before, kept by players or not yet moved */
static struct arena_gametype_alias const arena_gametype_aliases[] =
{
	{ "AE SLAYER", "AE FFA SLAY" },
	{ "AE ODDBALL", "AE FFA BALL" },
	{ "AE PRO FFA", "AE COMP FFA" },
	{ "AE PRO TS", "AE COMP TS" },
	{ "AE PRO CTF", "AE COMP CTF" },
	{ "AE PRO KING", "AE COMP KOH" },
	{ "AE PRO BALL", "AE COMP OB" },
	{ "NHE 1V1", "1 V 1 NR" },
	{ "NHE 2V2 TS", "TS 50" },
	{ "NHE CTF", "CTF 3C 7S" },
	{ "PRACTICE", "TS PRACTICE" },
	{ "NHE VANILLA", "AE VANILLA" },
	{ "NHE POWERUP", "AE POWERUPS" },
	{ "NHE TRAIN", "AE TRAINING" },
};

static char const *const arena_gametype_set_names[NUMBER_OF_ARENA_GAMETYPE_SETS] =
{
	"", "AE", "AE COMP", "NHE", "TRAINING"
};

/* ---------- private code */

/* a stored name (wide, NUL-ended or 11 characters at most) and an ASCII
one, letters' case aside */
static boolean arena_gametype_name_equal(
	wchar_t const *stored_name,
	char const *name)
{
	short index;

	for (index = 0; index <= ARENA_STORED_NAME_MAXIMUM; index++)
	{
		wchar_t a = index < ARENA_STORED_NAME_MAXIMUM ? stored_name[index] : 0;
		wchar_t b = (wchar_t)(unsigned char)name[index];

		if (a >= 'a' && a <= 'z')
			a = (wchar_t)(a - 'a' + 'A');
		if (b >= 'a' && b <= 'z')
			b = (wchar_t)(b - 'a' + 'A');
		if (a != b)
			return FALSE;
		if (!a)
			return TRUE;
	}
	return FALSE;
}

/* two of the table's ASCII names, exactly (no C library: the check links
this file alone) */
static boolean arena_gametype_ascii_equal(
	char const *a,
	char const *b)
{
	for (; *a == *b; a++, b++)
	{
		if (!*a)
			return TRUE;
	}
	return FALSE;
}

/* the table's entry of that stored name, else NONE */
static short arena_gametype_name_index(
	wchar_t const *stored_name)
{
	short index;

	for (index = 0; index < (short)NUMBEROF(arena_gametype_names); index++)
	{
		if (arena_gametype_name_equal(stored_name, arena_gametype_names[index].stored_name))
			return index;
	}
	return NONE;
}

/* the entry's place among its set's */
static short arena_gametype_name_order(
	short name_index)
{
	short order = 0;
	short index;

	for (index = 0; index < name_index; index++)
	{
		if (arena_gametype_names[index].set == arena_gametype_names[name_index].set)
			order++;
	}
	return order;
}

static void arena_gametype_info_fill(
	short name_index,
	struct arena_gametype_info *info)
{
	struct arena_gametype_name const *name = &arena_gametype_names[name_index];

	info->set = name->set;
	info->order = arena_gametype_name_order(name_index);
	info->team = name->team;
	info->special = name->special;
	info->game_engine = name->game_engine;
	info->display_name = name->display_name;
	info->description = name->description;
}

/* ---------- public code */

boolean arena_gametype_info(
	wchar_t const *stored_name,
	struct arena_gametype_info *info)
{
	short name_index;
	short index;

	if (!stored_name)
		return FALSE;
	name_index = arena_gametype_name_index(stored_name);
	if (name_index != NONE)
	{
		if (info)
			arena_gametype_info_fill(name_index, info);
		return TRUE;
	}
	for (index = 0; index < (short)NUMBEROF(arena_gametype_aliases); index++)
	{
		struct arena_gametype_alias const *alias = &arena_gametype_aliases[index];
		short successor;

		if (!arena_gametype_name_equal(stored_name, alias->stored_name))
			continue;
		successor = NONE;
		for (name_index = 0; name_index < (short)NUMBEROF(arena_gametype_names); name_index++)
		{
			if (arena_gametype_ascii_equal(arena_gametype_names[name_index].stored_name, alias->successor))
				successor = name_index;
		}
		if (successor == NONE)
			return FALSE;
		if (info)
		{
			/* (its successor's place, its own name) */
			arena_gametype_info_fill(successor, info);
			info->special = FALSE;
			info->display_name = alias->stored_name;
			info->description = NULL;
		}
		return TRUE;
	}
	return FALSE;
}

void arena_gametype_display_name(
	wchar_t const *stored_name,
	wchar_t *display_name,
	short size)
{
	short name_index;
	short index;

	if (!display_name || size <= 0)
		return;
	display_name[0] = 0;
	if (!stored_name)
		return;
	name_index = arena_gametype_name_index(stored_name);
	if (name_index != NONE)
	{
		char const *name = arena_gametype_names[name_index].display_name;

		for (index = 0; index < size - 1 && name[index]; index++)
			display_name[index] = (wchar_t)(unsigned char)name[index];
		display_name[index] = 0;
		return;
	}
	/* (the stored name as it is, an alias's too) */
	for (index = 0; index < size - 1 && index < ARENA_STORED_NAME_MAXIMUM && stored_name[index]; index++)
		display_name[index] = stored_name[index];
	display_name[index] = 0;
}

char const *arena_gametype_set_name(
	short set)
{
	return set >= 0 && set < NUMBER_OF_ARENA_GAMETYPE_SETS ? arena_gametype_set_names[set] : "";
}

short arena_gametype_catalog_count(
	void)
{
	return (short)NUMBEROF(arena_gametype_names);
}

char const *arena_gametype_catalog_stored_name(
	short index)
{
	return index >= 0 && index < (short)NUMBEROF(arena_gametype_names) ? arena_gametype_names[index].stored_name :
		NULL;
}
