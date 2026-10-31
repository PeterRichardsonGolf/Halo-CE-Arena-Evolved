/*
AE_BOMB_ASSAULT.C

Arena Evolved's Bomb rules: FLAG SPAWNS, DEFUSE and FUSE (ae_bomb_assault.h).
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cseries.h"
#include "real_math.h"
#include "game/game_engine.h"
#include "game/game.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"
#include "sound/game_sound.h"
#include "units/units.h"
#include "items/items.h"
#include "physics/collisions.h"
#include "text/text_group.h"

#include "ae_bomb_assault.h"
#include "ae_hud_style.h"

/* port_config.c */
int config_boolean(char const *name);
double config_real(char const *name);
long config_integer(char const *name);
void platform_log(const char *format, ...);
boolean network_game_distributed_client(void);

#ifdef HALO_GAME_BROWSER

/* (the defuse under way: its player, the tick it last counted, and its ticks: one player's alone, a teammate who takes
over starts again) */
static struct
{
	long player_index;
	long last_tick;
	long ticks;
} ae_bomb_defuse = { NONE, NONE, 0 };

/* AE's rules: a Bomb game that is not melee only, not on a Grifball court */
static boolean ae_bomb_assault_rules(void)
{
	return game_engine_bomb_weapon() != NONE && !game_engine_bomb_melee_only() && !game_engine_bomb_scripts_off();
}

long ae_bomb_assault_attacking_team(void)
{
	struct game_variant const *variant;
	long turn;

	if (!ae_bomb_assault_rules() || !config_boolean("game.bomb_flag_spawns"))
		return NONE;
	variant = game_engine_get_variant();
	/* (a turn a round; a score that starts no new round is a turn too; SEARCH AND DESTROY: a turn every
	game.bomb_snd_round_switch rounds) */
	turn = game_engine_bomb_round() - 1;
	if (ae_bomb_snd_shown())
	{
		long rounds = config_integer("game.bomb_snd_round_switch");

		turn = rounds > 1 ? turn / rounds : turn;
	}
	else if (!TEST_FLAG(variant->game_engine_variant.bomb.flags, _bomb_new_round_on_score_bit))
		turn += game_engine_bomb_team_score(0) + game_engine_bomb_team_score(1);
	return turn < 0 ? 0 : turn % 2;
}

boolean ae_bomb_assault_spawn(real_point3d *point)
{
	long team = ae_bomb_assault_attacking_team();

	if (team == NONE || !game_engine_bomb_goal(team, point))
		return FALSE;
	platform_log("ae bomb: the bomb spawns at %s's flag, %s attacks (round %ld)", team ? "blue" : "red",
		team ? "blue" : "red", game_engine_bomb_round());
	return TRUE;
}

boolean ae_bomb_assault_may_carry(long unit_index)
{
	long team = ae_bomb_assault_attacking_team();
	struct unit_datum *unit;

	if (team == NONE || unit_index == NONE)
		return TRUE;
	unit = unit_get(unit_index);
	if (unit->unit.player_index == NONE)
		return TRUE;
	/* (HALO_BOMB_BOTS_ONLY, the bot tests: the person who stands idle at the spawn does not hold the bomb) */
	if (player_get(unit->unit.player_index)->local_player_index != NONE && getenv("HALO_BOMB_BOTS_ONLY"))
		return FALSE;
	return player_get(unit->unit.player_index)->team_index == team;
}

double ae_bomb_assault_fuse_time(double chupa_seconds)
{
	if (!ae_bomb_assault_rules())
		return chupa_seconds;
	return config_real(ae_bomb_snd_on() ? "game.bomb_snd_fuse_time" : "game.bomb_fuse_time");
}

double ae_bomb_assault_fuse_maximum(double chupa_maximum)
{
	return ae_bomb_snd_on() ? 120.0 : chupa_maximum;
}

/* a defuse's ticks (this machine's setting: the host's decides) */
static long ae_bomb_defuse_needed(boolean snd)
{
	return (long)(config_real(snd ? "game.bomb_snd_defuse_time" : "game.bomb_defuse_time") * TICKS_PER_SECOND + 0.5);
}

boolean ae_bomb_assault_defused(long player_index)
{
	long now = game_time_get();
	long needed;

	if (!ae_bomb_assault_rules())
		return TRUE;
	needed = ae_bomb_defuse_needed(ae_bomb_snd_on());
	if (needed <= 0)
		return TRUE;
	if (ae_bomb_defuse.last_tick != now)
	{
		boolean going_on = ae_bomb_defuse.last_tick == now - 1 && ae_bomb_defuse.player_index == player_index;

		ae_bomb_defuse.ticks = going_on ? ae_bomb_defuse.ticks + 1 : 1;
		ae_bomb_defuse.last_tick = now;
		ae_bomb_defuse.player_index = player_index;
		if (ae_bomb_defuse.ticks == 1)
			platform_log("ae bomb: player %ld defusing", player_index & 0xFFFF);
	}
	if (ae_bomb_defuse.ticks < needed)
		return FALSE;
	ae_bomb_defuse.last_tick = NONE;
	ae_bomb_defuse.player_index = NONE;
	ae_bomb_defuse.ticks = 0;
	return TRUE;
}

