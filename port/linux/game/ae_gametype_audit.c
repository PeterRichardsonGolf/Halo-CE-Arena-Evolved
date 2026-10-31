/*
AE_GAMETYPE_AUDIT.C

debug.arena_audit (HALO_ARENA_AUDIT; ae_gametype_audit.h): after the arena
gametypes are seeded, the save root's z/arena_audit.tsv lists every field of
each seeded gametype as its file has it (struct game_variant whole: the
universal variant's fields, each flag bit, pad5's and pack_loadout's AE
fields, its engine's own union fields; and struct game_variant_options), one
row per gametype of arena_gametype_names.c's catalog (the same names as
arena_gametypes.c's table), then one row per stock builder
(game_engine_playlist.h's build_game_variant_*, with the PC options a new
gametype gets from it) for comparison. Columns are named by the structs'
members (game_engine.h). tools/ae_gametype_audit.py reads it.

A fresh save root audits the seeds as this build writes them; an old one,
the files as they are now (the "as_seeded" column says which still match).
Off (the default), nothing is read or written.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game_engine.h"
#include "game/game_engine_playlist.h"
#include "saved games/arena_gametypes.h"
#include "saved games/playlist_profile.h"
#include "saved games/saved_game_files.h"

#include "ae_bomb_snd_rules.h"
#include "ae_weapon_slots.h"
#include "ae_saved_limits.h"
#include "ae_gametype_audit.h"
#include "ae_sensor_range_rules.h"
#include "ae_weapon_rules.h"

#include <stddef.h>
#include <stdio.h>
/* (MAX_GAMENAME: a saved game's display name) */
#include <xtl.h>

int config_boolean(const char *name);
const char *platform_save_root(void);

/* (an Xbox path, as the game's fopen takes: the save root's z/arena_audit.tsv) */
#define AE_AUDIT_FILE_NAME "z:\\arena_audit.tsv"
#define AE_AUDIT_HOST_NAME "z/arena_audit.tsv"
/* a stored name: game_variant's 11 characters and its NUL */
#define AE_AUDIT_NAME_LENGTH 12
#define AE_AUDIT_DISPLAY_LENGTH 64

enum
{
	_audit_variant = 0,
	_audit_options,
};

enum
{
	/* a field read whole, as its type */
	_audit_boolean = 0,
	_audit_byte,
	_audit_short,
	_audit_word,
	_audit_long,
	_audit_real,
	/* some bits of a byte or a long field (mask), shifted down to their lowest */
	_audit_bits_byte,
	_audit_bits_long,
};

struct ae_audit_column
{
	char const *name;
	/* NONE: every row's; else the engine (enum game_engine_type) whose union view it is */
	short engine;
	short owner;
	short kind;
	short offset;
	unsigned long mask;
};

/* each column: name, engine, owner, kind, offset, mask */
#define VC(name, field, kind) { name, NONE, _audit_variant, kind, (short)offsetof(struct game_variant, field), 0 }
#define EC(engine, name, field, kind) \
	{ name, engine, _audit_variant, kind, (short)offsetof(struct game_variant, field), 0 }
#define OC(name, field, kind) \
	{ name, NONE, _audit_options, kind, (short)offsetof(struct game_variant_options, field), 0 }
#define BITS(name, field, kind, mask) \
	{ name, NONE, _audit_variant, kind, (short)offsetof(struct game_variant, field), (mask) }
/* some bits of an engine's own byte (its union view only) */
#define EBITS(engine, name, field, mask) \
	{ name, engine, _audit_variant, _audit_bits_byte, (short)offsetof(struct game_variant, field), (mask) }

#define FLAG_BIT(name, bit) BITS("universal_variant.flags." name, universal_variant.flags, _audit_bits_long, 1UL << (bit))
#define PAD5(name, mask) BITS("universal_variant.pad5." name, universal_variant.pad5, _audit_bits_byte, (mask))
#define PACK(name, mask) \
	BITS("universal_variant.pack_loadout." name, universal_variant.pack_loadout, _audit_bits_byte, (mask))
