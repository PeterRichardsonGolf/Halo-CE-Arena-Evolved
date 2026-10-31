/*
AE_MCC_SOUND_NAMES.H

(Arena Evolved) an MCC stock map's sounds (MCC's halo1/maps/<map>.map) in Custom Edition's sounds.map, by name. MCC
plays its stock maps' audio from its own sound banks, so their sound tags' external samples name a sounds.map that MCC
does not ship; every one of those tags is in MCC's custom_edition\sounds.map by its path (Phase D, D0.2). There, a
resource is the sound tag itself: its 0xa4 bytes, its pitch ranges, then each range's permutations, each giving its
samples' size and their offset in the file (ce_resources.c reads the same for Custom Edition maps).
*/

#ifndef __AE_MCC_SOUND_NAMES_H
#define __AE_MCC_SOUND_NAMES_H

#define AE_MCC_SOUND_HEADER_SIZE 0xa4UL
#define AE_MCC_SOUND_PITCH_RANGES_OFFSET 0x98UL
#define AE_MCC_SOUND_PITCH_RANGE_SIZE 0x48UL
#define AE_MCC_PITCH_RANGE_PERMUTATIONS_OFFSET 0x3cUL
#define AE_MCC_SOUND_PERMUTATION_SIZE 0x7cUL
/* (a permutation's samples: their size, flags, then offset in the file) */
#define AE_MCC_PERMUTATION_SAMPLES_OFFSET 0x40UL

/* an MCC resource's name, <tag>__<pitch range>__<permutation>, split (1); 0 when it is not one, or the tag does not
fit tag_size */
int ae_mcc_sound_name_split(const char *name, char *tag, unsigned long tag_size, unsigned long *range,
	unsigned long *permutation);

/* a sound resource's (item, size bytes read whole) permutation: its samples' offset in the file and their size (1);
0 when the item has no such permutation or its counts pass its end */
int ae_mcc_sound_permutation(const unsigned char *item, unsigned long size, unsigned long range,
	unsigned long permutation, unsigned long *offset, unsigned long *bytes);

#endif