boolean ae_bomb_assault_untouched_resets(void)
{
	return !ae_bomb_snd_on();
}

/* the map's lowest player spawn (0 with none) and the fall that kills (globals' falling damage: the maximum falling
distance at 0x28, as ae_bots_falls.inc reads it; 15 with none), found once a map */
static struct scenario const *ae_bomb_snd_reach_read_for = NULL;

static void ae_bomb_snd_reach(real *lowest_spawn_z, real *lethal)
{
	static real lowest = 0.0f, fall = 15.0f;
	struct scenario *scenario = global_scenario_get();

	/* (read again each round too: the next map's scenario can load at the same address) */
	if (scenario != ae_bomb_snd_reach_read_for)
	{
		struct game_globals *globals = scenario_get_game_globals();
		long index;

		ae_bomb_snd_reach_read_for = scenario;
		lowest = 0.0f;
		fall = 15.0f;
		for (index = 0; index < scenario->players.count; index++)
		{
			struct player_starting_location const *location =
				TAG_BLOCK_GET_ELEMENT(&scenario->players, index, struct player_starting_location);

			if (index == 0 || location->position.z < lowest)
				lowest = location->position.z;
		}
		if (globals && globals->falling_damage.count > 0)
		{
			real maximum = *(real const *)((byte const *)TAG_BLOCK_GET_ELEMENT(&globals->falling_damage, 0, byte) + 0x28);

			if (maximum > 0.0f)
				fall = maximum;
		}
		platform_log("ae snd: a loose bomb is out of reach below z %.2f (lowest spawn %.2f, fall %.2f)", lowest - fall,
			lowest, fall);
	}
	*lowest_spawn_z = lowest;
	*lethal = fall;
}

boolean ae_bomb_assault_out_of_reach(long weapon_index)
{
	struct object_datum *object;
	real lowest, lethal, water_depth;

	if (!ae_bomb_snd_on() || weapon_index == NONE || network_game_distributed_client())
		return FALSE;
	object = (struct object_datum *)object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon);
	/* (loose on the map: not in a player's hands, nor in an inventory, which is not connected to the map) */
	if (!object || object->object.parent_object_index != NONE ||
		!TEST_FLAG(object->object.flags, _object_connected_to_map_bit))
	{
		return FALSE;
	}
	ae_bomb_snd_reach(&lowest, &lethal);
	if (ae_bomb_snd_out_of_reach(object->object.position.z, lowest, lethal))
		return TRUE;
	water_depth = scenario_location_water_depth(&object->object.location, &object->object.position);
	if (!ae_bomb_snd_in_deep_water(water_depth))
		return FALSE;
	platform_log("ae snd: the loose bomb lies %.2f deep in water: out of reach", water_depth);
	return TRUE;
}

boolean ae_bomb_assault_defuse_held_by_other(long player_index, union real_point3d const *bomb_position, real radius)
{
	struct player_datum *defuser;
	struct object_datum *unit;

	if (!ae_bomb_assault_rules() || ae_bomb_defuse.player_index == NONE || ae_bomb_defuse.player_index == player_index ||
		ae_bomb_defuse.last_tick < game_time_get() - 1)
	{
		return FALSE;
	}
	/* (the defuser still alive at the bomb keeps it: the others wait) */
	defuser = player_try_and_get(ae_bomb_defuse.player_index);
	if (!defuser || defuser->unit_index == NONE)
		return FALSE;
	unit = object_get(defuser->unit_index);
	return !TEST_FLAG(unit->object.damage_flags, _object_dead_bit) &&
		distance_squared3d(bomb_position, &unit->object.position) < radius * radius;
}

/* ---------- the countdown each round */

/* a round's countdown's ticks with AE's rules and the PRE-GAME COUNTDOWN: SEARCH AND DESTROY's
(game.bomb_snd_countdown: the strategy time, 6 s), else the PRE-GAME COUNTDOWN's; 0 without one (this machine's
setting: the host's should agree, or a client's moves are held back or corrected at the end) */
static long ae_bomb_round_countdown_length(void)
{
	/* (once a tick: the game asks for its countdown many times a tick, and the settings are looked up by name) */
	static long asked_at = NONE, length = 0;
	long now = game_time_get();

	if (now == asked_at)
		return length;
	asked_at = now;
	if (!ae_bomb_assault_rules() || !game_engine_pregame_countdown())
		length = 0;
	else if (!ae_bomb_snd_shown())
		length = PREGAME_COUNTDOWN_TICKS;
	else
	{
		length = (long)(config_real("game.bomb_snd_countdown") * TICKS_PER_SECOND + 0.5);
		length = length < 0 ? 0 : length > 30 * TICKS_PER_SECOND ? 30 * TICKS_PER_SECOND : length;
	}
	return length;
}

