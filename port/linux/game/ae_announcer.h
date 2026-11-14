/*
AE_ANNOUNCER.H

An announcer pack: <data root>/announcers/<pack>/<key>.wav, 16 bit, chosen by game.announcer "pack:<pack>" (or HALO_ANNOUNCER_PACK).
A medal with a clip speaks it, one at a time, and so do the events it has a clip for, in the map's lines' place. No pack, nothing
changes (ae_announcer.c).
*/

#ifndef __AE_ANNOUNCER_H
#define __AE_ANNOUNCER_H

/* a medal earned by a local player: its clip joins the queue */
void ae_announcer_medal(int medal);
/* each frame: the next clip when the one speaking has ended */
void ae_announcer_update(void);
/* a new map: the clips read now, the queue empty */
void ae_announcer_new_map(void);
/* a kill: its killer's and victim's local indexes (-1 for none), betrayal, area damage, the damage's tag, the tick */
void ae_announcer_count_kill(int local_player, int victim_local, int betrayal, int area, long source, long tick);
/* whether the pack speaks this line of the map's multiplayer sounds in its place */
int ae_announcer_replaces(short multiplayer_sound);

#endif
