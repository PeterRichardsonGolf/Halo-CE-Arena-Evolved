/*
AE_ROUND.C

The round modes' host machine (ae_round.h; the rules: ae_round_rules.h). Each tick of a running game the host follows
every player (alive, down, out, waiting), judges the round, counts the wins into the Slayer scores, moves the round
through its phases, runs the revives and the zone (ae_round_zone.c), and sends the state (ae_round_net.c). A round's
reset is ae_h2_match.c's (everyone killed with no death counted, the engine's scores and the map's items back). The
engine's own lives count within the round (the deaths a player brought into it), so one life a round is the engine's
lives of 1; the spawn hold keeps a late joiner, and everyone once the round is over, from spawning.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "game/players_runtime.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "units/units.h"

#include "ae_dev_events.h"
#include "ae_h2_match.h"
#include "bots.h"
#include "ae_round_internal.h"
#include "ae_variant_ext.h"
#ifdef HALO_PORT_CHAT
#include "chat.h"
#endif

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);
void ae_dev_slayer_score_set(long slot, boolean team, long score);
boolean ae_silent_rounds_refuse_non_ae(void);

struct ae_round_host ae_round_host;

/* ---------- small things */

void ae_round_log(char const *format, ...)
{
	char text[240];
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(text, sizeof(text), format, arguments);
	va_end(arguments);
	error(_error_log, "elimination: %s", text);
}

static void say(char const *text)
{
	ae_round_log("%s", text);
#ifdef HALO_PORT_CHAT
	if (game_connection() == _game_connection_network_server)
		chat_server_say(text);
#endif
}

long ae_round_random(long count)
{
	if (count <= 1)
		return 0;
	ae_round_host.random = ae_round_host.random * 1103515245UL + 12345UL;
	return (long)((ae_round_host.random >> 16) % (unsigned long)count);
}

boolean ae_round_on(void)
{
	struct game_variant *variant;

	if (!game_engine || !game_engine_running())
		return FALSE;
	variant = game_engine_get_variant();
	return variant->game_engine_index == game_engine_slayer &&
		ae_variant_ext_game()->mode == AE_EXT_MODE_ELIMINATION;
}

static boolean host_side(void)
{
	return ae_round_on() && !network_game_distributed_client();
}

struct ae_round_slot *ae_round_slot_of(long player_index)
{
	long slot;

	if (player_index == NONE)
		return NULL;
	slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	return slot >= 0 && slot < AE_ROUND_SLOTS ? &ae_round_host.slots[slot] : NULL;
}

boolean ae_round_zone_killed(long player_index)
{
	struct ae_round_slot const *slot = ae_round_on() ? ae_round_slot_of(player_index) : NULL;

	return (boolean)(slot && slot->zone_death);
}

long ae_round_living_unit(long player_index)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	struct unit_datum *unit;

	if (!player || player->unit_index == NONE)
		return NONE;
	unit = unit_try_and_get(player->unit_index);
	/* (a unit told to die, by a dev command or a round's reset, is dead already) */
	return unit && !(unit->object.damage_flags & (FLAG(_object_dead_bit) | FLAG(_object_die_act_of_god_bit) |
		FLAG(_object_die_act_of_god_silent_bit) | FLAG(_object_die_act_of_god_no_statistics_bit))) ?
		player->unit_index : NONE;
}

static boolean in_game(struct player_datum const *player)
{
	return !player->quit_out_of_game;
}

/* a player's side: their team, or (no teams) their slot */
static long side_of(long player_index, struct player_datum const *player)
{
	if (ae_round_host.teams)
		return player->team_index >= 0 && player->team_index < AE_ROUND_SIDES ? player->team_index : NONE;
	return DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) < AE_ROUND_SIDES ? DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) :
		NONE;
}

static char const *side_name(long side)
{
	static char text[32];

	if (side == AE_ROUND_RESULT_TIE)
		return "tie";
	if (ae_round_host.teams)
		return side == _team_red ? "red" : side == _team_blue ? "blue" : "a team";
	snprintf(text, sizeof(text), "player %ld", side);
	return text;
}

static void name_of(long player_index, char *out, int size)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	int index;

	out[0] = 0;
	if (!player || size <= 0)
		return;
	for (index = 0; index < 11 && index < size - 1 && player->name[index]; index++)
		out[index] = player->name[index] < 128 && player->name[index] >= 32 ? (char)player->name[index] : '?';
	out[index] = 0;
}

static long sides_with_players(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	boolean seen[AE_ROUND_SIDES];
	long count = 0;

	memset(seen, 0, sizeof(seen));
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long side = in_game(player) ? side_of(iterator.datum_index, player) : NONE;

		if (side != NONE && !seen[side])
		{
			seen[side] = TRUE;
			count++;
		}
	}
	return count;
}

/* the leading two sides' wins */
static void leaders(long *best, long *runner_up)
{
	long index;

	*best = *runner_up = 0;
	for (index = 0; index < AE_ROUND_SIDES; index++)
	{
		long wins = ae_round_host.teams ? ae_round_host.team_wins[index] : ae_round_host.slots[index].wins;

		if (wins > *best)
			*runner_up = *best, *best = wins;
		else if (wins > *runner_up)
			*runner_up = wins;
	}
}

/* the Slayer scores are the round wins (a team's, or a player's without teams) */
static void scores_apply(void)
{
	long index;

	for (index = 0; index < AE_ROUND_SIDES; index++)
	{
		if (ae_round_host.teams)
			ae_dev_slayer_score_set(index, TRUE, ae_round_host.team_wins[index]);
		else
			ae_dev_slayer_score_set(index, FALSE, ae_round_host.slots[index].wins);
	}
}

/* ---------- the machine */

