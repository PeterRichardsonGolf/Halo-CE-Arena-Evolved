/*
AE_H2_MATCH.C

See ae_h2_match.h. The rules' arithmetic is in ae_h2_match_rules.c and ae_variant_ext_rules.c.

ROUNDS: the engines end a game with game_engine_end_game when their score is reached (or the clock runs out,
game_engine_should_end_game). With rounds on, that call is a round's end: this module counts the round for the side
that leads, and when more rounds follow it keeps the game going, as a new round: everyone is killed (no death
counted) and comes back at the spawns, the engine's scores start again (its own start-of-map setup is called again),
and, unless ROUNDS RESET MAP is off, the map's items and vehicles are put back. The game's score is the round's:
the scoreboard shows the last round's scores, not the wins (the wins are in the log and in the chat line). The race
engine plays no rounds. The Bomb engine has its own.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_engine_slayer.h"
#include "game/players.h"
#include "memory/data.h"
#include "items/equipment_definitions.h"
#include "networking/network_game_globals.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "units/units.h"

#include "ae_h2_match.h"
#include "ae_h2_match_rules.h"
#include "ae_variant_ext.h"
#ifdef HALO_PORT_CHAT
#include "chat.h"
#endif

#include <stdio.h>
#include <string.h>

void platform_log(char const *format, ...);
boolean multiple_teams_alive(void);

enum
{
	MAX_SIDES = HALO_PORT_MAXIMUM_NETWORK_PLAYERS,   /* players, or teams: one slot each, no sharing */
	CLEANUP_DELAY_TICKS = 4,
	/* no respawn: far past any game (155 hours), and small enough that the HUD timers can add a game time to it */
	NEVER_RESPAWNS_TICKS = 1 << 24,
};

static struct
{
	int started;
	long last_time;
	long round_start;
	long played;
	long wins[MAX_SIDES];       /* by team, or by player (absolute index) */
	long spree[MAX_SIDES];
	long deaths_base[MAX_SIDES]; /* deaths when the round began: lives count within the round */
	long last_unit[MAX_SIDES];
	long cleanup_at;            /* NONE: no cleanup waiting */
	boolean force_items;
	boolean overtime_said;
	boolean final_end;          /* the end now asked for is the host's: the whole game, not a round */
} st;

/* the players a round's reset killed: until which tick their death is the reset's (the kill comes at the unit's next
update) */
static long reset_death_until[MAX_SIDES];
static long reset_death_player[MAX_SIDES];   /* the full player datum handle: a replacement in the slot is not it */

boolean ae_h2_match_quiet_death(long dead_player_index)
{
	long slot = dead_player_index == NONE ? -1 : DATUM_INDEX_TO_ABSOLUTE_INDEX(dead_player_index);

	if (slot < 0 || slot >= MAX_SIDES || reset_death_until[slot] <= 0 || reset_death_until[slot] < game_time_get() ||
		reset_death_player[slot] != dead_player_index)
		return FALSE;
	reset_death_until[slot] = 0;
	reset_death_player[slot] = NONE;
	return TRUE;
}

static boolean playing_engine(void)
{
	long type;

	if (!game_engine || !game_engine_running())
		return FALSE;
	type = game_engine_get_variant()->game_engine_index;
	return type == game_engine_ctf || type == game_engine_slayer || type == game_engine_oddball ||
		type == game_engine_king;
}

static boolean teams_play(void)
{
	return game_engine_get_variant()->universal_variant.teams != 0;
}

static void say(char const *text)
{
	platform_log("ae h2 match: %s", text);
#ifdef HALO_PORT_CHAT
	chat_server_say(text);
#endif
}

static boolean unit_alive(long unit_index)
{
	struct unit_datum *unit = unit_index == NONE ? NULL : unit_try_and_get(unit_index);

	return unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit);
}

static boolean in_game(struct player_datum const *player)
{
	return !player->quit_out_of_game;
}

