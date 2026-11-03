/*
AE_STRIP_ENGINE.C

The score strip's engine reader (ae_strip_engine.h). Plan
notes/plans/2026-10-12-score-strip.md Task 4; spec 11.6.

CLIENT RULE: every value comes from what a joined machine has as the host's:
the variant (sent at the game's start), game time, the players (names, teams,
units, statistics) and objects (the flags' places and holders), and the game
types' network state (game_engine_write_network_state: CTF's flags and scores,
oddball's scores and ball owners, king's scores, hill and move timer, race's
globals, slayer's scores and kill-in-order targets). Two values are worked out
here instead of read, as the client lacks the host's own: king's hill state
(the host's is not sent: here from who stands in the hill's hull, as the host
counts it) and a dropped CTF flag's return time on a client (the host's from
the flag's last owned time; a client's from when it saw the flag dropped).
*/

#include <stdlib.h>
#include <string.h>

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "items/items.h"
#include "items/weapons.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"

#include "ae_bomb_assault.h"
#include "ae_panel_engine.h"
#include "ae_power_list.h"
#include "ae_score_strip.h"
#include "ae_strip_engine.h"

void platform_log(char const *format, ...);
int config_boolean(const char *name);
char const *config_string(const char *name);
boolean network_game_distributed_client(void);

/* game_engine.c's ranking (populate_statistic_buffer, _postgame_statistic_ranking): the same layout */
struct ae_strip_statistic { long player_index, score, custom, kills, deaths, assists, place; };
typedef char verify_ae_strip_statistic_size[sizeof(struct ae_strip_statistic) == 7 * sizeof(long) ? 1 : -1];
long populate_statistic_buffer(struct ae_strip_statistic *statistic_buffer, long statistic, boolean inverse);
#define AE_STRIP_RANKING 0
#define AE_STRIP_PLACE_TIED 0x80000000UL
#define AE_STRIP_MAXIMUM_PLAYERS 128 /* MULTIPLAYER_MAXIMUM_PLAYERS (halo_port_limits.h) */

/* oddball's ball types (game_engine_oddball.c) */
enum { AE_ODDBALL_NORMAL = 0, AE_ODDBALL_MAGIC, AE_ODDBALL_TERMINATOR };
/* race's types (game_engine_race.c) */
enum { AE_RACE_NORMAL = 0, AE_RACE_ANY_ORDER, AE_RACE_RALLY };
/* CTF: a flag returns home 17 seconds after it was last held (game_engine_ctf.c CTF_FLAG_RETURN_TIME) */
#define AE_STRIP_FLAG_RETURN_TICKS (17 * TICKS_PER_SECOND)
/* a flag this near its stand is home (world units) */
#define AE_STRIP_FLAG_HOME_DISTANCE 0.5f
#define AE_STRIP_METRES_PER_WORLD_UNIT 3.048f
#define AE_STRIP_LOG_TICKS (5 * TICKS_PER_SECOND)

/* ---------- players */

static struct player_datum *player_of(long player_index)
{
	return player_index == NONE ? NULL : player_try_and_get(player_index);
}

static void copy_name(wchar_t *to, struct player_datum const *player)
{
	long i;

	for (i = 0; i < AE_STRIP_NAME_LENGTH - 1 && player && i < (long)NUMBEROF(player->name) && player->name[i]; i++)
		to[i] = player->name[i];
	to[i] = 0;
}

/* the player whose unit this is, or NONE */
static long player_of_unit(long unit_index)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (unit_index == NONE)
		return NONE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index == unit_index)
			return iterator.datum_index;
	}
	return NONE;
}

static long player_score(long player_index, boolean team)
{
	return game_engine && game_engine->get_player_score ? game_engine->get_player_score(player_index, team) : 0;
}

/* ---------- the clock */

