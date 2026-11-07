/*
AE_DEV_CMD.C

DEV MODE's commands in the game (ae_dev_cmd.h; the grammar and the host-only gate: ae_dev_cmd_rules.h).

Commands that only read or answer run when the line comes; the ones that make, move or kill wait for the game tick
(game_engine_update), where the game makes its own objects. "me" is the first local player. A tag is found by its
last name (sniper_rifle, "sniper rifle" or SNIPER RIFLE alike), else by a part of its path when only one has it.
*/

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "errors.h"
#include "cache/cache_files.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_engine_slayer.h"
#include "game/players.h"
#include "items/equipment_definitions.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "main/console.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "units/vehicle_definitions.h"
#include "../src/capture.h"
#include "../src/port_config.h"

#include "ae_grenade_catalogue.h"
#include "ae_grenades.h"
#include "ae_bomb_assault.h"
#include "ae_round.h"
#include "ae_zone_world.h"
#include "ae_glue_setup_options.h"
#include "ae_test_run_agent.h"
unsigned long system_milliseconds(void);
#include "infection.h"
#include "ae_dev_cmd.h"
#include "ae_dev_cmd_rules.h"
#include "ae_dev_cmd_state.h"
#include "ae_dev_events.h"
#include "ae_dev_mode.h"
#include "ae_view_model.h"
#include "ae_dev_ttk.h"
#include "ae_dev_dummy.h"
#include "ae_hud_offsets.h"
#include "ae_hud_style.h"
#include "ae_fireteam.h"
#include "ae_weapon_ids.h"
#include "ae_weapon_selection_game.h"
#include "bots.h"
#include "ae_server_playlist.h"

extern struct game_variant game_variant_global;

void platform_log(char const *format, ...);
/* (network_chat.c) the host's notice in every player's chat, and here */
void ae_chat_host_notice(char const *text);
/* (each game engine's file) a score slot set as it is: a team's (team) or a player's own (its absolute index) */
void ae_dev_slayer_score_set(long slot, boolean team, long score);
void ae_dev_oddball_score_set(long slot, boolean team, long score);
void ae_dev_ctf_score_set(long slot, boolean team, long score);
void ae_dev_king_score_set(long slot, boolean team, long score);
void ae_dev_bomb_score_set(long slot, boolean team, long score);
/* (game_engine_oddball.c) a point of Oddball's score in its own units: a second of ball time in ticks, or 1 */
long ae_dev_oddball_score_units(void);
/* (game_engine_ctf.c, game_engine_oddball.c) the engine's own return: each flag not held that was handled back on its
stand, however near it lies; the ball (a weapon) back at a ball spawn when it was picked up and is not held, falling
or not. The count sent home, or TRUE */
long ae_dev_ctf_flags_home(void);
boolean ae_dev_oddball_ball_home(long weapon_index);
/* (hs.c) hs_compile_and_evaluate */
boolean hs_compile_and_evaluate(char const *expression);

#define QUEUE 16
#define LINE 256
#define MATCHES 6
/* how far ahead of the player a vehicle is made (world units), and the ring of give all's weapons */
#define SPAWN_AHEAD 3.0f
#define GIVE_ALL_RING 1.5f
#define GIVE_ALL_MOST 64
/* how long a respawn waits for its player's death (ticks) */
#define RESPAWN_WAIT (10 * TICKS_PER_SECOND)
/* the objects dev commands made, which reset deletes */
#define MADE 128
/* reset's wait between the players' deaths and the clean-up (their held weapons dropped by then), and how much longer
it waits for a flag or ball a dead body still holds */
#define RESET_WAIT (TICKS_PER_SECOND / 2)
#define RESET_WAIT_MORE (2 * TICKS_PER_SECOND)

static char const *const source_names[AE_DEV_CMD_SOURCES] = { "console", "telnet", "test run", "script" };

