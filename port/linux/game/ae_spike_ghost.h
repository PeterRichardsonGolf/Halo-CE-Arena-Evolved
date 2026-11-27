/*
AE_SPIKE_GHOST.H

SPIKE (local tests only): CMT's ghosts in place of the map's (ae_spike_ghost.c).
*/

#ifndef __AE_SPIKE_GHOST_H
#define __AE_SPIKE_GHOST_H

/* game_engine.c's remapping of a placed vehicle: the spike's ghost for the map's, else the same */
long ae_spike_ghost_remap(long definition_index);

#endif
