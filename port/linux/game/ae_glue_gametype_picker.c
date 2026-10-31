/*
AE_GLUE_GAMETYPE_PICKER.C

CHANGE GAMETYPE's glue. See ae_glue_gametype_picker.h.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game_engine.h"
#include "saved games/arena_gametypes.h"
#include "saved games/playlist_profile.h"
#include "ae_glue_gametype_picker.h"
#include "ae_glue_picker_stores.h"
#include "ae_glue_setup_maps.h"
#include "ae_glue_setup_options.h"
#include "ae_saved_limits.h"
#include "ae_setup_options.h"
#include "ae_weapon_slots.h"
#include "game_modes.h"
#include "ae_modes.h"

void platform_log(char const *format, ...);

/* a variant's flags word bit 0 (playlist_profile.c's _game_variant_is_system_default_bit): a built-in gametype */
#define SYSTEM_DEFAULT_BIT 0
/* ticks a second */
#define TICKS_PER_SECOND 30

/* a saved gametype's id's mark (the player's own: their names can be AE's or a built-in's too) */
#define SAVED_PREFIX "saved:"

static const char *const stock_descriptions[] =
{
	"Halo CE's own gametype, as the game ships it.",
	"Halo CE's own Capture the Flag: take the other team's flag to your base.",
	"Halo CE's own Slayer: the most kills wins.",
	"Halo CE's own Oddball: hold the skull the longest.",
	"Halo CE's own King of the Hill: hold the hill the longest.",
	"Halo CE's own Race: run the course's flags in order.",
};
/* OpenCE's built-in gametypes (game_modes.c), by their mode */
static const char *const opence_descriptions[NUMBER_OF_GAME_MODES] =
{
	"OpenCE's Hardcore: no motion sensor, no aim assist, and a kill restores the killer's health and shields.",
	"OpenCE's SWAT: no shields, and a shot to the head kills. Pistol and assault rifle, no grenades.",
	"OpenCE's Infection: humans killed join the zombies, who win when no human is left.",
	"OpenCE's Gun Game: each kill gives the next weapon; a kill with the last wins.",
};
/* Arena Evolved's built-in gametypes (ae_modes.c, after OpenCE's in game_modes.c's list, so stock OpenCE players play
them too): their words and weapons, in AE ACTION SACK (their stored names are short and their loadouts the modes') */
static const struct { const char *description, *weapons, *start; } ae_mode_words[] =
{
	{ "Arena Evolved's One in the Chamber (after Chalwk's SPCLib): a pistol with one round, a hit kills, each kill gives "
		"a round. 3 lives: the last player standing wins.", "PISTOL, 1 ROUND", "PISTOL START" },
	{ "Arena Evolved's Frag Nation (after Chalwk's SPCLib): fists, 2 frags and 2 plasmas; a grenade kill gives "
		"grenades back.", "FISTS, GRENADES", "FISTS START" },
	{ "Arena Evolved's Melee Only (after Chalwk's SPCLib): fists only, nothing on the map.", "FISTS", "FISTS START" },
	{ "Arena Evolved's Needles (after Chalwk's SPCLib): needlers with bottomless clips, no grenades.", NULL, NULL },
	{ "Arena Evolved's Snowball Fight: fists and 4 plasma grenades, none on the map; a plasma kill gives a plasma "
		"back.", "FISTS, PLASMAS", "FISTS START" },
};
#define MINE_DESCRIPTION "Your own gametype. Change it in GAME OPTIONS, and SAVE AS NEW keeps the changes."

static int category_folder(short category)
{
	switch (category)
	{
	case _arena_gametype_category_ffa: return AE_GTP_FOLDER_FFA;
	case _arena_gametype_category_2v2: return AE_GTP_FOLDER_2V2;
	case _arena_gametype_category_4v4: return AE_GTP_FOLDER_4V4;
	case _arena_gametype_category_btb: return AE_GTP_FOLDER_BTB;
	case _arena_gametype_category_objective: return AE_GTP_FOLDER_OBJECTIVE;
	case _arena_gametype_category_action_sack: return AE_GTP_FOLDER_ACTION_SACK;
	case _arena_gametype_category_practice: return AE_GTP_FOLDER_PRACTICE;
	case _arena_gametype_category_nhe: return AE_GTP_FOLDER_NHE;
	case _arena_gametype_category_stock: return AE_GTP_FOLDER_STOCK;
	}
	return AE_GTP_FOLDER_MINE;
}