static void slot_clear(struct ae_round_slot *slot, long player_index, struct player_datum const *player, boolean in_round)
{
	memset(slot, 0, sizeof(*slot));
	slot->player_index = player_index;
	slot->in_round = in_round;
	slot->state = in_round ? AE_ROUND_PLAYER_NONE : AE_ROUND_PLAYER_WAITING;
	slot->deaths_seen = player ? player->statistics.deaths : 0;
	slot->deaths_base = slot->deaths_seen;
	slot->reviver = NONE;
}

/* every player present takes part in the round that begins (a player's own wins kept, without teams) */
static void slots_round_begins(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	boolean present[AE_ROUND_SLOTS];
	long index;

	memset(present, 0, sizeof(present));
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct ae_round_slot *slot = ae_round_slot_of(iterator.datum_index);
		long wins;

		if (!slot)
			continue;
		wins = slot->player_index == iterator.datum_index ? slot->wins : 0;
		slot_clear(slot, iterator.datum_index, player, in_game(player));
		slot->wins = wins;
		present[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] = TRUE;
	}
	for (index = 0; index < AE_ROUND_SLOTS; index++)
	{
		if (!present[index])
			slot_clear(&ae_round_host.slots[index], NONE, NULL, TRUE);
	}
}

static void round_begins(long now)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();

	ae_round_host.phase = AE_ROUND_PHASE_PRE;
	ae_round_host.round_start = now;
	ae_round_host.live_start = NONE;
	ae_round_host.post_end = NONE;
	ae_round_host.switched = ae_round_sides_switched(ae_round_host.round, ae_round_host.max_rounds,
		ae_round_host.to_win, ae_round_switch_on(ext, ae_round_host.teams));
	slots_round_begins();
	ae_round_zone_new_round();
	scores_apply();
	ae_round_log("round %d of %d starts%s (wins %s)", ae_round_host.round, ae_round_host.max_rounds,
		ae_round_host.switched ? ", sides switched" : "", ae_round_host.teams ? "by team" : "by player");
	/* (the AE-only rule, as the host's join check has it for a machine without Arena Evolved: each round, so a TEST RUN
	window holds it) */
	ae_round_log("AE players only: a machine without Arena Evolved %s (code 68)",
		ae_silent_rounds_refuse_non_ae() ? "may not join" : "MAY join");
	if (ae_dev_events_on())
		ae_dev_event("round_start", "\"round\":%d,\"switched\":%s", ae_round_host.round,
			ae_round_host.switched ? "true" : "false");
}

static void match_starts(long now)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	unsigned long seed = ae_round_host.random;

	memset(&ae_round_host, 0, sizeof(ae_round_host));
	ae_round_host.random = seed ^ (unsigned long)now ^ 0x5EED1234UL;
	ae_round_host.running = TRUE;
	ae_round_host.teams = game_engine_has_teams();
	ae_round_host.revive = ae_round_revive_on(ext, ae_round_host.teams);
	ae_round_host.zone = ae_round_zone_on(ext);
	ae_round_host.max_rounds = (short)ae_round_max_rounds(ext);
	ae_round_host.to_win = (short)ae_round_to_win(ext);
	ae_round_host.round = 1;
	ae_round_host.last_result = AE_ROUND_RESULT_NONE;
	ae_round_host.sent_at = NONE;
	{
		long index;

		for (index = 0; index < AE_ROUND_SLOTS; index++)
			ae_round_host.slots[index].player_index = NONE;
	}
	ae_round_log("the match starts: first to %d of %d rounds, %s, revive %s, danger zone %s, one life a round",
		ae_round_host.to_win, ae_round_host.max_rounds, ae_round_host.teams ? "teams" : "no teams",
		ae_round_host.revive ? "on" : "off", ae_round_host.zone ? "on" : "off");
	round_begins(now);
}

static void round_ends(long now, int verdict, long winner)
{
	char text[96];

	ae_round_host.phase = AE_ROUND_PHASE_POST;
	ae_round_host.post_end = now + AE_ROUND_POST_TICKS;
	ae_round_host.played++;
	if (verdict == AE_ROUND_WON && winner >= 0 && winner < AE_ROUND_SIDES)
	{
		if (ae_round_host.teams)
			ae_round_host.team_wins[winner]++;
		else
			ae_round_host.slots[winner].wins++;
		ae_round_host.last_result = (short)winner;
	}
	else
		ae_round_host.last_result = AE_ROUND_RESULT_TIE;
	scores_apply();
	if (ae_round_host.teams)
		snprintf(text, sizeof(text), "Round %d: %s (red %ld, blue %ld)", ae_round_host.round,
			verdict == AE_ROUND_WON ? (winner == _team_red ? "red wins" : winner == _team_blue ? "blue wins" :
			"a team wins") : "tied", ae_round_host.team_wins[_team_red], ae_round_host.team_wins[_team_blue]);
	else
		snprintf(text, sizeof(text), "Round %d: %s", ae_round_host.round,
			verdict == AE_ROUND_WON ? "won" : "tied");
	say(text);
	if (ae_dev_events_on())
		ae_dev_event("round_end", "\"round\":%d,\"winner\":%ld,\"red\":%ld,\"blue\":%ld", ae_round_host.round,
			verdict == AE_ROUND_WON ? winner : -1L, ae_round_host.team_wins[_team_red],
			ae_round_host.team_wins[_team_blue]);
}

static void match_over_or_next(long now)
{
	long best, runner_up;
	struct ae_variant_ext const *ext = ae_variant_ext_game();

	leaders(&best, &runner_up);
	if (ae_round_match_over(ae_round_host.max_rounds, ae_round_host.to_win,
		(ext->match_flags & AE_EXT_MATCH_RESOLVE_TIES) != 0, ae_round_host.played, (int)best, (int)runner_up))
	{
		ae_round_host.phase = AE_ROUND_PHASE_OVER;
		ae_round_log("the match is over after %d rounds (%ld to %ld)", ae_round_host.played, best, runner_up);
		if (ae_dev_events_on())
			ae_dev_event("match_over", "\"rounds\":%d,\"best\":%ld,\"runner_up\":%ld", ae_round_host.played, best,
				runner_up);
		ae_round_net_host_tick(now);
		game_engine_end_game_final();
		return;
	}
	ae_round_host.round++;
	ae_h2_match_reset_round();
	round_begins(now);
}

