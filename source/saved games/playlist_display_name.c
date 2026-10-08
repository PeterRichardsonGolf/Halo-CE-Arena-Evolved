/*
PLAYLIST_DISPLAY_NAME.C

port: a player's own gametype's optional longer display name (see the
header). At PLAYLIST_DISPLAY_NAME_OFFSET: magic 'AEDN', version, size (the
name's bytes), 32 UTF-16 little-endian units (NUL-filled), and a signature
of those bytes as the saved game files' own (saved_game_file_generate_checksum).
Builds before it never read or write these bytes (their zeros are not a
block). The first 0x68 bytes and the 'GPVO' block are not touched.
*/

#include "cseries/cseries.h"
#include "saved games/playlist_display_name.h"
#include "saved games/saved_game_files.h"

#include <xtl.h>

struct playlist_display_name_header
{
	unsigned long magic;
	word version;
	word size;
};

typedef char verify_playlist_display_name_fits[
	PLAYLIST_DISPLAY_NAME_OFFSET + PLAYLIST_DISPLAY_NAME_BLOCK_SIZE <= SAVED_GAME_FILE_BLOCK_SIZE ? 1 : -1];
typedef char verify_playlist_display_name_header_size[
	sizeof(struct playlist_display_name_header) == 8 ? 1 : -1];

#define UNITS (PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 1)

boolean playlist_display_name_from_block(
	byte const *block,
	wchar_t *name)
{
	byte const *data = block + PLAYLIST_DISPLAY_NAME_OFFSET;
	struct playlist_display_name_header header;
	unsigned long size = sizeof(header) + 2 * UNITS;
	XCALCSIG_SIGNATURE checksum;
	short index;
	short length = 0;

	name[0] = 0;
	csmemcpy(&header, data, sizeof(header));
	if (header.magic != PLAYLIST_DISPLAY_NAME_MAGIC || header.version != PLAYLIST_DISPLAY_NAME_VERSION ||
		header.size != 2 * UNITS)
	{
		return FALSE;
	}
	saved_game_file_generate_checksum((byte *)data, (word)size, &checksum);
	if (csmemcmp(&checksum, data + size, sizeof(checksum)))
		return FALSE;
	for (index = 0; index < UNITS; index++)
	{
		word unit = (word)(data[sizeof(header) + 2 * index] | (data[sizeof(header) + 2 * index + 1] << 8));

		if (unit < 0x20)
			break;
		name[length++] = (wchar_t)unit;
	}
	name[length] = 0;
	return length > 0;
}

void playlist_display_name_to_block(
	byte *block,
	wchar_t const *name)
{
	byte *data = block + PLAYLIST_DISPLAY_NAME_OFFSET;
	struct playlist_display_name_header header;
	unsigned long size = sizeof(header) + 2 * UNITS;
	short index;

	csmemset(data, 0, PLAYLIST_DISPLAY_NAME_BLOCK_SIZE);
	if (!name || name[0] < 0x20)
		return;
	header.magic = PLAYLIST_DISPLAY_NAME_MAGIC;
	header.version = PLAYLIST_DISPLAY_NAME_VERSION;
	header.size = 2 * UNITS;
	csmemcpy(data, &header, sizeof(header));
	for (index = 0; index < PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH && name[index] >= 0x20; index++)
	{
		/* (16 bits: a character past them is a question mark) */
		word unit = (unsigned long)name[index] > 0xFFFF ? (word)'?' : (word)name[index];

		data[sizeof(header) + 2 * index] = (byte)(unit & 0xFF);
		data[sizeof(header) + 2 * index + 1] = (byte)(unit >> 8);
	}
	saved_game_file_generate_checksum(data, (word)size, (XCALCSIG_SIGNATURE *)(data + size));
}

void playlist_display_name_carry(
	byte *block,
	byte const *old_block)
{
	wchar_t name[UNITS];

	if (playlist_display_name_from_block(old_block, name))
		playlist_display_name_to_block(block, name);
}