#define VEHICLES(side, index, vehicle, name) \
	OC("options.vehicle_counts." side "." name, vehicle_counts[index][vehicle], _audit_byte)

static struct ae_audit_column const ae_audit_columns[] =
{
	VC("game_variant.game_engine_index", game_engine_index, _audit_long),
	VC("game_variant.flags", flags, _audit_word),
	VC("universal_variant.teams", universal_variant.teams, _audit_boolean),
	VC("universal_variant.nhe_mode", universal_variant.nhe_mode, _audit_byte),
	VC("universal_variant.starting_frags", universal_variant.starting_frags, _audit_byte),
	VC("universal_variant.ball_melee", universal_variant.ball_melee, _audit_byte),
	/* (each bit of the flags: game_engine.h's _game_variant_*_bit names; the unnamed ones by number) */
	FLAG_BIT("draw_object_in_motion_sensor", _game_variant_draw_object_in_motion_sensor_bit),
	FLAG_BIT("allow_friendly_navpoints", _game_variant_allow_friendly_navpoints_bit),
	FLAG_BIT("infinite_grenades", _game_variant_infinite_grenades_bit),
	FLAG_BIT("no_shields", _game_variant_no_shields_bit),
	FLAG_BIT("always_invisible", _game_variant_always_invisible_bit),
	FLAG_BIT("generic_starting_equipment", _game_variant_generic_starting_equipment_bit),
	/* (bits 6-7: SENSOR RANGE, 0 stock, 1 10 m, 2 15 m, 3 25 m: ae_sensor_range_rules.h, seed revision 19) */
	BITS("universal_variant.flags.sensor_range", universal_variant.flags, _audit_bits_long, AE_SENSOR_RANGE_MASK),
	/* (bit 8: CONTROLLER CROUCH, 1 STAYS DOWN, 0 HALO CE: ae_crouch_rules.h, seed revision 21) */
	FLAG_BIT("controller_crouch", _game_variant_controller_crouch_bit),
	/* (bit 9: MELEE UNZOOMS, 1 ON, 0 OFF: ae_melee_unzoom_rules.h, seed revision 22) */
	FLAG_BIT("melee_unzoom", _game_variant_melee_unzoom_bit),
	/* (bit 10: MOUSE AIM ASSIST, 1 OFF (blocked), 0 ALLOWED: ae_mouse_aim_rules.h, seed revision 22) */
	FLAG_BIT("mouse_aim_assist_off", _game_variant_block_mouse_aim_assist_bit),
	/* (bit 11: VEHICLES DESTRUCTIBLE, 1 ON, 0 OFF: ae_vehicle_death_rules.h, seed revision 23) */
	FLAG_BIT("vehicles_destructible", _game_variant_vehicles_destructible_bit),
	/* (bits 12-15: a later pack as a start weapon, two bits the PRIMARY's then the SECONDARY's: ae_weapon_slots.h) */
	BITS("universal_variant.flags.pack_start_later", universal_variant.flags, _audit_bits_long, AE_WEAPON_SLOTS_LATER_MASK),
	FLAG_BIT("no_falling_damage", _game_variant_no_falling_damage_bit),
	FLAG_BIT("item_timers", _game_variant_item_timers_bit),
	FLAG_BIT("training", _game_variant_training_bit),
	FLAG_BIT("health_style_first", _game_variant_health_style_first_bit),
	FLAG_BIT("health_style_second", _game_variant_health_style_second_bit),
	FLAG_BIT("no_spread", _game_variant_no_spread_bit),
	FLAG_BIT("pregame_countdown", _game_variant_pregame_countdown_bit),
	FLAG_BIT("practice", _game_variant_practice_bit),
	FLAG_BIT("no_spread_full", _game_variant_no_spread_full_bit),
	FLAG_BIT("no_spawn_heat", _game_variant_no_spawn_heat_bit),
	FLAG_BIT("item_waypoints", _game_variant_item_waypoints_bit),
	FLAG_BIT("objective_in_sight", _game_variant_objective_in_sight_bit),
	FLAG_BIT("nhe_extras", _game_variant_nhe_extras_bit),
	FLAG_BIT("drop_secondary_first", _game_variant_drop_secondary_first_bit),
	FLAG_BIT("drop_secondary_second", _game_variant_drop_secondary_second_bit),
	FLAG_BIT("bit31", 31),
	VC("universal_variant.goal_radar", universal_variant.goal_radar, _audit_long),
	VC("universal_variant.odd_man_out", universal_variant.odd_man_out, _audit_boolean),
	VC("universal_variant.weapon_pack", universal_variant.weapon_pack, _audit_byte),
	/* (pad5: COMBAT FEEDBACK bits 0-1, ae_hit_feedback_rules.h; JUGGERNAUT bit 2, ae_juggernaut_rules.h;
	FRIENDLY SPLATTER bit 3, ae_splatter_rules.h; SILENT MOVEMENT bits 4-5 and SILENT WEAPONS 6-7, ae_silent_rules.h) */
	PAD5("hit_feedback", 0x03),
	PAD5("juggernaut_h2", 0x04),
	PAD5("friendly_splatter", 0x08),
	PAD5("silent_movement", 0x30),
	PAD5("silent_weapons", 0xC0),
	/* (pack_loadout: each slot's source, the map spawns' bit, INFINITE AMMO and BOTTOMLESS CLIP (seed revision 16,
	ae_weapon_rules.h), MAP GRENADES OFF (revision 17), the rest: game_engine.h) */
	PACK("primary", 0x03),
	PACK("secondary", 0x0C),
	PACK("map_spawns", PACK_MAP_SPAWNS_BIT),
	PACK("infinite_ammo", AE_WEAPON_INFINITE_AMMO_BIT),
	PACK("bottomless_clip", AE_WEAPON_BOTTOMLESS_CLIP_BIT),
	PACK("no_map_grenades", AE_MAP_GRENADES_OFF_BIT),
	VC("universal_variant.respawn_time_growth", universal_variant.respawn_time_growth, _audit_long),
	VC("universal_variant.respawn_time", universal_variant.respawn_time, _audit_long),
	VC("universal_variant.suicide_penalty", universal_variant.suicide_penalty, _audit_long),
	VC("universal_variant.lives", universal_variant.lives, _audit_long),
	VC("universal_variant.health", universal_variant.health, _audit_real),
	VC("universal_variant.score_to_win", universal_variant.score_to_win, _audit_long),
	VC("universal_variant.weapon_set", universal_variant.weapon_set, _audit_long),
	VC("universal_variant.vehicle_set", universal_variant.vehicle_set, _audit_long),
	/* (the engine's own variant: the union's view of its engine only) */
	EC(game_engine_ctf, "game_engine_variant.ctf.assault", game_engine_variant.ctf.assault, _audit_boolean),
	EC(game_engine_ctf, "game_engine_variant.ctf.reset_on_capture", game_engine_variant.ctf.reset_on_capture,
		_audit_boolean),
	EC(game_engine_ctf, "game_engine_variant.ctf.flag_must_reset", game_engine_variant.ctf.flag_must_reset,
		_audit_boolean),
	EC(game_engine_ctf, "game_engine_variant.ctf.flag_at_home_to_score",
		game_engine_variant.ctf.flag_at_home_to_score, _audit_boolean),
	EC(game_engine_ctf, "game_engine_variant.ctf.single_flag_time", game_engine_variant.ctf.single_flag_time,
		_audit_long),
	EC(game_engine_slayer, "game_engine_variant.slayer.no_death_bonus", game_engine_variant.slayer.no_death_bonus,
		_audit_boolean),
	EC(game_engine_slayer, "game_engine_variant.slayer.no_kill_penalty",
		game_engine_variant.slayer.no_kill_penalty, _audit_boolean),
	EC(game_engine_slayer, "game_engine_variant.slayer.kill_in_order", game_engine_variant.slayer.kill_in_order,
		_audit_boolean),
	EC(game_engine_king, "game_engine_variant.king.moving_hill", game_engine_variant.king.moving_hill,
		_audit_boolean),
	EC(game_engine_oddball, "game_engine_variant.oddball.random_start", game_engine_variant.oddball.random_start,
		_audit_boolean),
	EC(game_engine_oddball, "game_engine_variant.oddball.ball_spawn_delay",
		game_engine_variant.oddball.ball_spawn_delay, _audit_boolean),
	EC(game_engine_oddball, "game_engine_variant.oddball.speed_with_ball",
		game_engine_variant.oddball.speed_with_ball, _audit_long),
	EC(game_engine_oddball, "game_engine_variant.oddball.trait_with_ball",
		game_engine_variant.oddball.trait_with_ball, _audit_long),
	EC(game_engine_oddball, "game_engine_variant.oddball.trait_without_ball",
		game_engine_variant.oddball.trait_without_ball, _audit_long),
	EC(game_engine_oddball, "game_engine_variant.oddball.oddball_ball_type",
		game_engine_variant.oddball.oddball_ball_type, _audit_long),
	EC(game_engine_oddball, "game_engine_variant.oddball.ball_spawn_count",
		game_engine_variant.oddball.ball_spawn_count, _audit_long),
	EC(game_engine_race, "game_engine_variant.race.race_type", game_engine_variant.race.race_type, _audit_long),
	EC(game_engine_race, "game_engine_variant.race.team_scoring", game_engine_variant.race.team_scoring,
		_audit_long),
	/* (Bomb's, game_engine.h's struct bomb_variant: flags bits 0-1 Chupa's, bit 7 AE SEARCH AND DESTROY,
	ae_bomb_snd_rules.h, seed revision 26) */
	EC(game_engine_bomb, "game_engine_variant.bomb.arm_time", game_engine_variant.bomb.arm_time, _audit_long),
	EC(game_engine_bomb, "game_engine_variant.bomb.reset_time", game_engine_variant.bomb.reset_time, _audit_long),
	EC(game_engine_bomb, "game_engine_variant.bomb.round_time", game_engine_variant.bomb.round_time, _audit_long),
	EBITS(game_engine_bomb, "game_engine_variant.bomb.flags.new_round_on_score", game_engine_variant.bomb.flags,
		FLAG(_bomb_new_round_on_score_bit)),
	EBITS(game_engine_bomb, "game_engine_variant.bomb.flags.melee_only", game_engine_variant.bomb.flags,
		FLAG(_bomb_melee_only_bit)),
	EBITS(game_engine_bomb, "game_engine_variant.bomb.flags.search_and_destroy", game_engine_variant.bomb.flags,
		AE_BOMB_SND_FLAG),
	EBITS(game_engine_bomb, "game_engine_variant.bomb.flags.bomb_drop", game_engine_variant.bomb.flags,
		AE_BOMB_SND_DROP_FLAG),
	EC(game_engine_bomb, "game_engine_variant.bomb.traits", game_engine_variant.bomb.traits, _audit_byte),
	/* (the PC options: game_engine.h's struct game_variant_options) */
	OC("options.time_limit", time_limit, _audit_short),
	OC("options.friendly_fire", friendly_fire, _audit_short),
	OC("options.friendly_fire_penalty", friendly_fire_penalty, _audit_short),
	OC("options.vehicle_respawn_time", vehicle_respawn_time, _audit_short),
	OC("options.auto_team_balance", auto_team_balance, _audit_boolean),
	OC("options.radar_players", radar_players, _audit_byte),
	OC("options.vehicle_set.red", vehicle_set[0], _audit_byte),
	OC("options.vehicle_set.blue", vehicle_set[1], _audit_byte),
	VEHICLES("red", 0, _variant_vehicle_warthog, "warthog"),
	VEHICLES("red", 0, _variant_vehicle_ghost, "ghost"),
	VEHICLES("red", 0, _variant_vehicle_scorpion, "scorpion"),
	VEHICLES("red", 0, _variant_vehicle_rocket_warthog, "rocket_warthog"),
	VEHICLES("red", 0, _variant_vehicle_banshee, "banshee"),
	VEHICLES("red", 0, _variant_vehicle_gun_turret, "gun_turret"),
	VEHICLES("blue", 1, _variant_vehicle_warthog, "warthog"),
	VEHICLES("blue", 1, _variant_vehicle_ghost, "ghost"),
	VEHICLES("blue", 1, _variant_vehicle_scorpion, "scorpion"),
	VEHICLES("blue", 1, _variant_vehicle_rocket_warthog, "rocket_warthog"),
	VEHICLES("blue", 1, _variant_vehicle_banshee, "banshee"),
	VEHICLES("blue", 1, _variant_vehicle_gun_turret, "gun_turret"),
	OC("options.loadout", loadout, _audit_byte),
	OC("options.primary_weapon", primary_weapon, _audit_byte),
	OC("options.secondary_weapon", secondary_weapon, _audit_byte),
	OC("options.no_map_weapons", no_map_weapons, _audit_boolean),
};