static void narrow_upper(wchar_t const *wide, int length, char *text, int size)
{
	int index;

	for (index = 0; index < size - 1 && index < length && wide[index]; index++)
	{
		wchar_t c = wide[index];

		text[index] = c >= 'a' && c <= 'z' ? (char)(c - 32) : c >= 0x20 && c < 0x7F ? (char)c : '?';
	}
	text[index] = 0;
}

/* a loadout slot's pack weapon's words ("BR" for the battle rifle in a start, its display name else), NULL for none */
static const char *pack_words(struct game_variant const *variant, short slot, int start)
{
	short pack = game_variant_pack_loadout_weapon_slot(variant->universal_variant.weapon_pack,
		variant->universal_variant.pack_loadout, variant->universal_variant.flags, variant->universal_variant.ball_melee, slot);

	if (pack == AE_WEAPON_SLOT_NONE)
		return NULL;
	if (start && pack == AE_WEAPON_SLOT_BATTLE_RIFLE)
		return "BR";
	/* (0.7.0: the START switch's third, "EVO START") */
	if (start && pack == AE_WEAPON_SLOT_EVO_RIFLE)
		return "EVO";
	return ae_weapon_slots_display(pack);
}

/* the start weapon and the WEAPONS fact */
static void weapons(struct game_variant const *variant, struct game_variant_options const *options,
	struct ae_gtp_gametype *gametype)
{
	int custom = options->loadout == _loadout_custom, set = (int)variant->universal_variant.weapon_set;
	const char *pack = custom ? pack_words(variant, _pack_loadout_primary, 1) : NULL;

	ae_gtp_start_text(pack, custom, options->primary_weapon, set, gametype->start, sizeof(gametype->start));
	if (custom)
	{
		const char *first = pack_words(variant, _pack_loadout_primary, 0);
		const char *second = pack_words(variant, _pack_loadout_secondary, 0);

		if (!first)
			first = ae_gtp_weapon_name(options->primary_weapon);
		if (!second && options->secondary_weapon != _loadout_weapon_none)
			second = ae_gtp_weapon_name(options->secondary_weapon);
		snprintf(gametype->weapons, sizeof(gametype->weapons), "%s%s%s", first, second ? ", " : "", second ? second : "");
	}
	else if (TEST_FLAG(variant->universal_variant.flags, _game_variant_generic_starting_equipment_bit))
		snprintf(gametype->weapons, sizeof(gametype->weapons), "%s, %s",
			ae_gtp_weapon_name(ae_gtp_weapon_in_set(AE_GTW_ASSAULT_RIFLE, set)),
			ae_gtp_weapon_name(ae_gtp_weapon_in_set(AE_GTW_PISTOL, set)));
	else if (set == 0)
		snprintf(gametype->weapons, sizeof(gametype->weapons), "THE MAP'S OWN");
	else
		snprintf(gametype->weapons, sizeof(gametype->weapons), "THE MAP'S, AS %s",
			ae_gtp_weapon_name(ae_gtp_weapon_in_set(AE_GTW_ASSAULT_RIFLE, set)));
	/* (a melee-only Bomb gametype, Grifball's: the map's sword or the unseen fists, whatever the loadout reads) */
	if (variant->game_engine_index == game_engine_bomb &&
		TEST_FLAG(variant->game_engine_variant.bomb.flags, _bomb_melee_only_bit))
	{
		snprintf(gametype->start, sizeof(gametype->start), "MELEE ONLY");
		snprintf(gametype->weapons, sizeof(gametype->weapons), "MELEE ONLY: SWORD OR FISTS");
	}
}

static void plural(char *text, int size, long count, const char *one, const char *many)
{
	snprintf(text, (size_t)size, "%ld %s", count, count == 1 ? one : many);
}

/* the score to win, the time, the shields, the motion sensor, the respawn */
static void facts(long profile, struct game_variant const *variant, struct game_variant_options const *options,
	struct ae_gtp_gametype *gametype)
{
	static int values[AE_SETUP_OPTIONS];
	long score = variant->universal_variant.score_to_win, respawn = variant->universal_variant.respawn_time;