/* the current round's countdown's first tick, NONE: none (the first round's is the game's own PRE-GAME COUNTDOWN,
unless SEARCH AND DESTROY's is longer; Grifball keeps Chupa's rounds) */
static long ae_bomb_round_countdown_start(void)
{
	struct ae_bomb_state state;

	if (ae_bomb_round_countdown_length() <= 0)
		return NONE;
	ae_bomb_state_get(&state);
	if (!state.running)
		return NONE;
	if (state.round <= 1)
		return ae_bomb_round_countdown_length() > PREGAME_COUNTDOWN_TICKS ? 0 : NONE;
	return state.round_start_time > 0 ? state.round_start_time : NONE;
}

long ae_bomb_round_countdown_ticks_left(void)
{
	long start = ae_bomb_round_countdown_start();
	long length = ae_bomb_round_countdown_length();
	long now = game_time_get();

	if (start == NONE || now < start || now - start >= length)
		return 0;
	return length - (now - start);
}

boolean ae_bomb_round_countdown_covers(long time)
{
	long start = ae_bomb_round_countdown_start();

	return start != NONE && time >= start && time - start < ae_bomb_round_countdown_length();
}

boolean ae_bomb_round_line(long *round, long *attackers)
{
	struct ae_bomb_state state;

	if (!ae_bomb_assault_rules())
		return FALSE;
	ae_bomb_state_get(&state);
	if (!state.running || state.round <= 0)
		return FALSE;
	*round = state.round;
	*attackers = ae_bomb_assault_attacking_team();
	return TRUE;
}

/* ---------- SEARCH AND DESTROY */

/* (the round's start, game time: elimination waits a moment for the dead to respawn) */
static long ae_bomb_snd_round_start;

/* the dropped bomb pinned where it fell ("nothing moves it afterwards"; plan rule 4): the host's, the
bomb and its spot, lying flat its long side the carrier's way, until someone carries it (owner 10-28 test: the
engine moved it again the tick after the drop, and the ball's own physics rolled the bomb pack's long box into the
floor) */
static long ae_bomb_snd_pin_weapon = NONE;
static real_point3d ae_bomb_snd_pin_point;
static real_vector3d ae_bomb_snd_pin_forward;

/* (the deaths "back mid-round" was said for, by player slot) */
#define AE_BOMB_SND_PLAYER_SLOTS 128
static long ae_bomb_snd_back_logged[AE_BOMB_SND_PLAYER_SLOTS];

/* a few seconds of a round before elimination counts (everyone spawned) */
#define AE_BOMB_SND_GRACE_TICKS (4 * TICKS_PER_SECOND)

boolean ae_bomb_snd_on(void)
{
	/* (the gametype's bit, AE SND's; config game.bomb_snd turns it on for any Bomb game, a test override. The host's
	decides: a client follows its bomb state, ae_bomb_snd_shown) */
	return ae_bomb_assault_rules() && config_boolean("game.bomb_flag_spawns") &&
		(ae_bomb_snd_flag(game_engine_get_variant()->game_engine_variant.bomb.flags) || config_boolean("game.bomb_snd"));
}

void ae_bomb_snd_round_started(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (!ae_bomb_snd_on())
		return;
	/* (a new round's bomb is the reset's, at its spawn: the last round's pin let go) */
	ae_bomb_snd_pin_weapon = NONE;
	ae_bomb_snd_reach_read_for = NULL;
	ae_bomb_snd_round_start = game_time_get();
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index == NONE)
			player->respawn_timer = MIN(player->respawn_timer, 1);
	}
	platform_log("ae snd: round %ld, %s attacks", game_engine_bomb_round(),
		ae_bomb_assault_attacking_team() ? "blue" : "red");
}

static void ae_bomb_snd_drop_update(void);
static void ae_bomb_snd_pin_update(void);

void ae_bomb_snd_update(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long attackers = ae_bomb_assault_attacking_team(), defenders;
	long alive[2] = { 0, 0 }, players[2] = { 0, 0 };
	long now = game_time_get();
	boolean planted;

	if (!ae_bomb_snd_on() || attackers == NONE || !game_engine_can_score() || game_engine_bomb_holds_spawns())
		return;
	ae_bomb_snd_drop_update();
	ae_bomb_snd_pin_update();
	if (ae_bomb_snd_round_start > now)
		ae_bomb_snd_round_start = 0;
	/* (the round's countdown first: its grace and its clock count after it) */
	if (now - ae_bomb_snd_round_start < ae_bomb_round_countdown_length() + AE_BOMB_SND_GRACE_TICKS)
		return;
	defenders = 1 - attackers;
	planted = game_engine_bomb_planted_goal() != NONE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit;

		if (!VALID_INDEX(player->team_index, 2))
			continue;
		players[player->team_index]++;
		unit = player->unit_index != NONE ? unit_get(player->unit_index) : NULL;
		if (unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		{
			long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);

			alive[player->team_index]++;
			/* (one life a round: alive again after a death this round is a fault, said once a death; owner 10-26
			saw dead enemies back mid-round) */
			if (player->death_time > ae_bomb_snd_round_start + ae_bomb_round_countdown_length() &&
				slot < AE_BOMB_SND_PLAYER_SLOTS &&
				ae_bomb_snd_back_logged[slot] != player->death_time)
			{
				ae_bomb_snd_back_logged[slot] = player->death_time;
				platform_log("ae snd: player %ld (team %d) back mid-round %ld: died at tick %ld, round from %ld, now %ld",
					slot, (int)player->team_index, game_engine_bomb_round(), player->death_time,
					ae_bomb_snd_round_start, now);
			}
		}
	}
	if (players[defenders] > 0 && alive[defenders] == 0)
	{
		platform_log("ae snd: every defender is dead");
		ae_bomb_snd_round_won(attackers);
	}
	else if (players[attackers] > 0 && alive[attackers] == 0 && !planted)
	{
		platform_log("ae snd: every attacker is dead, the bomb not planted");
		ae_bomb_snd_round_won(defenders);
	}
	else if (!planted && ae_bomb_snd_round_ticks_left() == 0)
	{
		platform_log("ae snd: time, the bomb not planted");
		ae_bomb_snd_round_won(defenders);
	}
}