static void read_clock(struct ae_strip_snapshot *out)
{
	wchar_t text[16], elapsed[16];
	long now = game_time_get();

	/* (none where the corner clock has none: MATCH CLOCK off, the countdown, the game over; nor over Halo 1: NHE's
	countdown titles) */
	if (!game_engine_match_clock(text, NUMBEROF(text)) || cinematic_nhe_countdown_title_showing())
	{
		out->clock_hidden = TRUE;
		return;
	}
	if (game_engine_match_clock_counts_down())
	{
		long left = (long)game_variant_options_get()->time_limit * 60L * TICKS_PER_SECOND - now;

		out->clock_seconds = left <= 0 ? 0 : (left + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
		/* (MATCH CLOCK BOTH: the time played too, over it) */
		if (game_engine_match_clock_elapsed(elapsed, NUMBEROF(elapsed)))
		{
			out->played_shown = TRUE;
			out->played_seconds = now / TICKS_PER_SECOND;
		}
	}
	else
	{
		out->clock_counts_up = TRUE;
		out->clock_seconds = now / TICKS_PER_SECOND;
	}
}

/* ---------- sides */

/* team games: my team and the other's scores, players and alive */
static void read_teams(long me_index, struct player_datum const *me, boolean timed, struct ae_strip_snapshot *out)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long divisor = timed ? TICKS_PER_SECOND : 1;
	long other_team = me->team_index == 0 ? 1 : 0;

	out->me.team_index = me->team_index;
	out->other.team_index = other_team;
	out->me.score = game_engine_get_team_score(me->team_index) / divisor;
	out->other.score = game_engine_get_team_score(other_team) / divisor;
	out->i_lead = (boolean)(out->me.score > out->other.score);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct ae_strip_side_snapshot *side;

		if (player->quit_out_of_game)
			continue;
		side = player->team_index == me->team_index ? &out->me : player->team_index == other_team ? &out->other : NULL;
		if (!side)
			continue;
		side->players++;
		if (player->unit_index != NONE)
			side->alive++;
	}
	copy_name(out->me.name, me);
	(void)me_index;
}

/* the scoreboard's ranking, sorted once a game tick (the scores change only at ticks) for every view and every use of
it in a frame, and again a few times a second while the game's time stands (paused, the game over: its last tick's
scores may have come after the sort); *count gets how many players it has */
static struct ae_strip_statistic const *ranking_now(long *count)
{
	static struct ae_strip_statistic ranking[AE_STRIP_MAXIMUM_PLAYERS];
	static long ranked_at = NONE, ranked_count;
	static unsigned long sorted_at;
	long now = game_time_get();

	if (now != ranked_at || system_milliseconds() - sorted_at >= 200)
	{
		ranked_count = populate_statistic_buffer(ranking, AE_STRIP_RANKING, FALSE);
		ranked_at = now;
		sorted_at = system_milliseconds();
	}
	*count = ranked_count;
	return ranking;
}

/* free for all: me, and the leader (the second when I lead), by the scoreboard's ranking */
static void read_free_for_all(long me_index, struct player_datum const *me, boolean timed,
	struct ae_strip_snapshot *out)
{
	long count;
	struct ae_strip_statistic const *ranking = ranking_now(&count);
	long divisor = timed ? TICKS_PER_SECOND : 1;
	long mine = NONE, other = NONE, i;

	for (i = 0; i < count && i < AE_STRIP_MAXIMUM_PLAYERS; i++)
	{
		if (ranking[i].player_index == me_index)
			mine = i;
	}
	if (mine == 0)
		other = count > 1 ? 1 : NONE;
	else if (count > 0)
		other = 0;
	out->me.team_index = NONE;
	out->other.team_index = NONE;
	out->me.score = player_score(me_index, TRUE) / divisor;
	out->me.place = mine != NONE ? (long)(ranking[mine].place & ~AE_STRIP_PLACE_TIED) + 1 : 1;
	out->i_lead = (boolean)(mine == 0);
	copy_name(out->me.name, me);
	if (other != NONE)
	{
		struct player_datum *player = player_of(ranking[other].player_index);

		out->other.score = player_score(ranking[other].player_index, TRUE) / divisor;
		out->other.place = (long)(ranking[other].place & ~AE_STRIP_PLACE_TIED) + 1;
		copy_name(out->other.name, player);
		out->other.players = 1;
		out->other.alive = player && player->unit_index != NONE;
	}
	out->me.players = 1;
	out->me.alive = me->unit_index != NONE;
}

/* the other side's player of a FFA strip (the leader, or the second): its index */
static long other_ffa_player(long me_index)
{
	long count;
	struct ae_strip_statistic const *ranking = ranking_now(&count);

	if (count <= 0)
		return NONE;
	if (ranking[0].player_index == me_index)
		return count > 1 ? ranking[1].player_index : NONE;
	return ranking[0].player_index;
}

