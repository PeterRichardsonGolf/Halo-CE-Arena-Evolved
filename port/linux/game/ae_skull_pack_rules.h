/*
AE_SKULL_PACK_RULES.H

The skull pictures' files, as pure rules (port/linux/tests/ae_skull_pack_rules_test.c runs them): the picture pack's index
(skullstexturepack.perm.bin) in the same layout as the medal pack's (ae_medal_pack_rules.h), its records named by the skull alone
(ANGER, THATSJUSTWRONG) instead of MEDAL_..., and which record is a Content Evolved skull's by its key. The pixels' file, the records'
checks and the block decoding are the medal pack's. The files are the player's own, read from their MCC install
(ae_skull_pack.c); nothing of them ships with the game, and every length and offset is checked against the file it sits in.
*/

#ifndef __AE_SKULL_PACK_RULES_H
#define __AE_SKULL_PACK_RULES_H

#include <stddef.h>

#include "ae_medal_pack_rules.h"

/* how many records the index holds (the row ends at the first record with no name of capitals): 0 for a file that is too short or
not an index */
int ae_skull_index_count(unsigned char const *index, size_t index_size);
/* record number i (0 up to the count): 1 when it is read, its name ended and of capitals and digits only */
int ae_skull_index_record(unsigned char const *index, size_t index_size, int i, struct ae_medal_record *record);
/* the first record that is the skull with this Content Evolved key (famine, thats_just_wrong): the case and the underscores are left out
of the match, and the few names MCC spells another way are known (bandanna reads BANDANA). -1 for none */
int ae_skull_index_find(unsigned char const *index, size_t index_size, char const *key);
/* moves an RGBA picture's pixels to its visible box (alpha greater than 8), returning its new size; 0 when no pixel is visible or
the arguments are not a picture */
int ae_skull_rgba_trim(unsigned char *rgba, int width, int height, int *trimmed_width, int *trimmed_height);

#endif /* __AE_SKULL_PACK_RULES_H */