boolean ae_bomb_assault_play_ball(void)
{
	return !ae_bomb_assault_rules();
}

long ae_bomb_assault_goal_countdown(long chupa_ticks)
{
	return ae_bomb_assault_rules() ? 0 : chupa_ticks;
}

long ae_bomb_assault_plant_ticks(long chupa_ticks)
{
	double seconds;

	if (!ae_bomb_snd_shown())
		return chupa_ticks;
	seconds = config_real("game.bomb_snd_plant_time");
	return seconds > 0.0 ? (long)(MIN(seconds, 30.0) * TICKS_PER_SECOND + 0.5) : chupa_ticks;
}

void ae_bomb_snd_score_to_win(long *score_to_win)
{
	long rounds;

	if (!ae_bomb_snd_shown())
		return;
	rounds = config_integer("game.bomb_snd_rounds_to_win");
	if (rounds > 0)
		*score_to_win = rounds;
}

/* ---------- the HUD's, and the host's two spare bytes */

boolean ae_bomb_assault_hud_replaced(void)
{
	return ae_hud_style_is_ae();
}

/* (a client's: the host's spare bytes as last read, and when its fuse and defuse last changed) */
static struct
{
	long fuse_halves, fuse_time;
	long defuse, defuse_time;
	boolean snd;
} ae_bomb_client = { 0, NONE, 0, NONE, FALSE };

/* half a second's ticks (the fuse's unit in the spare byte) */
#define AE_BOMB_HALF_TICKS (TICKS_PER_SECOND / 2)
/* a client's defuse runs on from the host's last word this long (the host's state comes every 6-12 ticks) */
#define AE_BOMB_DEFUSE_EXTRAPOLATE_TICKS 15

long ae_bomb_assault_fuse_ticks(void)
{
	struct ae_bomb_state state;
	long left;

	ae_bomb_state_get(&state);
	if (!state.running)
		return 0;
	if (!network_game_distributed_client())
		return state.plant_ticks > 0 ? state.plant_ticks : 0;
	if (ae_bomb_client.fuse_halves <= 0 || ae_bomb_client.fuse_time == NONE)
		return 0;
	left = ae_bomb_client.fuse_halves * AE_BOMB_HALF_TICKS - (game_time_get() - ae_bomb_client.fuse_time);
	return left > 1 ? left : 1;
}

real ae_bomb_assault_defuse_progress(void)
{
	long now = game_time_get();
	real progress;

	if (!network_game_distributed_client())
	{
		long needed = ae_bomb_defuse_needed(ae_bomb_snd_on());

		if (!ae_bomb_assault_rules() || needed <= 0 || ae_bomb_defuse.last_tick == NONE ||
			now - ae_bomb_defuse.last_tick > 1 || ae_bomb_defuse.ticks <= 0)
		{
			return 0.0f;
		}
		progress = (real)ae_bomb_defuse.ticks / (real)needed;
		return progress > 1.0f ? 1.0f : progress;
	}
	if (ae_bomb_client.defuse <= 0 || ae_bomb_client.defuse_time == NONE)
		return 0.0f;
	progress = (real)ae_bomb_client.defuse / 127.0f;
	/* (on from the host's last word at this machine's defuse rate, a moment at most) */
	{
		long since = now - ae_bomb_client.defuse_time, needed = ae_bomb_defuse_needed(ae_bomb_client.snd);

		if (since > AE_BOMB_DEFUSE_EXTRAPOLATE_TICKS)
			since = AE_BOMB_DEFUSE_EXTRAPOLATE_TICKS;
		if (since > 0 && needed > 0)
			progress += (real)since / (real)needed;
	}
	return progress > 1.0f ? 1.0f : progress;
}

boolean ae_bomb_snd_shown(void)
{
	return network_game_distributed_client() ? ae_bomb_client.snd : ae_bomb_snd_on();
}