static struct
{
	char line[LINE];
	int source;
} queue[QUEUE];
static int queued;
static boolean making;
static boolean announced;
/* the game time of the last tick: a new game starts it again, and its DEV MODE line is said again */
static long announced_time;
/* players killed by respawn, waiting to be put back at once: the ticks left to wait, 0 for none, and the player's
whole datum index (a slot another player takes later is not theirs) */
static long respawn_wait[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static long respawn_player[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
/* (the unit each waiting player had when put back: a different living unit is their next life, whoever spawned it) */
static long respawn_unit[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
/* the players a reset killed (whole datum index; NONE for none), whose deaths leave no trace */
static long quiet_death[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static boolean quiet_deaths_ready;
static long made[MADE];
static short made_count;
/* ticks to reset's clean-up, 0 for none; the source and line that asked */
static long reset_wait;
static int reset_source;
static char reset_line[LINE];
/* the settings the reset put back (at its start); the ticks it waited past RESET_WAIT */
static short reset_settings;
static long reset_waited;
/* the ticks time commands added to this game's time limit */
static long time_extra;
/* which game a pending reset and the kept rules belong to (ae_dev_cmd_state.h); the game's rules before this game's
first option command, which reset puts back */
static struct ae_dev_game_state game_state;
static struct game_variant saved_variant;
static struct game_variant_options saved_options;
/* the settings bots and config changed, each with its value before the first change, which reset (and a TEST RUN's
next load, its end, and DEV MODE turned off) puts back */
static struct ae_dev_settings settings_saved;
/* (game_engine_initialize_for_new_map) a new game's set-up came, for the tick to see */
static boolean new_game_set_up;
/* (hs.c's way in) the source of the line being evaluated: the console unless a caller said otherwise
(ae_dev_cmd_evaluate_from) */
static int evaluating_source = AE_DEV_CMD_FROM_CONSOLE;
/* whether the last answer said ok */
static boolean answered_ok;
/* (DEV MODE off) something of a game is kept that the off tick clears; FALSE once cleared, so an off tick costs a test */
static boolean state_kept = TRUE;

/* ---------- answers */

static void answer(int source, char const *line, boolean ok, char const *format, ...)
{
	char text[200];
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(text, sizeof text, format, arguments);
	va_end(arguments);
	answered_ok = ok;
	platform_log("dev cmd %s: %s -> %s %s", source >= 0 && source < AE_DEV_CMD_SOURCES ? source_names[source] : "?",
		line, ok ? "ok" : "error", text);
	/* (a plain line either way: a warning shows only while a typed command runs, and the queued ones run later, in
	the tick) */
	console_printf(FALSE, ok ? "%s" : "error: %s", text);
	if (ae_dev_events_on())
	{
		char line_text[LINE + 64], answer_text[256];

		ae_dev_event_text(line_text, sizeof line_text, line);
		ae_dev_event_text(answer_text, sizeof answer_text, text);
		ae_dev_event("cmd", "\"source\":\"%s\",\"line\":\"%s\",\"ok\":%s,\"text\":\"%s\"",
			source >= 0 && source < AE_DEV_CMD_SOURCES ? source_names[source] : "?", line_text, ok ? "true" : "false",
			answer_text);
	}
}

static boolean is_bot(struct player_datum const *player);
static void quiet_deaths_clear(void);
static short settings_back(void);

/* this machine hosts a game another person plays in: a player who is neither this machine's nor a bot (a local or
offline game, a TEST RUN's included, has none, and nobody needs telling) */
static boolean hosting(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (game_connection() == _game_connection_local || !global_network_game_server_get())
		return FALSE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->local_player_index == NONE && !is_bot(player))
			return TRUE;
	}
	return FALSE;
}

/* a change of the game, said in every player's chat when this machine hosts a network game (DEV MODE is seen by
all) */
static void tell_players(char const *format, ...)
{
	char text[160];
	va_list arguments;

	if (!hosting())
		return;
	va_start(arguments, format);
	vsnprintf(text, sizeof text, format, arguments);
	va_end(arguments);
	ae_chat_host_notice(text);
}

/* ---------- names */

/* a tag's last name, from its last backslash */
static char const *last_name(char const *name)
{
	char const *last = name ? strrchr(name, '\\') : NULL;

	return last ? last + 1 : name ? name : "";
}

/* a typed name and a tag's name the same, ignoring case, '_' and ' ' alike */
static boolean same_word(char const *typed, char const *name)
{
	for (; *typed && *name; typed++, name++)
	{
		char a = *typed == '_' ? ' ' : (char)tolower((unsigned char)*typed);
		char b = *name == '_' ? ' ' : (char)tolower((unsigned char)*name);

		if (a != b)
			return FALSE;
	}
	return *typed == *name;
}

static boolean has_part(char const *name, char const *typed)
{
	size_t length = strlen(typed), at;

	for (at = 0; name[at]; at++)
	{
		size_t i;

		for (i = 0; i < length && name[at + i]; i++)
		{
			char a = typed[i] == '_' ? ' ' : (char)tolower((unsigned char)typed[i]);
			char b = name[at + i] == '_' ? ' ' : (char)tolower((unsigned char)name[at + i]);

			if (a != b)
				break;
		}
		if (i == length)
			return TRUE;
	}
	return FALSE;
}

static boolean objective(char const *name)
{
	char const *last = last_name(name);

	return !strcmp(last, "flag") || !strcmp(last, "ball") || strstr(last, "oddball") != NULL;
}

/* the tag of group named typed: NONE, with why in error, when none or several are */
static long find_tag(long group, char const *typed, char const *kind, char *error, int size)
{
	struct tag_iterator iterator;
	long tag_index, exact[MATCHES], part[MATCHES];
	short exacts = 0, parts = 0;

	/* (a cache weapon by its registry key or layout word: "dmr", "evo", "pdw": its weapon, whatever the map's tags are called) */
	if (group == WEAPON_DEFINITION_TAG)
	{
		unsigned int id = ae_weapon_registry_id_of_word(typed);
		long definition_index = id ? ae_weapon_id_definition(id) : NONE;

		if (definition_index != NONE)
			return definition_index;
	}
	tag_iterator_new(&iterator, group);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		char const *name = tag_get_name(tag_index);

		if (!name || (group == WEAPON_DEFINITION_TAG && (objective(name) || !ae_weapon_ids_is_listed(tag_index))))
			continue;
		if (same_word(typed, last_name(name)))
		{
			if (exacts < MATCHES)
				exact[exacts] = tag_index;
			exacts++;
		}
		else if (has_part(name, typed))
		{
			if (parts < MATCHES)
				part[parts] = tag_index;
			parts++;
		}
	}
	if (exacts == 1)
		return exact[0];
	/* (several of one name, as a pack's weapon named after the stock one it was made from: the one whose pack is
	named so, "...\packs\dmr\...") */
	if (exacts > 1)
	{
		char folder[96];
		short index, found = -1, count = 0;

		snprintf(folder, sizeof folder, "packs\\%s\\", typed);
		for (index = 0; index < exacts && index < MATCHES; index++)
		{
			char const *name = tag_get_name(exact[index]);

			if (name && has_part(name, folder))
			{
				found = index;
				count++;
			}
		}
		if (count == 1)
			return exact[found];
	}
	if (exacts == 0 && parts == 1)
		return part[0];
	if (exacts == 0 && parts == 0)
	{
		snprintf(error, size, "no %s named %s on this map (dev list %ss)", kind, typed, kind);
		return NONE;
	}
	{
		long *list = exacts ? exact : part;
		short count = exacts ? exacts : parts, i;
		int used = snprintf(error, size, "%d %ss match %s:", (int)count, kind, typed);

		for (i = 0; i < count && i < MATCHES && used > 0 && used < size; i++)
			used += snprintf(error + used, size - used, " %s", tag_get_name(list[i]));
	}
	return NONE;
}

/* ---------- players */

static long me(void)
{
	return local_player_get_player_index(0);
}

static long living_unit(long player_index)
{
	struct player_datum *player;
	struct unit_datum *unit;

	if (player_index == NONE)
		return NONE;
	player = player_get(player_index);
	if (player->unit_index == NONE)
		return NONE;
	unit = unit_get(player->unit_index);
	return TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ? NONE : player->unit_index;
}

static boolean is_bot(struct player_datum const *player)
{
	return player->local_player_index == NONE && bots_machine_is_bot(player->network_player_data.machine_index);
}

/* the point ahead of the player's unit, level, and that direction */
static boolean ahead_of(long unit_index, real distance, real_point3d *point, real_vector3d *forward)
{
	real_vector3d aim;
	real length;

	object_get_origin(unit_index, point);
	unit_get_aiming_vector(unit_index, &aim);
	aim.k = 0.0f;
	length = (real)sqrt(aim.i * aim.i + aim.j * aim.j);
	if (length < 0.01f)
	{
		aim.i = 1.0f;
		aim.j = 0.0f;
		length = 1.0f;
	}
	forward->i = aim.i / length;
	forward->j = aim.j / length;
	forward->k = 0.0f;
	point->x += forward->i * distance;
	point->y += forward->j * distance;
	point->z += 0.5f;
	return TRUE;
}

/* a spot of the map by name: a cutscene flag (the Test Box's named spots), its position and (yaw) the way it faces */
static boolean find_spot_facing(char const *typed, real_point3d *point, real *yaw)
{
	struct scenario *scenario = global_scenario_get();
	long index;

	if (!scenario)
		return FALSE;
	for (index = 0; index < scenario->cutscene_flags.count; index++)
	{
		struct scenario_cutscene_flag *flag =
			TAG_BLOCK_GET_ELEMENT(&scenario->cutscene_flags, index, struct scenario_cutscene_flag);

		if (same_word(typed, flag->name))
		{
			*point = flag->position;
			*yaw = flag->facing.yaw;
			return TRUE;
		}
	}
	return FALSE;
}

/* a spot's position alone */
static boolean find_spot(char const *typed, real_point3d *point)
{
	real yaw;

	return find_spot_facing(typed, point, &yaw);
}

/* ---------- making objects */

int ae_dev_cmd_making(void)
{
	return making;
}

static long make_object(long definition_index, real_point3d const *position, real_vector3d const *forward)
{
	struct object_placement_data placement_data;
	long object_index;

	object_placement_data_new(&placement_data, definition_index, NONE);
	placement_data.position = *position;
	if (forward)
	{
		placement_data.forward = *forward;
		placement_data.up.i = 0.0f;
		placement_data.up.j = 0.0f;
		placement_data.up.k = 1.0f;
	}
	making = TRUE;
	object_index = object_new(&placement_data);
	making = FALSE;
	if (object_index != NONE)
	{
		/* (the oldest forgotten when full: reset then leaves it) */
		if (made_count == MADE)
		{
			memmove(made, made + 1, sizeof(made[0]) * (MADE - 1));
			made_count--;
		}
		made[made_count++] = object_index;
	}
	return object_index;
}

int ae_dev_cmd_give_weapon(long player_index, long definition_index)
{
	long unit_index = living_unit(player_index), weapon_index;
	struct unit_datum *unit;
	short slot, held = 0;

	if (unit_index == NONE || definition_index == NONE)
		return FALSE;
	/* (a cache weapon in a network game that does not use the cache: refused, as a loadout's is) */
	if (ae_weapon_id_of_definition(definition_index) &&
		ae_weapon_selection_guard(&game_variant_global, game_variant_options_get(), "a DEV give"))
	{
		return FALSE;
	}
	unit = unit_get(unit_index);
	if (unit->object.parent_object_index != NONE)
		return FALSE;
	/* (two held: the one in hand goes to the floor for it) */
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
		held += unit->unit.weapon_object_indices[slot] != NONE;
	if (held >= 2)
		unit_drop_current_weapon(unit_index, TRUE);
	weapon_index = make_object(definition_index, &unit->object.position, NULL);
	if (weapon_index == NONE)
		return FALSE;
	if (!unit_add_weapon_to_inventory(unit_index, weapon_index, _unit_add_weapon_starting))
	{
		object_delete(weapon_index);
		return FALSE;
	}
	return TRUE;
}

/* ---------- the commands */

static void command_give(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	char error[160];
	long definition_index, unit_index = living_unit(me());

	if (unit_index == NONE)
	{
		answer(source, line, FALSE, "no living player to give to");
		return;
	}
	if (!strcmp(cmd->word[0], "all"))
	{
		struct tag_iterator iterator;
		long tag_index;
		short placed = 0, count = 0;
		real_point3d centre;

		object_get_origin(unit_index, &centre);
		tag_iterator_new(&iterator, WEAPON_DEFINITION_TAG);
		while ((tag_index = tag_iterator_next(&iterator)) != NONE)
			count += !objective(tag_get_name(tag_index)) && ae_weapon_ids_is_listed(tag_index) && count < GIVE_ALL_MOST;
		tag_iterator_new(&iterator, WEAPON_DEFINITION_TAG);
		while ((tag_index = tag_iterator_next(&iterator)) != NONE && placed < GIVE_ALL_MOST)
		{
			real_point3d at = centre;
			real angle;

			if (objective(tag_get_name(tag_index)) || !ae_weapon_ids_is_listed(tag_index))
				continue;
			angle = 2.0f * (real)M_PI * (real)placed / (real)(count ? count : 1);
			at.x += GIVE_ALL_RING * (real)cos(angle);
			at.y += GIVE_ALL_RING * (real)sin(angle);
			at.z += 0.3f;
			placed += make_object(tag_index, &at, NULL) != NONE;
		}
		answer(source, line, TRUE, "%d weapons on the floor round you", (int)placed);
		tell_players("DEV: the host put every weapon on the floor");
		return;
	}
	/* (0.7.6) an AE grenade type by its catalogue name ("give emp"): the most a player carries */
	if (ae_grenade_catalogue_id(cmd->word[0]) >= 0)
	{
		short id = (short)ae_grenade_catalogue_id(cmd->word[0]);
		short given = ae_grenades_give(unit_index, id, -1);

		if (given == NONE)
		{
			answer(source, line, FALSE, "%s is not in this game (the gametype's grenade types, and the grenade pack)",
				ae_grenade_catalogue_entry(id)->display);
			return;
		}
		answer(source, line, TRUE, "gave %d %s (grenade slot %d)", (int)given, ae_grenade_catalogue_entry(id)->display,
			(int)ae_grenades_slot_of(id));
		tell_players("DEV: the host took %s", ae_grenade_catalogue_entry(id)->display);
		return;
	}
	definition_index = find_tag(WEAPON_DEFINITION_TAG, cmd->word[0], "weapon", error, sizeof error);
	if (definition_index == NONE)
	{
		answer(source, line, FALSE, "%s", error);
		return;
	}
	if (!ae_dev_cmd_give_weapon(me(), definition_index))
	{
		answer(source, line, FALSE, "%s could not be given (seated, or not made)", tag_get_name(definition_index));
		return;
	}
	answer(source, line, TRUE, "gave %s", tag_get_name(definition_index));
	tell_players("DEV: the host took a %s", last_name(tag_get_name(definition_index)));
}

static void command_spawn(struct ae_dev_cmd const *cmd, char const *line, int source, boolean powerup)
{
	char error[160];
	long definition_index, unit_index = living_unit(me()), object_index;
	real_point3d at;
	real_vector3d forward;
	char const *kind = powerup ? "item" : "vehicle";

	definition_index = find_tag(powerup ? EQUIPMENT_DEFINITION_TAG : VEHICLE_DEFINITION_TAG, cmd->word[0], kind,
		error, sizeof error);
	if (definition_index == NONE)
	{
		answer(source, line, FALSE, "%s", error);
		return;
	}
	if (!powerup && cmd->words == 2 && strcmp(cmd->word[1], "here") && strcmp(cmd->word[1], "ahead"))
	{
		real yaw;

		if (!find_spot_facing(cmd->word[1], &at, &yaw))
		{
			answer(source, line, FALSE, "no spot named %s on this map (dev list spots)", cmd->word[1]);
			return;
		}
		/* (the way the spot faces, as goto turns a player) */
		forward.i = (real)cos(yaw);
		forward.j = (real)sin(yaw);
		forward.k = 0.0f;
	}
	else if (unit_index == NONE)
	{
		answer(source, line, FALSE, "no living player to place it by");
		return;
	}
	else if (powerup)
	{
		/* (at the player's feet: picked up as they stand there, so "now" and not alike) */
		object_get_origin(unit_index, &at);
		forward.i = 1.0f;
		forward.j = 0.0f;
		forward.k = 0.0f;
	}
	else
		ahead_of(unit_index, cmd->words == 2 && !strcmp(cmd->word[1], "here") ? SPAWN_AHEAD * 0.7f : SPAWN_AHEAD,
			&at, &forward);
	object_index = make_object(definition_index, &at, &forward);
	if (object_index == NONE)
	{
		answer(source, line, FALSE, "%s could not be made", tag_get_name(definition_index));
		return;
	}
	answer(source, line, TRUE, "made %s at %.1f %.1f %.1f", tag_get_name(definition_index), at.x, at.y, at.z);
	tell_players("DEV: the host made a %s", last_name(tag_get_name(definition_index)));
}

/* the players a respawn or kill is for: me, all, or bots */
static boolean targeted(long player_index, char const *who)
{
	struct player_datum *player = player_get(player_index);

	if (!strcmp(who, "all"))
		return TRUE;
	if (!strcmp(who, "bots"))
		return is_bot(player);
	return player_index == me();
}

static void command_kill(struct ae_dev_cmd const *cmd, char const *line, int source, boolean respawn)
{
	struct data_iterator iterator;
	struct player_datum *player;
	char const *who = cmd->words ? cmd->word[0] : "me";
	short killed = 0, waiting = 0;

	if (respawn && !quiet_deaths_ready)
		quiet_deaths_clear();
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long player_index = iterator.datum_index, unit_index;

		if (!targeted(player_index, who))
			continue;
		unit_index = living_unit(player_index);
		if (unit_index != NONE && respawn)
		{
			/* (respawn's death is a reset's: nobody's kill, no suicide counted, no feed line, no medal) */
			quiet_death[DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index)] = player_index;
			unit_kill_no_statistics(unit_index);
			killed++;
		}
		else if (unit_index != NONE)
		{
			unit_kill(unit_index);
			killed++;
		}
		/* (a round mode's player not spawned yet in the live round: dies as they spawn) */
		else if (!respawn && ae_round_dev_kill_unspawned(player_index))
			killed++;
		if (respawn)
		{
			respawn_wait[DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index)] = RESPAWN_WAIT;
			respawn_player[DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index)] = player_index;
			respawn_unit[DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index)] = player->unit_index;
			waiting++;
		}
	}
	if (respawn)
		answer(source, line, TRUE, "%d players back at once (%d killed first)", (int)waiting, (int)killed);
	else
		answer(source, line, TRUE, "%d killed", (int)killed);
	tell_players(respawn ? "DEV: the host respawned %s" : "DEV: the host killed %s",
		!strcmp(who, "me") ? "themselves" : who);
}