static void state_reset(void)
{
	memset(&st, 0, sizeof(st));
	memset(reset_death_until, 0, sizeof(reset_death_until));
	memset(reset_death_player, 0xFF, sizeof(reset_death_player));
	st.cleanup_at = NONE;
	st.started = 1;
}

/* the ticks one round may last, 0 for no limit: with rounds the round time (else the gametype's time limit, which
is a round's then); without them the round time is the game's clock */
static long round_limit_ticks(struct ae_variant_ext const *ext)
{
	long seconds = ae_ext_round_time_seconds(ext);

	if (!seconds && ae_ext_rounds_count(ext) > 1)
		seconds = game_variant_options_get()->time_limit * 60L;
	return seconds * TICKS_PER_SECOND;
}

/* the leading side's index and whether two share the lead: a team's (team games) or a player's score. Returns
the index into scores, with the sides' ids in ids (a team index, or a player's datum index) */
static int round_leader(long *leader_id)
{
	long scores[MAX_SIDES];
	long ids[MAX_SIDES];
	int count = 0;
	int leader;
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long id = teams_play() ? player->team_index : iterator.datum_index;
		int slot;

		if (!in_game(player) || id == NONE)
			continue;
		for (slot = 0; slot < count && ids[slot] != id; slot++)
			;
		if (slot == count && count < MAX_SIDES)
		{
			ids[count] = id;
			scores[count] = game_engine->get_player_score(iterator.datum_index,
				teams_play() ? _get_score_team : _get_score_individual);
			count++;
		}
	}
	leader = ae_h2_unique_leader(scores, count);
	if (leader_id)
		*leader_id = leader >= 0 ? ids[leader] : NONE;
	return leader;
}

/* a player's slot in the per-player arrays, -1 for none */
static int player_slot(long player_index)
{
	return ae_h2_slot_of(DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), MAX_SIDES);
}

static long wins_slot(long id)
{
	return ae_h2_slot_of(teams_play() ? id : DATUM_INDEX_TO_ABSOLUTE_INDEX(id), MAX_SIDES);
}

static void wins_leaders(long *best, long *runner_up)
{
	int index;

	*best = 0;
	*runner_up = 0;
	for (index = 0; index < MAX_SIDES; index++)
	{
		if (st.wins[index] > *best)
			*runner_up = *best, *best = st.wins[index];
		else if (st.wins[index] > *runner_up)
			*runner_up = st.wins[index];
	}
}

void ae_h2_match_update(void)
{
	struct ae_variant_ext const *ext;
	long now = game_time_get();
	struct data_iterator iterator;
	struct player_datum *player;

	if (!playing_engine() || network_game_distributed_client())
		return;
	if (!st.started || now < st.last_time)
		state_reset();
	st.last_time = now;
	st.force_items = FALSE;
	ext = ae_variant_ext_game();

	if (st.cleanup_at != NONE && now >= st.cleanup_at)
	{
		struct object_iterator objects;
		long keep = game_engine_get_variant()->game_engine_index == game_engine_ctf ? get_flag_definition_index() :
			game_engine_get_variant()->game_engine_index == game_engine_oddball ? get_ball_definition_index() : NONE;

		st.cleanup_at = NONE;
		/* (the map back: loose weapons, powerups and garbage go, the engine's own objects (a flag, a ball) stay; the
		items return on this tick's item spawn; vehicles go and come back at their homes after the vehicle respawn
		time) */
		object_iterator_new(&objects, _object_mask_item | _object_mask_vehicle, 0);
		while (object_iterator_next(&objects))
		{
			struct object_datum *object = object_get(objects.index);

			if (object->object.parent_object_index != NONE || object->definition_index == keep)
				continue;
			object_delete(objects.index);
		}
		st.force_items = TRUE;
	}

	/* SPAWN WITH OVERSHIELD: a unit that is new to its player */
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		int slot = player_slot(iterator.datum_index);

		if (slot >= 0 && player->unit_index != st.last_unit[slot])
		{
			st.last_unit[slot] = player->unit_index;
			if (ext->player_flags & AE_EXT_PLAYER_OVERSHIELDS && unit_alive(player->unit_index))
				object_double_charge_shield(player->unit_index);
		}
	}
}

