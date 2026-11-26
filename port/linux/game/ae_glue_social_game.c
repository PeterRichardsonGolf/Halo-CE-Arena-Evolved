/* A social career game from the lobby's GO to the way out (see the header). */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "main/main.h"
#include "ae_bots_seat.h"
#include "ae_bots_seat_rules.h"
#include "ae_bots_style_rules.h"
#include "ae_glue_bot_records.h"
#include "ae_social_lobby_rules.h"
#include "ae_social_names_rules.h"
#include "ae_dev_events.h"
#include "ae_dev_mode.h"
#include "ae_glue_career.h"
#include "ae_glue_postgame.h"
#include "ae_glue_social.h"
#include "ae_glue_recent_maps.h"
#include "ae_glue_party.h"
#include "ae_party_follow.h"
#include "ae_party_model_rules.h"
#include "ae_rematch_rules.h"
#include "ae_glue_setup_options.h"
#include "ae_glue_social_game.h"
#include "ae_glue_profiles.h"
#include "interface/player_ui.h"
#include "ae_screen_create_game.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_matchmaking.h"
#include "ae_screen_postgame.h"
#include "ae_screen_social_lobby.h"
#include "ae_service_record.h"
#include "ae_social_rules.h"
#include "ae_social_view_rules.h"
#include "ae_weapon_cache.h"
#include "ae_weapon_pack.h"
#include "saved games/arena_gametypes.h"
#include "ae_ui.h"

void platform_log(char const *format, ...);
boolean main_menu_is_active(void);
char const *config_string(char const *name);
int config_set(char const *name, char const *value);
long config_integer(char const *name);
int config_boolean(char const *name);
boolean game_in_progress(void);

enum
{
	SAVED = 24,
	CONFIG_KEYS = 11
};

static char const *const config_keys[CONFIG_KEYS] =
	{ "bots.count", "bots.skill", "bots.teams", "bots.shooting_skill", "bots.movement_skill", "bots.playfight",
	  "bots.routes", "bots.vantage", "bots.high_ground", "bots.drive_obstacles", "bots.learn_map" };

/* the boolean ones of config_keys (config_string answers "" for them) */
static int boolean_key(char const *key)
{
	return !strcmp(key, "bots.playfight") || !strcmp(key, "bots.vantage") || !strcmp(key, "bots.high_ground") ||
		!strcmp(key, "bots.drive_obstacles") || !strcmp(key, "bots.learn_map");
}

enum { AE_SOCIAL_LEAVE = 2 };

static struct
{
	int active;                 /* from the start until the settings are back */
	int running;                /* the game has been seen running */
	int counted;                /* the result (or the quit) is counted */
	int saved;
	char saved_value[CONFIG_KEYS][SAVED];
	/* the bot seats' own pairs before the game (DEV botskill): put back after it, levels + 1 = steps; 0 = none */
	int saved_shooting[AE_SEAT_MOST];
	int saved_movement[AE_SEAT_MOST];
	/* and their play styles (DEV botstyle) */
	struct ae_bots_style saved_style[AE_SEAT_MOST];
	unsigned char has_style[AE_SEAT_MOST];
	struct ae_social_plan plan;
	int pending_written;
	int pending_tries;
	char key[AE_SOCIAL_LOCAL][AE_SR_KEY];
	int has_key[AE_SOCIAL_LOCAL];
	struct ae_social_pg pg[AE_SOCIAL_LOCAL];
	int reopen;                 /* the post-game's CONTINUE: MATCHMAKING comes back when the menus do */
	unsigned long last_pick;    /* the entry index of the last map played + 1 (never the same map twice in a row) */
	int dev;                    /* a made-up post-game is up */
	unsigned long started;      /* the clock at the plan's start */
	/* the player's own settings a tournament game borrows (put back with the others): WEAPON PACKS, and the START the mode's
	   file reads as (arena_gametypes.c's start choice, the name of it; "" when the game type has none) */
	int saved_packs;            /* game.weapon_packs before the game: 1 on, 0 off */
	int packs_forced;           /* the setting was off and is on for this game (the cache read again at its start) */
	int packs_back;             /* the setting is put back, the cache to be read again when no game is up */
	char saved_start[AE_SOCIAL_GAMETYPE];
	int start_saved;
	int force_start;            /* ae_social_plan_force_start's, one time (-1 none) */
	struct ae_social_last last;   /* the finished game, for the post-game lobby */
	int rematch;                /* games played in a row by the same lobby: 0 the first */
	/* each local player's profile before the game (the game's teardown clears them all: a split screen player 2 would come
	   back as a guest), -1 for none, and whether it is to be put back once the menus are up */
	long profile_saved[AE_SOCIAL_LOCAL];
	int profiles_back;
} game = { .force_start = -1 };

/* ---------- who */

int ae_social_screen_key(short local, char *key, size_t size)
{
	if (ae_social_profile_key(local, key, size))
		return 1;
	if (!ae_dev_mode_on() || size < 16)
		return 0;
	snprintf(key, size, "dev-player%d", (int)local + 1);
	return 1;
}

static int dev_party;

void ae_social_dev_party(int players)
{
	dev_party = players >= 1 && players <= AE_SOCIAL_LOCAL ? players : 0;
}

int ae_social_party_size(void)
{
	short player;
	int size = 1;

	if (dev_party && ae_dev_mode_on())
		return dev_party;

	for (player = 1; player < AE_SOCIAL_LOCAL; player++)
		size += ae_screen_main_menu_signed_in(player) != 0;
	return size;
}

static unsigned long now_seed(void)
{
	return ((unsigned long)time(NULL) * 2654435761ul) % 99999989ul + 1ul;
}

/* ---------- a plan */

void ae_social_plan_force_start(int start)
{
	game.force_start = start >= 0 && start <= AE_SOCIAL_STARTS ? start : -1;
}

static void name_wide(char const *name, wchar_t *wide, int size)
{
	int at;

	for (at = 0; name[at] && at < size - 1; at++)
		wide[at] = (wchar_t)(unsigned char)name[at];
	wide[at] = 0;
}