/* the stock builders (game_engine_playlist.h), by their names less "build_game_variant_" */
static struct
{
	char const *name;
	struct game_variant *(*build)(struct game_variant *variant);
} const ae_audit_stock[] =
{
	{ "slayer", build_game_variant_slayer },
	{ "slayer_pro", build_game_variant_slayer_pro },
	{ "elimination", build_game_variant_elimination },
	{ "phantoms", build_game_variant_phantoms },
	{ "endurance", build_game_variant_endurance },
	{ "rockets", build_game_variant_rockets },
	{ "snipers", build_game_variant_snipers },
	{ "oddball", build_game_variant_oddball },
	{ "reverse_tag", build_game_variant_reverse_tag },
	{ "accumulation", build_game_variant_accumulation },
	{ "juggernaut", build_game_variant_juggernaut },
	{ "stalker", build_game_variant_stalker },
	{ "king", build_game_variant_king },
	{ "king_pro", build_game_variant_king_pro },
	{ "crazy_king", build_game_variant_crazy_king },
	{ "race", build_game_variant_race },
	{ "rally", build_game_variant_rally },
	{ "ctf", build_game_variant_ctf },
	{ "invasion", build_game_variant_invasion },
	{ "iron_ctf", build_game_variant_iron_ctf },
	{ "ctf_pro", build_game_variant_ctf_pro },
	{ "team_race", build_game_variant_team_race },
	{ "team_rally", build_game_variant_team_rally },
	{ "team_oddball", build_game_variant_team_oddball },
	{ "team_king", build_game_variant_team_king },
	{ "team_slayer", build_game_variant_team_slayer },
#ifdef HALO_GAME_BROWSER
	/* (Bomb's: the builds with Delta Peer) */
	{ "bomb", build_game_variant_bomb },
	{ "grifball", build_game_variant_grifball },
	{ "grifball_open_pitch", build_game_variant_grifball_open_pitch },
#endif
};

