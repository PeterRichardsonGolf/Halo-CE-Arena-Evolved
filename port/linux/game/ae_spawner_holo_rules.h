/*
AE_SPAWNER_HOLO_RULES.H

port: AE weapon spawners (rc2). The pure rules of the spawner hologram (ae_spawner_holo.c draws it): while a weapon is
returning, its ghost lies exactly where and how the weapon will lie: the weapon's own shape in its lying pose, its outline
drawn bright and its body filling from the stock to the muzzle as the timer runs, a bright line at the fill edge (no rack
and no plate is drawn). The triangle clip and cut at the fill edge, the line width, the fill fraction, the fade by
distance and the last seconds' pulse. No engine includes; port/linux/tests/ae_spawner_holo_rules_test.c compiles this
alone.
*/

#ifndef AE_SPAWNER_HOLO_RULES_H
#define AE_SPAWNER_HOLO_RULES_H

/* a line about this many radians a pixel (a 1920 wide view of the default field of view) and never thinner than the
minimum, world units */
#define AE_HOLO_RADIANS_PER_PIXEL 0.0007f
#define AE_HOLO_LINE_WIDTH_MINIMUM 0.002f

/* the hologram is whole up to this distance (world units, as the waypoint's AE_SPAWNER_WAYPOINT_*), gone from the far one */
#define AE_HOLO_FULL_DISTANCE 10.0f
#define AE_HOLO_GONE_DISTANCE 16.0f
/* the last seconds of a return, the hologram pulses once a second */
#define AE_HOLO_PULSE_SECONDS 3

struct ae_holo_point3
{
	float x, y, z;
};

/* the part of a triangle with dot(point, axis) <= limit, in order: 3 or 4 corners, 0 for nothing left */
int ae_holo_clip_triangle(struct ae_holo_point3 const triangle[3], struct ae_holo_point3 axis, float limit,
	struct ae_holo_point3 out[4]);

/* where a triangle crosses dot(point, axis) == limit: 1 with the segment's two ends, 0 when it doesn't cross */
int ae_holo_cut_triangle(struct ae_holo_point3 const triangle[3], struct ae_holo_point3 axis, float limit,
	struct ae_holo_point3 *from, struct ae_holo_point3 *to);

/* a line's width in world units to look about `pixels` wide at `distance` */
float ae_holo_line_width(float distance, float pixels);

/* how much of the ghost is filled, 0..1: the timer's whole run, ticks_left of period (a returned weapon: 1) */
float ae_holo_fill(long ticks_left, long period);

/* the fill edge's place along the weapon's length, between low (the stock) and high (the muzzle) */
float ae_holo_edge(float low, float high, float fill);

/* alpha from the viewer's distance: 1 up to the full distance, a straight ramp to 0 at the gone one */
float ae_holo_distance_alpha(float distance);

/* the last seconds' pulse factor (0.55..1): 1 before them, then once a second */
float ae_holo_pulse(long ticks_left);

/* what a weapon lying on a spawner's spot means to the ghost (AE_HOLO_SEEN_* bits): PRESENT when it is a weapon the
spawner makes (a different weapon dropped there does not stand in for it: the ghost stays), POSE when, besides, the
spawner calls itself ready (its own weapon settled there: the ghost learns its pose) */
#define AE_HOLO_SEEN_PRESENT 1
#define AE_HOLO_SEEN_POSE 2
int ae_holo_seen(int spawners_weapon, int ready);

/* the ghost's pose before its weapon was ever seen on the spot: the forward the spawn gives the weapon (the map item's
facing, radians, when the game faces the map's items; else the placement's default, +x), the weapon on its side (up
horizontal, a quarter turn left of forward) */
void ae_holo_first_pose(int faces_item, float facing, struct ae_holo_point3 *forward, struct ae_holo_point3 *up);

/* the outline's ink: red while the spot is RED, white for a POWER pad, the ice blue of a base rack (the body's fill is
ice blue on both; the outline alone says power) */
enum { AE_HOLO_INK_ICE = 0, AE_HOLO_INK_WHITE, AE_HOLO_INK_RED };
int ae_holo_outline_ink(int power, int red);

/* the slot of a small cache to fill: the first free one (used < 0), else the one used least recently (the smallest use
stamp; the first of a tie) */
int ae_holo_cache_slot(long const *used, int count);

#endif
