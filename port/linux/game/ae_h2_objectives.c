/* The second-generation game's objective carrier rules (see the header) */
#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "units/units.h"
#include "memory/data.h"
#include "items/items.h"
#include "items/weapons.h"
#include "objects/objects.h"
#include "math/real_math.h"
#include "ae_bomb_assault.h"
#include "ae_variant_ext.h"
#include "ae_h2_objectives.h"
#include "interface/hud.h"
#include "ae_ctf_return.h"
#include "ae_ctf_return_rules.h"
#include "ae_dev_mode.h"
#include "networking/network_server_manager.h"
#include "../src/port_config.h"

struct network_game_server *global_network_game_server_get(void);

boolean player_has_ball(long player_index);
boolean game_engine_player_has_flag(long player_index);
boolean ae_bomb_snd_unit_carries(long unit_index);
boolean unit_seat_is_driver(long unit_index, short seat_index);
boolean network_game_distributed_client(void);

/* (the slow carrier's and the juggernaut's speeds, as the oddball engine's own: 75 %, 100 %, 125 %) */
float ae_h2_oddball_speed(long player_index)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	struct game_variant const *variant = game_engine_get_variant();

	if (!variant || !ext->juggernaut_movement || variant->game_engine_variant.oddball.oddball_ball_type != 2 ||
		!player_has_ball(player_index))
	{
		return 0.0f;
	}
	return ae_ext_movement_percent(ext->juggernaut_movement) / 100.0f;
}

int ae_h2_ball_waypoint_hidden(int held)
{
	int shows = ae_ext_waypoint_shows(ae_variant_ext_game()->oddball_flags & 0x0F, held, 0, 0);

	return shows == 0;
}

int ae_h2_seat_refused(long unit_index, long target_unit_index, short seat_index)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	struct game_variant const *variant = game_engine_get_variant();
	long player_index;

	if (!game_engine_running() || !variant || ae_variant_ext_is_default(ext) || unit_index == NONE)
		return 0;
	player_index = unit_get(unit_index)->unit.player_index;
	if (player_index == NONE)
		return 0;
	switch (variant->game_engine_index)
	{
	case game_engine_oddball:
		return (ext->oddball_flags & AE_EXT_ODDBALL_NO_VEHICLES) && player_has_ball(player_index);
	case game_engine_ctf:
		return (ext->ctf_flags & AE_EXT_CTF_NO_DRIVING) && game_engine_player_has_flag(player_index) &&
			unit_seat_is_driver(target_unit_index, seat_index);
	case game_engine_bomb:
		return (ext->bomb_flags & AE_EXT_BOMB_NO_DRIVING) && ae_bomb_snd_unit_carries(unit_index) &&
			unit_seat_is_driver(target_unit_index, seat_index);
	}
	return 0;
}

/* DEV MODE's debug.player_speed_percent (50 to 150; 0, the default, changes nothing): this machine's own choice, so only in a game
with no other machine in it (two machines would move a player at different speeds and fall out of step) */
static float dev_speed_scale(void)
{
	long percent;

	if (!ae_dev_mode_on() || game_connection() == _game_connection_network_client)
		return 1.0f;
	if (game_connection() == _game_connection_network_server)
	{
		struct network_game_server *server = global_network_game_server_get();

		if (server && network_game_server_port_remote_machine_count(server, FALSE) > 0)
			return 1.0f;
	}
	percent = config_integer("debug.player_speed_percent");
	if (percent < 50 || percent > 150 || percent == 100)
		return 1.0f;
	return (float)percent / 100.0f;
}

float ae_h2_carrier_speed(long player_index)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	struct game_variant const *variant = game_engine_get_variant();
	struct player_datum *player;
	float dev = dev_speed_scale();

	if (!variant || player_index == NONE)
		return dev;
	player = player_get(player_index);
	if (variant->game_engine_index == game_engine_ctf && (ext->ctf_flags & AE_EXT_CTF_SLOW_CARRIER) &&
		game_engine_player_has_flag(player_index))
	{
		return dev * ae_ext_movement_percent(AE_EXT_MOVEMENT_SLOW) / 100.0f;
	}
	if (variant->game_engine_index == game_engine_bomb && (ext->bomb_flags & AE_EXT_BOMB_SLOW_CARRIER) &&
		player->unit_index != NONE && ae_bomb_snd_unit_carries(player->unit_index))
	{
		return dev * ae_ext_movement_percent(AE_EXT_MOVEMENT_SLOW) / 100.0f;
	}
	return dev;
}

long ae_h2_ctf_return_ticks(long default_ticks)
{
	unsigned char seconds = ae_variant_ext_game()->ctf_reset_time;

	return seconds ? seconds * TICKS_PER_SECOND : default_ticks;
}

