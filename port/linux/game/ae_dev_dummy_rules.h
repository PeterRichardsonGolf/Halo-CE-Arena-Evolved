/*
AE_DEV_DUMMY_RULES.H

DEV MODE's target dummies (ae_dev_dummy.h) as pure rules: where they stand, when they heal and respawn, and which bots
they are. No engine includes: port/linux/tests/ae_dev_dummy_rules_test.c compiles these alone.

A dummy is a bot of a local game that stands still where it was put, facing the player who made it, never moving or
shooting. It heals to full AE_DUMMY_HEAL_TICKS after it was last hurt, and respawns at the same spot AE_DUMMY_RESPAWN_TICKS
after it died.
*/

#ifndef __AE_DEV_DUMMY_RULES_H
#define __AE_DEV_DUMMY_RULES_H

/* the most dummies at once, and the distances a dummy may be put at (world units) */
#define AE_DUMMY_MAXIMUM 8
#define AE_DUMMY_DISTANCE_DEFAULT 10.0f
#define AE_DUMMY_DISTANCE_MINIMUM 1.0f
#define AE_DUMMY_DISTANCE_MAXIMUM 200.0f
/* the ticks (30 a second) from the last hurt to the heal, and from a death to the respawn */
#define AE_DUMMY_HEAL_TICKS 90
#define AE_DUMMY_RESPAWN_TICKS 30
/* the ticks a dummy's bot is waited for before it is given up (the game had no room, the bot never came) */
#define AE_DUMMY_JOIN_TICKS 300
/* the sideways gap between dummies made together (world units) */
#define AE_DUMMY_SPACING 1.5f

struct ae_dummy_spot
{
	float x, y, z;
	/* the way the dummy faces (radians), toward the player */
	float yaw;
};

/* dummy `index` of `count` for a player at (ox, oy, oz) facing (fx, fy) (not unit length): `distance` ahead (or at the
player's own spot when `here`), the dummies of a group side by side across the player's line of sight, each facing back
at the player. z is the player's, a little up */
struct ae_dummy_spot ae_dummy_spot(int index, int count, float ox, float oy, float oz, float fx, float fy,
	float distance, int here);
/* whether a dummy hurt at hurt_tick is to heal at now */
int ae_dummy_heal_due(long hurt_tick, long now);
/* the respawn timer a dead dummy's player is to have: the engine's own (current) or shorter, so the respawn comes
AE_DUMMY_RESPAWN_TICKS after the death at dead_tick (never under 1) */
long ae_dummy_respawn_timer(long dead_tick, long now, long current);
/* whether a dummy's bot, flagged at join_tick, is given up at now: it never joined the game (present 0, seen 0) in
AE_DUMMY_JOIN_TICKS, or it joined and has left (seen 1, present 0) */
int ae_dummy_given_up(int present, int seen, long join_tick, long now);
/* the bots a new group of `count` dummies are: the bots (0 up) that are not in the game, in the order bots.count lets
them join, after the `claimed` that are already on their way; up to `maximum` bots. present[i] is non-zero for a bot in
the game. How many were found (fewer than count: not enough room) */
int ae_dummy_pick(unsigned char const *present, int maximum, int claimed, int count, int *out);
/* the dummy that leaves first when bots.count falls: a flagged bot in the game (flag[i], present[i]) before any
other, the lowest first; `fallback` (the game's own pick) when there is none */
int ae_dummy_leaver(unsigned char const *flag, unsigned char const *present, int maximum, int fallback);

#endif