/* the stored name of a game type's start of this kind, NULL when it has none */
static char const *start_name_of_kind(char const *gametype, int kind)
{
	wchar_t wide[AE_SOCIAL_GAMETYPE];
	short place;

	name_wide(gametype, wide, AE_SOCIAL_GAMETYPE);
	/* (rc2: the PROTOMAG start is the pistol start with the weapon put over its pistol: a game type has it where it has that;
	rc5: the SINGLESHOT BR start the BR start's, with the weapon put over its BR) */
	place = arena_gametype_start_place_of_kind(wide, (short)(kind == AE_SOCIAL_START_PROTOMAG ? AE_SOCIAL_START_MAGNUM :
		kind == AE_SOCIAL_START_SINGLESHOT_BR ? AE_SOCIAL_START_BR : kind));
	return place < 0 ? NULL : arena_gametype_start_name(wide, place);
}

/* the plan's game type becomes the start's (forced: 0 the game type's own, 1 .. 3 that start when it has it; -1: by the seed) */
static void start_resolve(struct ae_social_playlist const *list, int forced, struct ae_social_plan *plan)
{
	unsigned supported = 0;
	int kind, pick;
	char const *name;

	for (kind = 1; kind <= AE_SOCIAL_STARTS; kind++)
		if (start_name_of_kind(plan->gametype, kind))
			supported |= 1u << kind;
	if (forced >= 0)
		pick = forced && (supported & (1u << forced)) ? forced : AE_SOCIAL_START_NONE;
	else
		pick = ae_social_playlist_start_pick(list, supported, plan->seed * 13ul + 5ul);
	name = pick ? start_name_of_kind(plan->gametype, pick) : NULL;
	if (!name || strlen(name) >= sizeof(plan->gametype))
		return;
	snprintf(plan->gametype, sizeof(plan->gametype), "%s", name);
	plan->start = pick;
}

/* AE's weapons can play: the weapon cache is loaded, or this PC has the cache file to read when the setting is forced on */
static int ae_social_weapons_available(void)
{
	enum ae_weapon_cache_state state = ae_weapon_cache_state();

	return state == AE_WEAPON_CACHE_LOADED || state == AE_WEAPON_CACHE_DEV || (!config_boolean("game.weapon_packs") &&
		ae_weapon_cache_file_present());
}

int ae_social_plan_names(struct ae_social_plan const *plan, char out[][24])
{
	int bots = plan->roster.count - plan->roster.humans, at, other;

	memset(out, 0, 24 * (size_t)(bots > 0 ? bots : 0));
	if (bots <= 0 || !ae_social_lobby_names(plan->seed, bots, out))
		return 0;
	for (at = 0; at < bots && at < AE_SOCIAL_SEATS; at++)
		if (plan->bot_name[at][0])
			snprintf(out[at], 24, "%s", plan->bot_name[at]);
	/* (a seed's name that a bot of the plan's own has too: the seed's bot takes another, so no two bots share a name) */
	for (at = 0; at < bots && at < AE_SOCIAL_SEATS; at++)
	{
		int clash = 0;

		if (plan->bot_name[at][0])
			continue;
		for (other = 0; other < bots && other < AE_SOCIAL_SEATS; other++)
			clash |= other != at && !strcmp(out[other], out[at]);
		if (clash)
		{
			char taken[AE_SOCIAL_SEATS][24];

			memcpy(taken, out, sizeof(taken[0]) * (size_t)bots);
			if (!ae_rematch_fill_in(plan->seed + 7919ul * (unsigned long)(at + 1), (char const (*)[24])taken, bots, out[at]))
				snprintf(out[at], 24, "BOT %d", at + 1);
		}
	}
	return 1;
}

/* the party's bots take the first bot seats: their names and tiers, and every party seat on the leader's team */
static void party_seat(struct ae_social_plan *plan, struct ae_social_playlist const *list)
{
	char names[AE_PARTY_MAX][AE_PARTY_NAME];
	int tiers[AE_PARTY_MAX], count, at, limit = ae_social_party_limit(list), bots = plan->roster.count - plan->roster.humans;
	int team[AE_SOCIAL_SEATS], member[AE_SOCIAL_SEATS];

	count = ae_glue_party_bots(names, tiers, AE_PARTY_MAX);
	if (count > limit - plan->humans)
		count = limit - plan->humans;
	if (count > bots - 1)
		count = bots - 1;
	if (count <= 0)
		return;
	for (at = 0; at < count; at++)
	{
		struct ae_social_seat *seat = &plan->roster.seat[plan->roster.humans + at];
		struct ae_social_tier const *tier = ae_social_tier(tiers[at]);

		snprintf(plan->bot_name[at], sizeof(plan->bot_name[at]), "%s", names[at]);
		if (tier)
		{
			seat->tier = tiers[at];
			seat->shooting = tier->shooting;
			seat->movement = tier->movement;
			seat->level = tier->level;
		}
	}
	plan->party_bots = count;
	for (at = 0; at < count; at++)
		plan->party_seat[plan->roster.humans + at] = 1;
	if (plan->roster.teams > 0)
	{
		for (at = 0; at < plan->roster.count; at++)
		{
			team[at] = plan->roster.seat[at].team;
			member[at] = at < plan->roster.humans + count;
		}
		ae_party_keep_on_team(team, member, plan->roster.count, plan->roster.seat[0].team);
		for (at = 0; at < plan->roster.count; at++)
			plan->roster.seat[at].team = team[at];
	}
	platform_log("ae party: %d bots in the party play with you (%s ...)", count, names[0]);
}