/* a player of the round who has not spawned in it yet, in LIVE's first ticks (the countdown ended early by a dev
command, or the spawn's own tick gate): may still spawn */
static boolean late_start(struct ae_round_slot const *slot, long now)
{
	return ae_round_host.phase == AE_ROUND_PHASE_LIVE && slot->in_round && slot->state == AE_ROUND_PLAYER_NONE &&
		now - ae_round_host.live_start < AE_ROUND_GRACE_TICKS;
}

/* a death in the round: down at the spot (revive on, the spot inside the zone, not the zone's death), else out */
static void goes_down(long player_index, struct ae_round_slot *slot, long now)
{
	char name[16];
	boolean revivable = ae_round_host.revive && !slot->zone_death && ae_round_zone_inside(&slot->last_position, now);
	real_point3d spot = slot->last_position;

	name_of(player_index, name, sizeof(name));
	if (revivable)
	{
		/* (the orb and the revive sit on the ground: a death in the air drops straight down to the floor; no floor in reach
		= nobody to revive there, out) */
		struct collision_result collision;
		real_point3d from = slot->last_position;
		real_vector3d down = { 0.0f, 0.0f, -(AE_ROUND_GROUND_ABOVE + AE_ROUND_GROUND_REACH) };
		float ground_z;
		boolean hit;

		from.z += AE_ROUND_GROUND_ABOVE;
		hit = collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) |
			FLAG(_collision_test_ignore_invisible_surfaces_bit) | FLAG(_collision_test_structure_bit), &from, &down, NONE,
			&collision) != 0;
		if (ae_round_ground_spot_rule(hit, hit ? collision.point.z : 0.0f, slot->last_position.z, &ground_z))
		{
			if (fabsf(ground_z - spot.z) > 0.05f)
				ae_round_log("%s's spot drops %.1f to the ground (%.1f %.1f %.1f)", name, spot.z - ground_z, spot.x,
					spot.y, ground_z);
			spot.z = ground_z;
		}
		else
		{
			revivable = FALSE;
			ae_round_log("%s has no ground under the death spot (%.1f %.1f %.1f): out", name, spot.x, spot.y, spot.z);
		}
	}
	slot->state = revivable ? AE_ROUND_PLAYER_DOWN : AE_ROUND_PLAYER_OUT;
	slot->spot = spot;
	slot->progress = 0;
	slot->reviver = NONE;
	slot->down_since = now;
	ae_round_log("%s %s at %.1f %.1f %.1f%s", name, revivable ? "is down" : "is out",
		slot->spot.x, slot->spot.y, slot->spot.z, slot->zone_death ? " (the zone)" : "");
	if (ae_dev_events_on())
		ae_dev_event(revivable ? "down" : "out", "\"player\":%ld,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"zone\":%s",
			DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), slot->spot.x, slot->spot.y, slot->spot.z,
			slot->zone_death ? "true" : "false");
}

/* a dev command's down: beside the host's player first (here), then dead with no killer, down at once (a revive
command after it finds them; the death the game counts later is this one) */
static void down_now(long target, struct ae_round_slot *slot, long unit_index, boolean here)
{
	long me = local_player_get_player_index(0);

	if (here && target != me && ae_round_living_unit(me) != NONE)
	{
		real_point3d at;
		real_vector3d aim;
		real length;

		object_get_origin(ae_round_living_unit(me), &at);
		unit_get_aiming_vector(ae_round_living_unit(me), &aim);
		length = (real)sqrt(aim.i * aim.i + aim.j * aim.j);
		if (length > 0.01f)
		{
			at.x += aim.i / length * 1.5f;
			at.y += aim.j / length * 1.5f;
		}
		at.z += 0.3f;
		if (player_teleport(target, NONE, &at))
			slot->last_position = at;
	}
	unit_kill(unit_index);
	slot->deaths_seen = player_get(target)->statistics.deaths + 1;
	goes_down(target, slot, game_time_get());
}

/* each player's state followed: new players (a late joiner waits), deaths (down at their spot, or out), spawns */
static void players_follow(long now)
{
	struct data_iterator iterator;
	struct player_datum *player;
	boolean present[AE_ROUND_SLOTS];

	memset(present, 0, sizeof(present));
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct ae_round_slot *slot = ae_round_slot_of(iterator.datum_index);
		long unit_index;

		if (!slot)
			continue;
		present[DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index)] = TRUE;
		if (slot->player_index != iterator.datum_index)
		{
			boolean joins = ae_round_host.phase == AE_ROUND_PHASE_PRE;
			long wins = 0;

			slot_clear(slot, iterator.datum_index, player, joins);
			slot->wins = wins;
			if (!joins)
			{
				char name[16];

				name_of(iterator.datum_index, name, sizeof(name));
				ae_round_log("%s joined during round %d: waits for the next round", name, ae_round_host.round);
				if (ae_dev_events_on())
					ae_dev_event("round_wait", "\"player\":%ld,\"round\":%d",
						DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index), ae_round_host.round);
			}
		}
		if (!in_game(player))
			continue;
		unit_index = ae_round_living_unit(iterator.datum_index);
		if (unit_index != NONE)
		{
			object_get_origin(unit_index, &slot->last_position);
			if (slot->state != AE_ROUND_PLAYER_ALIVE)
			{
				/* (a spawn: the round's, a revive's, or a dev command's for a waiting player, who now plays) */
				slot->state = AE_ROUND_PLAYER_ALIVE;
				slot->in_round = TRUE;
				slot->progress = 0;
				slot->reviver = NONE;
				slot->zone_capped = FALSE;
				if (slot->dev_down && ae_round_host.phase == AE_ROUND_PHASE_LIVE)
					down_now(iterator.datum_index, slot, unit_index, slot->dev_down == 2);
				slot->dev_down = 0;
			}
		}
		/* (a death counted; one a dev command already put down is counted ahead) */
		if (player->statistics.deaths > slot->deaths_seen)
		{
			slot->deaths_seen = player->statistics.deaths;
			if (ae_round_host.phase == AE_ROUND_PHASE_LIVE && slot->in_round && slot->state != AE_ROUND_PLAYER_WAITING)
				goes_down(iterator.datum_index, slot, now);
			slot->zone_death = FALSE;
		}
		else if (unit_index == NONE && slot->state == AE_ROUND_PLAYER_ALIVE)
			/* (killed with no death counted: a round's or the dev reset's) */
			slot->state = AE_ROUND_PLAYER_NONE;
		/* the round's start: the dead come back at once, in the countdown; and the revived (a death the game counts
		after the revive sets its own wait) */
		if (unit_index == NONE && (late_start(slot, now) || slot->state == AE_ROUND_PLAYER_REVIVED ||
			(ae_round_host.phase == AE_ROUND_PHASE_PRE && slot->in_round)))
		{
			player->respawn_timer = MIN(player->respawn_timer, 1);
		}
	}
	{
		long index;

		for (index = 0; index < AE_ROUND_SLOTS; index++)
		{
			if (!present[index] && ae_round_host.slots[index].player_index != NONE)
				ae_round_host.slots[index].player_index = NONE;
		}
	}
}