static char const *const ae_audit_engines[] =
{
	"none", "ctf", "slayer", "oddball", "king", "race", "bomb", "stub"
};

/* a wide name as ASCII for the file (others '?', a tab or line break a space) */
static void ae_audit_ascii(wchar_t const *wide, short wide_length, char *out, short size)
{
	short index;

	for (index = 0; index < size - 1 && index < wide_length && wide[index]; index++)
	{
		unsigned long character = (unsigned long)wide[index];

		out[index] = character == '\t' || character == '\n' || character == '\r' ? ' ' :
			character >= 32 && character < 127 ? (char)character : '?';
	}
	out[index] = 0;
}

static void ae_audit_wide(char const *ascii, wchar_t *out, short size)
{
	short index;

	for (index = 0; index < size - 1 && ascii[index]; index++)
		out[index] = (wchar_t)(unsigned char)ascii[index];
	for (; index < size; index++)
		out[index] = 0;
}

/* the names the same but for ASCII letters' case */
static boolean ae_audit_same_name(wchar_t const *a, wchar_t const *b)
{
	short index;

	for (index = 0; index < AE_AUDIT_NAME_LENGTH; index++)
	{
		unsigned long x = (unsigned long)a[index];
		unsigned long y = (unsigned long)b[index];

		if (x >= 'a' && x <= 'z')
			x -= 'a' - 'A';
		if (y >= 'a' && y <= 'z')
			y -= 'a' - 'A';
		if (x != y)
			return FALSE;
		if (!x)
			return TRUE;
	}
	return TRUE;
}

