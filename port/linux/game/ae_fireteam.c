/*
AE_FIRETEAM.C

Fireteams (ae_fireteam.h): the engine's side of ae_fireteam_rules.c. Three hooks tell it of the game's start, a
person's spawn and a change of the rosters; each one splits the teams' bots over the teams' leaders again. Host only
(bots live there), and only in a team game with the marks on (the gate ae_bots_marks.inc uses).
*/

#include "cseries.h"
#include "errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"

#include "saved games/arena_gametypes.h"

#include "ae_fireteam.h"
#include "ae_fireteam_rules.h"
#include "ae_marks.h"
#include "bots.h"


#define AE_FIRETEAM_TEAMS 2

static struct ae_fireteam_table fireteam_table;
static boolean fireteam_started;
/* a person who has spawned this game leads a fireteam (by the player's absolute index) */
static byte fireteam_spawned[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];

static boolean fireteam_gate(void)
{
	/* (host only: the bots live there; a client's table would only be a copy of nothing) */
	return game_connection() != _game_connection_network_client && game_engine_running() && game_engine_has_teams() &&
		ae_marks_rule_on();
}

/* the rules the game's fireteams play by (ae_fireteam_rules.h): chosen again at each rebuild (a roster change can move a non-AE
gametype over the team-size line), logged once a game and whenever it changes */
static int fireteam_rules_logged = -1;

static void fireteam_choose_rules(long largest_team)
{
	struct game_variant *variant = game_engine_get_variant();
	wchar_t stored[NUMBEROF(variant->human_readable_game_description) + 1];
	struct arena_gametype_info info;
	char const *reason = "";
	int known, arena;

	csmemcpy(stored, variant->human_readable_game_description, sizeof(variant->human_readable_game_description));
	stored[NUMBEROF(stored) - 1] = 0;
	known = arena_gametype_info(stored, &info) && info.category != _arena_gametype_category_none &&
		info.category != _arena_gametype_category_stock;
	arena = ae_fireteam_rules_are_arena(known, known && info.category == _arena_gametype_category_btb, (int)largest_team, &reason);
	ae_fireteam_set_rules(&fireteam_table, arena);
	if (fireteam_rules_logged != arena)
	{
		fireteam_rules_logged = arena;
		error(_error_silent, "ae fireteam: %s rules (%s)", arena ? "arena" : "big team", reason);
	}
}

int ae_fireteam_arena_rules(void)
{
	return fireteam_started && fireteam_table.arena_rules;
}

boolean ae_fireteam_regroup_is_complete(long fireteam)
{
	return fireteam_started && ae_fireteam_regroup_complete(&fireteam_table, (int)fireteam);
}

void ae_fireteam_release(long fireteam)
{
	if (fireteam_started)
		ae_fireteam_release_order(&fireteam_table, (int)fireteam);
}

static void fireteam_rebuild(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long leaders[AE_FIRETEAM_TEAMS][HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	short bots[AE_FIRETEAM_TEAMS][AE_FIRETEAM_BRAINS];
	short leader_count[AE_FIRETEAM_TEAMS] = { 0 };
	short bot_count[AE_FIRETEAM_TEAMS] = { 0 };
	short team;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long machine = player->network_player_data.machine_index;
		long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);

		if (player->team_index < 0 || player->team_index >= AE_FIRETEAM_TEAMS || player->quit_out_of_game)
			continue;
		if (bots_machine_is_bot(machine))
		{
			long brain = machine - BOTS_FIRST_MACHINE;

			if (brain >= 0 && brain < AE_FIRETEAM_BRAINS)
				bots[player->team_index][bot_count[player->team_index]++] = (short)brain;
		}
		else if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS && fireteam_spawned[absolute])
		{
			leaders[player->team_index][leader_count[player->team_index]++] = iterator.datum_index;
		}
	}
	fireteam_choose_rules(leader_count[0] + bot_count[0] > leader_count[1] + bot_count[1] ? leader_count[0] + bot_count[0] :
		leader_count[1] + bot_count[1]);
	for (team = 0; team < AE_FIRETEAM_TEAMS; team++)
	{
		ae_fireteam_split(&fireteam_table, team, leaders[team], leader_count[team], bots[team], bot_count[team]);
		error(_error_silent, "ae fireteam: team %d: %d leaders, %d bots", (int)team, (int)leader_count[team],
			(int)bot_count[team]);
	}
}