int ae_social_plan_make(char const *playlist, unsigned long seed, char const *map, char const *gametype, struct ae_social_plan *plan)
{
	struct ae_social_playlist const *list = ae_social_playlist_by_id(playlist);
	char key[AE_SR_KEY];
	int pick = -1, humans = ae_social_party_size();

	memset(plan, 0, sizeof(*plan));
	if (!list)
		return 0;
	if (humans > ae_social_party_limit(list))
		humans = ae_social_party_limit(list);
	snprintf(plan->playlist, sizeof(plan->playlist), "%s", list->id);
	snprintf(plan->label, sizeof(plan->label), "%s", list->label);
	plan->seed = seed ? seed : now_seed();
	plan->humans = humans;
	plan->mode = ae_social_mode(list->id);
	if (map && map[0])
	{
		int each;

		for (each = 0; each < list->count; each++)
			if (!strcmp(list->entry[each].map, map))
			{
				pick = each;
				break;
			}
		snprintf(plan->map, sizeof(plan->map), "%s", map);
		if (pick >= 0)
			snprintf(plan->gametype, sizeof(plan->gametype), "%s", list->entry[pick].gametype);
		else if (list->count)
			snprintf(plan->gametype, sizeof(plan->gametype), "%s", list->entry[0].gametype);
	}
	else
	{
		pick = ae_glue_recent_pick(list, ae_social_map_here, NULL, (int)game.last_pick - 1, plan->seed * 7ul + 3ul);
		if (pick < 0)
			return 0;
		snprintf(plan->map, sizeof(plan->map), "%s", list->entry[pick].map);
		snprintf(plan->gametype, sizeof(plan->gametype), "%s", list->entry[pick].gametype);
	}
	if (gametype && gametype[0])
		snprintf(plan->gametype, sizeof(plan->gametype), "%s", gametype);
	plan->entry = pick;
	/* the start loadout: by the plan's seed among the playlist's starts the game type has (a game type the caller named: its own) */
	if (!(gametype && gametype[0]) && list->starts)
		start_resolve(list, game.force_start, plan);
	game.force_start = -1;
	plan->ae_weapons = list->weapons_own ? 2 : ae_social_weapons_available();
	if (!ae_social_screen_key(0, key, sizeof(key)))
		key[0] = 0;
	if (!ae_social_roster_for(list->id, key, humans, plan->seed, &plan->roster))
		return 0;
	party_seat(plan, list);
	plan->ok = 1;
	return 1;
}

int ae_social_plan_vote_choices(struct ae_social_plan const *plan, struct ae_social_vote_choice *out, int max)
{
	struct ae_social_playlist const *list = ae_social_playlist_by_id(plan->playlist);

	if (!list)
		return 0;
	return ae_glue_recent_vote(list, ae_social_map_here, NULL, (int)game.last_pick - 1, plan->seed * 11ul + 1ul, out, max);
}

int ae_social_plan_choose(struct ae_social_plan *plan, int entry)
{
	struct ae_social_playlist const *list = ae_social_playlist_by_id(plan->playlist);

	if (!list || entry < 0 || entry >= list->count)
		return 0;
	snprintf(plan->map, sizeof(plan->map), "%s", list->entry[entry].map);
	snprintf(plan->gametype, sizeof(plan->gametype), "%s", list->entry[entry].gametype);
	plan->entry = entry;
	plan->start = AE_SOCIAL_START_NONE;
	if (list->starts)
		start_resolve(list, -1, plan);
	return 1;
}

/* ---------- starting */

static void settings_save(void)
{
	int each;

	if (game.saved)
		return;
	for (each = 0; each < CONFIG_KEYS; each++)
	{
		/* bots.count is an integer setting: config_string answers "" for it */
		if (!strcmp(config_keys[each], "bots.count"))
			snprintf(game.saved_value[each], SAVED, "%ld", config_integer(config_keys[each]));
		else if (boolean_key(config_keys[each]))
			snprintf(game.saved_value[each], SAVED, "%s", config_boolean(config_keys[each]) ? "true" : "false");
		else
		{
			char const *value = config_string(config_keys[each]);

			snprintf(game.saved_value[each], SAVED, "%s", value ? value : "");
		}
	}
	for (each = 0; each < AE_SEAT_MOST; each++)
	{
		int shooting, movement;

		game.has_style[each] = ae_bots_seat_style_get(each, &game.saved_style[each]) ? 1 : 0;
		game.saved_shooting[each] = game.saved_movement[each] = 0;
		if (ae_bots_seat_skill_levels(each, &shooting, &movement))
		{
			game.saved_shooting[each] = shooting + 1;
			game.saved_movement[each] = movement + 1;
		}
	}
	/* WEAPON PACKS (the player's own) and the START the mode's file reads as */
	game.saved_packs = config_boolean("game.weapon_packs") ? 1 : 0;
	game.packs_forced = 0;
	game.start_saved = 0;
	game.saved = 1;
}

/* a tournament game plays AE's weapons whatever the player's own WEAPON PACKS: off is turned on and the cache read now (the menus
   are up: no map is loaded); with no usable cache the game plays stock weapons, and the log says why */
static void weapons_force(struct ae_social_plan const *plan)
{
	/* (a playlist of the maps' own weapons: the player's setting is left alone, unless its start is a cache weapon: the PROTOMAG
	   needs the cache, and the map's own weapons still play, ae_weapons staying 2) */
	if (plan->ae_weapons == 2 && plan->start != AE_SOCIAL_START_PROTOMAG && plan->start != AE_SOCIAL_START_SINGLESHOT_BR)
	{
		platform_log("ae social: the maps' own weapons play (%s)", plan->playlist);
		return;
	}
	if (!game.saved_packs)
	{
		config_set("game.weapon_packs", "true");
		ae_weapon_pack_reload();
		game.packs_forced = 1;
	}
	if (ae_weapon_cache_state() == AE_WEAPON_CACHE_LOADED || ae_weapon_cache_state() == AE_WEAPON_CACHE_DEV)
		platform_log("ae social: AE weapons on (the weapon cache is loaded%s)", game.packs_forced ? ", WEAPON PACKS was off" : "");
	else
		platform_log("ae social: AE weapons off, stock weapons play: %s", ae_weapon_cache_refusal() ? ae_weapon_cache_refusal() :
			"this PC has no weapon cache (no packs folder)");
}

