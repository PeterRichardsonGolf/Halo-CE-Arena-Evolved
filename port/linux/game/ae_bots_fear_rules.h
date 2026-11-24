/*
AE_BOTS_FEAR_RULES.H

Arena Evolved: when a bot may start a flank (ae_bots_fear.inc). Pure numbers only (port/linux/tests/ae_bots_fear_rules_test.c).

FLANK COOL-DOWN (owner 2026-11-14, Temple, his log: one bot on a small side piece with no path started the same flank 9 times in
13 s; over a game the flanks ended unseen 82, target 87, reached 6): after a flank ends unseen, target or time, the bot starts
no flank for 5 s; reached has no cool-down. And no flank starts while the bot has no path and was stuck in the last 2 s.
*/

#ifndef __AE_BOTS_FEAR_RULES_H
#define __AE_BOTS_FEAR_RULES_H

/* ticks (30 a second) a bot waits after a flank that ended unseen, target or time */
#define AE_FEAR_FLANK_COOLDOWN_TICKS 150
/* ticks past the unstick's end (half a second after a stuck report) that still count as stuck: 2 s in all */
#define AE_FEAR_FLANK_STUCK_TICKS 45

/* whether a bot may start a flank at tick now: not inside its cool-down (cooldown_until), and not stuck with no path */
int ae_bots_fear_flank_may_start(long now, long cooldown_until, int path_valid, int stuck_recent);

/* the cool-down's end for a flank that ended with this why (unseen, target, time: now + the cool-down; else 0) */
long ae_bots_fear_flank_cooldown_end(long now, char const *why);

#endif
