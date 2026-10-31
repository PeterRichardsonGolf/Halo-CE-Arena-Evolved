/*
AE_BOTS_REVENGE.INC

Included by ae_bots_lan.inc, so at the end of bots.c: REVENGE (ae_bots_revenge_rules.h), a bot hunting the player who
killed it three times in a row. The game's kills reach it from game_engine_player_killed (its AE hook line, the host's
game only: bots play there); bots_choose_goal asks it after the objectives (its AE hook line), bots_target_score takes
AE_BOTS_REVENGE_TARGET_SCORE off the nemesis, and the fight adds its tilt to the bot's confidence
(ae_bots_combat.inc). The hunter goes where it last knew its nemesis to be, never where they are now: where they
killed it last, then each time it saw them (a nemesis in sight wins the target pick: the revenge score), heard their shot
(ae_bots_team.inc's hearing makes them the target) or had them shown by a teammate (ae_bots_fight_share). Reached with
no one there, the spot is spent and the bot roams as it would until it learns more. Logged as bot events "revenge" (state=start / state=done) with debug.bot_events. (A player handle is 32 bits, negative
as an int: only NONE is nobody.)
*/

#include "ae_bots_revenge_rules.h"

static struct
{
	struct ae_bots_revenge state[BOTS_BRAINS];
	/* the player each state was counted for (a bot slot given to another player starts clean) */
	long player_index[BOTS_BRAINS];
	/* a smart hunter's spot (a roaming place's index) and when it picks again */
	short spot[BOTS_BRAINS];
	long spot_tick[BOTS_BRAINS];
	/* where it last knew the nemesis to be (its ground and surface), and when; known_tick NONE for nowhere */
	real_point3d known_point[BOTS_BRAINS];
	long known_surface[BOTS_BRAINS];
	long known_tick[BOTS_BRAINS];
} ae_bots_revenge;

static void ae_bots_revenge_forget(long bot_index)
{
	ae_bots_revenge.spot[bot_index] = -1;
	ae_bots_revenge.spot_tick[bot_index] = 0;
	ae_bots_revenge.known_surface[bot_index] = NONE;
	ae_bots_revenge.known_tick[bot_index] = NONE;
}

static void ae_bots_revenge_reset(void)
{
	long index;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		ae_bots_revenge_clear(&ae_bots_revenge.state[index]);
		ae_bots_revenge.player_index[index] = NONE;
		ae_bots_revenge_forget(index);
	}
}

/* the bot of a player's state, or NULL for a human */
static struct ae_bots_revenge *ae_bots_revenge_of(long player_index, long *bot_index)
{
	long index;

	if (player_index == NONE)
		return NULL;
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		if (bots_globals.bots[index].player_index != player_index)
			continue;
		if (ae_bots_revenge.player_index[index] != player_index)
		{
			ae_bots_revenge_clear(&ae_bots_revenge.state[index]);
			ae_bots_revenge.player_index[index] = player_index;
			ae_bots_revenge_forget(index);
		}
		if (bot_index)
			*bot_index = index;
		return &ae_bots_revenge.state[index];
	}
	return NULL;
}