/* whether a living teammate of the downed player stands in reach of their spot */
static boolean in_reach(long unit_index, real_point3d const *spot)
{
	real_point3d at;
	real dx, dy, dz;

	object_get_origin(unit_index, &at);
	dx = at.x - spot->x;
	dy = at.y - spot->y;
	dz = at.z - spot->z;
	return dx * dx + dy * dy <= AE_ROUND_REVIVE_REACH * AE_ROUND_REVIVE_REACH &&
		(real)fabs(dz) <= AE_ROUND_REVIVE_REACH_HEIGHT;
}

static boolean may_revive(long reviver_index, struct player_datum const *downed)
{
	struct player_datum *reviver = reviver_index != NONE ? player_try_and_get(reviver_index) : NULL;
	struct ae_round_slot *slot = ae_round_slot_of(reviver_index);

	return reviver && slot && in_game(reviver) && reviver->team_index == downed->team_index &&
		slot->state == AE_ROUND_PLAYER_ALIVE && slot->action_held && ae_round_living_unit(reviver_index) != NONE;
}

static void revive_now(long player_index, struct ae_round_slot *slot, long by)
{
	struct player_datum *player = player_get(player_index);
	char name[16], by_name[16];

	slot->revives++;
	slot->deaths_base++;
	slot->state = AE_ROUND_PLAYER_REVIVED;
	slot->zone_capped = FALSE;
	slot->progress = 0;
	slot->reviver = NONE;
	player->respawn_timer = 0;
	name_of(player_index, name, sizeof(name));
	name_of(by, by_name, sizeof(by_name));
	ae_round_log("%s revived by %s (revive %d this round)", name, by != NONE ? by_name : "the host", slot->revives);
	if (ae_dev_events_on())
		ae_dev_event("revived", "\"player\":%ld,\"by\":%ld,\"revives\":%d", DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index),
			by != NONE ? DATUM_INDEX_TO_ABSOLUTE_INDEX(by) : -1L, slot->revives);
}

/* each downed player's revive: a teammate in reach holding the action button fills it; letting go starts it over */
static void revives_update(void)
{
	long index;
	int tenths = ae_round_revive_tenths(ae_variant_ext_game());
	long bleed_ticks = (long)ae_round_bleedout_seconds(ae_variant_ext_game()) * AE_ROUND_TICKS_PER_SECOND;

	for (index = 0; index < AE_ROUND_SLOTS; index++)
	{
		struct ae_round_slot *slot = &ae_round_host.slots[index];
		struct player_datum *downed;
		long holder = NONE;

		if (slot->state != AE_ROUND_PLAYER_DOWN || slot->player_index == NONE ||
			!(downed = player_try_and_get(slot->player_index)) || !in_game(downed))
		{
			continue;
		}
		if (!ae_round_zone_inside(&slot->spot, game_time_get()))
		{
			/* (the danger zone closed over the marker: the zone takes them, as it takes a living player; out until the
			round ends) */
			char name[16];

			name_of(slot->player_index, name, sizeof(name));
			slot->state = AE_ROUND_PLAYER_OUT;
			slot->progress = 0;
			slot->reviver = NONE;
			slot->zone_death = TRUE;
			ae_round_log("%s is out: the zone closed over the marker", name);
			if (ae_dev_events_on())
				ae_dev_event("out", "\"player\":%ld,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"zone\":true",
					DATUM_INDEX_TO_ABSOLUTE_INDEX(slot->player_index), slot->spot.x, slot->spot.y, slot->spot.z);
			continue;
		}
		if (may_revive(slot->reviver, downed) && in_reach(ae_round_living_unit(slot->reviver), &slot->spot))
			holder = slot->reviver;
		else
		{
			long other;

			for (other = 0; other < AE_ROUND_SLOTS && holder == NONE; other++)
			{
				long candidate = ae_round_host.slots[other].player_index;

				if (candidate != NONE && candidate != slot->player_index && may_revive(candidate, downed) &&
					in_reach(ae_round_living_unit(candidate), &slot->spot))
				{
					holder = candidate;
				}
			}
		}
		if (bleed_ticks > 0 && game_time_get() - slot->down_since >= bleed_ticks)
		{
			/* (BLEEDOUT: the marker's time is up, whoever holds: out until the round ends) */
			char name[16];

			name_of(slot->player_index, name, sizeof(name));
			slot->state = AE_ROUND_PLAYER_OUT;
			slot->progress = 0;
			slot->reviver = NONE;
			ae_round_log("%s bleeds out", name);
			if (ae_dev_events_on())
				ae_dev_event("bleed_out", "\"player\":%ld", DATUM_INDEX_TO_ABSOLUTE_INDEX(slot->player_index));
			continue;
		}
		if (holder != slot->reviver)
			slot->progress = 0;
		slot->reviver = holder;
		slot->progress = ae_round_revive_step(slot->progress, holder != NONE);
		if (holder != NONE && slot->progress >= ae_round_revive_ticks(tenths, slot->revives))
			revive_now(slot->player_index, slot, holder);
	}
}