/* WHEN SPOTTED: per flag team, the tick a teammate last saw the enemy carrier, the tick it was last tested (a test every
3 ticks) and the carrier it was of; the sighting holds 1.5 s. Each machine tests from the teammates' units it has, so a
guest needs no message from the host */
#define AE_FLAG_SPOT_TEST_TICKS 3
#define AE_FLAG_SPOT_HOLD_TICKS ((long)(1.5f * TICKS_PER_SECOND))
static long flag_spot_seen_at[2] = { NONE, NONE };
static long flag_spot_tested_at[2] = { NONE, NONE };
static long flag_spot_carrier[2] = { NONE, NONE };

/* whether a living player of the team sees the point (the carrier's unit seen, its vehicle too) */
static int team_sees_carrier(short team_index, long carrier_unit_index, real_point3d const *position)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d head;

		if (player->team_index != team_index || player->unit_index == NONE ||
			!object_try_and_get_and_verify_type(player->unit_index, NONE))
		{
			continue;
		}
		unit_get_head_position(player->unit_index, &head);
		if (hud_nav_point_in_sight_of_unit(player->unit_index, &head, position, carrier_unit_index))
			return 1;
	}
	return 0;
}

static int flag_spotted(short team_index, long carrier_player_index, real_point3d const *position)
{
	long now = game_time_get();
	long carrier_unit_index = NONE;
	struct player_datum *carrier = (struct player_datum *)datum_try_and_get(player_data, carrier_player_index);

	if (team_index < 0 || team_index > 1)
		return 0;
	if (carrier)
		carrier_unit_index = carrier->unit_index;
	/* (a new carrier, or a new game's clock: nothing seen yet) */
	if (flag_spot_carrier[team_index] != carrier_player_index || now < flag_spot_tested_at[team_index])
	{
		flag_spot_carrier[team_index] = carrier_player_index;
		flag_spot_seen_at[team_index] = NONE;
		flag_spot_tested_at[team_index] = NONE;
	}
	if (flag_spot_tested_at[team_index] == NONE || now - flag_spot_tested_at[team_index] >= AE_FLAG_SPOT_TEST_TICKS)
	{
		flag_spot_tested_at[team_index] = now;
		if (team_sees_carrier(team_index, carrier_unit_index, position))
			flag_spot_seen_at[team_index] = now;
	}
	{
		/* (debug.waypoint_log: each change said once, in the game's log and a TEST RUN's event stream, so a test's
		expect line can tell when the carrier came into sight and when its waypoint went) */
		static int said[2] = { NONE, NONE };
		int held = ae_ext_spot_held(now, flag_spot_seen_at[team_index], AE_FLAG_SPOT_HOLD_TICKS);

		if (held != said[team_index] && hud_debug_flag(_hud_debug_waypoint_log))
		{
			error(_error_silent, "flag spotted: team %d's flag carrier %s at tick %ld", (int)team_index,
				held ? "seen, its waypoint shows" : "out of sight, its waypoint goes", now);
		}
		said[team_index] = held;
		return held;
	}
}

/* (debug.waypoint_log) what the own flag's waypoint rule gave, said when it changes: the rule in force, the flag away
from home or not, carried or not, and whether the waypoint shows (-1: the gametype's own rule decides) */
static void own_flag_said(short team_index, int rule, int away, int carried, int shows)
{
	static int said[2] = { NONE, NONE };
	int now = rule * 1000 + (away != 0) * 100 + (carried != 0) * 10 + shows + 1;

	if (team_index < 0 || team_index > 1 || said[team_index] == now)
		return;
	said[team_index] = now;
	if (hud_debug_flag(_hud_debug_waypoint_log))
		error(_error_silent, "own flag waypoint: team %d's flag: rule %d (connection %d), %s, %s: %s at tick %ld", (int)team_index,
			rule, (int)game_connection(), away ? "away" : "home", carried ? "carried" : "not carried", shows > 0 ? "shows" :
			shows ? "the gametype's rule" : "hidden", game_time_get());
}

int ae_h2_ctf_own_flag_shows(int away, short team_index, long carrier_player_index, real_point3d const *position)
{
	int rule = ae_variant_ext_game()->ctf_own_flag_waypoint;
	int carried = carrier_player_index != NONE, shows;

	if (rule == AE_EXT_WAYPOINT_SPOTTED)
		shows = ae_ext_flag_spotted_shows(!away, carried, carried && flag_spotted(team_index, carrier_player_index, position));
	else
		shows = ae_ext_waypoint_shows(rule, 0, 0, !away);
	own_flag_said(team_index, rule, away, carried, shows);
	return shows;
}

static int any_player(int (*test)(long player_index))
{
	struct data_iterator iterator;

	data_iterator_new(&iterator, player_data);
	while (data_iterator_next(&iterator))
	{
		if (test(iterator.datum_index))
			return 1;
	}
	return 0;
}

