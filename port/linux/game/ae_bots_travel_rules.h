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
- JUMP HINTS: the jump is made once the bot moves within JUMP_FACING_DEGREES of the hop's way (the bots'
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
/* (0.9.0) a bot fighting an enemy it saw within AE_BOTS_TRAVEL_FIGHT_SEEN_TICKS takes a link (a teleporter, a jump) by
choice only when it saves this much of the walk: a short cut is no reason to leave a fight (a link the route between
the walkable surfaces' pieces needs, no walk there at all, is taken as before) */
#define AE_BOTS_TRAVEL_FIGHT_SAVING 30.0f
#define AE_BOTS_TRAVEL_FIGHT_SEEN_TICKS AE_BOTS_TRAVEL_TICKS_PER_SECOND
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
/* whether a link chosen by its estimate may be taken in or out of a fight (AE_BOTS_TRAVEL_FIGHT_SAVING) */
boolean ae_bots_travel_link_fight_allows(boolean fighting, real estimate, real walk);
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

/* WALK LINKS (owner 10-25, Madhouse: bots stuck under or over their goal): a map's walkable surfaces (its pathfinding
data) fall in pieces the bots' searches cannot cross, a floor and the floor above joined only by stairs or a ramp the
data leaves out (Madhouse: three floors, 24 pieces). A walk link joins two pieces where the level itself lets a biped
walk straight from one to the other: the line is probed every AE_BOTS_TRAVEL_WALK_STEP for ground under it, each step
up or down within a stair's, a clear way at the knees and the chest, room above. */
#define AE_BOTS_TRAVEL_WALK_STEP 0.25f
/* a bridge's ends: this far apart across at least and at most; no steeper than this rise across (stairs) */
#define AE_BOTS_TRAVEL_WALK_SHORTEST 0.6f
#define AE_BOTS_TRAVEL_WALK_LONGEST 6.0f
#define AE_BOTS_TRAVEL_WALK_SLOPE 0.8f
/* one step of the probe: up at most, down at most */
#define AE_BOTS_TRAVEL_WALK_RISE 0.4f
#define AE_BOTS_TRAVEL_WALK_FALL 0.7f
/* whether two points (across apart, rise from the first to the second) may be the ends of a walk link */
boolean ae_bots_travel_walk_ends(real across, real rise);
/* (0.4.0) climb mode (a bot with no route heads straight at its goal, jumping): not over a drop. Ground ahead that lies
this far below the bot's feet, or none, with the goal not down there, ends it */
#define AE_BOTS_TRAVEL_CLIMB_DROP 1.2f
boolean ae_bots_travel_climb_drops(boolean ground, real here_z, real ground_z, real goal_z);
/* (0.7.4) but a goal a hop away (across and up within a crouch jump's reach: Temple's overshield and camo on their
raised spots) is jumped to over the drop; and a climb ended on the same item this many times gives the item up a while
(it was re-picked every second: 394 ends in a 4 minute game) */
#define AE_BOTS_TRAVEL_HOP_ACROSS 2.5f
#define AE_BOTS_TRAVEL_HOP_RISE 0.9f
#define AE_BOTS_TRAVEL_CLIMB_DROP_GIVE_UP 3
boolean ae_bots_travel_climb_hop(real across, real rise);
/* HUNG ITEMS (Digsite's workshop maps, owner 11-08: powerups and weapons hang in the air, to be shot down; bots stood
under them jumping): an item lying more than a standing grab (AE_BOTS_TRAVEL_GRAB_REACH) above a crouch jump's rise
(_HOP_RISE) over the ground below it is out of a bot's reach, and is no goal. A stand (Temple's 0.75) is within */
#define AE_BOTS_TRAVEL_GRAB_REACH 0.6f
boolean ae_bots_travel_item_hung(real rise);
/* JUMP LINKS FOUND (Wizard 11-08: the centre hill, a pad 0.45 above the platforms' edges and 1.8 across a gap from them,
a piece of the walkable surfaces no walk or teleporter joins, so that 6 bots piled at its foot): a piece the links reach
from the main one by no route gets a jump from the nearest ground of a reached piece, when the two edges are a hop's
reach apart (a gap across and a rise either way within a crouch jump's: AE_BOTS_TRAVEL_HOP_ACROSS, _HOP_RISE). A jump's
ends are this far in from the edges, on their own ground */
#define AE_BOTS_TRAVEL_JUMP_PULL 0.3f
/* its landing is this far in, so that a jump that falls a little short still lands on it */
#define AE_BOTS_TRAVEL_JUMP_LAND 1.0f
/* a jump across less than this is wanted only for a rise of a step or more (a pad a step above its neighbour) */
#define AE_BOTS_TRAVEL_JUMP_SHORTEST 1.2f
#define AE_BOTS_TRAVEL_JUMP_STEP 0.3f
boolean ae_bots_travel_jump_ends(real across, real rise);
/* A BOT ON A CLOSED SURFACE (Digsite's Imminent 11-09: a scenery piece's rail across the spawn platform's surface closed it as
"in the way", and the bot stood on it the whole game, no search able to start, its way out only an escape step away from the
object): a surface the map has walkable and the bot's own copy has closed, with room above the bot's feet (a floor under a
wall has none), is open again. Headroom the bot needs, world units */
#define AE_BOTS_TRAVEL_REOPEN_HEADROOM 1.5f
/* but only once it has stood on it this long (the escape step away from what closed it, a second at a time, takes a bot off a
surface beside a blast door, which must stay closed: Sidewinder's, 78 stuck reports at its door when it was opened at once) */
#define AE_BOTS_TRAVEL_REOPEN_TICKS 150
boolean ae_bots_travel_reopen_wanted(boolean closed_now, boolean map_walkable, boolean roofed, long stood_ticks);
/* DROP LINKS FOUND (Digsite's Weld 11-09: a team's spawn on a platform 2.8 above the floor, a piece no link leaves, so that
its bot stood at the ledge the whole game: the jump links go up or across by a crouch jump's rise at most, none down): from a
piece no route leaves, a one way jump off an edge onto reached ground below it, when the drop is a short fall (no falling
damage in AE's gametypes; the others hurt a little) and the landing is out from the edge by how far the fall carries a
running bot: DROP_OUT_BASE and DROP_OUT_PER_UNIT of the drop */
#define AE_BOTS_TRAVEL_DROP_LEAST 0.6f
#define AE_BOTS_TRAVEL_DROP_MOST 6.0f
#define AE_BOTS_TRAVEL_DROP_OUT_BASE 1.2f
#define AE_BOTS_TRAVEL_DROP_OUT_PER_UNIT 0.3f
boolean ae_bots_travel_drop_ends(real drop);
/* how far out from the edge the landing is looked for, for a drop (world units) */
real ae_bots_travel_drop_out(real drop);
/* whether one probe step (rise from the last ground to this one) is walked */
boolean ae_bots_travel_walk_step(real rise);
/* GOALS NO ROUTE REACHES (Digsite 11-09: 19% of the stuck reports, a bot on one piece of the walkable surfaces with a goal
on another that no walk, link or drop joins it to: Boarding Action's decks, Chiron's side rooms, Platoe). The pieces and
links are the walkable surfaces' as found, which a game's climbs and hops sometimes beat (Sidewinder's bases are goals the
graph says no route to, and bots get there), so a goal is not given up on the graph's say alone: a bot that stands stuck
on a goal on a piece its own reaches by no route has that piece (from its own) banned for GOAL_BAN_TICKS, and the goals it
chooses on a banned piece are swapped for a roaming place its piece reaches, held for GOAL_HOLD_TICKS before its goals are
chosen again. (A bot's climbs and hops beat the graph's say where the pieces are small, Temple's raised spots, so the ban
waits for the stuck report whatever the pieces' size.) */
#define AE_BOTS_TRAVEL_REACH_PIECES 256
#define AE_BOTS_TRAVEL_GOAL_HOLD_TICKS (15 * AE_BOTS_TRAVEL_TICKS_PER_SECOND)
#define AE_BOTS_TRAVEL_GOAL_BAN_TICKS (90 * AE_BOTS_TRAVEL_TICKS_PER_SECOND)
#define AE_BOTS_TRAVEL_GOAL_BANS 4
/* the pieces a bot on start reaches by the links (from[i], to[i], -1 for an unknown end; a link taken in its direction),
start first, up to maximum into reached; how many */
int ae_bots_travel_reach_set(int link_count, long const *from, long const *to, long start, long *reached, int maximum);
/* whether a goal on goal_piece is cut off from a bot on start_piece, whose reach set is reached[count]: both pieces known
(not -1), the goal's not in the set */
boolean ae_bots_travel_goal_cut_off(long start_piece, long goal_piece, long const *reached, int count);
/* the goal pieces banned for a bot, each from the piece it stood on */
struct ae_bots_travel_bans
{
	long start[AE_BOTS_TRAVEL_GOAL_BANS], piece[AE_BOTS_TRAVEL_GOAL_BANS], until[AE_BOTS_TRAVEL_GOAL_BANS];
};
/* (a pair again: its time renewed; a full table: the one that ends first goes) */
void ae_bots_travel_ban_add(struct ae_bots_travel_bans *bans, long start_piece, long goal_piece, long now);
boolean ae_bots_travel_banned(struct ae_bots_travel_bans const *bans, long start_piece, long goal_piece, long now);

