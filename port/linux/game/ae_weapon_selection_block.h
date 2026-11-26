/* The 'AECW' block of a gametype file (ae_weapon_selection.h: the rules; design section 7.3): at offset 0x80 of the
512 bytes, 40 bytes (version 3): magic, version, size, the 12-byte payload, a 20-byte signature of the first 20 bytes.
A block that fails is none; only version 3 is read. */
#ifndef AE_WEAPON_SELECTION_BLOCK_H
#define AE_WEAPON_SELECTION_BLOCK_H

#include "ae_weapon_selection.h"

/* the stored AECW of a file taken into the block a save of its variant would write (`expected`): the hook of
playlist_profile_matches, so a seeded gametype that holds an AECW does not read as changed for it alone. The SEEDS step
(owner: last) gives seed rows their own AECW and compares it here. Nothing is written when the file holds none */
void ae_weapon_selection_block_match(unsigned char *expected, unsigned char const *file_block);
/* TRUE and the payload when the 512 bytes hold a whole, signed block */
int ae_weapon_selection_block_read(unsigned char const *block, struct ae_weapon_selection_bytes *bytes);
/* writes the block into the 512 bytes (clearing the whole 0x80-0x100 window first); nothing is written for an empty payload */
void ae_weapon_selection_block_write(unsigned char *block, struct ae_weapon_selection_bytes const *bytes);
/* the block of old_block (when whole, version 3) written into block: every save carries it over (the pinned hazard: a save
by this build never drops the toggle, set or START an older save held) */
void ae_weapon_selection_block_carry(unsigned char *block, unsigned char const *old_block);

#endif