static void goes_live(long now)
{
	ae_round_host.phase = AE_ROUND_PHASE_LIVE;
	ae_round_host.live_start = now;
	ae_round_host.zone_start = now;
	ae_round_log("round %d is live", ae_round_host.round);
}

static boolean unit_standing(long player_index)
{
	struct player_datum *player = player_try_and_get(player_index);
	struct unit_datum *unit = player && player->unit_index != NONE ? unit_try_and_get(player->unit_index) : NULL;

	return unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit);
}

/* the wipe judge, on the players' states */
static void judge(long now)
{
	int members[AE_ROUND_SIDES], alive[AE_ROUND_SIDES];
	int verdict, winner = -1;
	struct data_iterator iterator;
	struct player_datum *player;
	long limit = ae_ext_round_time_seconds(ae_variant_ext_game()) * TICKS_PER_SECOND;

	memset(members, 0, sizeof(members));
	memset(alive, 0, sizeof(alive));
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct ae_round_slot *slot = ae_round_slot_of(iterator.datum_index);
		long side = side_of(iterator.datum_index, player);

		if (!slot || side == NONE || !in_game(player) || !slot->in_round || slot->state == AE_ROUND_PLAYER_WAITING)
			continue;
		members[side]++;
		/* (standing: a unit not dead yet, one told to die included until it does, so its death is counted as down or
		out before the round ends; or revived and about to spawn) */
		if (slot->state == AE_ROUND_PLAYER_REVIVED || unit_standing(iterator.datum_index))
			alive[side]++;
	}
	verdict = ae_round_judge(AE_ROUND_SIDES, members, alive, &winner);
	/* (a ROUND TIME LIMIT, for zone-less play: the time's end is a tied round) */
	if (verdict == AE_ROUND_GOES_ON && limit > 0 && now - ae_round_host.live_start >= limit)
		verdict = AE_ROUND_TIED;
	if (verdict != AE_ROUND_GOES_ON)
		round_ends(now, verdict, winner);
}

void ae_round_update(void)
{
	long now = game_time_get();

	if (!host_side())
	{
		ae_round_host.running = FALSE;
		return;
	}
	if (!ae_round_host.running || now < ae_round_host.last_time)
		match_starts(now);
	ae_round_host.last_time = now;
	players_follow(now);
	switch (ae_round_host.phase)
	{
	case AE_ROUND_PHASE_PRE:
		if (now - ae_round_host.round_start >= AE_ROUND_PRE_TICKS)
			goes_live(now);
		break;
	case AE_ROUND_PHASE_LIVE:
		ae_round_zone_tick(now);
		if (ae_round_host.revive)
			revives_update();
		if (now - ae_round_host.live_start >= AE_ROUND_GRACE_TICKS)
			judge(now);
		break;
	case AE_ROUND_PHASE_POST:
		if (now >= ae_round_host.post_end)
			match_over_or_next(now);
		break;
	default:
		break;
	}
	scores_apply();
	{
		long index;

		for (index = 0; index < AE_ROUND_SLOTS; index++)
			ae_round_host.slots[index].action_held = FALSE;
	}
	if (ae_round_host.phase != AE_ROUND_PHASE_OVER)
		ae_round_net_host_tick(now);
}

/* ---------- the engine's questions */

boolean ae_round_filter_end(boolean should_end)
{
	/* (the engine's end stands only when a side is left alone in the game: the others left) */
	if (sides_with_players() >= 2)
		return FALSE;
	return should_end;
}

boolean ae_round_end_game(boolean final_end)
{
	if (final_end || ae_round_host.phase == AE_ROUND_PHASE_OVER || sides_with_players() < 2)
		return FALSE;
	return TRUE;
}

boolean ae_round_holds_spawn(long player_index)
{
	struct ae_round_slot *slot;

	if (!host_side() || !ae_round_host.running)
		return FALSE;
	slot = ae_round_slot_of(player_index);
	if (!slot || slot->player_index != player_index)
		return ae_round_host.phase != AE_ROUND_PHASE_PRE;
	switch (ae_round_host.phase)
	{
	case AE_ROUND_PHASE_PRE:
		return !slot->in_round;
	case AE_ROUND_PHASE_LIVE:
		return slot->state != AE_ROUND_PLAYER_REVIVED && !late_start(slot, game_time_get());
	default:
		return TRUE;
	}
}

boolean ae_round_scores_own(void)
{
	return ae_round_on();
}

/* the starting locations of a team's start spawns (CTF's, as FORCE TEAM SPAWNS has them) */
static boolean team_spawn(struct player_starting_location const *location, long team_index)
{
	return location->team_index == team_index && match_game_type(game_engine_ctf, 4, location->game_types);
}

static boolean team_has_spawn(long team_index)
{
	short count = player_get_starting_location_count();
	short index;

	for (index = 0; index < count; index++)
	{
		if (team_spawn(player_get_starting_location(index), team_index))
			return TRUE;
	}
	return FALSE;
}

boolean ae_round_spawns_own(long player_index)
{
	struct player_datum *player;

	if (!host_side() || !game_engine_has_teams())
		return FALSE;
	player = player_get(player_index);
	return (player->team_index == _team_red || player->team_index == _team_blue) && team_has_spawn(_team_red) &&
		team_has_spawn(_team_blue);
}

