/*
AE_ROUND_ZONE_RULES.H

The round modes' DANGER ZONE, pure: a horizontal circle (a column of any height) that closes over the round, and the
damage it deals outside. No game dependency: unit-tested (port/linux/tests/ae_round_zone_rules_test.c). The game side:
ae_round_zone.c (the centre, the damage on the host), ae_round_net.c (its fields to the clients).

The timing, from the round's LIVE start: a wait (60 s), a closing to a tenth of the start radius (30 s), a wait (20 s),
a closing to nothing (30 s). A closing lasts longer where its edge would move faster than half a walking player's speed
(about 1.1 world units a second), so it never outruns a player on a big map. Outside, a living player loses an eighth
of their full shield and health each second (shield first): eight seconds from full, whatever the HEALTH style.

The centre: a map layout's `zone` line (one at random each round, ae_layout_rules.h), else a hill marker that sits
fairly between the two teams' start spawns (their distances differing by at most 15 % of their sum; one at random),
else the point halfway between the teams' start spawns (without teams: the middle of all spawns). The start radius is
the farthest spawn from the centre and a tenth more, unless the layout line gives one.
*/
#ifndef AE_ROUND_ZONE_RULES_H
#define AE_ROUND_ZONE_RULES_H

#include "ae_pure_types.h"

/* a walking player's speed, world units a second; the edge moves at most half of it */
#define AE_ZONE_WALK_SPEED 2.25f
/* what the first closing leaves of the start radius */
#define AE_ZONE_MID_FRACTION 0.1f
/* the second hold, after the first closing, in seconds (the first wait is the settings') */
#define AE_ZONE_WAIT_2_SECONDS 20
/* a fair hill: its distances to the two teams' start centres differ by at most this part of their sum */
#define AE_ZONE_FAIR_SHARE 0.15f
/* the start radius past the farthest spawn, and the least start radius (world units) */
#define AE_ZONE_START_MARGIN 1.1f
#define AE_ZONE_LEAST_RADIUS 5.0f
/* a bot in a fight near the edge of a closing ring (world units inside it) breaks off and goes back in */
#define AE_ZONE_LEASH_MARGIN 6.0f
/* the damage's share of full shield and health a second */
#define AE_ZONE_DAMAGE_SHARE (1.0f / 8.0f)

enum
{
	AE_ZONE_TICKS_PER_SECOND = 30,
	/* the most seconds a phase lasts (the wire's byte) */
	AE_ZONE_LONGEST_PHASE_SECONDS = 250,
};

/* the zone's phases, from the round's LIVE start */
enum ae_zone_phase
{
	AE_ZONE_PHASE_WAIT = 0,
	AE_ZONE_PHASE_CLOSING,
	AE_ZONE_PHASE_WAIT_2,
	AE_ZONE_PHASE_CLOSING_2,
	AE_ZONE_PHASE_CLOSED,
	AE_ZONE_PHASE_COUNT
};

struct ae_zone_plan
{
	real start_radius;
	real mid_radius;
	/* seconds of each phase: the wait, the first closing, the second wait, the last closing */
	int wait_seconds, closing_seconds, wait_2_seconds, closing_2_seconds;
};

/* a round's plan: its start radius, the settings' wait and least closing seconds; each closing lengthened so its edge
moves at most half of AE_ZONE_WALK_SPEED, and no phase past AE_ZONE_LONGEST_PHASE_SECONDS */
void ae_zone_plan_make(struct ae_zone_plan *plan, real start_radius, int wait_seconds, int closing_seconds);
/* the phase at elapsed ticks since the zone started (LIVE's start), the radius then, and the ticks left in the phase
(0 once closed) */
int ae_zone_phase_at(struct ae_zone_plan const *plan, long elapsed);
real ae_zone_radius_at(struct ae_zone_plan const *plan, long elapsed);
long ae_zone_phase_ticks_left(struct ae_zone_plan const *plan, long elapsed);
/* the radius the zone closes to next (its current radius once closed) */
real ae_zone_next_radius(struct ae_zone_plan const *plan, long elapsed);
/* whether a point stands inside (on the edge counts) */
int ae_zone_inside(real centre_x, real centre_y, real radius, real x, real y);

/* one second outside: shield and body (fractions of their maxima: a shield over 1 is an overshield) after it, from
the maxima in points (a unit without shields has 0). Returns 1 when it kills (the body at 0) */
int ae_zone_damage(real maximum_shield, real maximum_body, real *shield, real *body);