/* the route between pieces: the links' pieces (from[i] its entry's, to[i] its exit's), usable[i] whether link i may be
taken first now; the first link of a route from start to goal with the fewest links (the lowest index of a tie), -1
for none. Pieces are any numbers but -1 */
#define AE_BOTS_TRAVEL_ROUTE_NODES 256
int ae_bots_travel_route(int link_count, long const *from, long const *to, unsigned char const *usable, long start,
	long goal);

/* LONG WAYS (Danger Canyon's lower middle: a wall between the two flags, 14 units straight, 150 to 230 by the walk
round the canyon's back): the engine's search gives up on a way more than ten times the straight line (path.c's
"never close enough") or longer than its 1024 surfaces, and ends at the place nearest the goal in a straight line,
the wall's foot, where the bot then walks into the wall. When such a search ends no nearer by the walk, the bots'
own walk over the whole piece (each surface's walk to the goal, the way it goes on) gives a VIA on the way, about
VIA_REACH along it, which the search reaches at once; there the bot searches again. A search short of its goal that
leaves the bot GAIN_LEAST nearer by the walk is kept (a long way across a large level, searched again from its
end). */
#define AE_BOTS_TRAVEL_VIA_REACH 25.0f
#define AE_BOTS_TRAVEL_GAIN_LEAST 4.0f
/* whether a search that ended short of the goal gets the bot nearer by the walk: the walk to the goal from where it
starts and from where the search ended (a negative walk: no walk from there) */
boolean ae_bots_travel_partial_gains(real start_walk, real end_walk);
/* the via on a way: walked[i], the walk from the bot to the way's i-th point (rising), count points; the last point
within the reach, the first if none is (-1 for no points) */
int ae_bots_travel_way_via(real const *walked, int count, real reach);

