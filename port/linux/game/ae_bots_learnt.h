/*
AE_BOTS_LEARNT.H

What the bots keep of a map from game to game (owner 2026-11-15, ask-me: "Keep stuck spots" and "Learn your jumps"; the
rules: ae_bots_learnt_rules.h). Each map has a file in the config folder, bots_learnt/<map file>.txt, read when the map is
ready and written when a game ends (or another map loads):
- drivers' wedges: the pieces of the walkable surfaces where drivers wedged or crashed; from the next game on, a driver's
  route pays more for them from the start (ae_path_cost.h LEARNT WEDGES), not only once it has wedged there again;
- stuck spots: the pieces where bots on foot stood stuck, from several bots or games; a route on foot pays more to walk
  onto them (a cost, never a wall: the only way there is still taken);
- deadly volumes (0.9.4): the map's trigger volumes a unit was killed in by the map (a script's kill volume): the edge
  guard takes a drop into one as lethal from the next map load (ae_bots_falls.inc);
- the people's jumps: a jump a person on this machine landed (rising or dropping 0.8 units or more) and lived 5 s after
  is a link the bots may take from the next map load (ae_bots_travel.h's links: taken only when it beats the walk);
  one the bots keep missing is dropped.
What a piece no longer gets fades game by game. Setting bots.learn_map (on); off: nothing read, used or written.
*/

#ifndef __AE_BOTS_LEARNT_H
#define __AE_BOTS_LEARNT_H

#include "cseries.h"

/* bots.learn_map */
boolean ae_bots_learnt_on(void);
/* (a map or game made ready: ae_bots_travel_prepare, ae_bots_vehicle_reset) the map's learnt state ready: the last
game's news kept and written, another map's file read */
void ae_bots_learnt_ready(void);
/* (bots_dispose_from_old_map) the map is gone: its game's news kept and written, the state unused until the next map
whose bots get ready (a map without bots, a client's, learns nothing and writes nothing) */
void ae_bots_learnt_map_gone(void);
/* (ae_bots_vehicle_reset, after the wedges are cleared) the learnt wedges into ae_path_cost's */
void ae_bots_learnt_apply_wedges(void);
/* (ae_bots_travel_prepare) the learnt stuck spots into ae_path_cost's foot penalties */
void ae_bots_learnt_apply_stucks(void);
/* the game's news: a driver's wedge or crash on a piece; a bot on foot stuck on a piece, at a spot */
void ae_bots_learnt_wedge(long surface);
void ae_bots_learnt_stuck(long surface, long bot_index, real x, real y, real z);
/* the people's kept jumps (from the file: this game's new ones count from the next map load) */
long ae_bots_learnt_jump_count(void);
boolean ae_bots_learnt_jump_get(long index, real_point3d *from, real_point3d *to);
/* a bot took the kept jump of these ends (its link's): made (landed where it should) or missed (not started, landed
off, too long, died in the air) */
void ae_bots_learnt_jump_result(real_point3d const *from, real_point3d const *to, boolean made);
/* (each game tick, ae_bots_travel_author_tick) the people's jumps watched, the seconds of play and driving counted, the
file written at the game's end; driving: a bot drove this tick (ae_bots_vehicle_drive_tick) */
void ae_bots_learnt_tick(void);
/* (0.9.4, ae_bots_falls.inc) the map's deadly trigger volumes kept from game to game: how many, the one at an index
(NONE past them), one found deadly or found not to be */
long ae_bots_learnt_deadly_count(void);
long ae_bots_learnt_deadly_get(long index);
void ae_bots_learnt_deadly(long volume, boolean deadly);
void ae_bots_learnt_drove(void);

#endif