/* whether the lead is shared (teams, or players): the clock does not end a game that is tied while RESOLVE TIES is on */
static boolean tied_now(void)
{
	return round_leader(NULL) < 0;
}

boolean ae_h2_match_filter_end(boolean should_end)
{
	struct ae_variant_ext const *ext;
	long now;
	long limit;
	boolean rounds;
	boolean game_clock;

	if (!playing_engine() || network_game_distributed_client())
		return should_end;
	ext = ae_variant_ext_game();
	rounds = ae_ext_rounds_count(ext) > 1;
	if (!rounds && !ext->round_time_10s && !(ext->match_flags & AE_EXT_MATCH_RESOLVE_TIES))
		return should_end;
	now = game_time_get();
	limit = round_limit_ticks(ext);
	game_clock = game_variant_options_get()->time_limit > 0 &&
		now >= game_variant_options_get()->time_limit * 60L * TICKS_PER_SECOND;
	/* the clock of the engine's end: with rounds the game's time limit is a round's, so the end it found is not a
	clock's unless the last team left (multiple_teams_alive) */
	if (should_end && game_clock && rounds && multiple_teams_alive())
		should_end = FALSE;
	if (limit > 0 && now - st.round_start >= limit)
		should_end = TRUE;
	/* a tie at the clock plays on (overtime) */
	if (should_end && (ext->match_flags & AE_EXT_MATCH_RESOLVE_TIES) && multiple_teams_alive() &&
		(game_clock || (limit > 0 && now - st.round_start >= limit)) && tied_now())
	{
		if (!st.overtime_said)
		{
			st.overtime_said = TRUE;
			say("Tied at the time limit: overtime");
		}
		return FALSE;
	}
	return should_end;
}

static void new_round(struct ae_variant_ext const *ext)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long now = game_time_get();
	long type = game_engine_get_variant()->game_engine_index;
	char text[64];

	/* the engine's own objects of a round (a flag, a ball) go before its setup makes them again */
	if (type == game_engine_ctf || type == game_engine_oddball)
	{
		long definition = type == game_engine_ctf ? get_flag_definition_index() : get_ball_definition_index();
		struct object_iterator objects;

		object_iterator_new(&objects, _object_mask_weapon, 0);
		while (object_iterator_next(&objects))
		{
			if (object_get(objects.index)->definition_index == definition)
				object_delete(objects.index);
		}
	}
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (unit_alive(player->unit_index))
		{
			/* (the reset's own death: no FALL line in the feed, no suicide wait) */
			if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) < MAX_SIDES)
			{
				reset_death_until[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] = now + 4;
				reset_death_player[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] = iterator.datum_index;
			}
			unit_kill_no_statistics(player->unit_index);
		}
		player->death_time = now;
		if (player_slot(iterator.datum_index) >= 0)
		{
			st.spree[player_slot(iterator.datum_index)] = 0;
			st.deaths_base[player_slot(iterator.datum_index)] = player->statistics.deaths;
		}
	}
	/* the scores start again: the engine's setup for a new map */
	game_engine->initialize_for_new_map();
	st.round_start = now;
	st.overtime_said = FALSE;
	if (!(ext->match_flags & AE_EXT_MATCH_ROUNDS_KEEP_MAP))
		st.cleanup_at = now + CLEANUP_DELAY_TICKS;
	snprintf(text, sizeof(text), "Round %ld", st.played + 1);
	say(text);
}

boolean ae_h2_match_round_line(long viewer_team, long *round, long *mine, long *theirs)
{
	long index, best_other = 0;

	/* (the host's count: a joined PC has none, so it shows no line) */
	if (!playing_engine() || network_game_distributed_client() || !st.started ||
		ae_ext_rounds_count(ae_variant_ext_game()) <= 1)
	{
		return FALSE;
	}
	*round = st.played + 1;
	*mine = *theirs = NONE;
	if (teams_play() && viewer_team >= 0 && viewer_team < MAX_SIDES)
	{
		for (index = 0; index < MAX_SIDES; index++)
		{
			if (index != viewer_team && st.wins[index] > best_other)
				best_other = st.wins[index];
		}
		*mine = st.wins[viewer_team];
		*theirs = best_other;
	}
	return TRUE;
}