long ae_bomb_snd_round_ticks_left(void)
{
	struct ae_bomb_state state;
	long start, left;

	if (!ae_bomb_snd_shown())
		return NONE;
	ae_bomb_state_get(&state);
	/* (the host's own round start; a client's from the host's state) */
	start = network_game_distributed_client() ? state.round_start_time : ae_bomb_snd_round_start;
	if (start > game_time_get())
		start = 0;
	left = (long)(config_real("game.bomb_snd_round_time") * TICKS_PER_SECOND) -
		(game_time_get() - start - ae_bomb_round_countdown_length());
	if (left > (long)(config_real("game.bomb_snd_round_time") * TICKS_PER_SECOND))
		left = (long)(config_real("game.bomb_snd_round_time") * TICKS_PER_SECOND);
	return left > 0 ? left : 0;
}

void ae_bomb_assault_write_state(unsigned char spare[2])
{
	long fuse = ae_bomb_assault_fuse_ticks();
	long defuse = (long)(ae_bomb_assault_defuse_progress() * 127.0f + 0.5f);

	if (network_game_distributed_client())
		return;
	fuse = fuse > 0 ? (fuse + AE_BOMB_HALF_TICKS - 1) / AE_BOMB_HALF_TICKS : 0;
	spare[0] = (unsigned char)(fuse > 255 ? 255 : fuse);
	if (ae_bomb_assault_defuse_progress() > 0.0f && defuse < 1)
		defuse = 1;
	spare[1] = (unsigned char)((ae_bomb_snd_on() ? 0x80 : 0) | (defuse > 127 ? 127 : defuse));
}

void ae_bomb_assault_read_state(unsigned char const spare[2])
{
	long now = game_time_get();
	long defuse = spare[1] & 0x7F;

	if (spare[0] != ae_bomb_client.fuse_halves || ae_bomb_client.fuse_time == NONE || ae_bomb_client.fuse_time > now)
	{
		ae_bomb_client.fuse_halves = spare[0];
		ae_bomb_client.fuse_time = now;
	}
	if (defuse != ae_bomb_client.defuse || ae_bomb_client.defuse_time == NONE || ae_bomb_client.defuse_time > now)
	{
		ae_bomb_client.defuse = defuse;
		ae_bomb_client.defuse_time = now;
	}
	ae_bomb_client.snd = (boolean)((spare[1] & 0x80) != 0);
}


boolean ae_bomb_snd_carried_bomb(long weapon_index)
{
	/* (the cheap test first: weapons.c asks for each of a unit's weapons each tick) */
	return weapon_index != NONE && weapon_index == game_engine_bomb_weapon() && ae_bomb_snd_shown();
}

/* BOMB DROP (ae_bomb_snd_rules.h): the carrier's hold with nothing in reach, ticks, and who dropped it by hand (no
pickup by them until they let go of the button) */
static long ae_bomb_snd_drop_held;
static long ae_bomb_snd_dropper = NONE;
static boolean ae_bomb_snd_carrier_was_holding;

/* each player's action button this tick, from their action (ae_bomb_snd_note_action) */
static boolean ae_bomb_snd_action_buttons[AE_BOMB_SND_PLAYER_SLOTS];

void ae_bomb_snd_note_action(long player_index, boolean held)
{
	long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);

	if (player_index != NONE && slot >= 0 && slot < AE_BOMB_SND_PLAYER_SLOTS)
		ae_bomb_snd_action_buttons[slot] = held;
}

static boolean ae_bomb_snd_action_held(long player_index)
{
	long slot = player_index != NONE ? DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) : NONE;

	return slot >= 0 && slot < AE_BOMB_SND_PLAYER_SLOTS && ae_bomb_snd_action_buttons[slot];
}

/* BOMB DROP: the gametype's row, or the test override game.bomb_snd_drop */
static boolean ae_bomb_snd_drop_on(void)
{
	return ae_bomb_snd_drop_flag(game_engine_get_variant()->game_engine_variant.bomb.flags) ||
		config_boolean("game.bomb_snd_drop");
}

boolean ae_bomb_snd_pickup_blocked(long player_index)
{
	return player_index != NONE && player_index == ae_bomb_snd_dropper;
}