/* whether a hill sits fairly between the teams' start centres (red and blue), across */
int ae_zone_fair_hill(real hill_x, real hill_y, real red_x, real red_y, real blue_x, real blue_y);
/* the start radius for a centre: the farthest of the points (across) and a tenth more, at least AE_ZONE_LEAST_RADIUS */
real ae_zone_start_radius(real centre_x, real centre_y, real const (*points)[2], int count);

/* a point on the line from the centre to (x, y), at that distance from the centre (the centre itself when they
are one) */
void ae_zone_point_toward(real centre_x, real centre_y, real x, real y, real distance, real *out_x, real *out_y);

/* ---- what a client shows (ae_zone_view.c, ae_zone_hud.c, ae_zone_world.c): all of it from the plan and the time */

/* the strip's clock cell: a countdown before a closing ("ZONE 0:30"), CLOSING during the first, FINAL ZONE for the last
closing (and once closed); *seconds the countdown's whole seconds left (rounded up) */
enum ae_zone_clock_kind
{
	AE_ZONE_CLOCK_COUNT = 0,
	AE_ZONE_CLOCK_CLOSING,
	AE_ZONE_CLOCK_FINAL,
};
int ae_zone_clock(struct ae_zone_plan const *plan, long elapsed, long *seconds);

/* the warnings: in a wait, 30 s then 10 s before its closing (1, 2; a wait shorter than 35 s has no 30 s line); in a
closing 3 (the first) or 4 (the last); else 0 */
int ae_zone_warning(struct ae_zone_plan const *plan, long elapsed);

/* the distance to the edge, world units: positive inside, negative outside, 0 on it */
real ae_zone_edge_distance(real centre_x, real centre_y, real radius, real x, real y);

/* the outside tint's alpha at a point of the view (across, down: 0 to 1 from its top left): AE_ZONE_TINT_ALPHA at the
frame's edge, none inside the middle AE_ZONE_TINT_CLEAR of it (each way), smooth between */
#define AE_ZONE_TINT_ALPHA 0.30f
#define AE_ZONE_TINT_CLEAR 0.55f
real ae_zone_tint_alpha(real across, real down);

/* the damage tick's vignette: its alpha this many ticks after the tick (0.40, gone after AE_ZONE_PULSE_TICKS) */
#define AE_ZONE_PULSE_ALPHA 0.40f
#define AE_ZONE_PULSE_TICKS 15
real ae_zone_pulse_alpha(long ticks_after);

/* whether a bot at this edge distance (positive inside) breaks off its fight and goes back in: outside the ring, or
within AE_ZONE_LEASH_MARGIN of the edge while the ring closes */
int ae_zone_leashes(int phase, real edge_distance);

/* the round clock's whole seconds left, from the zone's total length and the round time limit (0 none): the least, never below 0 */
long ae_zone_round_seconds_left(long total_seconds, long limit_seconds, long elapsed_ticks);

/* how much of the wall (and the next ring's dashes) is drawn, 0..1: none through the first wait, when the ring sits out past
every start spot (nobody sees a zone before it comes in), then it eases in over AE_ZONE_REVEAL_TICKS of the first closing,
whole after that. A zone whose clock has not started (running 0) shows nothing */
#define AE_ZONE_REVEAL_TICKS 150
real ae_zone_reveal(struct ae_zone_plan const *plan, int running, long elapsed);

/* the wall's two looks (game.zone_look: 'a' the energy wall, 'b' the light curtain), per point of the ring: a point's
colour (violet ramp, 0 deep to 1 hot) and alpha at a height over the ground and an arc position, for time in ticks.
Pure, so the looks are unit tested. A: 10 wu tall; B: 5 wu tall with ribs every AE_ZONE_RIB_SPACING along the arc */
#define AE_ZONE_WALL_HEIGHT 10.0f
#define AE_ZONE_CURTAIN_HEIGHT 5.0f
#define AE_ZONE_RIB_SPACING 1.5f
real ae_zone_wall_height(int look);
/* alpha 0..1 and ramp 0..1 at height (0 foot .. the look's height), arc (world units along the ring) and ticks */
void ae_zone_wall_sample(int look, real height, real arc, long ticks, real *alpha, real *ramp);

#endif