long ae_round_choose_spawn(long player_index, short location_count)
{
	struct player_datum *player = player_get(player_index);
	long side = player->team_index;
	long best = NONE, inside_best = NONE;
	real best_room = -1.0f, inside_room = -1.0f;
	short index;
	long now = game_time_get();

	if (ae_round_host.switched)
		side = side == _team_red ? _team_blue : _team_red;
	for (index = 0; index < location_count; index++)
	{
		struct player_starting_location const *location = player_get_starting_location(index);
		struct data_iterator iterator;
		struct player_datum *other;
		real room = 1.0e6f;

		if (!team_spawn(location, side))
			continue;
		/* (the most room from the living: two spawning together not on one spot; a little chance between equals) */
		data_iterator_new(&iterator, player_data);
		while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			long unit_index = iterator.datum_index != player_index ? ae_round_living_unit(iterator.datum_index) : NONE;
			real_point3d at;
			real distance;

			if (unit_index == NONE)
				continue;
			object_get_origin(unit_index, &at);
			distance = (real)sqrt(distance_squared3d(&at, &location->position));
			if (distance < room)
				room = distance;
		}
		room += (real)ae_round_random(100) * 0.001f;
		if (room > best_room)
			best_room = room, best = index;
		/* (spawns outside the zone refused while it closes) */
		if (ae_round_zone_inside(&location->position, now) && room > inside_room)
			inside_room = room, inside_best = index;
	}
	return inside_best != NONE ? inside_best : best;
}

struct player_starting_location const *ae_round_revive_location(long player_index,
	struct player_starting_location *location)
{
	struct ae_round_slot *slot;

	if (!host_side())
		return NULL;
	slot = ae_round_slot_of(player_index);
	if (!slot || slot->player_index != player_index || slot->state != AE_ROUND_PLAYER_REVIVED)
		return NULL;
	memset(location, 0, sizeof(*location));
	location->position = slot->spot;
	location->position.z += 0.1f;
	location->team_index = player_get(player_index)->team_index;
	return location;
}

void ae_round_note_action(long player_index, boolean held)
{
	struct ae_round_slot *slot;

	if (!held || !host_side())
		return;
	slot = ae_round_slot_of(player_index);
	if (slot && slot->player_index == player_index)
		slot->action_held = TRUE;
}

boolean ae_round_pickup_blocked(long player_index)
{
	struct player_datum *player;
	long unit_index, index;

	if (!host_side() || !ae_round_host.revive || ae_round_host.phase != AE_ROUND_PHASE_LIVE)
		return FALSE;
	player = player_try_and_get(player_index);
	unit_index = ae_round_living_unit(player_index);
	if (!player || unit_index == NONE)
		return FALSE;
	for (index = 0; index < AE_ROUND_SLOTS; index++)
	{
		struct ae_round_slot *slot = &ae_round_host.slots[index];
		struct player_datum *downed = slot->player_index != NONE ? player_try_and_get(slot->player_index) : NULL;

		if (slot->state == AE_ROUND_PLAYER_DOWN && downed && downed->team_index == player->team_index &&
			in_reach(unit_index, &slot->spot))
		{
			return TRUE;
		}
	}
	return FALSE;
}

long ae_round_countdown_ticks_left(void)
{
	struct ae_round_wire const *view;

	if (!ae_round_on() || game_engine_game_over() || !(view = ae_round_view()) ||
		(view->phase != AE_ROUND_PHASE_PRE && view->phase != AE_ROUND_PHASE_LIVE))
	{
		return 0;
	}
	return ae_round_countdown_left(view->round_start, game_time_get());
}

boolean ae_round_countdown_covers(long time)
{
	struct ae_round_wire const *view;

	if (!ae_round_on() || !(view = ae_round_view()) || view->phase == AE_ROUND_PHASE_OFF)
		return FALSE;
	return time >= view->round_start && time - view->round_start < AE_ROUND_PRE_TICKS;
}

boolean ae_round_deaths_base(long player_index, long *base)
{
	struct ae_round_wire const *view;
	struct player_datum *player;
	long absolute;
	int index;

	if (!ae_round_on() || player_index == NONE || !(player = player_try_and_get(player_index)))
		return FALSE;
	if (!network_game_distributed_client())
	{
		struct ae_round_slot *slot = ae_round_slot_of(player_index);

		*base = slot && slot->player_index == player_index ? slot->deaths_base : player->statistics.deaths;
		return TRUE;
	}
	/* (a client: the host's word on who is out of the round, its lives spent) */
	*base = player->statistics.deaths;
	view = ae_round_view();
	absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	for (index = 0; view && index < view->count; index++)
	{
		if (view->players[index].player == absolute &&
			(view->players[index].state == AE_ROUND_PLAYER_DOWN || view->players[index].state == AE_ROUND_PLAYER_OUT))
		{
			*base = player->statistics.deaths - game_engine_get_variant()->universal_variant.lives;
		}
	}
	return TRUE;
}

boolean ae_round_holds_spawns(void)
{
	struct ae_round_wire const *view;

	return ae_round_on() && (view = ae_round_view()) != NULL &&
		(view->phase == AE_ROUND_PHASE_LIVE || view->phase == AE_ROUND_PHASE_POST);
}

/* ---------- bots */

boolean ae_round_bot_revive_spot_for(long player_index, real_point3d const *position, real_point3d *spot, boolean *human)
{
	struct player_datum *player;
	long index;
	real best = AE_ROUND_BOT_REVIVE_RANGE * AE_ROUND_BOT_REVIVE_RANGE;
	boolean found = FALSE;

	if (!host_side() || !ae_round_host.revive || ae_round_host.phase != AE_ROUND_PHASE_LIVE ||
		!(player = player_try_and_get(player_index)))
	{
		return FALSE;
	}
	for (index = 0; index < AE_ROUND_SLOTS; index++)
	{
		struct ae_round_slot *slot = &ae_round_host.slots[index];
		struct player_datum *downed = slot->player_index != NONE ? player_try_and_get(slot->player_index) : NULL;
		real distance;

		if (slot->state != AE_ROUND_PLAYER_DOWN || !downed || downed->team_index != player->team_index ||
			(slot->reviver != NONE && slot->reviver != player_index))
		{
			continue;
		}
		distance = distance_squared3d(position, &slot->spot);
		if (distance < best)
		{
			best = distance;
			*spot = slot->spot;
			if (human)
				*human = (boolean)(downed->local_player_index != NONE || !bots_machine_is_bot(downed->network_player_data.machine_index));
			found = TRUE;
		}
	}
	return found;
}