/* (each tick) a respawn's players put back once dead */
static void respawn_update(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long *wait = &respawn_wait[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)];

		if (*wait <= 0)
			continue;
		/* (another player's slot now, or a bot leaving the game: not brought back) */
		if (respawn_player[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] != iterator.datum_index ||
			player->quit_out_of_game_time != NONE || player->quit_out_of_game)
		{
			*wait = 0;
			continue;
		}
		/* (a reset's bots wait for its clean-up: by then the bots a smaller bots.count lets go are leaving) */
		if (reset_wait > 0 && is_bot(player))
			continue;
		/* (a round mode puts the dead back at once: a new living unit is the respawn done, not a wait for the old one's
		going) */
		if (player->unit_index != NONE && player->unit_index != respawn_unit[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] &&
			living_unit(iterator.datum_index) != NONE)
		{
			*wait = 0;
			continue;
		}
		(*wait)--;
		if (player->unit_index == NONE)
		{
			/* (1, not 0: the spawn's own test takes 1 as ready, next tick) */
			player->respawn_timer = MIN(player->respawn_timer, 1);
			*wait = 0;
		}
		/* (a quiet death asked for that never came is not some later one's) */
		else if (*wait == 0)
			quiet_death[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] = NONE;
	}
}

static void command_end(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	if (game_engine_showing_postgame())
	{
		answer(source, line, FALSE, "the game is over already");
		return;
	}
	if (!strcmp(cmd->word[0], "round") && ae_round_on())
	{
		char text[160];
		boolean ok = ae_round_dev_round("end", text, sizeof text);

		answer(source, line, ok, "%s", text);
		if (ok)
			tell_players("DEV: the host ended the round");
		return;
	}
	if (!strcmp(cmd->word[0], "round"))
	{
#ifdef HALO_GAME_BROWSER
		if (!game_engine || game_engine->type != game_engine_bomb || !game_engine_bomb_rounds())
		{
			answer(source, line, FALSE, "this gametype has no rounds (end game ends it)");
			return;
		}
		ae_bomb_snd_round_won(0);
		answer(source, line, TRUE, "round ended, won by red");
		tell_players("DEV: the host ended the round");
#else
		/* (no bomb engine on this platform: no round games to end) */
		answer(source, line, FALSE, "this gametype has no rounds (end game ends it)");
#endif
		return;
	}
	/* (a round mode's end is the host's whole game, not a round's) */
	if (ae_round_on())
		game_engine_end_game_final();
	else
		game_engine_end_game();
	answer(source, line, TRUE, "game ended");
	tell_players("DEV: the host ended the game");
}

/* ---------- seat */

/* `seat [driver|gunner|passenger]`: the host sits in the nearest vehicle's first free seat of that kind (driver when none is
named): a vehicle's gun, reticle and view can be tested without driving there */
static void command_seat(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	long unit_index = living_unit(me()), nearest = NONE;
	real nearest_distance = 0.0f;
	struct object_iterator vehicles;
	short seat, seats;
	char const *kind = cmd->words > 0 ? cmd->word[0] : "driver";

	if (unit_index == NONE)
	{
		answer(source, line, FALSE, "no living player to seat");
		return;
	}
	object_iterator_new(&vehicles, _object_mask_vehicle, 0);
	while (object_iterator_next(&vehicles))
	{
		real distance = distance_squared3d(&object_get(unit_index)->object.position,
			&object_get(vehicles.index)->object.position);

		if (nearest == NONE || distance < nearest_distance)
		{
			nearest = vehicles.index;
			nearest_distance = distance;
		}
	}
	if (nearest == NONE)
	{
		answer(source, line, FALSE, "no vehicle in the game (spawn one: spawn <vehicle> here)");
		return;
	}
	seats = unit_definition_get(object_get(nearest)->definition_index)->unit.seats.count;
	for (seat = 0; seat < seats; seat++)
	{
		boolean wanted = !strcmp(kind, "driver") ? unit_seat_is_driver(nearest, seat) :
			!strcmp(kind, "gunner") ? unit_seat_is_gunner(nearest, seat) :
			!unit_seat_is_driver(nearest, seat) && !unit_seat_is_gunner(nearest, seat);

		if (wanted && !unit_seat_filled(nearest, seat) && unit_enter_seat(unit_index, nearest, seat))
		{
			answer(source, line, TRUE, "seated as the %s of %s (seat %d)", kind,
				tag_get_name(object_get(nearest)->definition_index), (int)seat);
			return;
		}
	}
	answer(source, line, FALSE, "no free %s seat in the nearest vehicle", kind);
}

/* ---------- god */

/* the player `god` protects, NONE for nobody (this game only) */
static long god_player = NONE;

/* the player a god command names: me, an absolute index, or the start of a name (any case) */
static long god_target(char const *who)
{
	struct data_iterator iterator;
	struct player_datum *player;
	size_t length = strlen(who);

	if (!strcmp(who, "me"))
		return me();
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		size_t letter;
		boolean named = length > 0;

		if (who[0] >= '0' && who[0] <= '9')
		{
			if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == (long)atol(who))
				return iterator.datum_index;
			continue;
		}
		for (letter = 0; named && letter < length; letter++)
		{
			char a = letter < 11 && player->name[letter] < 128 ? (char)player->name[letter] : 0, b = who[letter];

			if (a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');
			if (b >= 'A' && b <= 'Z')
				b = (char)(b - 'A' + 'a');
			named = a == b;
		}
		if (named)
			return iterator.datum_index;
	}
	return NONE;
}

