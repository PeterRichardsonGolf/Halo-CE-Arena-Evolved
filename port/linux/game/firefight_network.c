/*
FIREFIGHT_NETWORK.C

Host -> Client Firefight state synchronization:
- Synchronizes Set, Round, Wave, Phase, Session Elapsed Time, Next Wave Timer,
  Shared Lives Pool, Active Skulls, Team Score, Enemies Alive, Generator Health/Shield/Lockdown,
  Per-Player Scores/Kills/Assists, and the objectives (firefight_objectives.c).
*/

#include <string.h>

#include "cseries/cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"

#include "halo_firefight.h"
#include "halo_skulls.h"

/* ---------- structures */

struct firefight_net_packet
{
	short current_set;
	short current_round;
	short current_wave;
	short phase_state;
	long phase_timer_ticks;
	long session_elapsed_ticks;
	long next_wave_timer_ticks;
	short shared_lives_remaining;
	short enemies_alive;
	unsigned long active_skulls;
	unsigned long temporary_skulls;
	long team_score;
	real active_multiplier;
	real set_vitality_scale;
	byte in_bonus_round;
	byte match_won;
	byte match_lost;
	byte generator_count;
	short gen_health[FIREFIGHT_MAX_GENERATORS];
	short gen_max_health[FIREFIGHT_MAX_GENERATORS];
	short gen_shield[FIREFIGHT_MAX_GENERATORS];
	short gen_max_shield[FIREFIGHT_MAX_GENERATORS];
	short gen_lockdown_ticks[FIREFIGHT_MAX_GENERATORS];
	byte gen_alive[FIREFIGHT_MAX_GENERATORS];
	short banner_type;
	short banner_data;
	short banner_timer_ticks;
	short dropship_object; /* its absolute index in flight, or NONE */
	byte dropship_scale;
	byte dropship_cloak;
	long player_score[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	short player_kills[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	short player_assists[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	/* (mod version 103) the ordnance pods: where each lands, and the game
	time they were let go at (NONE: none), which each machine shows falling */
	real_point3d ordnance_positions[FIREFIGHT_ORDNANCE_PODS_BEFORE_104];
	long ordnance_pod_drop_time;
	byte ordnance_pod_count;
	byte ordnance_heavy;
	/* (port) each player's set of pods (ordnance_pods_per_player), 0 from a
	host before it: one set for all */
	byte ordnance_pods_per_player;
	byte pad[1];
	/* (mod version 104) the rest, a weapon a pod */
	real_point3d more_ordnance_positions[FIREFIGHT_MAX_ORDNANCE_PODS - FIREFIGHT_ORDNANCE_PODS_BEFORE_104];
	/* (mod version 105) each pod's player, what it brings, and those used up */
	byte ordnance_pod_owners[FIREFIGHT_MAX_ORDNANCE_PODS];
	byte ordnance_pod_kinds[FIREFIGHT_MAX_ORDNANCE_PODS];
	unsigned long ordnance_pod_spent[(FIREFIGHT_MAX_ORDNANCE_PODS + 31) / 32];
	/* (mod version 106) the Flood's drop: the game time it started at (NONE:
	none), where and its facing, which each machine grows */
	long flood_drop_time;
	real_point3d flood_drop_position;
	real flood_drop_facing;
	/* (mod version 111) the objectives as the host has them
	(firefight_objectives.c), which each machine shows */
	struct firefight_objectives_view objectives;
};

/* (a host's or a replay's state from before the pods: all but them; and
from before a weapon a pod: four at most) */
#define FIREFIGHT_NET_PACKET_BEFORE_POD offsetof(struct firefight_net_packet, ordnance_positions)
#define FIREFIGHT_NET_PACKET_BEFORE_104 offsetof(struct firefight_net_packet, more_ordnance_positions)
#define FIREFIGHT_NET_PACKET_BEFORE_106 offsetof(struct firefight_net_packet, flood_drop_time)
#define FIREFIGHT_NET_PACKET_BEFORE_111 offsetof(struct firefight_net_packet, objectives)

/* ---------- public code */

long firefight_write_network_state(
	byte *buffer,
	long size)
{
	struct firefight_net_packet pkt;
	short i;

	if (!buffer || size < (long)sizeof(pkt))
		return 0;

	csmemset(&pkt, 0, sizeof(pkt));
	pkt.current_set = ff_state.current_set;
	pkt.current_round = ff_state.current_round;
	pkt.current_wave = ff_state.current_wave;
	pkt.phase_state = ff_state.phase_state;
	pkt.phase_timer_ticks = ff_state.phase_timer_ticks;
	pkt.session_elapsed_ticks = ff_state.session_elapsed_ticks;
	pkt.next_wave_timer_ticks = ff_state.next_wave_timer_ticks;
	pkt.shared_lives_remaining = ff_state.shared_lives_remaining;
	pkt.enemies_alive = ff_state.enemies_alive;
	pkt.active_skulls = ff_state.active_skulls;
	pkt.temporary_skulls = ff_state.temporary_skulls;
	pkt.team_score = ff_state.team_score;
	pkt.active_multiplier = ff_state.active_multiplier;
	pkt.set_vitality_scale = ff_state.set_vitality_scale;
	pkt.in_bonus_round = ff_state.in_bonus_round ? 1 : 0;
	pkt.match_won = ff_state.match_won ? 1 : 0;
	pkt.match_lost = ff_state.match_lost ? 1 : 0;
	pkt.generator_count = (byte)PIN(ff_state.generator_count, 0, FIREFIGHT_MAX_GENERATORS);

	for (i = 0; i < FIREFIGHT_MAX_GENERATORS; i++)
	{
		pkt.gen_health[i] = ff_state.generators[i].health;
		pkt.gen_max_health[i] = ff_state.generators[i].max_health;
		pkt.gen_shield[i] = ff_state.generators[i].shield;
		pkt.gen_max_shield[i] = ff_state.generators[i].max_shield;
		pkt.gen_lockdown_ticks[i] = ff_state.generators[i].lockdown_ticks;
		pkt.gen_alive[i] = ff_state.generators[i].alive ? 1 : 0;
	}

	pkt.banner_type = ff_state.banner_type;
	pkt.banner_data = ff_state.banner_data;
	pkt.banner_timer_ticks = ff_state.banner_timer_ticks;

	pkt.dropship_object = NONE;
	if (ff_state.active_dropship.phase != 0 && ff_state.active_dropship.vehicle_index != NONE)
	{
		pkt.dropship_object = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(ff_state.active_dropship.vehicle_index);
		pkt.dropship_scale = (byte)(PIN(ff_state.active_dropship.scale, 0.0f, 1.0f) * 255.0f);
		pkt.dropship_cloak = (byte)(PIN(ff_state.active_dropship.cloak, 0.0f, 1.0f) * 255.0f);
	}

	for (i = 0; i < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; i++)
	{
		pkt.player_score[i] = ff_state.player_stats[i].score;
		pkt.player_kills[i] = ff_state.player_stats[i].kills;
		pkt.player_assists[i] = ff_state.player_stats[i].assists;
	}
	for (i = 0; i < FIREFIGHT_MAX_ORDNANCE_PODS; i++)
	{
		if (i < FIREFIGHT_ORDNANCE_PODS_BEFORE_104)
			pkt.ordnance_positions[i] = ff_state.ordnance_pod_positions[i];
		else
			pkt.more_ordnance_positions[i - FIREFIGHT_ORDNANCE_PODS_BEFORE_104] = ff_state.ordnance_pod_positions[i];
	}
	pkt.ordnance_pod_count = (byte)PIN(ff_state.ordnance_pod_count, 0, FIREFIGHT_MAX_ORDNANCE_PODS);
	pkt.ordnance_pod_drop_time = ff_state.ordnance_pod_drop_time;
	pkt.ordnance_heavy = ff_state.last_ordnance_is_heavy ? 1 : 0;
	pkt.ordnance_pods_per_player = (byte)PIN(ff_state.ordnance_pods_per_player, 0, 255);
	csmemcpy(pkt.ordnance_pod_owners, ff_state.ordnance_pod_owners, sizeof(pkt.ordnance_pod_owners));
	csmemcpy(pkt.ordnance_pod_kinds, ff_state.ordnance_pod_kinds, sizeof(pkt.ordnance_pod_kinds));
	csmemcpy(pkt.ordnance_pod_spent, ff_state.ordnance_pod_spent, sizeof(pkt.ordnance_pod_spent));
	pkt.flood_drop_time = ff_state.flood_drop_time;
	pkt.flood_drop_position = ff_state.flood_drop_position;
	pkt.flood_drop_facing = ff_state.flood_drop_facing;
	pkt.objectives = *firefight_objectives_view_get();

	csmemcpy(buffer, &pkt, sizeof(pkt));
	return (long)sizeof(pkt);
}

boolean firefight_read_network_state(
	byte const *buffer,
	long size,
	boolean first)
{
	struct firefight_net_packet pkt;
	short i;

	(void)first;
	if (!buffer || size < (long)FIREFIGHT_NET_PACKET_BEFORE_POD)
		return FALSE;

	csmemset(&pkt, 0, sizeof(pkt));
	pkt.ordnance_pod_drop_time = NONE;
	pkt.flood_drop_time = NONE;
	csmemcpy(&pkt, buffer, (size_t)MIN(size, (long)sizeof(pkt)));

	ff_state.current_set = pkt.current_set;
	ff_state.current_round = pkt.current_round;
	ff_state.current_wave = pkt.current_wave;
	ff_state.phase_state = pkt.phase_state;
	ff_state.phase_timer_ticks = pkt.phase_timer_ticks;
	ff_state.session_elapsed_ticks = pkt.session_elapsed_ticks;
	ff_state.next_wave_timer_ticks = pkt.next_wave_timer_ticks;
	ff_state.shared_lives_remaining = pkt.shared_lives_remaining;
	ff_state.enemies_alive = pkt.enemies_alive;
	ff_state.active_skulls = pkt.active_skulls;
	ff_state.temporary_skulls = pkt.temporary_skulls;
	ff_state.team_score = pkt.team_score;
	ff_state.active_multiplier = pkt.active_multiplier;
	ff_state.set_vitality_scale = pkt.set_vitality_scale;
	ff_state.in_bonus_round = pkt.in_bonus_round != 0;
	ff_state.match_won = pkt.match_won != 0;
	ff_state.match_lost = pkt.match_lost != 0;
	ff_state.generator_count = PIN(pkt.generator_count, 0, FIREFIGHT_MAX_GENERATORS);
	ff_state.generators_alive = 0;

	for (i = 0; i < FIREFIGHT_MAX_GENERATORS; i++)
	{
		ff_state.generators[i].health = pkt.gen_health[i];
		ff_state.generators[i].max_health = pkt.gen_max_health[i];
		ff_state.generators[i].shield = pkt.gen_shield[i];
		ff_state.generators[i].max_shield = pkt.gen_max_shield[i];
		ff_state.generators[i].lockdown_ticks = pkt.gen_lockdown_ticks[i];
		ff_state.generators[i].alive = pkt.gen_alive[i] != 0;
		ff_state.generators[i].active = (i < ff_state.generator_count);
		if (ff_state.generators[i].active && ff_state.generators[i].alive)
			ff_state.generators_alive++;
	}

	if (pkt.banner_timer_ticks > 0 && pkt.banner_type != _ff_banner_none)
	{
		ff_state.banner_type = pkt.banner_type;
		ff_state.banner_data = pkt.banner_data;
		ff_state.banner_timer_ticks = pkt.banner_timer_ticks;
	}

	if (pkt.dropship_object != NONE)
	{
		firefight_dropship_apply_look(
			pkt.dropship_object,
			MAX((real)pkt.dropship_scale / 255.0f, 0.02f),
			(real)pkt.dropship_cloak / 255.0f);
	}
	else
	{
		ff_state.active_dropship.vehicle_index = NONE;
		ff_state.active_dropship.phase = 0;
	}

	for (i = 0; i < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; i++)
	{
		ff_state.player_stats[i].score = pkt.player_score[i];
		ff_state.player_stats[i].kills = pkt.player_kills[i];
		ff_state.player_stats[i].assists = pkt.player_assists[i];
	}
	if (pkt.ordnance_pod_drop_time != NONE && pkt.ordnance_pod_drop_time > 0)
	{
		for (i = 0; i < FIREFIGHT_MAX_ORDNANCE_PODS; i++)
		{
			ff_state.ordnance_pod_positions[i] = i < FIREFIGHT_ORDNANCE_PODS_BEFORE_104 ? pkt.ordnance_positions[i] :
				pkt.more_ordnance_positions[i - FIREFIGHT_ORDNANCE_PODS_BEFORE_104];
		}
		ff_state.ordnance_pod_count = (short)PIN(pkt.ordnance_pod_count, 1,
			size >= (long)FIREFIGHT_NET_PACKET_BEFORE_104 + (long)sizeof(pkt.more_ordnance_positions) ?
			FIREFIGHT_MAX_ORDNANCE_PODS : FIREFIGHT_ORDNANCE_PODS_BEFORE_104);
		ff_state.last_ordnance_position = pkt.ordnance_positions[0];
		ff_state.last_ordnance_is_heavy = pkt.ordnance_heavy != 0;
		ff_state.ordnance_pods_per_player = pkt.ordnance_pods_per_player;
		/* (a host from before: weapons all, the sets by ordnance_pods_per_player) */
		if (size >= (long)FIREFIGHT_NET_PACKET_BEFORE_106)
		{
			csmemcpy(ff_state.ordnance_pod_owners, pkt.ordnance_pod_owners, sizeof(ff_state.ordnance_pod_owners));
			csmemcpy(ff_state.ordnance_pod_kinds, pkt.ordnance_pod_kinds, sizeof(ff_state.ordnance_pod_kinds));
		}
		else
		{
			csmemset(ff_state.ordnance_pod_owners, FIREFIGHT_ORDNANCE_POD_ANYONE, sizeof(ff_state.ordnance_pod_owners));
			csmemset(ff_state.ordnance_pod_kinds, _ff_pod_kind_weapon, sizeof(ff_state.ordnance_pod_kinds));
		}
	}
	if (size >= (long)FIREFIGHT_NET_PACKET_BEFORE_106)
		csmemcpy(ff_state.ordnance_pod_spent, pkt.ordnance_pod_spent, sizeof(ff_state.ordnance_pod_spent));
	else
		csmemset(ff_state.ordnance_pod_spent, 0, sizeof(ff_state.ordnance_pod_spent));
	ff_state.ordnance_pod_drop_time = pkt.ordnance_pod_drop_time;
	ff_state.flood_drop_time = pkt.flood_drop_time;
	ff_state.flood_drop_position = pkt.flood_drop_position;
	ff_state.flood_drop_facing = pkt.flood_drop_facing;
	/* (a host from before them: none) */
	if (size >= (long)sizeof(pkt))
		firefight_objectives_view_set(&pkt.objectives);

	return TRUE;
}

long game_engine_firefight_write_network_state(
	byte *buffer,
	long size)
{
	return firefight_write_network_state(buffer, size);
}

boolean game_engine_firefight_read_network_state(
	byte const *buffer,
	long size,
	boolean first)
{
	return firefight_read_network_state(buffer, size, first);
}