/* the START the mode's file reads as is the plan's (a start of the plan's game type): the player's own is kept first */
static void start_keep(char const *gametype)
{
	wchar_t wide[AE_SOCIAL_GAMETYPE];
	short place;

	name_wide(gametype, wide, AE_SOCIAL_GAMETYPE);
	game.saved_start[0] = 0;
	for (place = 0; place < 3; place++)
	{
		char const *name = arena_gametype_start_name(wide, place);
		wchar_t named[AE_SOCIAL_GAMETYPE];

		if (!name)
			return;
		name_wide(name, named, AE_SOCIAL_GAMETYPE);
		if (arena_gametype_start_is_current(named))
		{
			snprintf(game.saved_start, sizeof(game.saved_start), "%s", name);
			game.start_saved = 1;
			return;
		}
	}
}

static void settings_restore(void)
{
	int each;

	ae_bots_seat_skill_clear();
	ae_bots_seat_style_clear();
	ae_bots_seat_name_clear();
	if (game.saved)
	{
		for (each = 0; each < CONFIG_KEYS; each++)
			config_set(config_keys[each], game.saved_value[each]);
		for (each = 0; each < AE_SEAT_MOST; each++)
		{
			if (game.saved_shooting[each])
				ae_bots_seat_skill_set(each, game.saved_shooting[each], game.saved_movement[each]);
			if (game.has_style[each])
				ae_bots_seat_style_set(each, &game.saved_style[each]);
		}
	}
	if (game.start_saved)
	{
		wchar_t wide[AE_SOCIAL_GAMETYPE];

		name_wide(game.saved_start, wide, AE_SOCIAL_GAMETYPE);
		arena_gametype_start_choose(wide);
		game.start_saved = 0;
	}
	arena_gametype_starts_set_run_only(FALSE);
	if (game.saved && !game.saved_packs && game.packs_forced)
	{
		config_set("game.weapon_packs", "false");
		game.packs_back = 1;
	}
	game.saved = 0;
}

/* the cache read again as the player's setting has it, once no game is up */
static void packs_put_back(void)
{
	if (game.packs_back && !game.active && !game_in_progress())
	{
		ae_weapon_pack_reload();
		game.packs_back = 0;
		game.packs_forced = 0;
		platform_log("ae social: WEAPON PACKS back as it was");
	}
}

/* the profiles the game's teardown took, put back once the menus are up and no game is (a guest has none, and keeps being one) */
static void profiles_put_back(void)
{
	short local;

	if (!game.profiles_back || game_in_progress() || !main_menu_is_active())
		return;
	for (local = 0; local < AE_SOCIAL_LOCAL; local++)
	{
		long now = player_ui_get_active_player_profile_index(local);

		if (ae_social_profile_put_back(game.profile_saved[local], now, ae_profile_is_guest(local)) &&
			ae_profile_switch(local, (int)game.profile_saved[local]).ok)
			platform_log("ae social: player %d's profile put back after the game", (int)local + 1);
	}
	if (!game.active)
		game.profiles_back = 0;
}

static char const *skill_word(int step)
{
	static char word[16];
	int at;

	snprintf(word, sizeof(word), "%s", ae_social_skill_name(step));
	for (at = 0; word[at]; at++)
		word[at] = (char)tolower((unsigned char)word[at]);
	return word;
}

int ae_social_plan_start(struct ae_social_plan const *plan)
{
	struct ae_social_roster const *roster = &plan->roster;
	char value[16];
	char names[AE_SOCIAL_SEATS][AE_SOCIAL_LOBBY_NAME];
	int seat, first_shooting = 3;

	if (!plan->ok || game.active)
		return 0;
	settings_save();
	start_keep(plan->gametype);
	/* the game's START is for this run only: arena_gametype_starts.txt keeps the player's own, whatever happens to the game
	   (a crash cannot leave the tournament's choice in it) */
	arena_gametype_starts_set_run_only(TRUE);
	platform_log("ae social: START choices are for this game only (the player's file is not written)");
	weapons_force(plan);
	snprintf(value, sizeof(value), "%d", roster->count - roster->humans);
	config_set("bots.count", value);
	config_set("bots.teams", "even");
	config_set("bots.shooting_skill", "same");
	config_set("bots.movement_skill", "same");
	/* (a tournament's bots always fight back: PLAYFIGHT, the aim practice, is put back after the game) */
	config_set("bots.playfight", "false");
	/* (and play their whole game: the bot test switches at their defaults, the player's put back after it too) */
	config_set("bots.routes", "auto");
	config_set("bots.vantage", "true");
	config_set("bots.high_ground", "true");
	config_set("bots.drive_obstacles", "true");
	config_set("bots.learn_map", "true");
	ae_bots_seat_skill_clear();
	ae_bots_seat_style_clear();
	ae_bots_seat_name_clear();
	/* each bot's play style follows its lobby name (the same name always plays the same way), and it plays under that
	   name (the scoreboard, kill feed and post-game say what the lobby showed) */
	memset(names, 0, sizeof(names));
	if (!ae_social_plan_names(plan, names))
		memset(names, 0, sizeof(names));
	for (seat = roster->humans; seat < roster->count; seat++)
	{
		struct ae_social_seat const *bot = &roster->seat[seat];
		struct ae_bots_style style;

		if (seat == roster->humans)
			first_shooting = bot->shooting;
		ae_bots_seat_skill_set(seat - roster->humans, bot->shooting, bot->movement);
		if (names[seat - roster->humans][0])
		{
			ae_bots_seat_name_set(seat - roster->humans, names[seat - roster->humans]);
			ae_bots_style_from_hash(ae_social_name_hash(names[seat - roster->humans]), &style);
			ae_bots_seat_style_set(seat - roster->humans, &style);
		}
	}
	/* (a bot without a pair of its own would play this) */
	config_set("bots.skill", skill_word(first_shooting));
	game.plan = *plan;
	game.rematch = plan->rematch_no;
	for (seat = 0; seat < AE_SOCIAL_LOCAL; seat++)
		game.profile_saved[seat] = seat < roster->humans ? player_ui_get_active_player_profile_index((short)seat) : NONE;
	game.counted = 0;
	game.running = 0;
	game.pending_written = 0;
	game.pending_tries = 0;
	game.reopen = 0;
	memset(game.pg, 0, sizeof(game.pg));
	memset(game.has_key, 0, sizeof(game.has_key));
	game.active = 1;
	game.started = (unsigned long)time(NULL);
	if (!ae_screen_create_game_social_open(plan->map, plan->gametype, (short)plan->humans))
	{
		platform_log("ae social: the game did not open (%s on %s)", plan->gametype, plan->map);
		game.active = 0;
		settings_restore();
		packs_put_back();
		return 0;
	}
	/* (rc2: the PROTOMAG start: the pistol start's primary weapon is the cache's starter pistol, put in the game's AE WEAPONS block) */
	if (plan->start == AE_SOCIAL_START_PROTOMAG)
		ae_setup_edit_start_weapon(AE_SOCIAL_PROTOMAG_ID);
	/* (rc5: the SINGLESHOT BR start: the BR start's primary weapon is the cache's SINGLESHOT BR) */
	if (plan->start == AE_SOCIAL_START_SINGLESHOT_BR)
		ae_setup_edit_start_weapon(AE_SOCIAL_SINGLESHOT_BR_ID);
	if (plan->entry >= 0)
	{
		game.last_pick = (unsigned long)plan->entry + 1ul;
		if (!plan->staged)
			ae_glue_recent_played(ae_social_playlist_by_id(plan->playlist), plan->entry);
	}
	platform_log("ae social: start %s: %s on %s, %d bots, mode %s, seed %lu, start %s", plan->playlist, plan->gametype, plan->map,
		roster->count - roster->humans, plan->mode ? skill_word(plan->mode) : "auto", plan->seed,
		plan->start ? ae_social_start_label(plan->start) : "game type's own");
	ae_dev_event("social_start", "\"id\":\"%s\",\"map\":\"%s\",\"gametype\":\"%s\",\"mode\":\"%s\",\"tier\":%d,\"start\":\"%s\"", plan->playlist,
		plan->map, plan->gametype, plan->mode ? skill_word(plan->mode) : "auto", roster->player_tier, ae_social_start_label(plan->start));
	return 1;
}