static void command_god(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	char const *who = cmd->words ? cmd->word[0] : "me";
	char name[16];
	long target;
	int index;

	if (!strcmp(who, "off"))
	{
		god_player = NONE;
		answer(source, line, TRUE, "god off");
		platform_log("dev: god off");
		tell_players("DEV: the host ended god mode");
		return;
	}
	target = god_target(who);
	if (target == NONE || !player_try_and_get(target))
	{
		answer(source, line, FALSE, "no player %s", who);
		return;
	}
	for (index = 0; index < 11 && player_get(target)->name[index]; index++)
		name[index] = player_get(target)->name[index] < 128 && player_get(target)->name[index] >= 32 ?
			(char)player_get(target)->name[index] : '?';
	name[index] = 0;
	god_player = target;
	answer(source, line, TRUE, "god on for %s (god off ends it)", name);
	platform_log("dev: god on for %s", name);
	tell_players("DEV: the host made %s unhurtable", name);
}

int ae_dev_cmd_god_protects(long object_index)
{
	long player_index;

	if (god_player == NONE)
		return FALSE;
	/* (DEV MODE turned off, the game over or gone: the protection ends) */
	if (!ae_dev_mode_on() || !game_engine_running() || game_engine_game_over() || !player_try_and_get(god_player))
	{
		god_player = NONE;
		return FALSE;
	}
	player_index = player_index_from_unit_index(object_index);
	return player_index != NONE && player_index == god_player;
}

/* the round modes' commands (ae_round.h): round end|next|live, zone now|small|off, down <player> [here], revive <player> */
static void command_round(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	char text[160];
	boolean ok;

	switch (cmd->id)
	{
	case AE_DEV_CMD_ROUND:
		ok = ae_round_dev_round(cmd->word[0], text, sizeof text);
		break;
	case AE_DEV_CMD_ZONE:
		if (cmd->words == 2)
		{
			/* (zone look a|b: this machine's drawing alone) */
			ae_zone_world_set_look(cmd->word[1][0]);
			snprintf(text, sizeof text, "the zone's edge is drawn as the %s here", cmd->word[1][0] == 'a' ? "energy wall (a)" :
				"light curtain (b)");
			answer(source, line, TRUE, "%s", text);
			return;
		}
		ok = ae_round_dev_zone(cmd->word[0], text, sizeof text);
		break;
	case AE_DEV_CMD_DOWN:
		ok = ae_round_dev_down(cmd->word[0], cmd->words == 2, text, sizeof text);
		break;
	default:
		ok = ae_round_dev_revive(cmd->word[0], text, sizeof text);
		break;
	}
	answer(source, line, ok, "%s", text);
	if (ok)
		tell_players("DEV: %s", text);
}

/* the running engine's score setter, NULL when it has none */
static void (*score_setter(void))(long, boolean, long)
{
	if (!game_engine)
		return NULL;
	switch (game_engine->type)
	{
	case game_engine_slayer:
		return ae_dev_slayer_score_set;
	case game_engine_oddball:
		return ae_dev_oddball_score_set;
	case game_engine_ctf:
		return ae_dev_ctf_score_set;
	case game_engine_king:
		return ae_dev_king_score_set;
	case game_engine_bomb:
		return ae_dev_bomb_score_set;
	default:
		return NULL;
	}
}

/* every score of the game back to 0 */
static void scores_cleared(void)
{
	void (*set)(long, boolean, long) = score_setter();
	long slot;

	if (!set)
		return;
	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		set(slot, TRUE, 0);
		set(slot, FALSE, 0);
	}
}

/* the running engine's units for one point the score command names: King and (most) Oddball count ticks of time */
static long score_units(void)
{
	if (game_engine && game_engine->type == game_engine_king)
		return TICKS_PER_SECOND;
	if (game_engine && game_engine->type == game_engine_oddball)
		return ae_dev_oddball_score_units();
	return 1;
}

static void command_score(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	void (*set)(long, boolean, long) = score_setter();
	long player_index = me(), units = score_units(), points = cmd->number * units;
	long slot = !strcmp(cmd->word[0], "blue") ? 1 : 0;

	if (!set)
	{
		answer(source, line, FALSE, "this gametype has no score to set");
		return;
	}
	if (!strcmp(cmd->word[0], "me"))
	{
		long team_slot;

		if (player_index == NONE)
		{
			answer(source, line, FALSE, "no player here");
			return;
		}
		team_slot = player_get(player_index)->team_index;
		/* (slayer and oddball keep a player's own score beside their team's, which decides the game even in free for
		all: the team's moves with it, as the game's own scoring does; the others score by team) */
		if (game_engine->type == game_engine_slayer || game_engine->type == game_engine_oddball)
		{
			long individual = game_engine->get_player_score(player_index, _get_score_individual);
			long team = game_engine->get_player_score(player_index, _get_score_team);

			set(DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), FALSE, points);
			set(team_slot, TRUE, team + points - individual);
		}
		else
			set(team_slot, TRUE, points);
	}
	else
		set(slot, TRUE, points);
	answer(source, line, TRUE, "%s now %ld%s", cmd->word[0], cmd->number, units > 1 ? " s" : "");
	tell_players("DEV: the host set %s's score to %ld", cmd->word[0], cmd->number);
}

/* viewmodel: the weapon in hand's first-person placement, live (ae_view_model.h) */
static void command_viewmodel(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	struct ae_view_model model;
	char key[AE_VIEW_MODEL_KEY];

	if (!ae_view_model_held(me(), key, sizeof key, &model))
	{
		answer(source, line, FALSE, "no weapon in hand");
		return;
	}
	if (cmd->words == 4)
	{
		model.offset[0] = cmd->position[0];
		model.offset[1] = cmd->position[1];
		model.offset[2] = cmd->position[2];
		model.scale = (float)atof(cmd->word[3]);
		if (!ae_view_model_set(key, &model))
		{
			answer(source, line, FALSE, "offsets up to %.0f either way and a scale of %.1f to %.1f", AE_VIEW_MODEL_OFFSET_MAXIMUM,
				AE_VIEW_MODEL_SCALE_MINIMUM, AE_VIEW_MODEL_SCALE_MAXIMUM);
			return;
		}
	}
	else if (cmd->words == 8 && !strcmp(cmd->word[0], "place"))
	{
		/* (the arms and gun as one piece: right, up, forward (model units), yaw, pitch, roll (degrees), scale) */
		model.place[0] = (float)atof(cmd->word[1]);
		model.place[1] = (float)atof(cmd->word[2]);
		model.place[2] = (float)atof(cmd->word[3]);
		model.turn[0] = (float)atof(cmd->word[4]);
		model.turn[1] = (float)atof(cmd->word[5]);
		model.turn[2] = (float)atof(cmd->word[6]);
		model.place_scale = (float)atof(cmd->word[7]);
		if (!ae_view_model_set(key, &model))
		{
			answer(source, line, FALSE, "moves up to %.0f either way, turns up to %.0f degrees and a scale of %.1f to %.1f",
				AE_VIEW_MODEL_PLACE_MAXIMUM, AE_VIEW_MODEL_TURN_MAXIMUM, AE_VIEW_MODEL_SCALE_MINIMUM,
				AE_VIEW_MODEL_SCALE_MAXIMUM);
			return;
		}
	}
	else if (cmd->words == 1 && !strcmp(cmd->word[0], "probe"))
	{
		ae_view_model_probe_request();
		answer(source, line, TRUE, "%s: the next frame's markers go to the log (viewmodel probe: lines)", key);
		return;
	}
	else if (cmd->words == 1 && !strcmp(cmd->word[0], "clear"))
	{
		ae_view_model_reset(key);
		ae_view_model_held(me(), key, sizeof key, &model);
	}
	else if (cmd->words == 1)
	{
		int kept = ae_view_model_save();

		answer(source, line, kept, "%s", kept ? "view_models.txt written" : "view_models.txt could not be written");
		return;
	}
	answer(source, line, TRUE, "%s: viewmodel %g %g %g %g, place %g %g %g %g %g %g %g", key, model.offset[0], model.offset[1],
		model.offset[2], model.scale, model.place[0], model.place[1], model.place[2], model.turn[0], model.turn[1],
		model.turn[2], model.place_scale == 0.0f ? 1.0f : model.place_scale);
}

/* ttk: every kill answered with the killer's time to it (ae_dev_ttk.h) */
static void command_ttk(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	if (cmd->words == 1)
		ae_dev_ttk_set(!strcmp(cmd->word[0], "on"));
	answer(source, line, TRUE, "ttk is %s%s", ae_dev_ttk_is_on() ? "on" : "off",
		cmd->words == 1 && ae_dev_ttk_is_on() ? ": each kill is answered with its time" : "");
}

static void setting_keep(char const *key);

/* dummy: bots that stand still (ae_dev_dummy.h); in a team game they are on the team the players are not on */
static struct ae_dev_dummy_settings dummy_settings;

/* bots.count and bots.teams back as they were before the first dummy (bots.count only when nobody changed it since) */
static void dummy_settings_back(void)
{
	char count[16], teams[16];

	if (!ae_dev_dummy_settings_take(&dummy_settings, config_integer("bots.count"), count, sizeof count, teams,
		sizeof teams))
		return;
	if (count[0])
		config_set("bots.count", count);
	config_set("bots.teams", teams);
}