	switch (gametype->engine)
	{
	case AE_SETUP_ENGINE_CTF:
		plural(gametype->score, sizeof(gametype->score), score, "CAPTURE", "CAPTURES");
		break;
	case AE_SETUP_ENGINE_KING:
		plural(gametype->score, sizeof(gametype->score), score, "MINUTE ON THE HILL", "MINUTES ON THE HILL");
		break;
	case AE_SETUP_ENGINE_ODDBALL:
		if (variant->game_engine_variant.oddball.oddball_ball_type == 0)
			plural(gametype->score, sizeof(gametype->score), score, "MINUTE WITH THE BALL", "MINUTES WITH THE BALL");
		else
			plural(gametype->score, sizeof(gametype->score), score, "POINT", "POINTS");
		break;
	case AE_SETUP_ENGINE_RACE:
		plural(gametype->score, sizeof(gametype->score), score, "LAP", "LAPS");
		break;
	case AE_SETUP_ENGINE_BOMB:
		plural(gametype->score, sizeof(gametype->score), score, "ROUND", "ROUNDS");
		break;
	default:
		plural(gametype->score, sizeof(gametype->score), score, "KILL", "KILLS");
		break;
	}
	if (options->time_limit > 0)
		plural(gametype->time, sizeof(gametype->time), options->time_limit, "MINUTE", "MINUTES");
	else
		snprintf(gametype->time, sizeof(gametype->time), "NO TIME LIMIT");
	if (respawn <= 0)
		snprintf(gametype->respawn, sizeof(gametype->respawn), "INSTANT");
	else
		snprintf(gametype->respawn, sizeof(gametype->respawn), "%g SECONDS", (double)respawn / TICKS_PER_SECOND);
	snprintf(gametype->shields, sizeof(gametype->shields), "%s",
		TEST_FLAG(variant->universal_variant.flags, _game_variant_no_shields_bit) ? "NONE" : "NORMAL");
	if (ae_setup_profile_values(profile, values, AE_SETUP_OPTIONS))
		snprintf(gametype->sensor, sizeof(gametype->sensor), "%s",
			ae_setup_value_text(AE_SO_MOTION_SENSOR, values[AE_SO_MOTION_SENSOR]));
}

/* players a side: by the folder, else teams of 4 and free for all */
static int side_players(int folder, int teams)
{
	switch (folder)
	{
	case AE_GTP_FOLDER_FFA: return 0;
	case AE_GTP_FOLDER_2V2: return 2;
	case AE_GTP_FOLDER_4V4: return 4;
	case AE_GTP_FOLDER_BTB: return 8;
	}
	return teams ? 4 : 0;
}

void ae_gtp_load(struct ae_gtp *gtp, short *current, int recent)
{
	static char names[AE_SAVED_GAMES_MAXIMUM][16];
	static long profiles[AE_SAVED_GAMES_MAXIMUM];
	short count, index, edit = -1, pairs = 0;

	memset(gtp, 0, sizeof(*gtp));
	*current = -1;
	count = ae_setup_gametypes(names, profiles, AE_SAVED_GAMES_MAXIMUM, &edit);
	for (index = 0; index < count && gtp->count < AE_GTP_MAXIMUM; index++)
	{
		struct game_variant variant;
		struct game_variant_options options;
		struct arena_gametype_info info;
		struct ae_gtp_gametype *gametype = &gtp->gametypes[gtp->count];
		int known;

		csmemset(&variant, 0, sizeof(variant));
		csmemset(&options, 0, sizeof(options));
		if (!playlist_profile_get(profiles[index], &variant) || !playlist_profile_get_options(profiles[index], &options))
			continue;
		memset(gametype, 0, sizeof(*gametype));
		gametype->profile = profiles[index];
		narrow_upper(variant.human_readable_game_description, 11, gametype->stored, sizeof(gametype->stored));
		known = arena_gametype_info(variant.human_readable_game_description, &info);
		if (known)
		{
			snprintf(gametype->name, sizeof(gametype->name), "%s", info.display_name);
			gametype->folder = (short)category_folder(info.category);
			gametype->order = info.category_order;
			gametype->description = info.description;
		}
		else
		{
			wchar_t own[AE_GTP_NAME];

			if (arena_gametype_own_display_name(profiles[index], own, AE_GTP_NAME))
				narrow_upper(own, AE_GTP_NAME, gametype->name, sizeof(gametype->name));
			else
				snprintf(gametype->name, sizeof(gametype->name), "%s", gametype->stored);
			gametype->folder = TEST_FLAG(variant.flags, SYSTEM_DEFAULT_BIT) ? AE_GTP_FOLDER_STOCK : AE_GTP_FOLDER_MINE;
			gametype->order = index;
			gametype->description = gametype->folder == AE_GTP_FOLDER_STOCK ?
				stock_descriptions[variant.game_engine_index >= 0 && variant.game_engine_index <= 5 ?
				variant.game_engine_index : 0] : MINE_DESCRIPTION;
			/* (a built-in one of OpenCE's: its own folder and words) */
			if (gametype->folder == AE_GTP_FOLDER_STOCK && game_modes_variant_is_opence(&variant, &options))
			{
				short mode = game_modes_variant_mode(&variant, &options);

				gametype->folder = AE_GTP_FOLDER_OPENCE;
				gametype->description = opence_descriptions[mode >= 0 && mode < NUMBER_OF_GAME_MODES ? mode : 0];
			}
		}
		{
			short mode = known ? NONE : ae_modes_gametype_of_stored_name(variant.human_readable_game_description);

			if (mode >= 0 && mode < (short)NUMBEROF(ae_mode_words))
			{
				wchar_t const *full = ae_modes_gametype_name(mode);

				narrow_upper(full, AE_GTP_NAME, gametype->name, sizeof(gametype->name));
				gametype->folder = AE_GTP_FOLDER_ACTION_SACK;
				gametype->description = ae_mode_words[mode].description;
			}
			gametype->engine = (int)variant.game_engine_index;
			gametype->ae = arena_gametype_has_ae_rules(variant.human_readable_game_description);
			gametype->team = side_players(gametype->folder, variant.universal_variant.teams);
			weapons(&variant, &options, gametype);
			if (mode >= 0 && mode < (short)NUMBEROF(ae_mode_words) && ae_mode_words[mode].weapons)
			{
				snprintf(gametype->weapons, sizeof(gametype->weapons), "%s", ae_mode_words[mode].weapons);
				snprintf(gametype->start, sizeof(gametype->start), "%s", ae_mode_words[mode].start);
			}
		}
		facts(profiles[index], &variant, &options, gametype);
		if (index == edit)
			*current = gtp->count;
		gtp->count++;
	}
	ae_gtp_finish(gtp);
	ae_gtp_folders_apply(gtp);
	if (recent)
		ae_gtp_recent_apply(gtp);
	for (index = 0; index < gtp->count; index++)
	{
		if (gtp->gametypes[index].twin > index || gtp->gametypes[index].evo_twin > index)
			pairs++;
	}
	/* (the edit's gametype shown on its pair's row) */
	if (*current >= 0)
		gtp->shown[ae_gtp_base(gtp, *current)] = *current;
	platform_log("ae change gametype: gametypes %d (pairs %d), current '%s'", gtp->count, pairs,
		*current >= 0 ? gtp->gametypes[*current].stored : "");
}