/* whose a player of the context line is: mine (me, or my team in team games) or the other side's */
static enum ae_strip_side side_of(long player_index, long me_index, struct player_datum const *me, boolean teams)
{
	struct player_datum *player = player_of(player_index);

	if (!player)
		return _ae_strip_side_none;
	if (player_index == me_index || (teams && player->team_index == me->team_index))
		return _ae_strip_side_me;
	return _ae_strip_side_other;
}

/* ---------- CTF */

/* a team's flag stand: its CTF netgame flag, or NULL */
static struct scenario_netgame_flag *flag_stand(long team_index)
{
	struct scenario *scenario = global_scenario_get();
	long i;

	if (!scenario)
		return NULL;
	for (i = 0; i < scenario->netgame_flags.count; i++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, i,
			struct scenario_netgame_flag);

		if (flag->type == _netgame_flag_ctf_flag && flag->team_index == team_index)
			return flag;
	}
	return NULL;
}

/* (a client: when each team's flag was first seen dropped, by game time) */
static long flag_dropped_since[2] = { NONE, NONE };

static void read_flag(long team_index, long me_index, struct player_datum const *me,
	struct ae_strip_side_snapshot *side)
{
	struct object_iterator iterator;
	struct weapon_datum *weapon;
	long flag_index = NONE;

	side->flag = _ae_flag_none;
	if (team_index != 0 && team_index != 1)
		return;
	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while ((weapon = (struct weapon_datum *)object_iterator_next(&iterator)) != NULL)
	{
		if (weapon_is_flag(iterator.index) && weapon->object.owner_team_index == team_index)
		{
			flag_index = iterator.index;
			break;
		}
	}
	if (flag_index == NONE)
	{
		flag_dropped_since[team_index] = NONE;
		return;
	}
	if (weapon->object.parent_object_index != NONE)
	{
		long carrier = player_of_unit(weapon->object.parent_object_index);

		side->flag = _ae_flag_carried;
		copy_name(side->flag_carrier, player_of(carrier));
		flag_dropped_since[team_index] = NONE;
	}
	else
	{
		struct scenario_netgame_flag *stand = flag_stand(team_index);
		real dx = stand ? weapon->object.position.x - stand->position.x : 0.0f;
		real dy = stand ? weapon->object.position.y - stand->position.y : 0.0f;
		real dz = stand ? weapon->object.position.z - stand->position.z : 0.0f;
		long now = game_time_get(), since;

		if (!stand || dx * dx + dy * dy + dz * dz <= AE_STRIP_FLAG_HOME_DISTANCE * AE_STRIP_FLAG_HOME_DISTANCE)
		{
			side->flag = _ae_flag_home;
			flag_dropped_since[team_index] = NONE;
			return;
		}
		side->flag = _ae_flag_dropped;
		if (flag_dropped_since[team_index] == NONE || flag_dropped_since[team_index] > now)
			flag_dropped_since[team_index] = now;
		since = network_game_distributed_client() ? flag_dropped_since[team_index] :
			weapon->item.last_owned_time;
		since = AE_STRIP_FLAG_RETURN_TICKS - (now - since);
		side->flag_return_seconds = since <= 0 ? 0 : (since + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
	}
	(void)me_index; (void)me;
}

/* ---------- the game types */

static void read_oddball(long me_index, struct player_datum const *me, struct game_variant const *variant,
	struct ae_strip_snapshot *out)
{
	long ball_type = variant->game_engine_variant.oddball.oddball_ball_type;
	long balls = variant->game_engine_variant.oddball.ball_spawn_count;
	struct data_iterator iterator;
	struct player_datum *player;

	out->mode = ball_type == AE_ODDBALL_TERMINATOR ? _ae_strip_juggernaut :
		ball_type == AE_ODDBALL_MAGIC ? _ae_strip_accumulation : _ae_strip_oddball;
	out->timed = (boolean)(ball_type != AE_ODDBALL_TERMINATOR);
	out->reverse_tag = (boolean)(ball_type == AE_ODDBALL_MAGIC && balls <= 1);
	out->score_to_win = variant->universal_variant.score_to_win * (out->timed ? 60 : 1);
	if (out->teams)
		read_teams(me_index, me, out->timed, out);
	else
		read_free_for_all(me_index, me, out->timed, out);
	out->own_score = player_score(me_index, FALSE) / (out->timed ? TICKS_PER_SECOND : 1);
	out->balls_held = ae_strip_oddball_ball_count(me_index);
	out->balls_total = balls;
	/* (the holder: the first player with a ball) */
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (ae_strip_oddball_ball_count(iterator.datum_index) > 0)
		{
			copy_name(out->context_name, player);
			out->context_side = side_of(iterator.datum_index, me_index, me, out->teams);
			break;
		}
	}
}