static void command_dummy(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	long wanted;
	char count[8], error[96];

	if (cmd->words == 1 && !strcmp(cmd->word[0], "clear"))
	{
		int gone = ae_dev_dummy_clear(&wanted);

		snprintf(count, sizeof count, "%ld", wanted);
		setting_keep("bots.count");
		config_set("bots.count", count);
		ae_dev_dummy_settings_set(&dummy_settings, wanted);
		dummy_settings_back();
		answer(source, line, TRUE, "%d dummies leave", gone);
		return;
	}
	if (!ae_dev_dummy_add(me(), (int)cmd->number, cmd->position[1] != 0.0f, cmd->position[0], &wanted, error,
		sizeof error))
	{
		answer(source, line, FALSE, "%s", error);
		return;
	}
	snprintf(count, sizeof count, "%ld", wanted);
	{
		char before_count[16], before_teams[16];

		snprintf(before_count, sizeof before_count, "%ld", config_integer("bots.count"));
		snprintf(before_teams, sizeof before_teams, "%s", config_string("bots.teams"));
		ae_dev_dummy_settings_keep(&dummy_settings, before_count, before_teams);
	}
	setting_keep("bots.count");
	config_set("bots.count", count);
	ae_dev_dummy_settings_set(&dummy_settings, wanted);
	setting_keep("bots.teams");
	config_set("bots.teams", "against");
	if (cmd->position[1] != 0.0f)
		answer(source, line, TRUE, "%ld dummies here, facing you", cmd->number);
	else
		answer(source, line, TRUE, "%ld dummies %g units ahead, facing you", cmd->number, (double)cmd->position[0]);
}

/* hud: the held weapon's HUD elements, moved at the draw (ae_hud_offsets.h) */
static void command_hud(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	struct ae_view_model unused;
	struct ae_hud_element element;
	char key[AE_VIEW_MODEL_KEY], seen_key[AE_HUD_KEY];
	int index;

	if (cmd->words == 1 && !strcmp(cmd->word[0], "save"))
	{
		int kept = ae_hud_offsets_save();

		answer(source, line, kept, "%s", kept ? "hud_offsets.txt written" : "hud_offsets.txt could not be written");
		return;
	}
	if (!ae_view_model_held(me(), key, sizeof key, &unused))
	{
		answer(source, line, FALSE, "no weapon in hand");
		return;
	}
	ae_hud_offsets_seen_key(seen_key, sizeof seen_key);
	/* (the elements a draw of this weapon's HUD had: the HUD is drawn a frame after the weapon is taken up) */
	if (strcmp(seen_key, key) || !ae_hud_offsets_seen_count())
	{
		answer(source, line, FALSE, "no HUD drawn for %s yet: try again in a moment", key);
		return;
	}
	if (cmd->words == 0)
	{
		struct ae_hud_seen seen;

		for (index = 0; ae_hud_offsets_seen_get(index, &seen); index++)
		{
			ae_hud_offsets_get(key, seen.index, &element);
			console_printf(FALSE, "%d %s %s: %g %g x%g%s", seen.index, ae_hud_offsets_kind_name(seen.kind),
				seen.bitmap[0] ? seen.bitmap : "-", element.dx, element.dy, element.scale, element.hide ? " hidden" : "");
			platform_log("dev hud: %s %d %s %s: %g %g x%g%s", key, seen.index, ae_hud_offsets_kind_name(seen.kind),
				seen.bitmap[0] ? seen.bitmap : "-", element.dx, element.dy, element.scale, element.hide ? " hidden" : "");
		}
		answer(source, line, TRUE, "%s: %d HUD elements%s", key, index,
			ae_hud_style_is_ae() ? " (HUD STYLE AE draws its own: set display.hud_style classic to see them)" : "");
		return;
	}
	if (cmd->words == 1 && !strcmp(cmd->word[0], "reset"))
	{
		int dropped = ae_hud_offsets_clear(key, -1);

		answer(source, line, TRUE, "%s: %d elements as the tag has them", key, dropped);
		return;
	}
	if (cmd->words == 2 && !strcmp(cmd->word[0], "reset"))
	{
		ae_hud_offsets_clear(key, (int)cmd->number);
		answer(source, line, TRUE, "%s: element %ld as the tag has it", key, cmd->number);
		return;
	}
	/* (an element the weapon's HUD has) */
	{
		struct ae_hud_seen seen;
		int found = 0;

		for (index = 0; ae_hud_offsets_seen_get(index, &seen); index++)
			found |= seen.index == (int)cmd->number;
		if (!found)
		{
			answer(source, line, FALSE, "%s has no HUD element %ld (hud lists them)", key, cmd->number);
			return;
		}
	}
	ae_hud_offsets_get(key, (int)cmd->number, &element);
	element.index = (int)cmd->number;
	if (cmd->words == 2)
		element.hide = !strcmp(cmd->word[1], "hide");
	else
	{
		element.dx = cmd->position[0];
		element.dy = cmd->position[1];
		if (cmd->words == 4)
			element.scale = cmd->position[2];
	}
	if (!ae_hud_offsets_set(key, &element))
	{
		answer(source, line, FALSE, "moves up to %.0f either way and a scale of %.1f to %.1f", AE_HUD_OFFSET_MAXIMUM,
			AE_HUD_SCALE_MINIMUM, AE_HUD_SCALE_MAXIMUM);
		return;
	}
	answer(source, line, TRUE, "%s: element %d: %g %g x%g%s", key, element.index, element.dx, element.dy, element.scale,
		element.hide ? " hidden" : "");
}

static void command_time(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	long limit = game_variant_options_get()->time_limit, left;

	if (infection_enabled())
	{
		answer(source, line, FALSE, "Infection keeps its own round time");
		return;
	}
	if (limit <= 0)
	{
		answer(source, line, FALSE, "this game has no time limit");
		return;
	}
	/* (as option: a network game's clients keep their own clock) */
	if (hosting())
	{
		answer(source, line, FALSE, "time changes this machine's clock only: not with other players in it");
		return;
	}
	left = limit * 60L * TICKS_PER_SECOND - game_time_get() + time_extra;
	time_extra += cmd->number * TICKS_PER_SECOND - left;
	answer(source, line, TRUE, "%ld s left", cmd->number);
	tell_players("DEV: the host set the time left to %ld s", cmd->number);
}

long ae_dev_cmd_time_extra(void)
{
	return (ae_dev_mode_on() ? time_extra : 0) + ae_server_playlist_time_extra();
}

static void quiet_deaths_clear(void)
{
	short slot;

	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
		quiet_death[slot] = NONE;
	quiet_deaths_ready = TRUE;
}

int ae_dev_cmd_quiet_death(long dead_player_index)
{
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(dead_player_index);

	if (!quiet_deaths_ready || dead_player_index == NONE || slot < 0 || slot >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS ||
		quiet_death[slot] != dead_player_index)
	{
		return FALSE;
	}
	quiet_death[slot] = NONE;
	return TRUE;
}

/* reset, first half: every player out of the game's way as a team change takes them (a death that is nobody's kill,
nothing counted, no message: ae_dev_cmd_quiet_death) and back at once; the clean-up waits for their held weapons to
drop */
static void command_reset(char const *line, int source)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (!quiet_deaths_ready)
		quiet_deaths_clear();
	/* (the settings first: a bot count going down lets its bots go while they are dead, before any comes back) */
	reset_settings = settings_back();
	/* (a round mode's match starts again: the reset's deaths are no round's) */
	ae_round_dev_restart();
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long unit_index = living_unit(iterator.datum_index);

		if (unit_index != NONE)
		{
			quiet_death[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] = iterator.datum_index;
			unit_kill_no_statistics(unit_index);
		}
		respawn_wait[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] = RESPAWN_WAIT;
		respawn_player[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] = iterator.datum_index;
		respawn_unit[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] = player->unit_index;
	}
	reset_wait = RESET_WAIT;
	reset_waited = 0;
	ae_dev_game_reset_begin(&game_state);
	reset_source = source;
	snprintf(reset_line, sizeof reset_line, "%s", line);
}

/* a flag or ball still held by a dead body (its drop is still to come) */
static boolean objective_held_by_dead(void)
{
	struct object_iterator iterator;

	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while (object_iterator_next(&iterator))
	{
		struct object_datum *weapon = object_get(iterator.index);
		struct object_datum *holder;

		if (weapon->object.parent_object_index == NONE || !weapon_is_flag(iterator.index))
			continue;
		holder = object_try_and_get(weapon->object.parent_object_index);
		if (holder && TEST_FLAG(holder->object.damage_flags, _object_dead_bit))
			return TRUE;
	}
	return FALSE;
}

/* the game's objectives back where the game puts them: CTF's flags on their stands, Oddball's ball at a ball spawn, each
by the engine's own return (the players' deaths dropped what they carried). How many went back */
static long objectives_home(void)
{
	struct object_iterator iterator;
	long count = 0;

	if (!game_engine || network_game_distributed_client())
		return 0;
	if (game_engine->type == game_engine_ctf)
		return ae_dev_ctf_flags_home();
	if (game_engine->type != game_engine_oddball)
		return 0;
	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while (object_iterator_next(&iterator))
		count += ae_dev_oddball_ball_home(iterator.index) != FALSE;
	return count;
}

/* reset, second half: what dev commands made deleted (not what is held), the scores and the time back, the rules
option changed, the settings bots and config changed, the flags and the ball home */
static void reset_finish(void)
{
	short index, deleted = 0, kept = 0, settings = reset_settings;
	boolean options;
	long home;

	for (index = 0; index < made_count; index++)
	{
		struct object_datum *object = object_try_and_get(made[index]);

		if (!object)
			continue;
		/* (held, a dead body's too: kept on the list, for the next reset to find on the ground) */
		if (object->object.parent_object_index != NONE)
		{
			made[kept++] = made[index];
			continue;
		}
		object_delete(made[index]);
		deleted++;
	}
	made_count = kept;
	/* (a death the reset asked for that never came is not some later one's) */
	quiet_deaths_clear();
	scores_cleared();
	time_extra = 0;
	ae_dev_game_reset_end(&game_state);
	/* (only this game's kept rules: another game's would go into a different gametype) */
	options = ae_dev_game_options_take(&game_state);
	if (options)
	{
		*game_engine_get_variant() = saved_variant;
		game_set_game_variant_options(&saved_options);
	}
	reset_settings = 0;
	home = objectives_home();
	if (home)
		error(_error_silent, "dev reset: %ld flags or balls home", home);
	answer(reset_source, reset_line, TRUE, "reset: everyone respawns, scores back to 0, time as the game's, %d made objects "
		"deleted (%d held, kept)%s, %d settings back, %ld flags or balls home", (int)deleted, (int)kept,
		options ? ", GAME OPTIONS back" : "", (int)settings, home);
	tell_players("DEV: the host reset the game");
}