static unsigned long ae_audit_lowest_bit(unsigned long mask)
{
	unsigned long shift = 0;

	while (mask && !(mask & 1))
	{
		mask >>= 1;
		shift++;
	}
	return shift;
}

static void ae_audit_value(FILE *file, struct ae_audit_column const *column, struct game_variant const *variant,
	struct game_variant_options const *options)
{
	byte const *base = column->owner == _audit_options ? (byte const *)options : (byte const *)variant;
	byte const *field = base + column->offset;

	switch (column->kind)
	{
	case _audit_boolean: fprintf(file, "%d", *field ? 1 : 0); break;
	case _audit_byte: fprintf(file, "%u", (unsigned)*field); break;
	case _audit_short: fprintf(file, "%d", (int)*(short const *)field); break;
	case _audit_word: fprintf(file, "%u", (unsigned)*(word const *)field); break;
	case _audit_long: fprintf(file, "%ld", (long)*(long const *)field); break;
	case _audit_real: fprintf(file, "%g", (double)*(real const *)field); break;
	case _audit_bits_byte:
		fprintf(file, "%lu", ((unsigned long)*field & column->mask) >> ae_audit_lowest_bit(column->mask));
		break;
	case _audit_bits_long:
		fprintf(file, "%lu", ((unsigned long)*(unsigned long const *)field & column->mask) >>
			ae_audit_lowest_bit(column->mask));
		break;
	}
}

