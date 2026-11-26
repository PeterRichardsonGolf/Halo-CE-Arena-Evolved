/*
AE_BOTS_SEAT.INC

Included by ae_bots_lan.inc, so at the end of bots.c: a bot whose seat holds a pair of its own (ae_bots_seat.h) is
updated with its own skill. Around its bots_bot_update (bots.c's AE hook line; bots_ae_lan_drive) bots_globals.skill is
its shooting step's skill, and ae_bots_shooting_skill / ae_bots_movement_skill (ae_bots_skill.inc) return its own
levels; after it, the game's skill again. So every read of the skill inside a bot's update (the definition, the aim,
the grenades, the strafes, the crouch, the fight by level, the falls) is that bot's. What runs for all bots at once
(the team's shared sightings, a minute's halo.log line) keeps the settings.
Each bot's pair is logged once a game: "bots: skill: bot 3 shooting LEGENDARY movement ELITE (its own)", or
"(the settings)".
*/

#include "ae_bots_seat.h"
#include "ae_bots_seat_rules.h"
#include "ae_bots_style_rules.h"

static short ae_bots_seat_saved_skill;
static struct ae_bots_style ae_bots_seat_style;
/* the style each bot's log line named this game (preset * 16 + weapon + 1), 0: none yet */
static int ae_bots_seat_style_logged[BOTS_BRAINS];
/* the pair each bot's log line named this game (shooting * 8 + movement + 1, own pairs + 64), 0: none yet; the game's
tick it last saw (a smaller one is a new game) */
static int ae_bots_seat_logged[BOTS_BRAINS];
static long ae_bots_seat_logged_tick = -1;

static void ae_bots_seat_log(long bot_index, int shooting_level, int movement_level, boolean own)
{
	int code = shooting_level * 8 + movement_level + 1 + (own ? 64 : 0);

	if (bot_index < 0 || bot_index >= BOTS_BRAINS || ae_bots_seat_logged[bot_index] == code)
		return;
	ae_bots_seat_logged[bot_index] = code;
	platform_log("bots: skill: bot %ld shooting %s movement %s (%s)", bot_index + 1, ae_seat_level_name(shooting_level),
		ae_seat_level_name(movement_level), own ? "its own" : "the settings");
}

/* a new game (its clock started over): every bot's lines are said again */
static void ae_bots_seat_game_check(void)
{
	long now = game_time_get();

	if (now < ae_bots_seat_logged_tick)
	{
		csmemset(ae_bots_seat_logged, 0, sizeof(ae_bots_seat_logged));
		csmemset(ae_bots_seat_style_logged, 0, sizeof(ae_bots_seat_style_logged));
	}
	ae_bots_seat_logged_tick = now;
}

/* the bot's style for its update (none: NULL), logged once a game */
static void ae_bots_seat_style_enter(long bot_index)
{
	int code;

	ae_bots_style_now = NULL;
	if (bot_index < 0 || bot_index >= BOTS_BRAINS || !ae_bots_seat_style_get(bot_index, &ae_bots_seat_style))
		return;
	ae_bots_style_now = &ae_bots_seat_style;
	code = ae_bots_seat_style.preset * 16 + ae_bots_seat_style.weapon + 1;
	if (ae_bots_seat_style_logged[bot_index] == code)
		return;
	ae_bots_seat_style_logged[bot_index] = code;
	platform_log("bots: style: bot %ld %s (aggression %+d jumpiness %+d patience %+d), favourite %s", bot_index + 1,
		ae_bots_style_preset_name(ae_bots_seat_style.preset), ae_bots_seat_style.aggression, ae_bots_seat_style.jumpiness,
		ae_bots_seat_style.patience, ae_bots_style_weapon_name(ae_bots_seat_style.weapon));
}

static void ae_bots_seat_enter(long bot_index)
{
	int shooting, movement;

	ae_bots_seat_saved_skill = bots_globals.skill;
	ae_bots_seat_now = -1;
	ae_bots_seat_game_check();
	ae_bots_seat_style_enter(bot_index);
	if (!ae_bots_seat_skill_levels(bot_index, &shooting, &movement))
	{
		ae_bots_seat_log(bot_index, ae_bots_level_of_skill(ae_bots_shooting_skill(bots_globals.skill)),
			ae_bots_level_of_skill(ae_bots_movement_skill(bots_globals.skill)), FALSE);
		return;
	}
	ae_bots_seat_shooting_level = shooting;
	ae_bots_seat_movement_level = movement;
	ae_bots_seat_now = bot_index;
	bots_globals.skill = ae_bots_skill_of_level(shooting);
	ae_bots_seat_log(bot_index, shooting, movement, TRUE);
}

static void ae_bots_seat_leave(void)
{
	ae_bots_style_now = NULL;
	if (ae_bots_seat_now < 0)
		return;
	bots_globals.skill = ae_bots_seat_saved_skill;
	ae_bots_seat_now = -1;
}