void ae_gtp_id(struct ae_gtp const *gtp, short gametype, char *id, int size)
{
	if (!id || size <= 0)
		return;
	id[0] = 0;
	if (!gtp || gametype < 0 || gametype >= gtp->count)
		return;
	snprintf(id, (size_t)size, "%s%s", gtp->gametypes[gametype].folder == AE_GTP_FOLDER_MINE ? SAVED_PREFIX : "",
		gtp->gametypes[gametype].stored);
}

short ae_gtp_find_id(struct ae_gtp const *gtp, const char *id)
{
	char own[AE_RECENT_ID];
	short index;

	for (index = 0; gtp && id && index < gtp->count; index++)
	{
		ae_gtp_id(gtp, index, own, sizeof(own));
		if (!strcmp(own, id))
			return index;
	}
	return -1;
}

void ae_gtp_folders_apply(struct ae_gtp *gtp)
{
	struct ae_picker_folders const *store = ae_picker_store(AE_PF_GAMETYPES);
	short index, folder;
	int entry;

	gtp->own_count = 0;
	for (folder = 1; folder < store->folder_count && gtp->own_count < AE_GTP_OWN; folder++)
		snprintf(gtp->own_names[gtp->own_count++], AE_GTP_OWN_NAME, "%s", store->names[folder]);
	for (index = 0; index < gtp->count; index++)
	{
		gtp->gametypes[index].favourite = 0;
		gtp->gametypes[index].own = 0;
	}
	/* (kept on the pair's base, either one's id; an id not here: kept in its folder, not listed) */
	for (entry = 0; entry < store->entry_count; entry++)
	{
		short found = ae_gtp_base(gtp, ae_gtp_find_id(gtp, store->entries[entry].id));

		if (found < 0)
			continue;
		folder = store->entries[entry].folder;
		if (folder == AE_PF_FAVOURITES)
			gtp->gametypes[found].favourite = 1;
		else if (folder - 1 < AE_GTP_OWN)
			gtp->gametypes[found].own |= 1ull << (folder - 1);
	}
}

/* RECENT's gametypes that are here (context: the gtp) */
static int recent_exists(const char *gametype, void *context)
{
	return ae_gtp_find_id(context, gametype) >= 0;
}