static void read_king(long me_index, struct player_datum const *me, struct game_variant const *variant,
	struct ae_strip_snapshot *out)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long mine = 0, others = 0;

	out->mode = _ae_strip_king;
	out->timed = TRUE;
	out->score_to_win = variant->universal_variant.score_to_win * 60;
	if (out->teams)
		read_teams(me_index, me, TRUE, out);
	else
		read_free_for_all(me_index, me, TRUE, out);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index == NONE || !ae_strip_king_player_on_hill(iterator.datum_index))
			continue;
		if (side_of(iterator.datum_index, me_index, me, out->teams) == _ae_strip_side_me)
			mine++;
		else
			others++;
	}
	if (!mine && !others)
		out->hill = _ae_hill_empty;
	else if (out->teams ? (mine && others) : (mine + others > 1))
		out->hill = _ae_hill_contested;
	else
		out->hill = mine ? _ae_hill_yours : _ae_hill_theirs;
	if (variant->game_engine_variant.king.moving_hill)
	{
		long ticks = ae_strip_king_hill_timer();

		out->hill_move_seconds = ticks <= 0 ? 0 : (ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
	}
}

/* race and rally: my place, my lap (or points), the leader's (the second's when I lead) */
static void read_race(long me_index, struct player_datum const *me, struct game_variant const *variant,
	struct ae_strip_snapshot *out)
{
	long race_type = variant->game_engine_variant.race.race_type;
	long laps = variant->universal_variant.score_to_win;
	long other_index;
	struct player_datum *other;

	out->mode = race_type == AE_RACE_RALLY ? _ae_strip_rally : _ae_strip_race;
	out->score_to_win = laps;
	out->laps = laps;
	read_free_for_all(me_index, me, FALSE, out);
	if (out->teams)
	{
		out->me.team_index = me->team_index;
		out->other.team_index = me->team_index == 0 ? 1 : 0;
	}
	other_index = other_ffa_player(me_index);
	other = player_of(other_index);
	if (out->mode == _ae_strip_race)
	{
		/* (the lap each is on: laps done + 1, never past the race's) */
		long mine = me->statistics.multiplayer_statistics.race_statistics.laps + 1;
		long theirs = other ? other->statistics.multiplayer_statistics.race_statistics.laps + 1 : 0;

		out->me.score = laps > 0 && mine > laps ? laps : mine;
		out->other.score = laps > 0 && theirs > laps ? laps : theirs;
		out->checkpoints = ae_strip_race_flag_count();
		out->checkpoint = ae_strip_race_flags_touched(me_index) + 1;
		if (out->checkpoints > 0 && out->checkpoint > out->checkpoints)
			out->checkpoint = out->checkpoints;
	}
	else
	{
		/* rally: a point a flag touched; the next flag's distance */
		long rally_flag = ae_strip_race_rally_flag();
		struct scenario *scenario = global_scenario_get();
		struct unit_datum *unit = me->unit_index != NONE ? unit_get(me->unit_index) : NULL;
		long i;

		out->me.score = me->statistics.multiplayer_statistics.race_statistics.laps;
		out->other.score = other ? other->statistics.multiplayer_statistics.race_statistics.laps : 0;
		out->next_flag_meters = -1;
		for (i = 0; unit && scenario && rally_flag != NONE && i < scenario->netgame_flags.count; i++)
		{
			struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, i,
				struct scenario_netgame_flag);

			if (flag->type == _netgame_flag_race_track && flag->team_index == rally_flag)
			{
				real dx = flag->position.x - unit->object.position.x;
				real dy = flag->position.y - unit->object.position.y;
				real dz = flag->position.z - unit->object.position.z;

				out->next_flag_meters = (long)(square_root(dx * dx + dy * dy + dz * dz) *
					AE_STRIP_METRES_PER_WORLD_UNIT + 0.5f);
				break;
			}
		}
	}
}

