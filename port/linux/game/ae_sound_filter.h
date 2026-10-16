/*
AE_SOUND_FILTER.H

NHE SOUND FILTER (audio.nhe_sound_filter, Settings > Audio): this machine
mutes the sounds Halo 1: NHE 1.0 took out of its maps' tags (notes: NHE's
release notes): the plasma pistol's charging hum and the needler's firing.
Any map: matched by the sound tags' names (the stock maps' paths). A
player's own setting: nothing goes over the network.
*/

#ifndef AE_SOUND_FILTER_H
#define AE_SOUND_FILTER_H

/* (sound_manager.c, each playing sound's channel update: AE hook lines) 0 for a muted sound, else 1 */
float ae_sound_filter_gain(long definition_index);
/* whether a sound tag's name is one the filter mutes (pure) */
int ae_sound_filter_muted_name(const char *name);

#endif
