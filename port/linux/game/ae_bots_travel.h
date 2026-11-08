/*
AE_BOTS_TRAVEL.H

How the bots get around a map (notes/specs/2026-10-16-bots-infinite.md area 2): the glue in bots.c's path search,
path following and tick (ae_bots_travel.inc, at bots.c's end through ae_bots_lan.inc; its AE hook lines are listed in
port/linux/game/ae_hooks.txt) and the engine's side (ae_bots_travel.c). The choices are ae_bots_travel_rules.c's.

- TELEPORTERS: the map's teleporter pads (netgame flags: a sender's channel is its team index, and the engine sends
  a player standing within half a unit of a sender to the first receiver of that channel) are links a bot's route
  may take: when the walk its search found is long (or finds no way) and a sender near it sends to near its goal, it
  searches to the pad instead, steps on the pad's centre and goes on from the receiver. Not back through the pad it
  came out at for a while.
- LAYOUT HINTS (the map's layout file, ae_layout_rules.h): `bot jump at A to B` is a link too (walk to A, face B,
  jump, on from where it lands), also taken when a bot's way passes within a unit of A moving towards B;
  `bot avoid P radius r` closes the bots' walkable surfaces there and drops the roaming places in it.
- HEALTH PACKS: with a gametype HEALTH whose health does not all come back (HALO CE, REACH) a hurt bot that is not
  fighting goes by a pack in reach on its way (a detour: its goal stays), the more hurt the farther; never to a pack
  that is not there now (taken, not yet back).
- STUCK SPOTS: where bots stood stuck, gathered, on halo.log's minute line ("bots: travel ..."), with the
  teleports, the jumps and the packs taken, and how long the bots took to far goals.
- debug.bot_links (true): false takes no links (the teleports are still counted: for before/after tests);
  debug.bot_hints: player 1's jumps logged as ready `bot jump` lines, a `bot avoid` line where player 1 crouches
  still a second, and the map's layout header line.
*/

#ifndef __AE_BOTS_TRAVEL_H
#define __AE_BOTS_TRAVEL_H

struct bots_bot;
struct bots_tick;
struct path_state;

/* (bots.c's bots_prepare_map, at its end) the map's links, avoid zones and their surfaces */
void ae_bots_travel_prepare(void);
/* (bots.c's bots_search, first) TRUE when the search was made here: to a link's entry or a pack, or the goal's,
then a link looked at */
boolean ae_bots_travel_search(struct bots_bot *bot, long unit_index);
/* (bots.c's bots_search, after the search) a search short of its goal and no nearer it by the walk, while the goal is
on the bot's piece of the walkable surfaces: searched again to a via on the walk (ae_bots_travel_rules.h LONG WAYS) */
void ae_bots_travel_long_way(struct bots_bot *bot, struct path_state *state, real_point3d const *start, long start_surface,
	boolean reached);
/* (bots.c's bots_search, the path made) the path to a via ends there: the bot searches again at its end */
void ae_bots_travel_long_way_path(struct bots_bot *bot);
/* (0.4.0) climb mode's straight line to the goal runs over a drop (ground 1 and 2.5 u ahead): it ends */
boolean ae_bots_travel_climb_blocked(struct bots_bot *bot, real_point3d const *position);
/* (bots.c's bots_follow_path) TRUE with the direction to walk: onto a pad, a jump, to a pack */
boolean ae_bots_travel_follow(struct bots_bot *bot, long unit_index, real_point3d const *position, real *direction);
/* (bots.c's bots_bot_update, before it keeps the bot's position) a teleport seen, a pack looked for, the times */
void ae_bots_travel_tick(struct bots_bot *bot, long bot_index, struct bots_tick *tick);
/* (bots.c's bots_bot_unstick, its test) whether a bot that moved too little in the last second counts as stuck: not
while it fights and its moves were the fight's own strafing (no objective's path taken), nor in the pre-game countdown */
boolean ae_bots_travel_may_stick(struct bots_bot *bot, struct bots_tick *tick);
/* (bots.c's bots_bot_unstick) the bot stood stuck where it is */
void ae_bots_travel_stuck(struct bots_bot *bot, struct bots_tick *tick);
/* (bots.c's bots_update_actions, first) debug.bot_hints: player 1's jumps and crouches logged as layout lines */
void ae_bots_travel_author_tick(void);

/* ---------- the engine's side (ae_bots_travel.c) */

enum
{
	AE_BOTS_TRAVEL_LINK_TELEPORTER = 0,
	AE_BOTS_TRAVEL_LINK_JUMP,
	/* found from the level as the game runs (ae_bots_travel.inc): stairs or a ramp between two pieces of the walkable
	surfaces, walked straight */
	AE_BOTS_TRAVEL_LINK_WALK
};

#define AE_BOTS_TRAVEL_MAXIMUM_LINKS 96
#define AE_BOTS_TRAVEL_MAXIMUM_AVOIDS 64
#define AE_BOTS_TRAVEL_MAXIMUM_PACKS 32

struct ae_bots_travel_link
{
	short kind;
	/* the netgame flags (a teleporter's), or the layout file's line (a jump's) */
	long source;
	long target;
	real_point3d entry;
	real_point3d exit;
};

struct ae_bots_travel_avoid
{
	real_point3d point;
	real radius;
	short line;
};

/* the map's links: its teleporter pads' (a sender and the receiver it sends to), then the layout's jumps (the walk
links are found later, by ae_bots_travel.inc) */
long ae_bots_travel_read_links(struct ae_bots_travel_link *links, long maximum);
/* the layout's avoid zones */
long ae_bots_travel_read_avoids(struct ae_bots_travel_avoid *avoids, long maximum);
/* the gametype's HEALTH as ae_bots_travel_rules.h knows it (AE_BOTS_TRAVEL_HEALTH_...) */
int ae_bots_travel_health_style(void);
/* the health packs lying in the world now (none held or taken): their objects and positions */
long ae_bots_travel_read_packs(long *objects, real_point3d *positions, long maximum);
/* whether the object is a health pack lying in the world now */
boolean ae_bots_travel_pack_there(long object_index);
/* debug.bot_links: whether the bots take links */
boolean ae_bots_travel_links_on(void);

#endif /* __AE_BOTS_TRAVEL_H */