static void read_slayer(long me_index, struct player_datum const *me, struct game_variant const *variant,
	struct ae_strip_snapshot *out)
{
	long lives = variant->universal_variant.lives;

	out->mode = variant->game_engine_variant.slayer.kill_in_order ? _ae_strip_phantoms :
		lives > 0 ? _ae_strip_elimination : _ae_strip_slayer;
	out->score_to_win = variant->universal_variant.score_to_win;
	if (out->teams)
		read_teams(me_index, me, FALSE, out);
	else
		read_free_for_all(me_index, me, FALSE, out);
	if (out->mode == _ae_strip_phantoms)
	{
		struct player_datum *target = player_of(me->multiplayer_special);

		if (target)
		{
			copy_name(out->context_name, target);
			out->context_side = side_of(me->multiplayer_special, me_index, me, out->teams);
		}
	}
	else if (out->mode == _ae_strip_elimination)
	{
		struct data_iterator iterator;
		struct player_datum *player;

		out->lives_left = lives - me->statistics.deaths;
		if (out->lives_left < 0)
			out->lives_left = 0;
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (player->quit_out_of_game)
				continue;
			out->players_total++;
			if (player->statistics.deaths < lives)
				out->players_left++;
		}
	}
}

static void read_ctf(long me_index, struct player_datum const *me, struct game_variant const *variant,
	struct ae_strip_snapshot *out)
{
	out->mode = _ae_strip_ctf;
	out->score_to_win = variant->universal_variant.score_to_win;
	read_teams(me_index, me, FALSE, out);
	read_flag(out->me.team_index, me_index, me, &out->me);
	read_flag(out->other.team_index, me_index, me, &out->other);
}

/* Bomb (Chupa's engine 6: AE BOMB, SEARCH AND DESTROY, Grifball), in round mode (spec M_11): rounds won as the
scores and the pips (to the score to win), a cell a player lit while alive; the clock the round's (SEARCH AND
DESTROY's time left before a plant, else Chupa's round clock, else the match clock), the planted bomb's fuse in its
place (to the blast: its fuse, then the goal's countdown); over it the viewer's job (AE's FLAG SPAWNS) or "BOMB
PLANTED". Every machine's: Chupa's state (scores, round, round clock, the goal's countdown), the units (alive), and
the host's spare bytes (the fuse, SEARCH AND DESTROY: ae_bomb_assault_read_state) */
static void read_bomb(long me_index, struct player_datum const *me, struct ae_strip_snapshot *out)
{
	struct ae_bomb_state bomb;
	struct data_iterator iterator;
	struct player_datum *player;
	long attackers = ae_bomb_assault_attacking_team();
	long fuse = ae_bomb_assault_fuse_ticks(), ticks = NONE;

	ae_bomb_state_get(&bomb);
	out->mode = _ae_strip_rounds;
	out->teams = TRUE;
	out->me.team_index = me->team_index == 1 ? 1 : 0;
	out->other.team_index = 1 - out->me.team_index;
	out->me.rounds_won = out->me.score = bomb.scores[out->me.team_index];
	out->other.rounds_won = out->other.score = bomb.scores[out->other.team_index];
	out->i_lead = (boolean)(out->me.score > out->other.score);
	out->rounds_to_win = bomb.score_to_win > 0 ? bomb.score_to_win : 0;
	copy_name(out->me.name, me);
	/* (alive: a unit not dead; a dead SEARCH AND DESTROY player's cell stays dark till the next round) */
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct ae_strip_side_snapshot *side;
		struct object_datum *unit;

		if (player->quit_out_of_game || !VALID_INDEX(player->team_index, 2))
			continue;
		side = player->team_index == out->me.team_index ? &out->me : &out->other;
		side->players++;
		unit = player->unit_index != NONE ? object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit) :
			NULL;
		if (unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			side->alive++;
	}
	/* the clock: the bomb's, else the round's */
	if (fuse > 0)
		ticks = fuse + bomb.countdown_ticks;
	else if (bomb.hold_ticks > bomb.blast_pause_ticks)
		ticks = bomb.hold_ticks - bomb.blast_pause_ticks;
	else if (bomb.hold_ticks > 0)
	{
		/* (the blast's pause: the clock stays at 0, not the match clock) */
		out->clock_hidden = FALSE;
		out->clock_counts_up = FALSE;
		out->played_shown = FALSE;
		out->clock_seconds = 0;
	}
	out->bomb_planted = (boolean)(ticks != NONE);
	if (out->bomb_planted)
	{
		/* (the planters: the attackers, else the team whose goal it is not) */
		long planters = attackers != NONE ? attackers : bomb.plant_goal_team != NONE ? 1 - bomb.plant_goal_team : NONE;

		out->bomb_planted_side = planters == NONE ? _ae_strip_side_none :
			planters == out->me.team_index ? _ae_strip_side_me : _ae_strip_side_other;
	}
	else if (bomb.hold_ticks <= 0)
	{
		ticks = ae_bomb_snd_round_ticks_left();
		if (ticks == NONE)
			ticks = bomb.round_ticks;
	}
	if (ticks != NONE && ticks >= 0)
	{
		out->clock_hidden = FALSE;
		out->clock_counts_up = FALSE;
		out->played_shown = FALSE;
		out->clock_seconds = (ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
	}
	if (attackers != NONE)
		out->bomb_role = attackers == out->me.team_index ? _ae_bomb_role_attack : _ae_bomb_role_defend;
	/* (the attackers' warning: a defender on the planted bomb, the host's word on a client) */
	out->bomb_defusing = (boolean)(out->bomb_planted && out->bomb_planted_side == _ae_strip_side_me &&
		ae_bomb_assault_defuse_progress() > 0.0f);
	out->bomb_carrying = (boolean)(!out->bomb_planted && me && ae_bomb_snd_unit_carries(me->unit_index));
	(void)me_index;
}

