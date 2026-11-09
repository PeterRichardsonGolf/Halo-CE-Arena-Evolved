/*
AE_ANNOUNCER.H

(spike) An announcer pack: <data root>/announcers/<pack>/<medal's key>.wav, 16 bit, chosen by HALO_ANNOUNCER_PACK. A medal with a clip
speaks it, one at a time; the map's own lines for the same events stay quiet. No pack, nothing changes.
*/

#ifndef __AE_ANNOUNCER_H
#define __AE_ANNOUNCER_H

/* a medal earned by a local player: its clip joins the queue */
void ae_announcer_medal(int medal);
/* each frame: the next clip when the one speaking has ended */
void ae_announcer_update(void);
/* a new map: the clips read now, the queue empty */
void ae_announcer_new_map(void);
/* whether the pack speaks this line of the map's multiplayer sounds in its place */
int ae_announcer_replaces(short multiplayer_sound);

#endif