/* the host's, every tick of a SEARCH AND DESTROY round (ae_bomb_snd_update) */
static void ae_bomb_snd_drop_update(void)
{
	struct ae_bomb_state state;
	struct player_datum *player;
	boolean holding;
	int drop;

	if (ae_bomb_snd_dropper != NONE && !ae_bomb_snd_action_held(ae_bomb_snd_dropper))
		ae_bomb_snd_dropper = NONE;
	ae_bomb_state_get(&state);
	if (state.carrier == NONE || state.weapon_index == NONE)
	{
		ae_bomb_snd_drop_held = 0;
		ae_bomb_snd_carrier_was_holding = FALSE;
		return;
	}
	/* (a carrier who left the game stays the engine's carrier until its own update, which a gone player no longer
	gets: nothing to read) */
	player = player_try_and_get(state.carrier);
	if (!player || player->unit_index == NONE)
	{
		ae_bomb_snd_drop_held = 0;
		ae_bomb_snd_carrier_was_holding = FALSE;
		return;
	}
	holding = ae_bomb_snd_action_held(state.carrier);
	/* (a press's start said once, on or off, as the host sees it: the drop's evidence for a TEST RUN) */
	if (holding && !ae_bomb_snd_carrier_was_holding)
	{
		platform_log("ae snd: the carrier holds the action button (BOMB DROP %s, %s)",
			ae_bomb_snd_drop_on() ? "on" : "off",
			player->action_result == _player_action_result_reload ? "nothing in reach" : "something in reach");
	}
	ae_bomb_snd_carrier_was_holding = holding;
	if (!ae_bomb_snd_drop_on())
	{
		ae_bomb_snd_drop_held = 0;
		return;
	}
	ae_bomb_snd_drop_held = ae_bomb_snd_drop_hold_next((int)ae_bomb_snd_drop_held, holding,
		player->action_result == _player_action_result_reload, &drop);
	if (drop && unit_drop_weapon(player->unit_index, state.weapon_index))
	{
		ae_bomb_snd_drop_held = 0;
		ae_bomb_snd_dropper = state.carrier;
		platform_log("ae snd: player %ld drops the bomb by hand (BOMB DROP)",
			(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(state.carrier));
	}
}

short ae_bomb_snd_item_name(long object_index, short name_index)
{
	return ae_bomb_snd_carried_bomb(object_index) ? AE_BOMB_SND_ITEM_NAME : name_index;
}

wchar_t const *ae_bomb_snd_item_text(long string_list_index, short string_index)
{
	if (string_index == AE_BOMB_SND_ITEM_NAME)
		return L"the bomb";
	return unicode_string_list_get_string(string_list_index, string_index);
}

boolean ae_bomb_snd_unit_carries(long unit_index)
{
	struct unit_datum *unit;
	short slot;

	if (unit_index == NONE || game_engine_bomb_weapon() == NONE ||
		!object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
	{
		return FALSE;
	}
	unit = unit_get(unit_index);
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		if (ae_bomb_snd_carried_bomb(unit->unit.weapon_object_indices[slot]))
			return TRUE;
	}
	return FALSE;
}

/* how far under the carrier's middle a floor is looked for, and how high over it the bomb is put down */
#define AE_BOMB_DROP_TEST_DEPTH 12.0f
#define AE_BOMB_DROP_REST_HEIGHT 0.05f


void ae_bomb_snd_drop_straight_down(long item_index, long unit_index)
{
	struct object_datum *item, *unit;
	struct collision_result collision;
	real_point3d from;
	real_vector3d down = { 0.0f, 0.0f, -AE_BOMB_DROP_TEST_DEPTH };

	if (!ae_bomb_snd_carried_bomb(item_index) || unit_index == NONE ||
		!(unit = (struct object_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit)))
	{
		return;
	}
	item = (struct object_datum *)object_get(item_index);
	item->object.translational_velocity = *global_zero_vector3d;
	item->object.angular_velocity = *global_zero_vector3d;
	from = unit->object.bounding_sphere_center;
	/* (a floor faces up, at the test going down, as the bomb's spawn finds its ground) */
	if (collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) |
			FLAG(_collision_test_ignore_invisible_surfaces_bit) | FLAG(_collision_test_structure_bit),
			&from, &down, NONE, &collision))
	{
		real_point3d rest = collision.point;
		/* (lying flat, its long side the way the carrier faced: the bomb pack's model is a long box, which the
		ball's own tilt would push into the floor; its half height is the rest height) */
		real_vector3d forward = { unit->object.forward.i, unit->object.forward.j, 0.0f };
		real_vector3d up = { 0.0f, 0.0f, 1.0f };

		if (normalize3d(&forward) == 0.0f)
			forward = *global_forward3d;
		rest.z += AE_BOMB_DROP_REST_HEIGHT;
		object_set_position(item_index, &rest, &forward, &up);
		ae_bomb_snd_pin_weapon = item_index;
		ae_bomb_snd_pin_point = rest;
		ae_bomb_snd_pin_forward = forward;
		platform_log("ae snd: the bomb falls straight down to (%.2f %.2f %.2f)", rest.x, rest.y, rest.z);
	}
	else
	{
		object_set_position(item_index, &from, NULL, NULL);
		platform_log("ae snd: no floor under the dropped bomb at (%.2f %.2f %.2f): it falls from there", from.x, from.y,
			from.z);
	}
}