boolean ae_strip_engine_snapshot(short local_player_index, struct ae_strip_snapshot *out)
{
	struct game_variant *variant;
	struct player_datum *me;
	long me_index;

	if (!out)
		return FALSE;
	memset(out, 0, sizeof(*out));
	out->me.team_index = out->other.team_index = NONE;
	out->hill_move_seconds = -1;
	out->next_flag_meters = -1;
	out->respawns_left = -1;
	if (!game_engine || !game_engine_running() || !game_engine->get_player_score)
		return FALSE;
	me_index = local_player_get_player_index(local_player_index);
	me = player_of(me_index);
	variant = game_engine_get_variant();
	if (!me || !variant)
		return FALSE;
	out->teams = game_engine_has_teams();
	read_clock(out);
	switch (variant->game_engine_index)
	{
	case game_engine_ctf: read_ctf(me_index, me, variant, out); break;
	case game_engine_slayer: read_slayer(me_index, me, variant, out); break;
	case game_engine_oddball: read_oddball(me_index, me, variant, out); break;
	case game_engine_king: read_king(me_index, me, variant, out); break;
	case game_engine_race: read_race(me_index, me, variant, out); break;
	case game_engine_bomb: read_bomb(me_index, me, out); break;
	default: return FALSE;
	}
	return TRUE;
}