int ae_social_game_active(void)
{
	return game.active;
}

/* (0.9.3 AE PICKUPS, ae_pickup_rules.h) a tournament game of AE's weapons: the map's weapons are AE's too (CLASSIC COMP's
   playlists play the maps' own: ae_weapons 2) */
int ae_social_game_pickups(void)
{
	return game.active && game.plan.ae_weapons == 1;
}

char const *ae_social_game_label(void)
{
	return game.active ? game.plan.label : "";
}

char const *ae_social_game_start_label(void)
{
	return game.active ? ae_social_start_label(game.plan.start) : "";
}

/* ---------- the way through */

static int game_running(void)
{
	return game_engine_running() && !main_menu_is_active();
}

static void keys_read(void)
{
	short local;

	for (local = 0; local < AE_SOCIAL_LOCAL && local < game.plan.humans; local++)
		game.has_key[local] = ae_social_screen_key(local, game.key[local], sizeof(game.key[local]));
}

static void pending_write(void)
{
	short local;
	int saved = 1;

	keys_read();
	for (local = 0; local < game.plan.humans; local++)
		if (game.has_key[local] && !ae_social_pending_begin(game.key[local], game.plan.playlist))
		{
			platform_log("ae social: the pending game for player %d could not be saved", (int)local + 1);
			saved = 0;
		}
	/* (a failed save is tried again on the next frames, three times in all: never a write every frame on a full disk) */
	game.pending_tries++;
	game.pending_written = saved || game.pending_tries >= 3;
}

static void pending_clear(void)
{
	short local;

	for (local = 0; local < game.plan.humans; local++)
		if (game.has_key[local])
			ae_social_pending_end(game.key[local]);
}

/* every seat's zero-based place for a free for all: the human at his place, the bots in the places left, the stronger ones first
   (the engine's bot numbers are not the roster's seats: the better placed bots are taken to be the stronger) */
static void places_of(struct ae_social_roster const *roster, int self, int place, int *seat_place)
{
	int order[AE_SOCIAL_SEATS], count = 0, seat, a, b, next = 0;
	int taken[AE_SOCIAL_SEATS] = { 0 };

	for (seat = 0; seat < roster->count; seat++)
		if (seat != self)
			order[count++] = seat;
	for (a = 0; a < count; a++)
		for (b = a + 1; b < count; b++)
			if (roster->seat[order[b]].level > roster->seat[order[a]].level)
			{
				int swap = order[a];

				order[a] = order[b];
				order[b] = swap;
			}
	seat_place[self] = place;
	taken[place < AE_SOCIAL_SEATS ? place : 0] = 1;
	for (a = 0; a < count; a++)
	{
		while (next < AE_SOCIAL_SEATS && taken[next])
			next++;
		seat_place[order[a]] = next++;
	}
}

static void fill_pg(short local, struct ae_postgame_seat const *seat, int outcome, int players, struct ae_social_result const *result)
{
	struct ae_social_pg *pg = &game.pg[local];
	int ffa = game.plan.roster.teams == 0;

	memset(pg, 0, sizeof(*pg));
	pg->shown = 1;
	pg->ffa = ffa;
	pg->players = players;
	pg->delta = ae_social_exp_change(outcome);
	if (pg->delta < 0)
		pg->delta = 0;
	if (!game.has_key[local])
	{
		pg->guest = 1;
		snprintf(pg->sub, sizeof(pg->sub), "Sign in a profile to keep a career");
		return;
	}
	/* (a result not counted or not written to career.txt is not claimed: the row says so, as a guest's does) */
	if (!result->saved)
	{
		pg->guest = 1;
		pg->delta = 0;
		snprintf(pg->sub, sizeof(pg->sub), "Not saved: career.txt could not be written");
		(void)seat;
		return;
	}
	pg->exp_before = result->exp_before;
	pg->exp_after = result->exp_after;
	pg->rank_before = result->rank_before;
	pg->rank_after = result->rank_after;
	ae_social_view_postgame_sub(game.plan.label, ffa, pg->delta > 0, players, result->rank_before, result->exp_after, pg->sub, (int)sizeof(pg->sub));
	(void)seat;
}

