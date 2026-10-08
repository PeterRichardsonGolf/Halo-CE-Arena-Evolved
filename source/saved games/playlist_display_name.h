/*
PLAYLIST_DISPLAY_NAME.H

port: a player's own gametype's optional longer display name, kept in a
second block of the saved gametype's 512 bytes (after the PC options' 'GPVO'
block), the stored name (11 characters) unchanged.
*/

#ifndef __PLAYLIST_DISPLAY_NAME_H
#define __PLAYLIST_DISPLAY_NAME_H
#pragma once

#include "cseries/cseries.h"

enum
{
	/* characters, without the NUL */
	PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH = 31,
	PLAYLIST_DISPLAY_NAME_OFFSET = 0x140,
	PLAYLIST_DISPLAY_NAME_MAGIC = 0x4E444541, /* 'AEDN' little-endian */
	PLAYLIST_DISPLAY_NAME_VERSION = 1,
	/* the block: header (magic, version, size), 32 UTF-16 units, signature */
	PLAYLIST_DISPLAY_NAME_BLOCK_SIZE = 8 + 2 * (PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 1) + 20
};

/* TRUE, and name (at least MAXIMUM_LENGTH + 1 characters), when the 512-byte
block holds a whole display name block with a right signature and a name
that is not empty; else FALSE and name empty */
boolean playlist_display_name_from_block(
	byte const *block,
	wchar_t *name);
/* the block written into the 512-byte block (cut at 31 characters, at a
control character); an empty (or NULL) name clears the block's place */
void playlist_display_name_to_block(
	byte *block,
	wchar_t const *name);
/* the display name block of old_block (if it holds a whole one) written
into block: every save carries it over */
void playlist_display_name_carry(
	byte *block,
	byte const *old_block);

#endif