/* (a player's: any player's, for the debug log) */
static boolean bomb_progress_of(long me_index, real *fraction, boolean *defusing)
{
	struct ae_bomb_state bomb;
	struct game_variant *variant;
	struct player_datum *me;
	struct object_datum *unit;
	long attackers, defenders;
	real_point3d position;

	*fraction = 0.0f;
	*defusing = FALSE;
	if (!game_engine || !game_engine_running())
		return FALSE;
	variant = game_engine_get_variant();
	me = player_of(me_index);
	if (!variant || variant->game_engine_index != game_engine_bomb || !me || me->unit_index == NONE)
		return FALSE;
	ae_bomb_state_get(&bomb);
	if (!bomb.running || bomb.weapon_index == NONE || bomb.hold_ticks > 0)
		return FALSE;
	/* (the carrier arming in the enemy goal) */
	if (bomb.carrier == me_index)
	{
		if (bomb.arm_ticks <= 0 || bomb.arm_ticks_needed <= 0)
			return FALSE;
		*fraction = (real)bomb.arm_ticks / (real)bomb.arm_ticks_needed;
		*fraction = *fraction > 1.0f ? 1.0f : *fraction;
		return TRUE;
	}
	/* (a living defender on the planted bomb) */
	if (ae_bomb_assault_fuse_ticks() <= 0)
		return FALSE;
	attackers = ae_bomb_assault_attacking_team();
	defenders = attackers != NONE ? 1 - attackers : bomb.plant_goal_team;
	unit = object_try_and_get_and_verify_type(me->unit_index, _object_mask_unit);
	if (defenders == NONE || me->team_index != defenders || !unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return FALSE;
	item_get_position_even_if_in_inventory(bomb.weapon_index, &position);
	{
		real dx = position.x - unit->object.position.x, dy = position.y - unit->object.position.y;
		real dz = position.z - unit->object.position.z;

		if (dx * dx + dy * dy + dz * dz >= bomb.save_radius * bomb.save_radius)
			return FALSE;
	}
	*fraction = ae_bomb_assault_defuse_progress();
	*defusing = TRUE;
	return TRUE;
}

/* (the tests' forced bar, view 0: debug.bomb_bar "planting:0.6", "defusing:0.35") */
static boolean bomb_bar_forced(real *fraction, boolean *defusing)
{
	static char forced[32];
	static int read = 0;
	char const *colon;

	if (!read)
	{
		char const *value = config_string("debug.bomb_bar");

		read = 1;
		strncpy(forced, value ? value : "", sizeof(forced) - 1);
		forced[sizeof(forced) - 1] = 0;
	}
	colon = strchr(forced, ':');
	if (!forced[0] || !colon)
		return FALSE;
	*defusing = (boolean)(strncmp(forced, "defusing", 8) == 0);
	*fraction = (real)atof(colon + 1);
	return TRUE;
}

boolean ae_strip_engine_bomb_progress(short local_player_index, real *fraction, boolean *defusing)
{
	if (local_player_index == 0 && game_engine && game_engine_running() && bomb_bar_forced(fraction, defusing))
		return TRUE;
	return bomb_progress_of(local_player_get_player_index(local_player_index), fraction, defusing);
}

/* (the debug log's: each player's PLANTING / DEFUSING bar as it starts and ends, with its furthest) */
static void bomb_progress_log(void)
{
	static struct { boolean on, defusing; real most; } bars[AE_STRIP_MAXIMUM_PLAYERS];
	struct data_iterator iterator;
	struct game_variant *variant = game_engine_get_variant();

	if (!variant || variant->game_engine_index != game_engine_bomb)
		return;
	data_iterator_new(&iterator, player_data);
	while (data_iterator_next(&iterator))
	{
		long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		real fraction;
		boolean defusing, on;

		if (absolute < 0 || absolute >= AE_STRIP_MAXIMUM_PLAYERS)
			continue;
		on = bomb_progress_of(iterator.datum_index, &fraction, &defusing);
		if (on && !bars[absolute].on)
		{
			platform_log("bomb bar: tick %d player %ld %s starts at %.2f", (int)game_time_get(), absolute,
				defusing ? "defusing" : "planting", fraction);
			bars[absolute].most = fraction;
			bars[absolute].defusing = defusing;
		}
		else if (!on && bars[absolute].on)
		{
			platform_log("bomb bar: tick %d player %ld %s ends, furthest %.2f", (int)game_time_get(), absolute,
				bars[absolute].defusing ? "defusing" : "planting", bars[absolute].most);
		}
		else if (on && fraction > bars[absolute].most)
			bars[absolute].most = fraction;
		bars[absolute].on = on;
	}
}

/* ---------- the debug log */

static char const *mode_name(enum ae_strip_mode mode)
{
	static char const *const names[] = { "slayer", "ctf", "oddball", "king", "race", "rally", "juggernaut",
		"accumulation", "elimination", "phantoms", "rounds" };

	return (long)mode >= 0 && (long)mode < (long)NUMBEROF(names) ? names[mode] : "?";
}

static char const *color_name(enum ae_strip_color color)
{
	static char const *const names[] = { "white", "grey", "red", "blue", "me", "clock" };

	return (long)color >= 0 && (long)color < (long)NUMBEROF(names) ? names[color] : "?";
}

/* a strip text as ASCII for the log (the game's wide strings are 16-bit: no %ls) */
static char const *ascii(struct ae_strip_text const *text, char *out, long count)
{
	long i;

	for (i = 0; i < count - 1 && i < AE_STRIP_TEXT_LENGTH && text->text[i]; i++)
		out[i] = text->text[i] >= 32 && text->text[i] < 127 ? (char)text->text[i] : '?';
	out[i] = 0;
	return out;
}

void ae_strip_engine_debug_log(void)
{
	static long last_logged = NONE;
	static int enabled = -1;
	long now;
	short local;

	/* (the AE panels' test controls, in the game's tick: seating a player draws the game's random numbers) */
	ae_panel_engine_debug_tick();

	/* (a client's dropped-flag times, whether or not a strip is drawn: a flag dropped while every local player is
	dead or watching starts its countdown when it drops, not when a strip first sees it) */
	if (game_engine && game_engine_running() && network_game_distributed_client())
	{
		struct game_variant *variant = game_engine_get_variant();

		if (variant && variant->game_engine_index == game_engine_ctf)
		{
			struct ae_strip_side_snapshot side;
			long team;

			for (team = 0; team < 2; team++)
			{
				memset(&side, 0, sizeof(side));
				read_flag(team, NONE, NULL, &side);
			}
		}
	}

	/* (read once: a test's setting) */
	if (enabled < 0)
		enabled = config_boolean("debug.score_strip_log") ? 1 : 0;
	if (!enabled || !game_engine || !game_engine_running())
		return;
	bomb_progress_log();
	now = game_time_get();
	if (last_logged != NONE && now >= last_logged && now - last_logged < AE_STRIP_LOG_TICKS)
		return;
	last_logged = now;
	for (local = 0; local < MAXIMUM_LOCAL_PLAYERS; local++)
	{
		struct ae_strip_snapshot snapshot;
		struct ae_strip_model model;
		char left[48], right[48], clock[48], goal[48], lead[48], context[64], left_sub[64], right_sub[64], played[48];
		real played_box[4];

		real alpha = 0.0f, clock_box[4] = { 0 }, score_box[4] = { 0 };
		real list_foot = 0.0f, list_left = 0.0f;
		boolean drawn, list;

		if (!ae_strip_engine_snapshot(local, &snapshot))
			continue;
		ae_strip_model_build(&snapshot, &model);
		drawn = ae_score_strip_last_drawn(local, &alpha, clock_box, score_box);
		if (!drawn || !ae_score_strip_last_played(local, played_box))
			memset(played_box, 0, sizeof(played_box));
		list = ae_power_list_last(local, &list_foot, &list_left);
		platform_log("score strip: tick %d view %d mode=%s teams=%d left=%s(%s) right=%s(%s) clock=%s up=%d goal=\"%s\" "
			"lead=\"%s\" context=\"%s\" left_sub=\"%s\" right_sub=\"%s\" bars=%d fill=%.2f/%.2f place=%d lead_me=%d "
			"drawn=%d alpha=%.2f clock_box=%.0f,%.0f,%.0f,%.0f score_box=%.0f,%.0f,%.0f,%.0f list=%d list_foot=%.1f "
			"list_left=%.1f line=%.1f corner_clock=%d played=\"%s\" played_box=%.0f,%.0f,%.0f,%.0f",
			(int)now, (int)local, mode_name(snapshot.mode), (int)snapshot.teams,
			ascii(&model.left_score, left, sizeof(left)), color_name(model.left_score.color),
			ascii(&model.right_score, right, sizeof(right)), color_name(model.right_score.color),
			model.clock.text[0] ? ascii(&model.clock, clock, sizeof(clock)) : "-", (int)snapshot.clock_counts_up,
			ascii(&model.goal, goal, sizeof(goal)), ascii(&model.lead, lead, sizeof(lead)),
			ascii(&model.context, context, sizeof(context)), ascii(&model.left_sub, left_sub, sizeof(left_sub)),
			ascii(&model.right_sub, right_sub, sizeof(right_sub)), (int)model.bars, model.left_fill,
			model.right_fill, (int)snapshot.me.place, (int)snapshot.i_lead, (int)drawn, alpha, clock_box[0],
			clock_box[1], clock_box[2], clock_box[3], score_box[0], score_box[1], score_box[2], score_box[3], (int)list,
			list_foot, list_left, drawn ? ae_score_strip_last_line(local) : 0.0f, (int)!ae_power_list_hides_clock(local),
			ascii(&model.played, played, sizeof(played)), played_box[0], played_box[1], played_box[2], played_box[3]);
	}
}
