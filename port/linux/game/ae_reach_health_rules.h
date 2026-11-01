/*
AE_REACH_HEALTH_RULES.H

HEALTH's REACH (game_engine.h's _health_style_reach), the
multiplayer numbers (the Spartan's damage sections),
on CE's pools (75 shield, 75 body: the rule works in fractions of them). Pure
rules (no engine includes: port/linux/tests/ae_reach_health_rules_test.c runs
them); objects/damage.c runs them on the host with its own body timer table.

- The body has its own wait: 4 s after the last damage that reached it. A hit
  that stays on the shield never delays the health.
- Then the health refills continuously, up to the top of the third it is in
  (the recharge stops 0.333, 0.666 and 1.0), at 1/9 of the body a second
  times the curve 1 + 2 (2x^3 - 3x^2 + 1) (x the health's fraction): three
  times as fast near empty as near full, about 1.1 s for the bottom third,
  1.5 s for the middle one and 2.6 s for the top one. On a stop it does not
  move: the rest comes from a health pack.
- The shields wait 6 s after a hit (a hit that reaches the body restarts both
  waits), then refill from empty to full in 3 s at a steady rate.
- A health pack fills the health at once and ends the shields' wait: they
  start to refill on the next tick (they are not filled).

Elites' numbers (a 7 s wait, health back to full, no packs) are not used:
CE's players are all Spartans.
*/

#ifndef __AE_REACH_HEALTH_RULES_H
#define __AE_REACH_HEALTH_RULES_H

/* the waits and the shields' refill, in seconds */
#define AE_REACH_BODY_STUN_SECONDS 4.0f
#define AE_REACH_SHIELD_STUN_SECONDS 6.0f
#define AE_REACH_SHIELD_RECHARGE_SECONDS 3.0f
/* the body's base refill: all of it in 9 s, before the curve */
#define AE_REACH_BODY_RECHARGE_SECONDS 9.0f
/* the stops below full (its own numbers, not exactly 1/3 and 2/3) */
#define AE_REACH_HEALTH_STOP_LOW 0.333f
#define AE_REACH_HEALTH_STOP_HIGH 0.666f

/* where the health's refill stops: the top of the third it is in (on a stop,
that stop) */
float ae_reach_health_cap(float health);

/* the health after a tick of `seconds` of refill, once the body's wait is
over: unchanged when dead, full or on a stop */
float ae_reach_health_step(float health, float seconds);

#endif
