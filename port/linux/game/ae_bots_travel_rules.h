/*
AE_BOTS_TRAVEL_RULES.H

How the bots get around a map (ae_bots_travel.c, notes/specs/2026-10-16-bots-infinite.md area 2) as pure rules: no
engine or config calls (port/linux/tests/ae_bots_travel_rules_test.c runs them). Distances are world units, times
ticks (30 a second).

- LINKS: a teleporter's sender pad and its receiver, or a layout file's `bot jump at A to B`, are links a bot's route
  may take: walk to the link's entry, hop (step on the pad's centre; face B and jump), then on from the exit. A link
  is worth trying when its estimate (straight lines stretched by WALK_STRETCH, the hop's own cost) beats the walk the
  search found by a clear margin, and is taken when the walk to the entry the search then finds still does.
- NO RETURN: the link whose entry is where the bot just came out (a two-way pad's other half) is not taken for a
  while, nor the same link again at once.
- HEALTH PACKS: what a pack would give (the gametype's HEALTH: HALO CE nothing comes back, so all that is missing;
  REACH health comes back to the top of its third, so the rest) against how far the pack is: a bot that is hurt goes
  for the best pack in reach.
- JUMP HINTS: the jump is made once the bot moves within JUMP_FACING_DEGREES of the hop's way (Halo Infinite's bots'
  jump hint facing threshold is 60 degrees).
- STUCK SPOTS: the places bots stood stuck, gathered within STUCK_SPOT_RADIUS, counted, for halo.log's minute line
  (layout authors add hints there).
*/

#ifndef __AE_BOTS_TRAVEL_RULES_H
#define __AE_BOTS_TRAVEL_RULES_H

#include "ae_pure_types.h"

#define AE_BOTS_TRAVEL_TICKS_PER_SECOND 30

/* a straight line's length times this is the walk it is reckoned to be (rooms, corners, ramps) */
#define AE_BOTS_TRAVEL_WALK_STRETCH 1.3f
/* a link is looked at when its estimate is under this much of the walk, less the margin */
#define AE_BOTS_TRAVEL_TRY_SHARE 0.7f
#define AE_BOTS_TRAVEL_TRY_MARGIN 6.0f
/* and taken when the walk to its entry, the hop and on from the exit is under this much of the walk, less the margin */
#define AE_BOTS_TRAVEL_TAKE_SHARE 0.8f
#define AE_BOTS_TRAVEL_TAKE_MARGIN 4.0f
/* a search that found no way at all: the walk is reckoned this long (a link may reach where walking cannot) */
#define AE_BOTS_TRAVEL_NO_WAY_STRETCH 3.0f
#define AE_BOTS_TRAVEL_NO_WAY_EXTRA 30.0f
/* the hop's cost: a teleporter's (stepping on, the flash), a jump's per unit flown and its fixed part */
#define AE_BOTS_TRAVEL_TELEPORT_COST 2.0f
#define AE_BOTS_TRAVEL_JUMP_STRETCH 1.2f
#define AE_BOTS_TRAVEL_JUMP_COST 1.5f

/* no return: the link whose entry lies within this of where the bot came out, for this long */
#define AE_BOTS_TRAVEL_RETURN_RADIUS 4.0f
#define AE_BOTS_TRAVEL_RETURN_TICKS (10 * AE_BOTS_TRAVEL_TICKS_PER_SECOND)
/* the same link again not before this */
#define AE_BOTS_TRAVEL_AGAIN_TICKS (5 * AE_BOTS_TRAVEL_TICKS_PER_SECOND)

/* the jump is made moving within this of the hop's way */
#define AE_BOTS_TRAVEL_JUMP_FACING_DEGREES 60.0f

/* health packs: none gone for when less than this would come of it; reach by what would */
#define AE_BOTS_TRAVEL_PACK_LEAST_NEED 0.2f
#define AE_BOTS_TRAVEL_PACK_LEAST_NEED_CHASING 0.4f
#define AE_BOTS_TRAVEL_PACK_REACH_BASE 6.0f
#define AE_BOTS_TRAVEL_PACK_REACH_PER_NEED 34.0f
#define AE_BOTS_TRAVEL_PACK_HEIGHT_STRETCH 2.0f

/* the gametype's HEALTH as these rules know it */
enum
{
	AE_BOTS_TRAVEL_HEALTH_CLASSIC = 0, /* HALO CE: no health comes back */
	AE_BOTS_TRAVEL_HEALTH_REACH,       /* back to the top of its third */
	AE_BOTS_TRAVEL_HEALTH_FULL         /* HALO 2, HALO 3: all of it comes back */
};

/* the walk a search found: the length of its way (steps), whether it reached the goal, and if not, the straight line
from where it ended to the goal; found: whether it found any way */
real ae_bots_travel_walk(boolean found, real way_length, boolean reached, real remainder_straight, real straight);
/* the hop's cost: a teleporter's, or a jump's of this length */
real ae_bots_travel_hop_cost(boolean jump, real hop_length);
/* a link's estimate from straight lines: to its entry, the hop's cost, from its exit to the goal */
real ae_bots_travel_link_estimate(real to_entry_straight, real hop_cost, real exit_to_goal_straight);
/* whether a link with this estimate is worth a search to its entry, against the walk */
boolean ae_bots_travel_link_worth(real estimate, real walk);
/* whether it is taken: the walk to its entry found, the hop's cost, from its exit to the goal straight */
boolean ae_bots_travel_link_taken(real entry_walk, real hop_cost, real exit_to_goal_straight, real walk);
/* whether a link may be taken now: its entry against where the bot last came out of one and how long ago; the same
link (same_link) not again so soon */
boolean ae_bots_travel_link_allowed(real entry_x, real entry_y, real entry_z, boolean arrived, real arrival_x,
	real arrival_y, real arrival_z, long ticks_since_arrival, boolean same_link);

/* what a health pack would give at this health (0..1), by the gametype's HEALTH */
real ae_bots_travel_pack_need(int health_style, real health);
/* a pack's worth (above 0: worth going for; the higher the better): what it would give, how far it is (straight, and
up or down), whether the bot was chasing an enemy (only for more) */
real ae_bots_travel_pack_score(real need, real distance, real height, boolean chasing);

/* whether the move (dx, dy) is within the jump's facing of the hop (hop_dx, hop_dy) */
boolean ae_bots_travel_jump_aligned(real dx, real dy, real hop_dx, real hop_dy);

/* the stuck spots of a game */
#define AE_BOTS_TRAVEL_STUCK_SPOTS 32
#define AE_BOTS_TRAVEL_STUCK_SPOT_RADIUS 2.0f
struct ae_bots_travel_stuck_spot
{
	real x, y, z;
	long total;
	long minute;
};
struct ae_bots_travel_stuck_spots
{
	int count;
	struct ae_bots_travel_stuck_spot spots[AE_BOTS_TRAVEL_STUCK_SPOTS];
	/* events past a full table, at a new spot */
	long dropped;
};
/* a bot stood stuck here: the nearest spot within the radius counts it (its place the mean of its events), else a
new spot */
void ae_bots_travel_stuck_add(struct ae_bots_travel_stuck_spots *spots, real x, real y, real z);
/* the spots stuck at since the last call, most first (at most maximum), as " x y z (n of total)" items into text;
the minute counts start again. The number listed */
int ae_bots_travel_stuck_minute(struct ae_bots_travel_stuck_spots *spots, int maximum, char *text, int size);

#endif /* __AE_BOTS_TRAVEL_RULES_H */
