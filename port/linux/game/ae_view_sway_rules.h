/*
AE_VIEW_SWAY_RULES.H

The first-person weapon's sway springs, run every frame with the frame's own time (ae_view_sway.h has the game's side).
The stock game moves them once a tick: the weapon leaning as it turns and walks, a capped acceleration toward the turn's
speed and the walk's throttle. Here the same spring is stepped by any time: at a step of one tick it is the stock step,
number for number, so the stock look is kept and the frames between ticks no longer wait for the next tick. A
strength scales what the springs chase (100: the stock lean, 0: none) and a speed makes them catch up sooner (1: stock).
No engine includes: port/linux/tests/ae_view_sway_rules_test.c compiles these alone.
*/

#ifndef __AE_VIEW_SWAY_RULES_H
#define __AE_VIEW_SWAY_RULES_H

#define AE_VIEW_SWAY_TICKS_PER_SECOND 30.0f
/* the stock springs, a tick's numbers: the walk (a velocity change of at most 0.08 a tick, a speed of at most 0.5 a tick)
and the turn (0.03 and 0.2) */
#define AE_VIEW_SWAY_WALK_ACCELERATION 0.08f
#define AE_VIEW_SWAY_WALK_SPEED 0.5f
#define AE_VIEW_SWAY_TURN_ACCELERATION 0.03f
#define AE_VIEW_SWAY_TURN_SPEED 0.2f
/* a frame longer than this is taken as this long (a hitch, a pause): the springs never jump */
#define AE_VIEW_SWAY_LONGEST_STEP 0.1f

/* one spring stepped by seconds toward target, its position kept within lower and upper: acceleration and speed are a
tick's (as the stock numbers above), velocity is kept a second's. At a step of one tick it is the stock step. */
void ae_view_sway_step(float *position, float *velocity, float target, float acceleration, float speed, float seconds,
	float lower, float upper);
/* what the turn spring chases: the facing's change over the step (radians) as a turn's speed, a second's change a
radian's worth being 1, held to -1..1 */
float ae_view_sway_turn_target(float radians, float seconds);
/* the springs' numbers for a strength (percent, 0..100) and a speed (1..8): target scale, and the acceleration and
speed multipliers (a spring speed times k catches up k times sooner: acceleration times k squared, speed times k) */
float ae_view_sway_strength(float percent);
void ae_view_sway_speed(float multiplier, float *acceleration_scale, float *speed_scale);

#endif
