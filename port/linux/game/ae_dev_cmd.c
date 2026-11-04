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
#include "units/vehicle_definitions.h"
#include "../src/capture.h"
#include "../src/port_config.h"

#include "ae_bomb_assault.h"
#include "ae_glue_setup_options.h"
#include "ae_test_run_agent.h"
unsigned long system_milliseconds(void);
#include "infection.h"
#include "ae_dev_cmd.h"
#include "ae_dev_cmd_rules.h"
#include "ae_dev_cmd_state.h"
#include "ae_dev_events.h"
#include "ae_dev_mode.h"
#include "bots.h"

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

	tag_iterator_new(&iterator, group);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		char const *name = tag_get_name(tag_index);

		if (!name || (group == WEAPON_DEFINITION_TAG && objective(name)))
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
			count += !objective(tag_get_name(tag_index)) && count < GIVE_ALL_MOST;
		tag_iterator_new(&iterator, WEAPON_DEFINITION_TAG);
		while ((tag_index = tag_iterator_next(&iterator)) != NONE && placed < GIVE_ALL_MOST)
		{
			real_point3d at = centre;
			real angle;

			if (objective(tag_get_name(tag_index)))
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
		if (respawn)
		{
			respawn_wait[DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index)] = RESPAWN_WAIT;
			respawn_player[DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index)] = player_index;
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
	game_engine_end_game();
	answer(source, line, TRUE, "game ended");
	tell_players("DEV: the host ended the game");
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
	return ae_dev_mode_on() ? time_extra : 0;
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
	case AE_DEV_CMD_VIEW:
		answer(source, line, FALSE, "the dev views are not built yet");
		break;
	case AE_DEV_CMD_RESET:
		command_reset(line, source);
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
		settings_back();
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