void ae_fireteam_game_start(void)
{
	ae_fireteam_clear(&fireteam_table);
	csmemset(fireteam_spawned, 0, sizeof(fireteam_spawned));
	fireteam_rules_logged = -1;
	fireteam_started = fireteam_gate();
	if (fireteam_started)
		fireteam_rebuild();
}

void ae_fireteam_player_spawned(long player_index)
{
	struct player_datum *player;
	long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);

	if (!fireteam_started || absolute < 0 || absolute >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS || fireteam_spawned[absolute])
		return;
	player = player_try_and_get(player_index);
	if (!player || bots_machine_is_bot(player->network_player_data.machine_index))
		return;
	fireteam_spawned[absolute] = TRUE;
	fireteam_rebuild();
}

void ae_fireteam_roster_changed(void)
{
	if (fireteam_started && fireteam_gate())
		fireteam_rebuild();
}

void ae_fireteam_player_left(long player_index)
{
	long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);

	if (!fireteam_started)
		return;
	if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		fireteam_spawned[absolute] = FALSE;
	ae_fireteam_leader_left(&fireteam_table, player_index);
	if (fireteam_gate())
		fireteam_rebuild();
}

long ae_fireteam_of_brain(long brain)
{
	return fireteam_started ? ae_fireteam_of_bot(&fireteam_table, (short)brain) : NONE;
}

long ae_fireteam_leader_player(long fireteam)
{
	return fireteam_started ? ae_fireteam_leader(&fireteam_table, (int)fireteam) : NONE;
}

void ae_fireteam_order(int fireteam, short kind, real_point3d const *point, long object_index)
{
	float spot[3];

	if (!fireteam_started)
		return;
	if (point)
	{
		spot[0] = point->x;
		spot[1] = point->y;
		spot[2] = point->z;
	}
	ae_fireteam_set_order(&fireteam_table, fireteam, kind, point ? spot : NULL, object_index);
	error(_error_silent, "ae fireteam: fireteam %d order %d", fireteam, (int)kind);
}

short ae_fireteam_order_of(long fireteam)
{
	return fireteam_started ? ae_fireteam_order_kind(&fireteam_table, (int)fireteam) : (short)_ae_order_none;
}

boolean ae_fireteam_order_point_of(long fireteam, real_point3d *point)
{
	float spot[3];

	if (!fireteam_started || !ae_fireteam_order_point(&fireteam_table, (int)fireteam, spot))
		return FALSE;
	point->x = spot[0];
	point->y = spot[1];
	point->z = spot[2];
	return TRUE;
}

boolean ae_fireteam_regroup_report(long brain, real distance)
{
	long fireteam = ae_fireteam_of_brain(brain);

	if (fireteam == NONE || !ae_fireteam_regroup_note(&fireteam_table, (int)fireteam, (short)brain, distance))
		return FALSE;
	error(_error_silent, "ae fireteam: fireteam %d regrouped", (int)fireteam);
	return TRUE;
}

boolean ae_fireteam_brain_arrived_of(long brain)
{
	return fireteam_started && ae_fireteam_brain_arrived(&fireteam_table, (short)brain);
}

short ae_fireteam_lead_brain_of(long fireteam)
{
	return fireteam_started ? ae_fireteam_lead_brain(&fireteam_table, (int)fireteam) : (short)-1;
}

boolean ae_fireteam_go_report(long brain, real distance, real now_seconds)
{
	long fireteam = ae_fireteam_of_brain(brain);

	if (fireteam == NONE || !ae_fireteam_go_note(&fireteam_table, (int)fireteam, (short)brain, distance, now_seconds))
		return FALSE;
	error(_error_silent, "ae fireteam: fireteam %d let go of its going order", (int)fireteam);
	return TRUE;
}