boolean ae_h2_match_rounds_active(void)
{
	return playing_engine() && ae_ext_rounds_count(ae_variant_ext_game()) > 1;
}

boolean ae_h2_match_restart_round(void)
{
	if (!ae_h2_match_rounds_active() || network_game_distributed_client())
		return FALSE;
	if (!st.started)
		state_reset();
	platform_log("ae h2 match: the host restarts round %ld", st.played + 1);
	new_round(ae_variant_ext_game());
	return TRUE;
}

boolean ae_h2_match_end_game(void)
{
	struct ae_variant_ext const *ext;
	long winner;
	long best;
	long runner_up;
	char text[96];

	if (!playing_engine() || network_game_distributed_client())
		return FALSE;
	ext = ae_variant_ext_game();
	if (!ae_h2_end_is_round(st.final_end, ae_ext_rounds_count(ext)))
		return FALSE;
	if (!st.started)
		state_reset();
	if (round_leader(&winner) >= 0 && winner != NONE && wins_slot(winner) >= 0)
		st.wins[wins_slot(winner)]++;
	st.played++;
	wins_leaders(&best, &runner_up);
	snprintf(text, sizeof(text), "Round %ld over (%s %ld, runner-up %ld)", st.played,
		round_leader(NULL) >= 0 ? "leader's wins" : "tied round, best wins", best, runner_up);
	platform_log("ae h2 match: %s", text);
	if (!ae_h2_rounds_go_on(ae_ext_rounds_count(ext), ae_ext_rounds_to_win(ext),
		(ext->match_flags & AE_EXT_MATCH_RESOLVE_TIES) != 0, (int)st.played, (int)best, (int)runner_up))
	{
		platform_log("ae h2 match: game over after %ld rounds", st.played);
		return FALSE;
	}
	new_round(ext);
	return TRUE;
}

void game_engine_end_game_final(void)
{
	st.final_end = TRUE;
	game_engine_end_game();
	st.final_end = FALSE;
}

boolean ae_h2_match_holds_spawn(long player_index)
{
	struct ae_variant_ext const *ext;
	struct data_iterator iterator;
	struct player_datum *other;
	struct player_datum *player;
	int alive = 0;
	int ahead = 0;

	if (!game_engine || network_game_distributed_client())
		return FALSE;
	ext = ae_variant_ext_game();
	if (!ext->max_active_players)
		return FALSE;
	player = player_get(player_index);
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (!in_game(other))
			continue;
		if (unit_alive(other->unit_index))
			alive++;
		else if (other != player && (other->death_time < player->death_time ||
			(other->death_time == player->death_time &&
				DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) < DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index))))
		{
			ahead++;
		}
	}
	return !ae_h2_may_spawn(ext->max_active_players, alive, ahead);
}

long ae_h2_match_respawn_timer(long dead_player_index, long timer)
{
	struct ae_variant_ext const *ext;
	struct player_datum *dead;
	struct data_iterator iterator;
	struct player_datum *other;
	long last_left = -1;
	long last_death = -1;

	if (!game_engine || !game_engine_has_teams())
		return timer;
	ext = ae_variant_ext_game();
	if (ext->respawn_modifier == AE_EXT_RESPAWN_MODIFIER_NONE)
		return timer;
	dead = player_get(dead_player_index);
	/* the teammate who died last and is still waiting: how long they have left */
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (iterator.datum_index == dead_player_index || !in_game(other) || other->team_index != dead->team_index ||
			unit_alive(other->unit_index) || other->respawn_timer <= 0)
			continue;
		if (other->death_time > last_death)
			last_death = other->death_time, last_left = other->respawn_timer;
	}
	return ae_ext_respawn_wait_ticks(ext->respawn_modifier, timer, last_left, game_time_get());
}