/* each host tick: the pinned bomb back on its spot, flat and still, if anything moved it; let go once it is carried,
gone or another object */
static void ae_bomb_snd_pin_update(void)
{
	struct object_datum *item;
	real_vector3d up = { 0.0f, 0.0f, 1.0f };
	real_vector3d offset;

	if (ae_bomb_snd_pin_weapon == NONE)
		return;
	if (ae_bomb_snd_pin_weapon != game_engine_bomb_weapon() ||
		!(item = (struct object_datum *)object_try_and_get_and_verify_type(ae_bomb_snd_pin_weapon, _object_mask_item)) ||
		item->object.parent_object_index != NONE)
	{
		ae_bomb_snd_pin_weapon = NONE;
		return;
	}
	vector_from_points3d(&ae_bomb_snd_pin_point, &item->object.position, &offset);
	/* (moved more than a world unit in a tick: put somewhere on purpose, a reset to its spawn; owner 10-28 test: the
	pin held the planted bomb at the site into the next round) */
	if (magnitude_squared3d(&offset) > 1.0f)
	{
		platform_log("ae snd: the bomb moved %.2f from its pinned spot: the pin let go", magnitude3d(&offset));
		ae_bomb_snd_pin_weapon = NONE;
		return;
	}
	item->object.translational_velocity = *global_zero_vector3d;
	item->object.angular_velocity = *global_zero_vector3d;
	/* (at rest, so the ball's physics stops pulling it down; and no longer ignoring its dropper, which items.c does
	only when its physics lands it, never here: owner 10-28 test, the dropper could not pick it up again. The AE rule
	still makes the dropper let go of the button first, ae_bomb_snd_pickup_blocked) */
	SET_FLAG(item->object.flags, _object_at_rest_bit, TRUE);
	item_get(ae_bomb_snd_pin_weapon)->item.ignore_object_index = NONE;
	if (magnitude_squared3d(&offset) < 0.0005f * 0.0005f && item->object.up.k > 0.9999f)
		return;
	object_set_position(ae_bomb_snd_pin_weapon, &ae_bomb_snd_pin_point, &ae_bomb_snd_pin_forward, &up);
}

/* SEARCH AND DESTROY's planted bomb beeps the whole fuse from where it lies (owner 2026-10-26: "i want a beep ... both
audio and visual feedback"; the strip's fuse clock is the visual): the map's own multiplayer countdown beep, every
1.2 s, every 0.6 s in its last 10 s, every 0.3 s in its last 3, faster as it runs out. Every machine's */
#define AE_BOMB_BEEP_SOUND 0x1A
/* (when the planted bomb last beeped on this machine: NONE, no fuse running) */
static long ae_bomb_snd_last_beep_time = NONE;

long ae_bomb_snd_beep_age(void)
{
	long now = game_time_get();

	return ae_bomb_snd_last_beep_time == NONE || now < ae_bomb_snd_last_beep_time ? NONE :
		now - ae_bomb_snd_last_beep_time;
}

/* the blast: 200 down to 20 damage over its radius (512 inches, ~4.3 wu); 100 or more kills. Owner 10-28 (TEST
RUN snd-blast-1028: "could use a bigger explosion visually and blast radius effect wise"; snd-blast2-1028, the ring
of fireballs: "should of killed both players"): 9 wu, the kill core 5 wu, as far as the fireballs reach */
#define AE_BOMB_SND_BLAST_RADIUS 9.0f
#define AE_BOMB_SND_BLAST_KILL_RADIUS (AE_BOMB_SND_BLAST_RADIUS * (200.0f - 100.0f) / (200.0f - 20.0f))

union real_point3d const *ae_bomb_snd_blast_point(union real_point3d const *goal)
{
	static union real_point3d at;
	struct ae_bomb_state state;

	if (!ae_bomb_snd_shown())
		return goal;
	ae_bomb_state_get(&state);
	if (state.weapon_index == NONE || !object_try_and_get_and_verify_type(state.weapon_index, _object_mask_weapon))
		return goal;
	object_get_origin(state.weapon_index, &at);
	platform_log("ae snd: the blast goes off at the bomb (%.2f %.2f %.2f), %.2f from the goal", (double)at.x,
		(double)at.y, (double)at.z, (double)sqrtf(distance_squared3d(&at, goal)));
	return &at;
}

real ae_bomb_snd_blast_radius(real chupa_radius)
{
	return ae_bomb_snd_shown() ? AE_BOMB_SND_BLAST_KILL_RADIUS : chupa_radius;
}

real ae_bomb_snd_blast_reach(void)
{
	return AE_BOMB_SND_BLAST_RADIUS;
}