static void ae_audit_header(FILE *file)
{
	short index;

	fputs("kind\tstored_name\tdisplay_name\tset\tengine\tbase\tfile\tas_seeded", file);
	for (index = 0; index < (short)NUMBEROF(ae_audit_columns); index++)
		fprintf(file, "\t%s", ae_audit_columns[index].name);
	fputc('\n', file);
}

/* a row: variant NULL (no file) leaves every field empty; as_seeded NONE: not a seed */
static void ae_audit_row(FILE *file, char const *kind, char const *stored_name, char const *display_name,
	char const *set, char const *base, char const *file_state, short as_seeded, struct game_variant const *variant,
	struct game_variant_options const *options)
{
	long engine = variant ? variant->game_engine_index : NONE;
	short index;

	fprintf(file, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s", kind, stored_name, display_name, set,
		engine >= 0 && engine < (long)NUMBEROF(ae_audit_engines) ? ae_audit_engines[engine] : "",
		base, file_state, as_seeded == NONE ? "" : as_seeded ? "1" : "0");
	for (index = 0; index < (short)NUMBEROF(ae_audit_columns); index++)
	{
		struct ae_audit_column const *column = &ae_audit_columns[index];

		fputc('\t', file);
		if (!variant || (column->engine != NONE && column->engine != engine))
			continue;
		ae_audit_value(file, column, variant, options);
	}
	fputc('\n', file);
}