/* game_engine_player_killed: a bot's death counts towards revenge on its killer (an enemy's kill only), a bot's kill
counts against its own count or ends its hunt */
void ae_bots_player_killed(long killing_player_index, long dead_player_index)
{
	struct player_datum *killer, *dead;
	struct ae_bots_revenge *state;
	long bot_index, now = game_time_get();

	if (!bots_globals.map_ready || killing_player_index == NONE || killing_player_index == dead_player_index)
		return;
	killer = player_try_and_get(killing_player_index);
	dead = player_try_and_get(dead_player_index);
	if (!killer || !dead || (game_engine_has_teams() && killer->team_index == dead->team_index))
		return;
	if ((state = ae_bots_revenge_of(dead_player_index, &bot_index)) != NULL)
	{
		boolean started = ae_bots_revenge_death(state, (int)killing_player_index, (int)now, TICKS_PER_SECOND);
		real_point3d ground;
		long surface_index;

		/* (its nemesis's kill: where they stood as it died is the hunt's newest spot) */
		if (state->nemesis == (int)killing_player_index && killer->unit_index != NONE &&
			(surface_index = bots_unit_surface(killer->unit_index, &ground)) != NONE)
		{
			ae_bots_revenge.spot[bot_index] = -1;
			ae_bots_revenge.known_point[bot_index] = ground;
			ae_bots_revenge.known_surface[bot_index] = surface_index;
			ae_bots_revenge.known_tick[bot_index] = now;
		}
		if (started && ae_bots_events_on())
			ae_bots_event_line(bot_index, now, "revenge", ae_bots_event_absolute(killing_player_index), -1.0f, "state=start");
	}
	if ((state = ae_bots_revenge_of(killing_player_index, &bot_index)) != NULL &&
		ae_bots_revenge_kill(state, (int)dead_player_index) && ae_bots_events_on())
	{
		ae_bots_event_line(bot_index, now, "revenge", ae_bots_event_absolute(dead_player_index), -1.0f, "state=done");
	}
}

/* the bot's nemesis now, NONE for none (one who left the game is let go) */
static long ae_bots_revenge_target(struct bots_bot const *bot)
{
	struct ae_bots_revenge *state = ae_bots_revenge_of(bot->player_index, NULL);
	long nemesis;

	if (!state || (nemesis = ae_bots_revenge_nemesis(state, (int)game_time_get())) == -1)
		return NONE;
	if (!player_try_and_get(nemesis))
	{
		ae_bots_revenge_clear(state);
		return NONE;
	}
	return nemesis;
}

/* bots_target_score: the nemesis scores less (picked first) */
static real ae_bots_revenge_score(struct bots_bot const *bot, long player_index)
{
	return player_index == ae_bots_revenge_target(bot) ? AE_BOTS_REVENGE_TARGET_SCORE : 0.0f;
}

/* what the bot knows of its nemesis: the target's last seen (or heard) spot, when the target is the nemesis and that is
newer than what it knew */
static void ae_bots_revenge_note(struct bots_bot const *bot, long bot_index, long nemesis)
{
	if (nemesis == NONE || bot->target_player_index != nemesis || bot->target_last_surface_index == NONE ||
		(ae_bots_revenge.known_tick[bot_index] != NONE && bot->target_seen_tick <= ae_bots_revenge.known_tick[bot_index]))
	{
		return;
	}
	ae_bots_revenge.spot[bot_index] = -1;
	ae_bots_revenge.known_point[bot_index] = bot->target_last_position;
	ae_bots_revenge.known_surface[bot_index] = bot->target_last_surface_index;
	ae_bots_revenge.known_tick[bot_index] = bot->target_seen_tick;
}

/* the fight's confidence with the nemesis: the tilt (ae_bots_revenge_confidence); a fight with them is a sighting */
static real ae_bots_revenge_tilt(struct bots_bot const *bot, int level)
{
	long bot_index = (long)(bot - bots_globals.bots), nemesis = ae_bots_revenge_target(bot);

	if (bot->target_player_index == NONE || bot->target_player_index != nemesis)
		return 0.0f;
	if (bot_index >= 0 && bot_index < BOTS_BRAINS)
		ae_bots_revenge_note(bot, bot_index, nemesis);
	return ae_bots_revenge_confidence(level);
}

/* a smart hunter's spot near the nemesis: the roaming place AE_BOTS_REVENGE_SMART_NEAR .. _FAR units from them, most
credit for its height over them, nearest to them; -1 for none */
static short ae_bots_revenge_smart_spot(struct bots_bot *bot, real_point3d const *at)
{
	short index, best = -1;
	real best_score = REAL_MAX;

	for (index = 0; index < bots_globals.roam_point_count; index++)
	{
		real distance = bots_distance2d(&bots_globals.roam_points[index], at), score;

		if (distance < AE_BOTS_REVENGE_SMART_NEAR || distance > AE_BOTS_REVENGE_SMART_FAR)
			continue;
		score = distance - ae_bots_roam_height(bot, &bots_globals.roam_points[index], at);
		if (score < best_score)
		{
			best_score = score;
			best = index;
		}
	}
	return best;
}