static int carries_flag(long player_index)
{
	return game_engine_player_has_flag(player_index);
}

static int carries_bomb(long player_index)
{
	struct player_datum *player = player_get(player_index);

	return player->unit_index != NONE && ae_bomb_snd_unit_carries(player->unit_index);
}

/* a flag on the map with an enemy by it */
static int any_flag_contested(void)
{
	struct object_iterator iterator;

	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while (object_iterator_next(&iterator))
	{
		if (weapon_is_flag(iterator.index) && ae_ctf_flag_contested(iterator.index))
			return 1;
	}
	return 0;
}

int ae_h2_sudden_death_holds(void)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	struct game_variant const *variant = game_engine_get_variant();

	if (!variant || !game_engine_running())
		return 0;
	if (variant->game_engine_index == game_engine_ctf && (ext->ctf_flags & AE_EXT_CTF_SUDDEN_DEATH))
	{
		/* (as the later Halos: a flag carried or contested, then 5 s more) */
		static long last_in_play = -1;

		/* (a game's own: one before this game's time up does not count) */
		if (last_in_play >= 0 && last_in_play < game_variant_options_get()->time_limit * 60L * TICKS_PER_SECOND)
			last_in_play = -1;
		return ae_sudden_death_hold(any_player(carries_flag) || any_flag_contested(), game_time_get(), &last_in_play,
			5 * TICKS_PER_SECOND);
	}
	if (variant->game_engine_index == game_engine_bomb && (ext->bomb_flags & AE_EXT_BOMB_SUDDEN_DEATH))
	{
		struct ae_bomb_state bomb;

		ae_bomb_state_get(&bomb);
		return any_player(carries_bomb) || bomb.plant_ticks > 0;
	}
	return 0;
}

int ae_h2_objective_melee_lethal(long damaging_player_index)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	struct game_variant const *variant = game_engine_get_variant();

	if (damaging_player_index == NONE || !variant)
		return 0;
	if (variant->game_engine_index == game_engine_ctf && (ext->ctf_flags & AE_EXT_CTF_MASSIVE_DAMAGE))
		return carries_flag(damaging_player_index);
	if (variant->game_engine_index == game_engine_bomb && (ext->bomb_flags & AE_EXT_BOMB_MASSIVE_DAMAGE))
		return carries_bomb(damaging_player_index);
	return 0;
}

int ae_h2_bomb_sticky(void)
{
	return (ae_variant_ext_game()->bomb_flags & AE_EXT_BOMB_STICKY_ARMING) != 0;
}

int ae_h2_bomb_touch_returns(long unit_index, long weapon_index)
{
	struct weapon_datum *weapon;
	union real_point3d spawn;
	real dx, dy, dz;

	if (!(ae_variant_ext_game()->bomb_flags & AE_EXT_BOMB_TOUCH_RETURN) || network_game_distributed_client() ||
		unit_index == NONE || weapon_index == NONE || ae_bomb_assault_may_carry(unit_index))
	{
		return 0;
	}
	weapon = weapon_get(weapon_index);
	if (weapon->object.parent_object_index != NONE || !ae_bomb_assault_spawn(&spawn))
		return 0;
	/* (a bomb at its spawn already: nothing to send back) */
	dx = weapon->object.position.x - spawn.x;
	dy = weapon->object.position.y - spawn.y;
	dz = weapon->object.position.z - spawn.z;
	return dx * dx + dy * dy + dz * dz > 1.0f;
}

void ae_h2_bomb_indicator(short carrier_enemies_goal, short bomb_goal, int planted)
{
	int rule = ae_variant_ext_game()->bomb_enemy_indicator;
	long attackers;
	int defenders_see_bomb;

	if (!rule)
		return;
	if (rule != AE_EXT_WAYPOINT_ALWAYS && game_engine_get_goal_in_use(carrier_enemies_goal))
		game_engine_clear_goal_position(carrier_enemies_goal);
	attackers = ae_bomb_assault_attacking_team();
	defenders_see_bomb = rule == AE_EXT_WAYPOINT_ALWAYS || rule == AE_EXT_WAYPOINT_LOOSE ||
		(rule == AE_EXT_WAYPOINT_HELD && planted);
	if (!defenders_see_bomb && attackers != NONE && game_engine_get_goal_in_use(bomb_goal))
	{
		real_point3d position;

		game_engine_get_goal_position(&position, bomb_goal);
		/* (the goal's height again: set_goal_position adds it) */
		position.z -= 0.63f;
		game_engine_set_goal_position(bomb_goal, &position, 0.0f, "ball_blue", NONE, (short)attackers, NONE);
	}
}
