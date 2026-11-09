/* The 'AECW' block of a gametype file (ae_weapon_selection_block.h). */
#include "cseries.h"
#include "tag_files/files.h"
#include "saved games/saved_game_files.h"
#include "ae_weapon_selection_block.h"

#include <string.h>
#include <xtl.h>

typedef char ae_weapon_selection_block_fits[AE_WEAPON_SELECTION_AECW_OFFSET + AE_WEAPON_SELECTION_AECW_WINDOW <= 0x100 &&
	AE_WEAPON_SELECTION_AECW_SIZE <= AE_WEAPON_SELECTION_AECW_WINDOW && AE_WEAPON_SELECTION_AECW_OFFSET >= 0x7C ? 1 : -1];
typedef char ae_weapon_selection_block_signature[sizeof(XCALCSIG_SIGNATURE) == 20 ? 1 : -1];

#define SIGNED_SIZE (AE_WEAPON_SELECTION_AECW_SIZE - 20)

int ae_weapon_selection_block_read(unsigned char const *block, struct ae_weapon_selection_bytes *bytes)
{
	unsigned char const *data = block + AE_WEAPON_SELECTION_AECW_OFFSET;
	XCALCSIG_SIGNATURE checksum;

	memset(bytes->aecw, 0, sizeof(bytes->aecw));
	bytes->aecw_present = 0;
	saved_game_file_generate_checksum((byte *)data, (word)SIGNED_SIZE, &checksum);
	if (memcmp(&checksum, data + SIGNED_SIZE, sizeof(checksum)))
		return 0;
	return ae_weapon_selection_aecw_decode(bytes, data);
}

void ae_weapon_selection_block_write(unsigned char *block, struct ae_weapon_selection_bytes const *bytes)
{
	unsigned char *data = block + AE_WEAPON_SELECTION_AECW_OFFSET;

	/* (the whole proven-free window: no byte of an older, longer block outlives this write) */
	memset(data, 0, AE_WEAPON_SELECTION_AECW_WINDOW);
	if (!ae_weapon_selection_aecw_encode(bytes, data))
		return;
	saved_game_file_generate_checksum(data, (word)SIGNED_SIZE, (XCALCSIG_SIGNATURE *)(data + SIGNED_SIZE));
}

void ae_weapon_selection_block_carry(unsigned char *block, unsigned char const *old_block)
{
	struct ae_weapon_selection_bytes bytes;

	if (ae_weapon_selection_block_read(old_block, &bytes))
		ae_weapon_selection_block_write(block, &bytes);
}

void ae_weapon_selection_block_match(unsigned char *expected, unsigned char const *file_block)
{
	ae_weapon_selection_block_carry(expected, file_block);
}