long ae_fireteam_led_by(struct player_datum const *player)
{
	long fireteam;

	if (!fireteam_started || !player)
		return NONE;
	for (fireteam = 0; fireteam < fireteam_table.count; fireteam++)
	{
		long leader = ae_fireteam_leader(&fireteam_table, (int)fireteam);

		if (leader != NONE && player_try_and_get(leader) == player)
			return fireteam;
	}
	return NONE;
}

int ae_fireteam_recruit_brain(long brain, long fireteam, boolean carrying_objective, boolean seated_driver)
{
	int result;

	if (!fireteam_started)
		return AE_RECRUIT_NONE;
	result = ae_fireteam_recruit_check(&fireteam_table, (short)brain, (int)fireteam, carrying_objective, seated_driver);
	if (result == AE_RECRUIT_OK && ae_fireteam_recruit(&fireteam_table, (short)brain, (int)fireteam))
		error(_error_silent, "ae fireteam: bot %ld recruited into fireteam %d", brain + 1, (int)fireteam);
	else if (result == AE_RECRUIT_OK)
		result = AE_RECRUIT_NONE;
	return result;
}

long ae_fireteam_slot_count(void)
{
	return fireteam_started ? fireteam_table.count : 0;
}

short ae_fireteam_team_of(long fireteam)
{
	return fireteam_started && fireteam >= 0 && fireteam < fireteam_table.count ? fireteam_table.fireteams[fireteam].team : (short)-1;
}

long ae_fireteam_generation(void)
{
	return fireteam_started ? fireteam_table.generation : -1;
}

int ae_fireteam_role_for(long fireteam, int mode)
{
	return fireteam_started ? ae_fireteam_role_of(&fireteam_table, (int)fireteam, mode) : AE_ROLE_NONE;
}

int ae_fireteam_role_taken(long fireteam)
{
	return fireteam_started ? ae_fireteam_role(&fireteam_table, (int)fireteam) : AE_ROLE_NONE;
}

void ae_fireteam_role_wiped_fireteam(long fireteam)
{
	if (fireteam_started)
		ae_fireteam_role_wiped(&fireteam_table, (int)fireteam);
}

boolean ae_fireteam_role_pass(long fireteam, int role, real_point3d const *point, boolean leader_alive, boolean flipped)
{
	float spot[3];

	if (!fireteam_started || !ae_fireteam_role_wants_order(&fireteam_table, (int)fireteam, leader_alive, flipped))
		return FALSE;
	if (point)
	{
		spot[0] = point->x;
		spot[1] = point->y;
		spot[2] = point->z;
	}
	ae_fireteam_set_role_order(&fireteam_table, (int)fireteam, role, point ? spot : NULL);
	error(_error_silent, "ae fireteam: role fireteam %ld team %d role %d order %d%s", fireteam, (int)fireteam_table.fireteams[fireteam].team,
		role, (int)ae_fireteam_order_kind(&fireteam_table, (int)fireteam), point ? "" : " (no point)");
	return TRUE;
}

boolean ae_fireteam_order_explicit_of(long fireteam)
{
	return fireteam_started && ae_fireteam_order_is_explicit(&fireteam_table, (int)fireteam);
}

long ae_fireteam_order_serial_of(long fireteam)
{
	return fireteam_started ? ae_fireteam_order_serial(&fireteam_table, (int)fireteam) : 0;
}

void ae_fireteam_check_leaders(void)
{
	long fireteam;

	if (!fireteam_started)
		return;
	for (fireteam = 0; fireteam < fireteam_table.count; fireteam++)
	{
		long leader = ae_fireteam_leader(&fireteam_table, (int)fireteam);
		struct player_datum const *player = leader != NONE ? player_try_and_get(leader) : NULL;

		if (leader != NONE && player && !player->quit_out_of_game && player->team_index != fireteam_table.fireteams[fireteam].team)
		{
			error(_error_silent, "ae fireteam: leader %ld changed teams (%d to %d): the rosters are split again", leader,
				(int)fireteam_table.fireteams[fireteam].team, (int)player->team_index);
			fireteam_rebuild();
			return;
		}
	}
}