void ae_h2_match_player_killed(long killing_player_index, long dead_player_index, boolean friendly_fire)
{
	struct ae_variant_ext const *ext;
	boolean suicide;
	boolean enemy_kill;
	int dead_slot;

	if (!game_engine || network_game_distributed_client() ||
		game_engine_get_variant()->game_engine_index != game_engine_slayer)
		return;
	ext = ae_variant_ext_game();
	if (!ext->slayer_flags)
		return;
	suicide = killing_player_index == NONE || killing_player_index == dead_player_index;
	enemy_kill = !suicide && !friendly_fire;
	dead_slot = player_slot(dead_player_index);
	if (dead_slot >= 0)
		st.spree[dead_slot] = 0;
	if (enemy_kill && (ext->slayer_flags & AE_EXT_SLAYER_BONUS_POINTS))
	{
		/* the victim led if they stood above everyone else, the kill counted: the killer one short of it */
		enum get_score_type type = teams_play() ? _get_score_team : _get_score_individual;
		struct data_iterator iterator;
		struct player_datum *player;
		long victim_score = game_engine->get_player_score(dead_player_index, type);
		long killer_before = game_engine->get_player_score(killing_player_index, type) - 1;
		boolean led = victim_score > 0 && victim_score >= killer_before + 1;
		int killer_slot = player_slot(killing_player_index);
		int bonus;

		data_iterator_new(&iterator, player_data);
		while (led && (player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (!in_game(player) || iterator.datum_index == killing_player_index)
				continue;
			if (teams_play() ? player->team_index == player_get(dead_player_index)->team_index &&
				player->team_index != player_get(killing_player_index)->team_index
				: iterator.datum_index == dead_player_index)
				continue;
			if (game_engine->get_player_score(iterator.datum_index, type) >= victim_score)
				led = FALSE;
		}
		if (killer_slot >= 0)
			st.spree[killer_slot]++;
		bonus = killer_slot >= 0 ? ae_h2_bonus_points(led, (int)st.spree[killer_slot]) : 0;
		if (bonus)
			game_engine_slayer_adjust_score(killing_player_index, bonus);
	}
	if (ext->slayer_flags & AE_EXT_SLAYER_DEATH_POINT_LOSS)
		game_engine_slayer_adjust_score(dead_player_index, -1);
	if (suicide && (ext->slayer_flags & AE_EXT_SLAYER_SUICIDE_POINT_LOSS))
		game_engine_slayer_adjust_score(dead_player_index, -1);
}

boolean ae_h2_match_powerup_banned(short powerup_type)
{
	struct ae_variant_ext const *ext;

	if (!game_engine)
		return FALSE;
	ext = ae_variant_ext_game();
	if (powerup_type == _equipment_powerup_overshield)
		return (ext->player_flags & AE_EXT_PLAYER_NO_OVERSHIELDS_ON_MAP) != 0;
	if (powerup_type == _equipment_powerup_active_camouflage)
		return (ext->player_flags & AE_EXT_PLAYER_NO_CAMO_ON_MAP) != 0;
	return FALSE;
}

long ae_h2_match_item_respawn_period(long period)
{
	long ticks;

	if (!game_engine)
		return period;
	ticks = ae_ext_weapon_respawn_ticks(ae_variant_ext_game(), period);
	/* none: the item the game starts with is the only one (the spawn is at game time zero, a multiple of any period) */
	return ticks == NONE ? NEVER_RESPAWNS_TICKS : (ticks < 1 ? 1 : ticks);
}

boolean ae_h2_match_items_reset_now(void)
{
	return st.force_items;
}

/* the deaths a player brought into the current round: a rounds game counts lives within the round
(game_engine_player_is_out_of_lives, the strip's lives left); 0 in a one-round game or for a slotless player */
long ae_h2_match_round_deaths_base(
	long player_index)
{
	int slot;

	if (ae_ext_rounds_count(ae_variant_ext_game()) <= 1)
		return 0;
	slot = player_slot(player_index);
	return slot >= 0 ? st.deaths_base[slot] : 0;
}