void ae_bomb_snd_beeps(void)
{
	static long next_beep_time = NONE;
	struct ae_bomb_state state;
	struct game_globals *game_globals;
	struct game_globals_multiplayer_information *information;
	struct tag_reference *sound;
	long fuse = ae_bomb_snd_on() ? ae_bomb_assault_fuse_ticks() : 0;
	long now = game_time_get(), gap;

	if (fuse <= 0)
	{
		next_beep_time = NONE;
		ae_bomb_snd_last_beep_time = NONE;
		return;
	}
	if (next_beep_time != NONE && now < next_beep_time)
		return;
	gap = fuse <= 3 * TICKS_PER_SECOND ? TICKS_PER_SECOND * 3 / 10 :
		fuse <= 10 * TICKS_PER_SECOND ? TICKS_PER_SECOND * 6 / 10 : TICKS_PER_SECOND * 12 / 10;
	next_beep_time = now + gap;
	ae_bomb_snd_last_beep_time = now;
	game_globals = scenario_get_game_globals();
	if (!game_globals || game_globals->multiplayer_information.count <= 0)
		return;
	information = TAG_BLOCK_GET_ELEMENT(&game_globals->multiplayer_information, 0,
		struct game_globals_multiplayer_information);
	if (!information || information->sounds.count <= AE_BOMB_BEEP_SOUND)
		return;
	sound = TAG_BLOCK_GET_ELEMENT(&information->sounds, AE_BOMB_BEEP_SOUND, struct tag_reference);
	if (!sound || sound->index == NONE)
		return;
	ae_bomb_state_get(&state);
	/* (from the bomb, nearer louder; without its object, everywhere) */
	if (state.weapon_index != NONE && object_try_and_get_and_verify_type(state.weapon_index, _object_mask_weapon))
	{
		if (gap == TICKS_PER_SECOND * 12 / 10 && fuse > 40 * TICKS_PER_SECOND)
			platform_log("ae snd: the planted bomb beeps from where it lies");
		object_impulse_sound_new(state.weapon_index, sound->index, NONE, global_origin3d, global_forward3d, 1.0f);
	}
	else
	{
		if (fuse > 40 * TICKS_PER_SECOND)
			platform_log("ae snd: the planted bomb beeps everywhere (no bomb object)");
		unspatialized_impulse_sound_new(sound->index, 0.6f);
	}
}
#else

long ae_bomb_assault_attacking_team(void) { return NONE; }
boolean ae_bomb_assault_spawn(real_point3d *point) { (void)point; return FALSE; }
boolean ae_bomb_assault_may_carry(long unit_index) { (void)unit_index; return TRUE; }
double ae_bomb_assault_fuse_time(double chupa_seconds) { return chupa_seconds; }
boolean ae_bomb_assault_defused(long player_index) { (void)player_index; return TRUE; }
boolean ae_bomb_assault_untouched_resets(void) { return TRUE; }
boolean ae_bomb_assault_out_of_reach(long weapon_index) { (void)weapon_index; return FALSE; }
boolean ae_bomb_assault_defuse_held_by_other(long player_index, union real_point3d const *bomb_position, real radius) { (void)player_index; (void)bomb_position; (void)radius; return FALSE; }
boolean ae_bomb_assault_play_ball(void) { return TRUE; }
long ae_bomb_assault_plant_ticks(long chupa_ticks) { return chupa_ticks; }
long ae_bomb_assault_goal_countdown(long chupa_ticks) { return chupa_ticks; }
void ae_bomb_snd_score_to_win(long *score_to_win) { (void)score_to_win; }
boolean ae_bomb_snd_on(void) { return FALSE; }
void ae_bomb_snd_round_started(void) {}
void ae_bomb_snd_update(void) {}
void ae_bomb_snd_beeps(void) {}
long ae_bomb_snd_beep_age(void) { return NONE; }
union real_point3d const *ae_bomb_snd_blast_point(union real_point3d const *goal) { return goal; }
real ae_bomb_snd_blast_radius(real chupa_radius) { return chupa_radius; }
real ae_bomb_snd_blast_reach(void) { return 0.0f; }
double ae_bomb_assault_fuse_maximum(double chupa_maximum) { return chupa_maximum; }
boolean ae_bomb_assault_hud_replaced(void) { return FALSE; }
void ae_bomb_state_get(struct ae_bomb_state *state) { memset(state, 0, sizeof(*state)); state->weapon_index = state->carrier = state->round_ticks = state->plant_goal_team = NONE; }
long ae_bomb_round_countdown_ticks_left(void) { return 0; }
boolean ae_bomb_round_line(long *round, long *attackers) { (void)round; (void)attackers; return FALSE; }
boolean ae_bomb_round_countdown_covers(long time) { (void)time; return FALSE; }
long ae_bomb_assault_fuse_ticks(void) { return 0; }
real ae_bomb_assault_defuse_progress(void) { return 0.0f; }
boolean ae_bomb_snd_shown(void) { return FALSE; }
long ae_bomb_snd_round_ticks_left(void) { return NONE; }
void ae_bomb_assault_write_state(unsigned char spare[2]) { (void)spare; }
void ae_bomb_assault_read_state(unsigned char const spare[2]) { (void)spare; }
boolean ae_bomb_snd_carried_bomb(long weapon_index) { (void)weapon_index; return FALSE; }
boolean ae_bomb_snd_unit_carries(long unit_index) { (void)unit_index; return FALSE; }
void ae_bomb_snd_drop_straight_down(long item_index, long unit_index) { (void)item_index; (void)unit_index; }
boolean ae_bomb_snd_pickup_blocked(long player_index) { (void)player_index; return FALSE; }
void ae_bomb_snd_note_action(long player_index, boolean held) { (void)player_index; (void)held; }
short ae_bomb_snd_item_name(long object_index, short name_index) { (void)object_index; return name_index; }
wchar_t const *ae_bomb_snd_item_text(long string_list_index, short string_index)
{
	return unicode_string_list_get_string(string_list_index, string_index);
}

#endif