void ae_gtp_recent_apply(struct ae_gtp *gtp)
{
	struct ae_recent_games const *recent = ae_recent_store();
	short shown[AE_RECENT_SHOWN], count, index;

	gtp->recent_count = 0;
	count = ae_recent_games_by(recent, AE_RECENT_BY_GAMETYPE, recent_exists, gtp, shown, AE_GTP_RECENT);
	for (index = 0; index < count; index++)
		ae_gtp_recent_put(gtp, ae_gtp_find_id(gtp, recent->games[shown[index]].gametype));
}

int ae_gtp_folder_toggle(struct ae_gtp *gtp, short gametype, short folder)
{
	struct ae_picker_folders *store = ae_picker_store(AE_PF_GAMETYPES);
	short base = ae_gtp_base(gtp, gametype), twin, evo;
	char id[AE_RECENT_ID], other[AE_RECENT_ID], third[AE_RECENT_ID];
	int in;

	if (base < 0 || folder < 0 || folder >= store->folder_count)
		return -1;
	ae_gtp_id(gtp, base, id, sizeof(id));
	twin = gtp->gametypes[base].twin;
	evo = gtp->gametypes[base].evo_twin;
	other[0] = third[0] = 0;
	if (twin >= 0)
		ae_gtp_id(gtp, twin, other, sizeof(other));
	if (evo >= 0)
		ae_gtp_id(gtp, evo, third, sizeof(third));
	in = ae_pf_has(store, folder, id) || (other[0] && ae_pf_has(store, folder, other)) ||
		(third[0] && ae_pf_has(store, folder, third));
	if (in)
	{
		ae_pf_take(store, folder, id);
		if (other[0])
			ae_pf_take(store, folder, other);
		if (third[0])
			ae_pf_take(store, folder, third);
	}
	else if (ae_pf_put(store, folder, id) != 1)
		return -1;
	ae_picker_store_save(AE_PF_GAMETYPES);
	ae_gtp_folders_apply(gtp);
	platform_log("ae change gametype: '%s' %s %s", id, in ? "out of" : "in", ae_pf_name(store, folder));
	return !in;
}

void ae_gtp_record_game(const char *map)
{
	static struct ae_gtp game;
	short current;
	char id[AE_RECENT_ID], map_id[AE_SETUP_MAP_TEXT];

	ae_gtp_load(&game, &current, 0);
	ae_gtp_id(&game, current, id, sizeof(id));
	if (!id[0])
	{
		platform_log("ae pickers: recent game not kept: no gametype in the edit");
		return;
	}
	ae_setup_map_id(map, map_id, sizeof(map_id));
	ae_recent_store_record(map_id, id);
}

void ae_gtp_preferred_map_groups(struct ae_setup_maps const *maps, unsigned int *groups)
{
	short group;

	memset(groups, 0, sizeof(groups[0]) * AE_SETUP_MAP_GROUP_WORDS);
	/* (the comp maps: NHE's and the community set's) */
	for (group = 0; maps && group < maps->group_count; group++)
	{
		if (!strcmp(maps->group_names[group], "NHE") || !strcmp(maps->group_names[group], "COMMUNITY COMPETITIVE"))
			groups[group / 32] |= 1u << (group % 32);
	}
}

/* ---------- deleting (owner 2026-10-26) */

/* (ui_widget_event_handler_functions.c: the classic menus' DELETE, playlist_profile_delete) */
boolean ui_widget_port_gametype_delete(long profile_index);

int ae_gtp_can_delete(struct ae_gtp const *gtp, short gametype)
{
	struct ae_gtp_gametype const *entry;

	if (!gtp || gametype < 0 || gametype >= gtp->count)
		return 0;
	entry = &gtp->gametypes[gametype];
	/* (a saved file: the low nibble 1; not read-only, the built-ins' bit) */
	return entry->folder == AE_GTP_FOLDER_MINE && entry->profile != NONE && (entry->profile & 0xF) == 1 &&
		!(entry->profile & 0x40000000);
}

int ae_gtp_delete(struct ae_gtp const *gtp, short gametype)
{
	if (!ae_gtp_can_delete(gtp, gametype) || !ui_widget_port_gametype_delete(gtp->gametypes[gametype].profile))
	{
		platform_log("ae change gametype: delete refused (%d)", (int)gametype);
		return 0;
	}
	platform_log("ae change gametype: deleted '%s'", gtp->gametypes[gametype].stored);
	return 1;
}
