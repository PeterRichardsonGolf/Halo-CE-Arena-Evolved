/*
AE_MARK_TARGETS.H

MARKS (ae_marks.c): what the aim is on, and the named places of the map that a press can take (ae_mark_targets.c).
Include after cseries.h, math/real_math.h and game/players.h.
*/

#ifndef __AE_MARK_TARGETS_H
#define __AE_MARK_TARGETS_H

#include "ae_mark_calls.h"

struct bitmap_data;

/* what a press would do now: tier is AE_TIER_*; verb AE_CALL_MARK (a new mark, in mark), or the answer to a marker, AE_CALL_OK,
AE_CALL_NEVER_MIND, AE_CALL_DIBS or AE_CALL_CANCEL, to the marker of player reply_to (an absolute index). mark is the target as
it would be marked, or the marker answered. found is FALSE when nothing is in range, and while the player is dead. */
struct ae_mark_aim
{
	boolean found;
	short tier;
	short verb;
	short reply_to;
	struct mark mark;
	long cast_at;
};

/* the local player's aim: recast at most every 3 ticks (the press always casts fresh) */
struct ae_mark_aim const *ae_marks_aim(short local_player_index);

/* a named place of the map: where, how far from its middle the aim still counts (a hill's size), and its word (MARK_WORD_*) */
struct ae_mark_place
{
	real_point3d point;
	real radius;
	short word;
};

#define AE_MARK_PLACES_MOST 32

/* the hills (in a King of the Hill game), the teleporters and the bomb sites (in a Bomb game) of the running map, from the
scenario's own points: the hill in play now is not known on a client, so every hill the map has is a place. Returns how many */
short ae_mark_targets_places(struct ae_mark_place *places, short maximum);

/* the assault rifle's picture from the map's own HUD message icons, as the power item waypoints draw a map's sprite (no stock file
is in the repository): the bitmap, and in *clip the sprite's bounds as fractions of it. NULL when the map has none, or it cannot be
drawn now (no renderer), and clip is not set: the caller falls back */
struct bitmap_data const *ae_mark_rifle_sprite(real_rectangle2d *clip);

#endif
