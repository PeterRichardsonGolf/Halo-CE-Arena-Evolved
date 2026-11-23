/*
AE_BOTS_CALLS_RULES.H

How often a bot may make a communication wheel call (ae_bots_calls.inc; notes/plans/2026-11-14-bot-wheel-calls-design.md) as
pure rules: no engine or config calls (port/linux/tests/ae_bots_calls_rules_test.c runs them). Times are ticks (30 a second),
distances world units.

- A bot calls only on a team with a player, only while it is alive and on foot or driving (RIDE only from a driver).
- One call per bot every 6 s (12 s at RECRUIT and EASY); the team's bots together one call every 2 s.
- The same call within 6 units of a call of the team in the last 20 s is not made again.
*/

#ifndef __AE_BOTS_CALLS_RULES_H
#define __AE_BOTS_CALLS_RULES_H

#include "ae_pure_types.h"

#define AE_BOTS_CALL_BOT_GAP_TICKS 180
#define AE_BOTS_CALL_TEAM_GAP_TICKS 60
#define AE_BOTS_CALL_REPEAT_TICKS 600
#define AE_BOTS_CALL_REPEAT_REACH 6.0f
/* the team's ring of recent calls: every call of the repeat window at the team's gap, and one more */
#define AE_BOTS_CALL_RECENT ((AE_BOTS_CALL_REPEAT_TICKS / AE_BOTS_CALL_TEAM_GAP_TICKS) + 1)

struct ae_bots_call_recent
{
	int entry;
	float x, y, z;
	long tick;	/* 0 for an empty place */
};

struct ae_bots_call_team
{
	long last_tick;	/* 0: none yet */
	int next;
	struct ae_bots_call_recent recent[AE_BOTS_CALL_RECENT];
};

/* whether the call may be made. bot_last_tick: the bot's own last call, 0 for never. low_level: RECRUIT or EASY (the gap doubles).
entry: an AE_WHEEL_* value. is_driver matters for RIDE alone. */
int ae_bots_call_allowed(struct ae_bots_call_team const *team, long bot_last_tick, int low_level, int team_has_human, int alive,
	int on_foot_or_driver, int is_driver, int entry, float x, float y, float z, long now);
/* records a call that was made */
void ae_bots_call_made(struct ae_bots_call_team *team, int entry, float x, float y, float z, long now);
void ae_bots_call_team_reset(struct ae_bots_call_team *team);

#endif