void ae_gametype_audit_write(void)
{
	static long saved[AE_SAVED_GAMES_MAXIMUM];
	word saved_count = NUMBEROF(saved);
	char path[1024];
	FILE *file;
	short catalog_count = arena_gametype_catalog_count();
	short written = 0;
	short missing = 0;
	short index;

	if (!config_boolean("debug.arena_audit"))
		return;

	snprintf(path, sizeof(path), "%s/%s", platform_save_root(), AE_AUDIT_HOST_NAME);
	file = fopen(AE_AUDIT_FILE_NAME, "w");
	if (!file)
	{
		error(_error_silent, "arena audit: could not write %s", path);
		return;
	}
	ae_audit_header(file);

	saved_game_files_enumerate_available_to_local_player_index(NONE, _saved_game_file_type_game_variant,
		&saved_count, saved, FALSE);
	for (index = 0; index < catalog_count; index++)
	{
		char const *stored_name = arena_gametype_catalog_stored_name(index);
		wchar_t wide_name[AE_AUDIT_NAME_LENGTH];
		wchar_t wide_display[AE_AUDIT_DISPLAY_LENGTH];
		char display[AE_AUDIT_DISPLAY_LENGTH];
		struct arena_gametype_info info;
		struct game_variant variant;
		struct game_variant_options options;
		boolean found = FALSE;
		short other;

		if (!stored_name)
			continue;
		ae_audit_wide(stored_name, wide_name, AE_AUDIT_NAME_LENGTH);
		arena_gametype_display_name(wide_name, wide_display, AE_AUDIT_DISPLAY_LENGTH);
		ae_audit_ascii(wide_display, AE_AUDIT_DISPLAY_LENGTH, display, sizeof(display));
		csmemset(&info, 0, sizeof(info));
		if (!arena_gametype_info(wide_name, &info))
			info.set = _arena_gametype_set_none;

		for (other = 0; other < (short)saved_count && !found; other++)
		{
			wchar_t file_name[MAX_GAMENAME];

			file_name[0] = 0;
			if (!playlist_profile_get_display_name(saved[other], file_name) ||
				!ae_audit_same_name(file_name, wide_name))
			{
				continue;
			}
			csmemset(&variant, 0, sizeof(variant));
			csmemset(&options, 0, sizeof(options));
			found = playlist_profile_get(saved[other], &variant);
			if (found && !playlist_profile_get_options(saved[other], &options))
				game_variant_options_default(&variant, &options);
		}
		if (!found)
			missing++;
		ae_audit_row(file, "ae", stored_name, display, arena_gametype_set_name(info.set), "",
			found ? "file" : "missing", found ? (short)(arena_gametype_as_seeded(wide_name) ? 1 : 0) : NONE,
			found ? &variant : NULL, &options);
		written++;
	}

	for (index = 0; index < (short)NUMBEROF(ae_audit_stock); index++)
	{
		struct game_variant temporary;
		struct game_variant variant;
		struct game_variant_options options;
		char display[AE_AUDIT_DISPLAY_LENGTH];

		csmemset(&temporary, 0, sizeof(temporary));
		variant = *ae_audit_stock[index].build(&temporary);
		game_variant_options_default(&variant, &options);
		ae_audit_ascii(variant.human_readable_game_description, NUMBEROF(variant.human_readable_game_description),
			display, sizeof(display));
		ae_audit_row(file, "stock", ae_audit_stock[index].name, display, "", ae_audit_stock[index].name, "built",
			NONE, &variant, &options);
	}

	fclose(file);
	error(_error_silent, "arena audit: %d seeded gametypes (%d without a file) and %d stock builders written to %s",
		(int)written, (int)missing, (int)NUMBEROF(ae_audit_stock), path);
}