/* A MOVING MACHINE IN THE WAY (a fan's blades, a door that swings): a bot ran into it and the object-shaped closing
found no walkable surface to close (the blades are gone between one tick's rays and the next). The walkable surfaces
whose middle lies within the machine's bounding sphere are closed then, the radius the sphere's but at least MIN and at
most MAX (Weld 10-28: a small fan stopped bots a game long) */
#define AE_BOTS_TRAVEL_DEVICE_BLOCK_MIN 0.5f
#define AE_BOTS_TRAVEL_DEVICE_BLOCK_MAX 2.0f
real ae_bots_travel_device_block_radius(real bounding_radius);

/* STUCK ON ONE GOAL: the stuck reports of one goal are counted across the times the bot moved in between (the plain
three in a row never came where it moved a step and stuck again); a count not added to for GOAL_STUCK_FORGET ticks
(20 seconds) starts again. The goal is given up at GOAL_STUCK_GIVE_UP. */
#define AE_BOTS_TRAVEL_GOAL_STUCK_GIVE_UP 4
#define AE_BOTS_TRAVEL_GOAL_STUCK_FORGET 600
/* the count after one more report: count was reported last at last_tick, now is now */
int ae_bots_travel_goal_stuck_next(int count, long last_tick, long now);
boolean ae_bots_travel_goal_stuck_gives_up(int count);

/* TRAPPED (owner 2026-11-13: "move it out"): a bot stuck on ground the paths do not know (no walkable piece of the map's
main ones), with no route, that has not got TRAP_MOVED units from where it first stuck there in TRAP_TICKS (20 s), is moved
to the nearest walkable spot of a main piece (at least TRAP_PIECE_LEAST surfaces) within TRAP_REACH. The servers' Danger
Canyon held bots on a ledge by its centre (walls about, the canyon below) for up to 19 minutes; any such pocket on any map. */
#define AE_BOTS_TRAVEL_TRAP_TICKS 600
#define AE_BOTS_TRAVEL_TRAP_MOVED 3.0f
#define AE_BOTS_TRAVEL_TRAP_REACH 15.0f
#define AE_BOTS_TRAVEL_TRAP_PIECE_LEAST 50
/* whether the bot is to be moved out: off the paths, how far it got from where it first stuck there, ticks since then */
boolean ae_bots_travel_trapped(boolean off_paths, boolean has_route, real moved, long since_ticks);

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

/* A FIGHT'S SLOW WALK (HOLD, SUPPRESS: throttle 0.2 / 0.35) is by choice while the bot moves, and stuck only when it
is PINNED: sampled about once a second (bots_bot_unstick, a bot that moved under 0.4 in a second), the bot has stayed
within AE_BOTS_TRAVEL_SLOW_RADIUS of where the run started, nearly at rest at each sample (a bot strafing to fire is
not at rest), AE_BOTS_TRAVEL_SLOW_SAMPLES samples in a row. The gap between samples over AE_BOTS_TRAVEL_SLOW_GAP_TICKS
(it moved in between, or was not slow) starts the run again. */
#define AE_BOTS_TRAVEL_SLOW_RADIUS 1.0f
#define AE_BOTS_TRAVEL_SLOW_SAMPLES 5
#define AE_BOTS_TRAVEL_SLOW_GAP_TICKS 60
#define AE_BOTS_TRAVEL_SLOW_REST_SPEED 0.012f	/* units a tick */
struct ae_bots_travel_slow
{
	real x, y, z;
	long last_tick;
	int still;
};
/* one sample at position (x, y, z) with the horizontal speed (units a tick): nonzero when the bot is pinned (the run
starts again after) */
int ae_bots_travel_slow_pinned(struct ae_bots_travel_slow *slow, long now, real x, real y, real z, real speed);

#endif /* __AE_BOTS_TRAVEL_RULES_H */