/* the player's unit turned to yaw (radians, level), as the script console's teleport turns a player */
static void face(long player_index, real yaw)
{
	long unit_index = living_unit(player_index);
	struct player_datum *player = player_index != NONE ? player_get(player_index) : NULL;
	real_vector3d forward;

	if (unit_index == NONE || !player)
		return;
	forward.i = (real)cos(yaw);
	forward.j = (real)sin(yaw);
	forward.k = 0.0f;
	unit_get(unit_index)->unit.desired_facing_vector = forward;
	unit_get(unit_index)->unit.desired_aiming_vector = forward;
	unit_get(unit_index)->unit.desired_looking_vector = forward;
	if (player->local_player_index != NONE)
		player_control_set_facing(player->local_player_index, &forward);
}

static void command_goto(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	real_point3d at, toward;
	real yaw = 0.0f;
	boolean facing = FALSE;

	if (cmd->has_position)
	{
		at.x = cmd->position[0];
		at.y = cmd->position[1];
		at.z = cmd->position[2];
	}
	else if (cmd->words == 2)
	{
		struct scenario *scenario = global_scenario_get();
		struct player_starting_location *start;

		if (!scenario || cmd->number >= scenario->players.count)
		{
			answer(source, line, FALSE, "this map has %ld starts (0 up)", scenario ? (long)scenario->players.count : 0L);
			return;
		}
		start = TAG_BLOCK_GET_ELEMENT(&scenario->players, cmd->number, struct player_starting_location);
		at = start->position;
		yaw = start->facing;
		facing = TRUE;
	}
	else if (!find_spot_facing(cmd->word[0], &at, &yaw))
	{
		answer(source, line, FALSE, "no spot named %s on this map (dev list spots)", cmd->word[0]);
		return;
	}
	else
		facing = TRUE;
	/* (goto <spot> facing <spot>: toward the other one, level) */
	if (cmd->words == 3 && !cmd->has_position)
	{
		real other_yaw;

		if (!find_spot_facing(cmd->word[2], &toward, &other_yaw))
		{
			answer(source, line, FALSE, "no spot named %s on this map (dev list spots)", cmd->word[2]);
			return;
		}
		yaw = (real)atan2(toward.y - at.y, toward.x - at.x);
	}
	if (living_unit(me()) == NONE || !player_teleport(me(), NONE, &at))
	{
		answer(source, line, FALSE, "could not move there (no living player, or no room)");
		return;
	}
	if (facing)
		face(me(), yaw);
	answer(source, line, TRUE, "at %.1f %.1f %.1f%s", at.x, at.y, at.z, facing ? ", facing as set" : "");
	tell_players("DEV: the host moved to %.0f %.0f %.0f", at.x, at.y, at.z);
}

/* a setting's value kept before bots or config first changes it (once: the first value is the one to go back to; never
system.dev_mode) */
static void setting_keep(char const *key)
{
	char value[AE_DEV_SETTING_VALUE];

	if (ae_dev_settings_wants(&settings_saved, key) && config_text(key, value, sizeof value))
		ae_dev_settings_keep(&settings_saved, key, value);
}

static int setting_put_back(char const *key, char const *value, void *context)
{
	(void)context;
	error(_error_silent, "dev settings: %s back to %s", key, value);
	return config_set(key, value);
}

/* the kept settings put back (each said in the log, and so in the event stream): how many */
static short settings_back(void)
{
	return (short)ae_dev_settings_back(&settings_saved, setting_put_back, NULL);
}

void ae_dev_cmd_settings_back(void)
{
	if (settings_saved.count)
		settings_back();
}

static void command_bots(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	char count[8];
	int i;

	snprintf(count, sizeof count, "%ld", cmd->number);
	setting_keep("bots.count");
	config_set("bots.count", count);
	for (i = 1; i < cmd->words; i++)
	{
		char const *key = !strcmp(cmd->word[i], "even") || !strcmp(cmd->word[i], "against") ? "bots.teams" :
			"bots.skill";

		setting_keep(key);
		config_set(key, cmd->word[i]);
	}
	answer(source, line, TRUE, "bots %ld (at once in a local game; a hosted game takes it at its next game)",
		cmd->number);
	tell_players("DEV: the host set bots to %ld", cmd->number);
}

static void list_options(char const *line, int source);

static void list_tags(long group, char const *kind, char const *line, int source)
{
	struct tag_iterator iterator;
	long tag_index;
	char text[200];
	int used = 0, count = 0;

	text[0] = 0;
	tag_iterator_new(&iterator, group);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		char const *name = last_name(tag_get_name(tag_index));

		if (group == WEAPON_DEFINITION_TAG && objective(tag_get_name(tag_index)))
			continue;
		if (used + (int)strlen(name) + 2 >= (int)sizeof text)
		{
			console_printf(FALSE, "%s", text);
			used = 0;
			text[0] = 0;
		}
		used += snprintf(text + used, sizeof text - used, "%s%s", used ? ", " : "", name);
		count++;
	}
	if (used)
		console_printf(FALSE, "%s", text);
	answer(source, line, TRUE, "%d %ss", count, kind);
}

static void command_list(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	struct scenario *scenario;
	long index;

	if (!strcmp(cmd->word[0], "weapons"))
	{
		list_tags(WEAPON_DEFINITION_TAG, "weapon", line, source);
		return;
	}
	if (!strcmp(cmd->word[0], "vehicles"))
	{
		list_tags(VEHICLE_DEFINITION_TAG, "vehicle", line, source);
		return;
	}
	if (!strcmp(cmd->word[0], "items"))
	{
		list_tags(EQUIPMENT_DEFINITION_TAG, "item", line, source);
		return;
	}
	if (!strcmp(cmd->word[0], "options"))
	{
		list_options(line, source);
		return;
	}
	scenario = global_scenario_get();
	if (!scenario)
	{
		answer(source, line, FALSE, "no map loaded");
		return;
	}
	for (index = 0; index < scenario->cutscene_flags.count; index++)
	{
		struct scenario_cutscene_flag *flag =
			TAG_BLOCK_GET_ELEMENT(&scenario->cutscene_flags, index, struct scenario_cutscene_flag);

		console_printf(FALSE, "%s", flag->name);
	}
	answer(source, line, TRUE, "%ld named spots; starts 0 to %ld (goto start <n>)", (long)scenario->cutscene_flags.count,
		(long)scenario->players.count - 1);
}

static void command_help(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	int id;

	if (cmd->words == 1)
	{
		answer(source, line, TRUE, "%s", ae_dev_cmd_usage(ae_dev_cmd_find(cmd->word[0])));
		return;
	}
	for (id = AE_DEV_CMD_NONE + 1; id < AE_DEV_CMD_COUNT; id++)
		console_printf(FALSE, "%s", ae_dev_cmd_usage(id));
	answer(source, line, TRUE, "DEV MODE commands (with DEV MODE on, no \"dev\" needed; help is dev help)");
}

/* ---------- GAME OPTIONS rows */

/* text as a key: lowercase letters and digits only ("FALL DAMAGE", "fall_damage" and "falldamage" alike) */
static void key_of(char const *text, char *out, int size)
{
	int length = 0;

	for (; text && *text && length < size - 1; text++)
	{
		if (isalnum((unsigned char)*text))
			out[length++] = (char)tolower((unsigned char)*text);
	}
	out[length] = 0;
}

/* opens the GAME OPTIONS model on the game being played: 0 when there is none */
static boolean options_begin(void)
{
	struct game_variant *running = game_engine_running() ? game_engine_get_variant() : NULL;

	return running && ae_setup_edit_begin_rules(running, game_variant_options_get());
}