static void result_count(void)
{
	struct ae_postgame_facts facts;
	char const *recorded[AE_SOCIAL_LOCAL];
	int saved = 0;
	short local;

	ae_glue_postgame_facts(&facts);
	keys_read();
	memset(recorded, 0, sizeof(recorded));
	for (local = 0; local < game.plan.humans && local < AE_POSTGAME_SEATS; local++)
	{
		struct ae_postgame_seat const *seat = &facts.seats[local];
		struct ae_social_result result;
		int outcome, seat_place[AE_SOCIAL_SEATS], *places = NULL;

		if (!seat->present)
			continue;
		memset(&result, 0, sizeof(result));
		if (game.plan.roster.teams == 0)
		{
			outcome = ae_social_outcome_ffa(seat->place_index < 0 ? 0 : seat->place_index, facts.players);
			if (local < game.plan.roster.count)
			{
				places_of(&game.plan.roster, local, seat->place_index < 0 ? 0 : seat->place_index, seat_place);
				places = seat_place;
			}
		}
		else
			outcome = seat->result == AE_POSTGAME_WIN ? AE_SOCIAL_WIN : seat->result == AE_POSTGAME_TIE ? AE_SOCIAL_TIE :
				AE_SOCIAL_LOSS;
		if (game.has_key[local])
		{
			if (!ae_social_result_apply(game.key[local], game.plan.playlist, outcome, &game.plan.roster, local, places,
				seat->kills >= 0 ? (int)seat->kills : -1, seat->deaths >= 0 ? (int)seat->deaths : -1, &result))
				platform_log("ae social: result not counted for player %d", (int)local + 1);
		}
		fill_pg(local, seat, outcome, facts.players, &result);
		/* (a player whose game was saved to career.txt: the bots keep their record against that profile) */
		if (game.has_key[local] && result.saved && local < AE_SOCIAL_LOCAL)
		{
			recorded[local] = game.key[local];
			saved++;
		}
	}
	game.counted = 1;
	pending_clear();
	/* (0.9.4) the bots' service records: once for a counted game, only when a result was saved (never for a forgiven
	   crash: that game never reaches here) */
	if (saved)
		ae_bot_records_game_counted(&game.plan, recorded, AE_SOCIAL_LOCAL);
}

void ae_social_game_tick(void)
{
	packs_put_back();
	profiles_put_back();
	if (game.dev)
	{
		if (!ae_screen_postgame_up())
			game.dev = 0;
		return;
	}
	if (game.reopen && !game.active && !game_running() && main_menu_is_active() && !ae_ui_depth())
	{
		game.reopen = 0;
		/* (the next game's lobby opens over AE TOURNAMENT at once: a game just played reopens as the REMATCH, its roster kept; else its
		   search, then its map vote; B in it goes back to the playlists) */
		if (ae_screen_tournament_open(NULL, game.plan.playlist))
		{
			if (!ae_screen_social_lobby_open_rematch())
				ae_screen_social_lobby_open(game.plan.playlist, 0, NULL, 0);
		}
		return;
	}
	if (!game.active)
		return;
	if (game_running())
	{
		game.running = 1;
		game.profiles_back = 1;
		if (!game.pending_written && !game.counted)
			pending_write();
		if (!game.counted && ae_glue_postgame_due())
			result_count();
		ae_bot_records_tick();
		return;
	}
	/* (the game is over, or never started (the menus are up and 180 s have passed since START, room for a slow
	   converted map's load): the settings come back) */
	if (game.running || (main_menu_is_active() && !ae_ui_depth() && (unsigned long)time(NULL) - game.started > 180ul))
	{
		if (!game.counted && game.running)
		{
			/* (the game closed with no post-game and no QUIT: counted as a crash would be: forgiven) */
			pending_clear();
			platform_log("ae social: the game ended with no result: not counted");
		}
		settings_restore();
		game.active = 0;
		platform_log("ae social: settings back");
	}
}

/* ---------- leaving */

int ae_social_game_quit_costs(short local)
{
	return game.active && !game.counted && local >= 0 && local < AE_SOCIAL_LOCAL && local < game.plan.humans &&
		ae_social_screen_key(local, game.key[local], sizeof(game.key[local])) &&
		ae_social_profile_exp(game.key[local], game.plan.playlist) > 0;
}

void ae_social_game_quit_text(short local, char *body, size_t body_size)
{
	long exp = 0;
	char key[AE_SR_KEY];

	if (ae_social_screen_key(local, key, sizeof(key)))
		exp = ae_social_profile_exp(key, game.plan.playlist);
	ae_social_view_quit_body(game.plan.label, exp, body, (int)body_size);
}

void ae_social_game_quit_help(char *help, size_t size)
{
	ae_social_view_quit_help(game.plan.label, help, (int)size);
}

int ae_social_game_quit(short local)
{
	struct ae_social_result result;
	char key[AE_SR_KEY];

	if (!game.active || game.counted || local < 0 || local >= game.plan.humans)
		return 0;
	if (!ae_social_screen_key(local, key, sizeof(key)))
		return 0;
	if (!ae_social_result_apply(key, game.plan.playlist, AE_SOCIAL_QUIT, &game.plan.roster, local, NULL, -1, -1, &result))
		return 0;
	platform_log("ae social: quit %s: player %d left early", game.plan.playlist, (int)local + 1);
	ae_dev_event("social_quit", "\"id\":\"%s\",\"player\":%d", game.plan.playlist, (int)local + 1);
	ae_social_pending_end(key);
	/* (every local player left: the game is over; one of a split screen leaves the others playing) */
	if (game.plan.humans <= 1)
		game.counted = 1;
	return 1;
}

int ae_social_game_postgame_seat(short local, struct ae_social_pg *out)
{
	if (local < 0 || local >= AE_SOCIAL_LOCAL || !game.pg[local].shown)
		return 0;
	*out = game.pg[local];
	return 1;
}