/* bots_choose_goal's ask (after the vehicles and objectives): not fighting, a bot with a nemesis goes after them, to
where it last knew them to be */
static boolean ae_bots_revenge_goal(struct bots_bot *bot, long player_index, real_point3d const *position,
	boolean fighting)
{
	long bot_index = (long)(bot - bots_globals.bots), nemesis, now = game_time_get();
	struct player_datum const *nemesis_player;
	struct object_datum const *object;
	real_point3d const *at;
	(void)player_index;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || (nemesis = ae_bots_revenge_target(bot)) == NONE)
		return FALSE;
	ae_bots_revenge_note(bot, bot_index, nemesis);
	if (fighting || ae_bots_revenge.known_tick[bot_index] == NONE)
		return FALSE;
	nemesis_player = player_get(nemesis);
	if (nemesis_player->unit_index == NONE ||
		(object = (struct object_datum const *)object_try_and_get_and_verify_type(nemesis_player->unit_index,
			_object_mask_unit)) == NULL ||
		TEST_FLAG(object->object.damage_flags, _object_dead_bit))
	{
		/* (dead or not spawned: waits for them as it would roam; where they died is no spot to go) */
		ae_bots_revenge_forget(bot_index);
		return FALSE;
	}
	at = &ae_bots_revenge.known_point[bot_index];
	/* (there, and no one: the spot is spent) */
	if (bots_distance2d(position, at) < 3.0f && (real)fabs(position->z - at->z) < 2.0f)
	{
		if (ae_bots_events_on())
			ae_bots_event_line(bot_index, now, "revenge", ae_bots_event_absolute(nemesis), -1.0f, "state=lost");
		ae_bots_revenge_forget(bot_index);
		return FALSE;
	}
	/* (a smart hunter's spot -2: done watching from a spot, it goes to look until it learns more) */
	if (ae_bots_revenge_style(ae_bots_level_of_skill(bots_globals.skill)) == AE_BOTS_REVENGE_SMART &&
		bots_globals.roam_point_count > 0 && ae_bots_revenge.spot[bot_index] != -2)
	{
		short spot = ae_bots_revenge.spot[bot_index];

		/* (picked again every 6 s, or when the spot it knows has moved off it) */
		if (spot < 0 || spot >= bots_globals.roam_point_count || now >= ae_bots_revenge.spot_tick[bot_index] ||
			bots_distance2d(&bots_globals.roam_points[spot], at) > AE_BOTS_REVENGE_SMART_FAR)
		{
			spot = ae_bots_revenge_smart_spot(bot, at);
			ae_bots_revenge.spot[bot_index] = spot;
			ae_bots_revenge.spot_tick[bot_index] = now + 6 * TICKS_PER_SECOND;
		}
		if (spot >= 0)
		{
			/* (on the spot near where they were: it watches from there, then goes to look) */
			if (bots_distance2d(position, &bots_globals.roam_points[spot]) < 2.0f &&
				now - ae_bots_revenge.known_tick[bot_index] > 6 * TICKS_PER_SECOND)
			{
				ae_bots_revenge.spot[bot_index] = -2;
			}
			else
			{
				bot->hunting = TRUE;
				bots_set_goal(bot, _bots_goal_roam, &bots_globals.roam_points[spot], bots_globals.roam_surfaces[spot]);
				return TRUE;
			}
		}
	}
	/* RECKLESS and DIRECT (and a smart hunter done watching): straight to the spot it knows (the path kept while the spot
	stays within 4 units of its goal: a goal moved every tick restarts the path search every tick, ae_bots_team.inc's
	wingman) */
	if (bot->goal_kind == _bots_goal_roam && bot->hunting && bots_distance3d(&bot->goal_point, at) < 4.0f)
		return TRUE;
	bot->hunting = TRUE;
	bots_set_goal(bot, _bots_goal_roam, at, ae_bots_revenge.known_surface[bot_index]);
	return TRUE;
}