static void option_in_edit(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	struct ae_setup_access access;
	struct game_variant variant;
	struct game_variant_options options;
	char want_row[64], want_value[64], key[64];
	int option, value, found_row = -1;

	if (!options_begin())
	{
		answer(source, line, FALSE, "no game running");
		return;
	}
	ae_setup_edit_access(&access);
	key_of(cmd->word[0], want_row, sizeof want_row);
	key_of(cmd->word[1], want_value, sizeof want_value);
	for (option = 0; option < AE_SETUP_OPTIONS && found_row < 0; option++)
	{
		struct ae_setup_row const *row = ae_setup_row(option);

		if (!row || !row->label || access.get(access.context, option) < 0)
			continue;
		key_of(row->label, key, sizeof key);
		if (!strcmp(key, want_row) && ae_setup_applies(&access, option))
			found_row = option;
	}
	if (found_row < 0)
	{
		answer(source, line, FALSE, "no GAME OPTIONS row %s in this gametype (dev list options)", cmd->word[0]);
		return;
	}
	/* (only the rows the game reads as it plays: the others (TEAM PLAY, the mode, the loadout...) are set up when a
	game starts, and changing them under a running game breaks it) */
	if (found_row != AE_SO_FALL_DAMAGE && found_row != AE_SO_TIMERS)
	{
		answer(source, line, FALSE, "%s is set when a game starts: give the test a gametype with it",
			ae_setup_row(found_row)->label);
		return;
	}
	/* (a network game's clients keep the rules they were given at its start) */
	if (hosting())
	{
		answer(source, line, FALSE, "option changes this machine's game only: not with other players in it");
		return;
	}
	for (value = 0; value < ae_setup_row(found_row)->value_count; value++)
	{
		key_of(ae_setup_value_text(found_row, value), key, sizeof key);
		if (!strcmp(key, want_value))
			break;
	}
	if (value == ae_setup_row(found_row)->value_count)
	{
		char values[160];
		int used = 0, index;

		values[0] = 0;
		for (index = 0; index < ae_setup_row(found_row)->value_count && used < (int)sizeof values - 2; index++)
			used += snprintf(values + used, sizeof values - used, "%s%s", index ? ", " : "",
				ae_setup_value_text(found_row, index));
		answer(source, line, FALSE, "%s takes: %s", ae_setup_row(found_row)->label, values);
		return;
	}
	ae_setup_edit_set(found_row, value);
	/* (the game being played takes the rules at once: what reads them each tick changes now, what the game set up at
	its start stays until the next) */
	if (!ae_setup_edit_copy(&variant, &options))
	{
		answer(source, line, FALSE, "the GAME OPTIONS model is not open");
		return;
	}
	/* (the rules as the game started, for reset: kept at this game's first option) */
	if (ae_dev_game_options_keep(&game_state))
	{
		saved_variant = *game_engine_get_variant();
		saved_options = *game_variant_options_get();
	}
	*game_engine_get_variant() = variant;
	game_set_game_variant_options(&options);
	answer(source, line, TRUE, "%s is %s in this game", ae_setup_row(found_row)->label,
		ae_setup_value_text(found_row, value));
	tell_players("DEV: the host set %s to %s", ae_setup_row(found_row)->label, ae_setup_value_text(found_row, value));
}

static void list_options_in_edit(char const *line, int source)
{
	struct ae_setup_access access;
	int option, count = 0;

	if (!options_begin())
	{
		answer(source, line, FALSE, "no game running");
		return;
	}
	ae_setup_edit_access(&access);
	for (option = 0; option < AE_SETUP_OPTIONS; option++)
	{
		struct ae_setup_row const *row = ae_setup_row(option);
		int now = access.get(access.context, option);

		if (!row || !row->label || now < 0 || !ae_setup_applies(&access, option))
			continue;
		console_printf(FALSE, "%s: %s", row->label, ae_setup_value_text(option, now));
		/* (and in the log, so in the event stream: an agent reads the rows as the owner does) */
		error(_error_silent, "dev list options: %s: %s", row->label, ae_setup_value_text(option, now));
		count++;
	}
	answer(source, line, TRUE, "%d GAME OPTIONS rows (option <row> <value>)", count);
}

/* option and list options read the game's rules through the GAME OPTIONS model, whose one edit a pause or lobby screen
may have open: that edit is put aside and back, so the screen never applies the dev command's rules */
static void command_option(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	ae_setup_edit_push();
	option_in_edit(cmd, line, source);
	ae_setup_edit_pop();
}

static void list_options(char const *line, int source)
{
	ae_setup_edit_push();
	list_options_in_edit(line, source);
	ae_setup_edit_pop();
}

/* fireteam <order>: the host player's fireteam takes it (attack, defend, watch and go at the host's feet); recruit <bot>:
a bot of the team (the number the logs give it) joins the host player's fireteam as a leader's mark would pull it. The same
paths as the wheel's calls: ae_fireteam_dev_order, ae_fireteam_recruit_brain */
static void command_fireteam(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	static struct { char const *name; short kind; } const orders[] = { { "follow", _ae_order_follow }, { "regroup", _ae_order_regroup },
		{ "attack", _ae_order_attack }, { "defend", _ae_order_defend }, { "watch", _ae_order_watch }, { "go", _ae_order_go },
		{ "clear", _ae_order_none } };
	long player_index = me();
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	long fireteam = player ? ae_fireteam_led_by(player) : NONE;
	real_point3d here;
	boolean has_here = FALSE;
	short index;

	if (!strcmp(cmd->word[0], "status"))
	{
		long slot;

		for (slot = 0; slot < ae_fireteam_slot_count(); slot++)
			if (ae_fireteam_team_of(slot) != -1)
				error(_error_silent, "dev fireteam: fireteam %ld team %d leader %ld role %d order %d%s", slot, (int)ae_fireteam_team_of(slot),
					(long)ae_fireteam_leader_player(slot), ae_fireteam_role_taken(slot), (int)ae_fireteam_order_of(slot),
					ae_fireteam_order_explicit_of(slot) ? " (the leader's)" : "");
		answer(source, line, TRUE, "fireteam status in the log");
		return;
	}
	if (fireteam == NONE)
	{
		answer(source, line, FALSE, "the host player leads no fireteam (a team game with the marks on, after the first spawn)");
		return;
	}
	if (!strcmp(cmd->word[0], "recruit"))
	{
		long brain = cmd->number - 1;
		int result = AE_RECRUIT_NONE;

		if (cmd->number > 0)
			result = ae_fireteam_recruit_brain(brain, fireteam, FALSE, FALSE);
		else
			/* (none named: the first bot of the team in another fireteam that can come) */
			for (brain = 0; brain < BOTS_BRAINS && result != AE_RECRUIT_OK; brain++)
				if (ae_fireteam_of_brain(brain) != NONE && ae_fireteam_of_brain(brain) != fireteam)
					result = ae_fireteam_recruit_brain(brain, fireteam, FALSE, FALSE);
		if (cmd->number <= 0 && result == AE_RECRUIT_OK)
			brain--;
		if (result == AE_RECRUIT_OK)
			answer(source, line, TRUE, "bot %ld is in fireteam %ld", brain + 1, fireteam);
		else
			answer(source, line, FALSE, "no bot could join (%d)", result);
		return;
	}
	if (player->unit_index != NONE && object_try_and_get(player->unit_index))
	{
		here = object_get(player->unit_index)->object.position;
		has_here = TRUE;
	}
	for (index = 0; index < (short)(sizeof(orders) / sizeof(orders[0])); index++)
		if (!strcmp(cmd->word[0], orders[index].name))
		{
			boolean needs_point = orders[index].kind >= _ae_order_attack;

			if (needs_point && !has_here)
			{
				answer(source, line, FALSE, "the host player is dead (no spot for the order)");
				return;
			}
			ae_fireteam_order((int)fireteam, orders[index].kind, needs_point ? &here : NULL, NONE);
			answer(source, line, TRUE, "fireteam %ld: %s", fireteam, orders[index].name);
			return;
		}
}

/* whether the command changes the game, so waits for the tick */
static boolean in_tick(int id)
{
	switch (id)
	{
	case AE_DEV_CMD_GIVE:
	case AE_DEV_CMD_SPAWN:
	case AE_DEV_CMD_POWERUP:
	case AE_DEV_CMD_RESPAWN:
	case AE_DEV_CMD_KILL:
	case AE_DEV_CMD_END:
	case AE_DEV_CMD_SCORE:
	case AE_DEV_CMD_GOTO:
	case AE_DEV_CMD_TIME:
	case AE_DEV_CMD_RESET:
	case AE_DEV_CMD_OPTION:
	case AE_DEV_CMD_FIRETEAM:
	case AE_DEV_CMD_ROUND:
	case AE_DEV_CMD_ZONE:
	case AE_DEV_CMD_DOWN:
	case AE_DEV_CMD_REVIVE:
	case AE_DEV_CMD_GOD:
	case AE_DEV_CMD_SEAT:
	case AE_DEV_CMD_DUMMY:
		return TRUE;
	default:
		return FALSE;
	}
}

