/* The second-generation game's objective carrier rules (see the header) */
#include "cseries.h"
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

float ae_h2_carrier_speed(long player_index)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	struct game_variant const *variant = game_engine_get_variant();
	struct player_datum *player;

	if (!variant || player_index == NONE)
		return 1.0f;
	player = player_get(player_index);
	if (variant->game_engine_index == game_engine_ctf && (ext->ctf_flags & AE_EXT_CTF_SLOW_CARRIER) &&
		game_engine_player_has_flag(player_index))
	{
		return ae_ext_movement_percent(AE_EXT_MOVEMENT_SLOW) / 100.0f;
	}
	if (variant->game_engine_index == game_engine_bomb && (ext->bomb_flags & AE_EXT_BOMB_SLOW_CARRIER) &&
		player->unit_index != NONE && ae_bomb_snd_unit_carries(player->unit_index))
	{
		return ae_ext_movement_percent(AE_EXT_MOVEMENT_SLOW) / 100.0f;
	}
	return 1.0f;
}

long ae_h2_ctf_return_ticks(long default_ticks)
{
	unsigned char seconds = ae_variant_ext_game()->ctf_reset_time;

	return seconds ? seconds * TICKS_PER_SECOND : default_ticks;
}

int ae_h2_ctf_own_flag_shows(int away)
{
	return ae_ext_waypoint_shows(ae_variant_ext_game()->ctf_own_flag_waypoint, 0, 0, !away);
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

int ae_h2_sudden_death_holds(void)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	struct game_variant const *variant = game_engine_get_variant();

	if (!variant || !game_engine_running())
		return 0;
	if (variant->game_engine_index == game_engine_ctf && (ext->ctf_flags & AE_EXT_CTF_SUDDEN_DEATH))
		return any_player(carries_flag);
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