/* the finished game's facts for the post-game lobby: the banner, the score, EXP and rank, each local player's kills and deaths */
static void last_capture(void)
{
	struct ae_postgame_facts facts;
	int local;

	memset(&game.last, 0, sizeof(game.last));
	if (!game.active || !game.plan.ok)
		return;
	ae_glue_postgame_facts(&facts);
	game.last.valid = 1;
	snprintf(game.last.title, sizeof(game.last.title), "%s", facts.banner);
	snprintf(game.last.score, sizeof(game.last.score), "%s", facts.banner_score);
	snprintf(game.last.map, sizeof(game.last.map), "%s", game.plan.map);
	game.last.exp_delta = game.pg[0].shown ? game.pg[0].delta : 0;
	game.last.rank_after = game.pg[0].shown ? game.pg[0].rank_after : 0;
	for (local = 0; local < game.plan.humans && local < AE_SOCIAL_LOCAL && local < AE_POSTGAME_SEATS; local++)
	{
		game.last.kills[local] = facts.seats[local].kills >= 0 ? (int)facts.seats[local].kills : -1;
		game.last.deaths[local] = facts.seats[local].deaths >= 0 ? (int)facts.seats[local].deaths : -1;
	}
	/* (each party bot that played: its record with the player) */
	{
		char names[AE_SOCIAL_SEATS][24];
		int seat;

		if (!game.plan.staged && ae_social_plan_names(&game.plan, names))
			for (seat = game.plan.roster.humans; seat < game.plan.roster.count; seat++)
			{
				int won = facts.seats[0].result == AE_POSTGAME_WIN;

				if (game.plan.party_seat[seat])
					ae_glue_party_credit(names[seat - game.plan.roster.humans], won);
				else
					ae_glue_party_note(names[seat - game.plan.roster.humans], game.plan.roster.seat[seat].tier,
						game.plan.roster.teams > 0 && game.plan.roster.seat[seat].team == game.plan.roster.seat[0].team, won);
			}
	}
}

int ae_social_game_last(struct ae_social_last *last)
{
	if (!game.last.valid)
		return 0;
	*last = game.last;
	return 1;
}

int ae_social_game_rematch(struct ae_social_plan *plan, int *game_no, int *fresh, char left[][24], int *left_count)
{
	int may[AE_SOCIAL_SEATS], out[AE_SOCIAL_LEAVE], bots, seat, count, at, humans;
	char names[AE_SOCIAL_SEATS][24];

	if (!game.last.valid || !game.plan.ok)
		return 0;
	*plan = game.plan;
	plan->rematch_no = game.rematch + 1;
	*left_count = 0;
	*game_no = game.rematch;
	humans = plan->roster.humans;
	bots = plan->roster.count - humans;
	memset(fresh, 0, sizeof(int) * AE_SOCIAL_SEATS);
	memset(may, 0, sizeof(may));
	for (seat = humans; seat < plan->roster.count; seat++)
		may[seat] = !plan->party_seat[seat];
	ae_social_plan_names(plan, names);
	count = ae_rematch_leavers(plan->seed, game.rematch + 1, plan->roster.count, may, out, AE_SOCIAL_LEAVE);
	for (at = 0; at < count; at++)
	{
		int bot = out[at] - humans;
		char fill[24];

		/* (a fill-in: the first name of a seeded pool that no seat has) */
		if (!ae_rematch_fill_in(plan->seed + (unsigned long)(game.rematch + 1) * 977ul + (unsigned long)bot, (char const (*)[24])names,
			bots, fill))
			continue;
		if (*left_count < AE_SOCIAL_LEAVE)
			snprintf(left[(*left_count)++], 24, "%s", names[bot]);
		snprintf(names[bot], 24, "%s", fill);
		snprintf(plan->bot_name[bot], sizeof(plan->bot_name[bot]), "%s", fill);
		fresh[out[at]] = 1;
	}
	platform_log("ae social: rematch %d: %d bots left, %d fill-ins", game.rematch + 1, *left_count, *left_count);
	return 1;
}

int ae_social_game_reopen_pending(void)
{
	return game.reopen && !game.dev;
}

int ae_social_game_continue(void)
{
	if (!game.active && !game.dev)
		return 0;
	if (game.active && game.plan.ok)
		last_capture();
	/* (an online party carries on together: its leader asks the service for the next game, no lobby of this PC's own opens) */
	if (!game.dev && ae_party_follow_continue(game.plan.playlist))
	{
		ae_glue_postgame_leave();
		return 1;
	}
	game.reopen = 1;
	ae_glue_postgame_leave();
	return 1;
}

/* ---------- DEV MODE */

static struct
{
	char kind[12], playlist[AE_SOCIAL_ID];
	long exp[AE_SOCIAL_LOCAL];
	int seats;
} dev_table;