static void run(struct ae_dev_cmd const *cmd, char const *line, int source)
{
	if (in_tick(cmd->id) && !game_engine_running())
	{
		answer(source, line, FALSE, "no game running");
		return;
	}
	/* (the postgame: the game is over, nothing in it changes; on a host the postgame's own path updates the engine
	outside the tick) */
	if (in_tick(cmd->id) && game_engine_game_over())
	{
		answer(source, line, FALSE, "the game is over");
		return;
	}
	switch (cmd->id)
	{
	case AE_DEV_CMD_GIVE:
		command_give(cmd, line, source);
		break;
	case AE_DEV_CMD_SPAWN:
		command_spawn(cmd, line, source, FALSE);
		break;
	case AE_DEV_CMD_POWERUP:
		command_spawn(cmd, line, source, TRUE);
		break;
	case AE_DEV_CMD_RESPAWN:
		command_kill(cmd, line, source, TRUE);
		break;
	case AE_DEV_CMD_KILL:
		command_kill(cmd, line, source, FALSE);
		break;
	case AE_DEV_CMD_END:
		command_end(cmd, line, source);
		break;
	case AE_DEV_CMD_SCORE:
		command_score(cmd, line, source);
		break;
	case AE_DEV_CMD_GOTO:
		command_goto(cmd, line, source);
		break;
	case AE_DEV_CMD_BOTS:
		command_bots(cmd, line, source);
		break;
	case AE_DEV_CMD_OPTION:
		command_option(cmd, line, source);
		break;
	case AE_DEV_CMD_AGENT:
		if (!strcmp(cmd->word[0], "check"))
		{
			ae_test_run_agent_check((int)cmd->number, cmd->word[2], cmd->rest, system_milliseconds());
			answer(source, line, TRUE, "check %ld: %s", cmd->number, cmd->word[2]);
		}
		else if (!strcmp(cmd->word[0], "say"))
		{
			ae_test_run_agent_say(cmd->rest, system_milliseconds());
			answer(source, line, TRUE, "said");
		}
		else
		{
			ae_test_run_agent_clear();
			answer(source, line, TRUE, "cleared");
		}
		break;
	case AE_DEV_CMD_FIRETEAM:
		command_fireteam(cmd, line, source);
		break;
	case AE_DEV_CMD_CONFIG:
		setting_keep(cmd->word[0]);
		if (config_set(cmd->word[0], cmd->rest))
			answer(source, line, TRUE, "%s = %s for this run", cmd->word[0], cmd->rest);
		else
			answer(source, line, FALSE, "no setting %s, or %s does not fit it", cmd->word[0], cmd->rest);
		break;
	case AE_DEV_CMD_MARK:
		answer(source, line, TRUE, "marked");
		break;
	case AE_DEV_CMD_SHOT:
		capture_request_screenshot();
		answer(source, line, TRUE, "screenshot next frame");
		break;
	case AE_DEV_CMD_LIST:
		command_list(cmd, line, source);
		break;
	case AE_DEV_CMD_HELP:
		command_help(cmd, line, source);
		break;
	case AE_DEV_CMD_TIME:
		command_time(cmd, line, source);
		break;
	case AE_DEV_CMD_GAMETYPE:
		answer(source, line, FALSE, "gametype is not built yet (a TEST RUN list's gametype line sets it)");
		break;
	case AE_DEV_CMD_VIEWMODEL:
		command_viewmodel(cmd, line, source);
		break;
	case AE_DEV_CMD_TTK:
		command_ttk(cmd, line, source);
		break;
	case AE_DEV_CMD_DUMMY:
		command_dummy(cmd, line, source);
		break;
	case AE_DEV_CMD_HUD:
		command_hud(cmd, line, source);
		break;
	case AE_DEV_CMD_VIEW:
		answer(source, line, FALSE, "the dev views are not built yet");
		break;
	case AE_DEV_CMD_RESET:
		command_reset(line, source);
		break;
	case AE_DEV_CMD_ROUND:
	case AE_DEV_CMD_ZONE:
	case AE_DEV_CMD_DOWN:
	case AE_DEV_CMD_REVIVE:
		command_round(cmd, line, source);
		break;
	case AE_DEV_CMD_GOD:
		command_god(cmd, line, source);
		break;
	case AE_DEV_CMD_SEAT:
		command_seat(cmd, line, source);
		break;
	default:
		answer(source, line, FALSE, "unknown command");
		break;
	}
}

/* ---------- the way in */

int ae_dev_cmd_line(const char *line, int source)
{
	struct ae_dev_cmd cmd;
	int gate;

	if (!ae_dev_cmd_parse(line, ae_dev_mode_on(), &cmd))
		return FALSE;
	if (cmd.error[0])
	{
		answer(source, line, FALSE, "%s", cmd.error);
		return TRUE;
	}
	gate = ae_dev_cmd_gate(cmd.id, ae_dev_mode_setting(), network_game_distributed_client());
	if (gate != AE_DEV_CMD_ALLOWED)
	{
		answer(source, line, FALSE, "%s", ae_dev_cmd_gate_text(gate));
		return TRUE;
	}
	if (!in_tick(cmd.id))
	{
		run(&cmd, line, source);
		return TRUE;
	}
	/* (no game's tick will come to run it: the menus, a screen test. Said now, not in some later game) */
	if (!game_engine_running())
	{
		answer(source, line, FALSE, "no game running");
		return TRUE;
	}
	if (queued == QUEUE)
	{
		answer(source, line, FALSE, "too many commands waiting for the game's tick");
		return TRUE;
	}
	snprintf(queue[queued].line, sizeof queue[queued].line, "%s", line);
	queue[queued].source = source;
	queued++;
	/* (taken: how it went is said when it runs) */
	answered_ok = TRUE;
	return TRUE;
}

int ae_dev_cmd_hs_line(const char *line, int *succeeded)
{
	answered_ok = TRUE;
	if (!ae_dev_cmd_line(line, evaluating_source))
		return FALSE;
	*succeeded = answered_ok;
	return TRUE;
}

int ae_dev_cmd_evaluate_from(const char *expression, int source)
{
	int previous = evaluating_source;
	boolean result;

	evaluating_source = source >= 0 && source < AE_DEV_CMD_SOURCES ? source : AE_DEV_CMD_FROM_CONSOLE;
	result = hs_compile_and_evaluate(expression);
	evaluating_source = previous;
	return result;
}

void ae_dev_cmd_new_game(void)
{
	god_player = NONE;
	dummy_settings_back();
	ae_dev_dummy_forget();
	ae_dev_ttk_new_game();
	ae_dev_game_new(&game_state);
	new_game_set_up = TRUE;
	/* (DEV MODE turned off out of a game: the settings it changed go back before this one) */
	if (settings_saved.count && !ae_dev_mode_on())
		settings_back();
}

void ae_dev_cmd_tick(void)
{
	int index, count = queued;

	if (!ae_dev_mode_on())
	{
		/* (off: nothing of this game is kept for a later one, whose start would go unseen. Cleared once: the ticks
		after cost a test) */
		if (!state_kept)
			return;
		queued = 0;
		announced = FALSE;
		time_extra = 0;
		made_count = 0;
		reset_wait = 0;
		ae_dev_game_reset_end(&game_state);
		ae_dev_game_options_drop(&game_state);
		/* (the settings bots and config changed go back now: kept, a later reset would put back stale values) */
		dummy_settings_back();
		settings_back();
		ae_dev_dummy_forget();
		ae_dev_ttk_set(FALSE);
		memset(respawn_wait, 0, sizeof(respawn_wait));
		quiet_deaths_clear();
		state_kept = FALSE;
		return;
	}
	state_kept = TRUE;
	/* (a new game (its set-up seen, or its clock started again): its own time and rules, nothing made yet, said again;
	a line still waiting, and a reset not finished, were the last game's) */
	if (new_game_set_up || game_time_get() < announced_time)
	{
		new_game_set_up = FALSE;
		announced = FALSE;
		time_extra = 0;
		made_count = 0;
		if (reset_wait > 0)
			answer(reset_source, reset_line, FALSE, "not finished: a new game started");
		reset_wait = 0;
		ae_dev_game_reset_end(&game_state);
		ae_dev_game_options_drop(&game_state);
		memset(respawn_wait, 0, sizeof(respawn_wait));
		quiet_deaths_clear();
		for (index = 0; index < queued; index++)
			answer(queue[index].source, queue[index].line, FALSE, "not run: a new game started");
		queued = 0;
		count = 0;
	}
	announced_time = game_time_get();
	if (!announced && hosting() && game_engine_running())
	{
		ae_chat_host_notice("DEV MODE is on: the host can give weapons, make vehicles and change the game");
		announced = TRUE;
	}
	ae_dev_events_tick();
	respawn_update();
	/* (the game is over: its dummies are forgotten, the bots settings they changed put back) */
	if (game_engine_game_over() && ae_dev_dummy_count())
	{
		dummy_settings_back();
		ae_dev_dummy_forget();
	}
	ae_dev_dummy_tick();
	if (reset_wait > 0 && --reset_wait == 0)
	{
		/* (a dead carrier's flag or ball not dropped yet: a little longer, so it goes home with the rest) */
		if (!game_engine_game_over() && reset_waited < RESET_WAIT_MORE && objective_held_by_dead())
		{
			reset_wait = 1;
			reset_waited++;
		}
		else if (game_engine_game_over())
			answer(reset_source, reset_line, FALSE, "the game ended before the reset was done");
		/* (asked in another game: never finished in this one) */
		else if (!ae_dev_game_reset_current(&game_state))
			answer(reset_source, reset_line, FALSE, "not finished: a new game started");
		else
			reset_finish();
	}
	queued = 0;
	for (index = 0; index < count; index++)
	{
		struct ae_dev_cmd cmd;

		/* (checked when it came; parsed again here, the state is the tick's) */
		if (ae_dev_cmd_parse(queue[index].line, TRUE, &cmd) && !cmd.error[0])
			run(&cmd, queue[index].line, queue[index].source);
	}
}

/* names compared as the console sorts them: any case */
static int before(char const *a, char const *b)
{
	for (; *a && tolower((unsigned char)*a) == tolower((unsigned char)*b); a++, b++)
		;
	return tolower((unsigned char)*a) < tolower((unsigned char)*b);
}

int ae_dev_cmd_settled(void)
{
	short slot;

	if (queued || reset_wait > 0)
		return FALSE;
	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		if (respawn_wait[slot] > 0)
			return FALSE;
	}
	return living_unit(me()) != NONE;
}

int ae_dev_cmd_waiting(void)
{
	return queued != 0;
}

int ae_dev_cmd_completions(const char *prefix, const char **items, int count, int maximum)
{
	const char *names[AE_DEV_CMD_COUNT];
	int found, i;

	if (!ae_dev_mode_on() || !prefix || count < 0)
		return count;
	found = ae_dev_cmd_complete(prefix, names, AE_DEV_CMD_COUNT);
	for (i = 0; i < found && count < maximum; i++)
	{
		int at = count++;

		/* (in place: the list stays sorted) */
		while (at > 0 && before(names[i], items[at - 1]))
		{
			items[at] = items[at - 1];
			at--;
		}
		items[at] = names[i];
	}
	return count;
}