boolean ae_round_bot_revive_spot(long player_index, real_point3d const *position, real_point3d *spot)
{
	return ae_round_bot_revive_spot_for(player_index, position, spot, NULL);
}

/* ---------- dev commands */

/* the player a dev command names: me, mate (the nearest teammate, living or downed as asked), enemy (the nearest
living enemy), an absolute index, or the start of a name (any case) */
static long find_player(char const *who, boolean downed)
{
	long me = local_player_get_player_index(0);
	struct player_datum *mine = me != NONE ? player_try_and_get(me) : NULL;
	struct data_iterator iterator;
	struct player_datum *player;
	long best = NONE;
	real best_distance = 1.0e12f;
	real_point3d here;
	boolean have_here = FALSE;
	boolean mate = !strcmp(who, "mate"), enemy = !strcmp(who, "enemy");

	if (!strcmp(who, "me"))
		return me;
	if (mine && ae_round_living_unit(me) != NONE)
	{
		object_get_origin(ae_round_living_unit(me), &here);
		have_here = TRUE;
	}
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct ae_round_slot *slot = ae_round_slot_of(iterator.datum_index);
		char name[16];
		size_t length = strlen(who), letter;
		boolean named;

		if (!in_game(player) || !slot)
			continue;
		if (mate || enemy)
		{
			real_point3d at;
			real distance;

			if (iterator.datum_index == me || !mine || (mate ? player->team_index != mine->team_index :
				(game_engine_has_teams() && player->team_index == mine->team_index)))
			{
				continue;
			}
			/* (living ones; or downed ones; one about to stand up, revived or not spawned yet this round, counts as
			living, at their spot or the map's middle) */
			if (downed ? slot->state != AE_ROUND_PLAYER_DOWN : ae_round_living_unit(iterator.datum_index) == NONE &&
				slot->state != AE_ROUND_PLAYER_REVIVED && !(slot->in_round && slot->state == AE_ROUND_PLAYER_NONE))
			{
				continue;
			}
			at = slot->spot;
			if (!downed && ae_round_living_unit(iterator.datum_index) != NONE)
				object_get_origin(ae_round_living_unit(iterator.datum_index), &at);
			distance = have_here ? distance_squared3d(&here, &at) : 0.0f;
			if (distance < best_distance)
				best_distance = distance, best = iterator.datum_index;
			continue;
		}
		if (who[0] >= '0' && who[0] <= '9')
		{
			if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == (long)atol(who))
				return iterator.datum_index;
			continue;
		}
		name_of(iterator.datum_index, name, sizeof(name));
		named = length > 0 && length <= strlen(name);
		for (letter = 0; named && letter < length; letter++)
		{
			char a = name[letter], b = who[letter];

			if (a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');
			if (b >= 'A' && b <= 'Z')
				b = (char)(b - 'A' + 'a');
			named = a == b;
		}
		if (named)
			return iterator.datum_index;
	}
	return best;
}

static boolean dev_ready(char *answer, int size)
{
	if (!host_side() || !ae_round_host.running)
	{
		snprintf(answer, size, "no round mode is running (an ELIMINATION gametype)");
		return FALSE;
	}
	return TRUE;
}

/* (the side standing with the most players, for a round ended by hand: a tie when level) */
static void end_by_hand(long now)
{
	int alive[AE_ROUND_SIDES];
	long side, best = NONE;
	int best_count = 0, level = 0;
	struct data_iterator iterator;
	struct player_datum *player;

	memset(alive, 0, sizeof(alive));
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		side = side_of(iterator.datum_index, player);
		if (side != NONE && in_game(player) && ae_round_living_unit(iterator.datum_index) != NONE)
			alive[side]++;
	}
	for (side = 0; side < AE_ROUND_SIDES; side++)
	{
		if (alive[side] > best_count)
			best_count = alive[side], best = side, level = 0;
		else if (alive[side] == best_count && best_count > 0)
			level = 1;
	}
	round_ends(now, best != NONE && !level ? AE_ROUND_WON : AE_ROUND_TIED, level ? -1 : best);
}

boolean ae_round_dev_round(char const *what, char *answer, int size)
{
	long now = game_time_get();

	if (!dev_ready(answer, size))
		return FALSE;
	if (ae_round_host.phase == AE_ROUND_PHASE_OVER)
	{
		snprintf(answer, size, "the match is over");
		return FALSE;
	}
	if (!strcmp(what, "live"))
	{
		if (ae_round_host.phase == AE_ROUND_PHASE_LIVE)
		{
			snprintf(answer, size, "round %d is live", ae_round_host.round);
			return TRUE;
		}
		if (ae_round_host.phase != AE_ROUND_PHASE_PRE)
		{
			snprintf(answer, size, "round %d is %s, not in its countdown", ae_round_host.round,
				ae_round_phase_name(ae_round_host.phase));
			return FALSE;
		}
		/* (the countdown's start moved back: it ends now, on every machine) */
		ae_round_host.round_start = now - AE_ROUND_PRE_TICKS;
		goes_live(now);
		snprintf(answer, size, "round %d is live", ae_round_host.round);
		return TRUE;
	}
	if (!strcmp(what, "end"))
	{
		if (ae_round_host.phase == AE_ROUND_PHASE_POST)
		{
			snprintf(answer, size, "round %d is over already", ae_round_host.round);
			return FALSE;
		}
		end_by_hand(now);
		snprintf(answer, size, "round %d ended (%s)", ae_round_host.round,
			ae_round_host.last_result == AE_ROUND_RESULT_TIE ? "tied" : side_name(ae_round_host.last_result));
		return TRUE;
	}
	/* next: the round ends tied (unless over already) and the next starts at once */
	if (ae_round_host.phase != AE_ROUND_PHASE_POST)
		round_ends(now, AE_ROUND_TIED, -1);
	match_over_or_next(now);
	if (ae_round_host.phase == AE_ROUND_PHASE_OVER)
		snprintf(answer, size, "the match is over");
	else
		snprintf(answer, size, "round %d starts%s", ae_round_host.round, ae_round_host.switched ? ", sides switched" : "");
	return TRUE;
}