int ae_social_game_dev_postgame(char const *kind, char const *playlist, long exp_before, int place_index, int place_players,
	int player, char *text, size_t size)
{
	int players;

	struct ae_social_playlist const *list = ae_social_playlist_by_id(playlist);
	struct ae_postgame_facts facts;
	int local, rankup = !strcmp(kind, "rankup"), ffa, outcome;
	static unsigned int const colors[AE_POSTGAME_SEATS] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };

	if (!list)
	{
		snprintf(text, size, "social postgame: no playlist \"%s\"", playlist);
		return 0;
	}
	if (game.active)
	{
		snprintf(text, size, "social postgame: a social game is on");
		return 0;
	}
	ffa = list->teams == 0;
	/* (the same made-up game still up: this player's view is added; else the table starts again) */
	if (!(game.dev && ae_screen_postgame_up() && !strcmp(dev_table.kind, kind) && !strcmp(dev_table.playlist, list->id)))
	{
		int each;

		memset(&dev_table, 0, sizeof(dev_table));
		for (each = 0; each < AE_SOCIAL_LOCAL; each++)
			dev_table.exp[each] = -1;
		snprintf(dev_table.kind, sizeof(dev_table.kind), "%s", kind);
		snprintf(dev_table.playlist, sizeof(dev_table.playlist), "%s", list->id);
	}
	dev_table.exp[player] = exp_before;
	if (player + 1 > dev_table.seats)
		dev_table.seats = player + 1;
	players = dev_table.seats;
	if (game.dev)
	{
		ae_screen_postgame_dev_close();
		game.dev = 0;
	}
	memset(&facts, 0, sizeof(facts));
	memset(game.pg, 0, sizeof(game.pg));
	snprintf(game.plan.playlist, sizeof(game.plan.playlist), "%s", list->id);
	snprintf(game.plan.label, sizeof(game.plan.label), "%s", list->label);
	facts.teams = ffa ? 0 : 1;
	facts.host = 1;
	facts.can_continue = 1;
	facts.players = (short)(ffa ? (place_players > 0 ? place_players : 8) : list->teams * list->size);
	snprintf(facts.gametype, sizeof(facts.gametype), "%s", list->count ? list->entry[0].gametype : "");
	{
		int at;

		snprintf(facts.map, sizeof(facts.map), "%s", list->count ? list->entry[0].map : "");
		for (at = 0; facts.map[at]; at++)
		{
			if (facts.map[at] == '@')
			{
				facts.map[at] = 0;
				break;
			}
			facts.map[at] = (char)toupper((unsigned char)facts.map[at]);
		}
	}
	snprintf(facts.played, sizeof(facts.played), "12:07");
	if (!ffa)
	{
		int win = !strcmp(kind, "win") || rankup, tie = !strcmp(kind, "tie");

		snprintf(facts.banner, sizeof(facts.banner), tie ? "TIE GAME" : win ? "RED TEAM WINS" : "BLUE TEAM WINS");
		snprintf(facts.banner_score, sizeof(facts.banner_score), win ? "50 - 38" : tie ? "50 - 50" : "38 - 50");
		facts.title_color = tie ? 0xEEF3F6FFu : win ? 0xC0392BFFu : 0x2E86DEFFu;
		facts.banner_color = facts.title_color;
		facts.banner_text_color = 0xFFFFFFFFu;
	}
	for (local = 0; local < players && local < AE_POSTGAME_SEATS; local++)
	{
		struct ae_postgame_seat *seat = &facts.seats[local];
		struct ae_social_result result;
		struct ae_social_pg *pg = &game.pg[local];
		char key[AE_SR_KEY];
		int has_key = ae_social_screen_key((short)local, key, sizeof(key)), win_here;
		long before = dev_table.exp[local] >= 0 ? dev_table.exp[local] : has_key ? ae_social_profile_exp(key, list->id) : 0;

		seat->present = 1;
		seat->slot = (short)(local + 1);
		seat->place_index = -1;
		snprintf(seat->name, sizeof(seat->name), "PLAYER %d", local + 1);
		seat->kills = 17 - 3 * local;
		seat->deaths = 9 + local;
		seat->assists = 4;
		seat->spree = 5;
		if (!ffa)
		{
			win_here = (!strcmp(kind, "win") || rankup) ? 1 : 0;
			seat->chip = seat->stripe = 0xC0392BFFu;
			seat->title_color = facts.title_color;
			snprintf(seat->team, sizeof(seat->team), "RED TEAM");
			snprintf(seat->title, sizeof(seat->title), "%s", facts.banner);
			snprintf(seat->result_sub, sizeof(seat->result_sub), "Red Team %s, Blue Team %s", win_here ? "50" : "38",
				win_here ? "38" : "50");
			seat->result = !strcmp(kind, "tie") ? AE_POSTGAME_TIE : win_here ? AE_POSTGAME_WIN : AE_POSTGAME_LOSS;
			snprintf(seat->result_text, sizeof(seat->result_text), "%s", seat->result == AE_POSTGAME_TIE ? "TIE" :
				seat->result == AE_POSTGAME_WIN ? "WIN" : "LOSS");
			outcome = seat->result == AE_POSTGAME_WIN ? AE_SOCIAL_WIN : seat->result == AE_POSTGAME_TIE ? AE_SOCIAL_TIE :
				AE_SOCIAL_LOSS;
		}
		else
		{
			char where[16], words[40];
			static char const *const spelled[] = { "FIRST", "SECOND", "THIRD", "FOURTH", "FIFTH", "SIXTH", "SEVENTH", "EIGHTH" };
			int at = place_index < 0 ? 0 : place_index;

			seat->chip = colors[local];
			seat->title_color = seat->stripe = 0xEEF3F6FFu;
			seat->result = AE_POSTGAME_PLACE;
			seat->place_index = (short)at;
			ae_social_view_ordinal(at, where, (int)sizeof(where));
			snprintf(seat->result_text, sizeof(seat->result_text), "%s", where);
			snprintf(words, sizeof(words), "%s", at < 8 ? spelled[at] : where);
			snprintf(seat->title, sizeof(seat->title), "%s PLACE", words);
			snprintf(seat->result_sub, sizeof(seat->result_sub), "of %d players", (int)facts.players);
			outcome = ae_social_outcome_ffa(at, facts.players);
		}
		/* (made up: the profile's numbers are only read, never written) */
		memset(&result, 0, sizeof(result));
		result.exp_before = before;
		result.exp_after = ae_social_exp_after(before, outcome);
		if (rankup && result.exp_after == result.exp_before)
			result.exp_after = before + 1;
		result.rank_before = ae_social_rank(result.exp_before);
		result.rank_after = ae_social_rank(result.exp_after);
		memset(pg, 0, sizeof(*pg));
		pg->shown = 1;
		pg->ffa = ffa;
		pg->players = facts.players;
		pg->delta = ae_social_exp_change(outcome) > 0 ? 1 : 0;
		pg->exp_before = result.exp_before;
		pg->exp_after = result.exp_after;
		pg->rank_before = result.rank_before;
		pg->rank_after = result.rank_after;
		ae_social_view_postgame_sub(list->label, ffa, pg->delta, facts.players, result.rank_before, result.exp_after, pg->sub, (int)sizeof(pg->sub));
		if (!has_key && !ae_dev_mode_on())
		{
			pg->guest = 1;
			snprintf(pg->sub, sizeof(pg->sub), "Sign in a profile to keep a career");
		}
	}
	if (!ae_screen_postgame_dev_open(&facts))
	{
		snprintf(text, size, "social postgame: the post-game did not open");
		return 0;
	}
	game.dev = 1;
	snprintf(text, size, "social postgame: %s %s, %d player%s", kind, list->id, players, players == 1 ? "" : "s");
	dev_table.seats = players;
	platform_log("ae social: %s", text);
	return 1;
}