boolean ae_round_dev_zone(char const *what, char *answer, int size)
{
	long now = game_time_get();

	if (!dev_ready(answer, size))
		return FALSE;
	if (!strcmp(what, "off"))
	{
		ae_round_host.zone_disabled = TRUE;
		snprintf(answer, size, "the danger zone is off for this match (zone now: on again)");
		ae_round_log("dev: the danger zone off");
		return TRUE;
	}
	if (ae_round_host.phase != AE_ROUND_PHASE_LIVE)
	{
		snprintf(answer, size, "the round is not live (%s)", ae_round_phase_name(ae_round_host.phase));
		return FALSE;
	}
	ae_round_host.zone_disabled = FALSE;
	ae_round_zone_start_now(now, !strcmp(what, "small"));
	snprintf(answer, size, "the danger zone %s now (radius %.1f, centre %.1f %.1f)",
		!strcmp(what, "small") ? "stands at its small ring" : "closes", ae_round_zone_radius(now),
		ae_round_host.zone_centre.x, ae_round_host.zone_centre.y);
	return TRUE;
}

boolean ae_round_dev_down(char const *who, boolean here, char *answer, int size)
{
	long target, unit_index, me = local_player_get_player_index(0);
	struct ae_round_slot *slot;
	char name[16];

	if (!dev_ready(answer, size))
		return FALSE;
	if (ae_round_host.phase != AE_ROUND_PHASE_LIVE)
	{
		snprintf(answer, size, "the round is not live (%s)", ae_round_phase_name(ae_round_host.phase));
		return FALSE;
	}
	target = find_player(who, FALSE);
	unit_index = ae_round_living_unit(target);
	slot = ae_round_slot_of(target);
	if (target == NONE || !slot || slot->player_index != target || (unit_index == NONE &&
		slot->state != AE_ROUND_PLAYER_REVIVED && !(slot->in_round && slot->state == AE_ROUND_PLAYER_NONE)))
	{
		snprintf(answer, size, "no living player %s", who);
		return FALSE;
	}
	name_of(target, name, sizeof(name));
	/* (revived, or not spawned yet this round, and about to stand up: down as they do) */
	if (unit_index == NONE)
	{
		slot->dev_down = here ? 2 : 1;
		snprintf(answer, size, "%s goes down as they stand up%s", name, here ? ", beside you" : "");
		return TRUE;
	}
	down_now(target, slot, unit_index, here);
	snprintf(answer, size, "%s goes %s%s", name, slot->state == AE_ROUND_PLAYER_DOWN ? "down" : "out",
		here ? " beside you" : "");
	(void)me;
	return TRUE;
}

boolean ae_round_dev_revive(char const *who, char *answer, int size)
{
	long target;
	struct ae_round_slot *slot;
	char name[16];

	if (!dev_ready(answer, size))
		return FALSE;
	target = find_player(who, TRUE);
	slot = ae_round_slot_of(target);
	if (target == NONE || !slot || slot->player_index != target || slot->state != AE_ROUND_PLAYER_DOWN)
	{
		snprintf(answer, size, "no downed player %s", who);
		return FALSE;
	}
	name_of(target, name, sizeof(name));
	revive_now(target, slot, NONE);
	snprintf(answer, size, "%s revived (revive %d this round)", name, slot->revives);
	return TRUE;
}

boolean ae_round_dev_kill_unspawned(long player_index)
{
	struct ae_round_slot *slot;

	if (!host_side() || !ae_round_host.running || ae_round_host.phase != AE_ROUND_PHASE_LIVE)
		return FALSE;
	slot = ae_round_slot_of(player_index);
	if (!slot || slot->player_index != player_index || !slot->in_round || slot->state != AE_ROUND_PLAYER_NONE ||
		ae_round_living_unit(player_index) != NONE)
	{
		return FALSE;
	}
	slot->dev_down = 1;
	return TRUE;
}

boolean ae_round_hud_line(long viewer_team, long *round, long *mine, long *theirs)
{
	struct ae_round_wire const *view = ae_round_view();

	if (!ae_round_on() || !view || view->phase == AE_ROUND_PHASE_OFF)
		return FALSE;
	*round = view->round;
	*mine = *theirs = NONE;
	if ((view->flags & AE_ROUND_WIRE_TEAMS) && (viewer_team == _team_red || viewer_team == _team_blue))
	{
		*mine = viewer_team == _team_red ? view->wins_red : view->wins_blue;
		*theirs = viewer_team == _team_red ? view->wins_blue : view->wins_red;
	}
	return TRUE;
}

boolean ae_round_restart(void)
{
	long now = game_time_get();

	if (!host_side() || !ae_round_host.running ||
		(ae_round_host.phase != AE_ROUND_PHASE_PRE && ae_round_host.phase != AE_ROUND_PHASE_LIVE))
	{
		return FALSE;
	}
	ae_round_log("the host restarts round %d", ae_round_host.round);
	ae_h2_match_reset_round();
	round_begins(now);
	return TRUE;
}

void ae_round_dev_restart(void)
{
	if (ae_round_host.running)
	{
		ae_round_log("dev reset: the match starts again");
		ae_round_host.running = FALSE;
	}
}
