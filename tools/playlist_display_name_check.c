/*
PLAYLIST_DISPLAY_NAME_CHECK.C

A check of the own gametypes' display name block (source/saved games/
playlist_display_name.c), built with the game's flags by tools/
test_linux_port.py with that file alone (included here) and a stand-in
signature (the real one, a SHA-1, is tested through the game's own files):

- round trip of a name; an absent block (zeros), a wrong magic, version or
  size, and a wrong signature are no name; a flipped byte of the name too;
- a name longer than 31 characters is cut at 31 (and reads back as such); a
  name of exactly 31 is whole; an empty or NULL name clears the block
  (all zeros); a control character ends a name; a character past 16 bits is
  a question mark;
- writing the block changes only its own bytes (0x140 .. 0x19B): the
  variant's 0..0x67, its signature, and the PC options' 'GPVO' block
  (0x100 .. 0x137) and the rest are untouched, and it ends inside the 512;
- carrying: a whole block of the old bytes arrives in the new; none, a torn
  one and an empty one carry nothing, and the new bytes outside the block
  are untouched.

Prints the failures, or PASS.
*/

#define BUILDING_CSERIES
#include "../source/saved games/playlist_display_name.c"

#include <stdio.h>
#include <string.h>

#undef printf

/* (a stand-in for the XDK signature: any function of the bytes) */
void saved_game_file_generate_checksum(
	void const *buffer,
	word buffer_size,
	struct _XCALCSIG_SIGNATURE *checksum)
{
	byte const *bytes = buffer;
	byte *out = (byte *)checksum;
	unsigned long hash = 2166136261u;
	word index;

	for (index = 0; index < buffer_size; index++)
		hash = (hash ^ bytes[index]) * 16777619u;
	for (index = 0; index < sizeof(*checksum); index++)
	{
		hash = (hash ^ index) * 16777619u;
		out[index] = (byte)(hash >> 8);
	}
}

static int failures = 0;

static void check(int ok, char const *what)
{
	if (!ok)
	{
		printf("FAIL: %s\n", what);
		failures++;
	}
}

static void fill(byte *block)
{
	int index;

	for (index = 0; index < SAVED_GAME_FILE_BLOCK_SIZE; index++)
		block[index] = (byte)(0x41 + index % 7);
}

static void wide(wchar_t *out, char const *text, int count)
{
	int index;

	for (index = 0; index < count; index++)
		out[index] = (wchar_t)(unsigned char)text[index % (int)strlen(text)];
	out[count] = 0;
}

int main(void)
{
	byte block[SAVED_GAME_FILE_BLOCK_SIZE];
	byte other[SAVED_GAME_FILE_BLOCK_SIZE];
	byte original[SAVED_GAME_FILE_BLOCK_SIZE];
	wchar_t name[PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 1];
	wchar_t long_name[80];
	int index;
	int outside;

	check(PLAYLIST_DISPLAY_NAME_OFFSET >= 0x100 + 8 + 0x1C + 20, "the block starts after the GPVO block");
	check(PLAYLIST_DISPLAY_NAME_OFFSET + PLAYLIST_DISPLAY_NAME_BLOCK_SIZE <= SAVED_GAME_FILE_BLOCK_SIZE,
		"the block fits in 512 bytes");

	/* absent */
	memset(block, 0, sizeof(block));
	check(!playlist_display_name_from_block(block, name) && name[0] == 0, "zeros are no name");

	/* round trip, and only its own bytes change */
	fill(block);
	memcpy(original, block, sizeof(block));
	playlist_display_name_to_block(block, L"Big Team Battle");
	check(playlist_display_name_from_block(block, name) && !wcscmp(name, L"Big Team Battle"), "round trip");
	outside = 0;
	for (index = 0; index < SAVED_GAME_FILE_BLOCK_SIZE; index++)
	{
		int inside = index >= PLAYLIST_DISPLAY_NAME_OFFSET &&
			index < PLAYLIST_DISPLAY_NAME_OFFSET + PLAYLIST_DISPLAY_NAME_BLOCK_SIZE;

		if (!inside && block[index] != original[index])
			outside++;
	}
	check(outside == 0, "no byte outside the block changed (0..0x67, GPVO, the rest)");

	/* tampering */
	memcpy(other, block, sizeof(block));
	other[PLAYLIST_DISPLAY_NAME_OFFSET + 8] ^= 1;
	check(!playlist_display_name_from_block(other, name) && name[0] == 0, "a flipped name byte fails the signature");
	memcpy(other, block, sizeof(block));
	other[PLAYLIST_DISPLAY_NAME_OFFSET + PLAYLIST_DISPLAY_NAME_BLOCK_SIZE - 1] ^= 1;
	check(!playlist_display_name_from_block(other, name), "a flipped signature byte fails");
	memcpy(other, block, sizeof(block));
	other[PLAYLIST_DISPLAY_NAME_OFFSET] ^= 1;
	check(!playlist_display_name_from_block(other, name), "a wrong magic fails");
	memcpy(other, block, sizeof(block));
	other[PLAYLIST_DISPLAY_NAME_OFFSET + 4] = 2;
	check(!playlist_display_name_from_block(other, name), "a wrong version fails");
	memcpy(other, block, sizeof(block));
	other[PLAYLIST_DISPLAY_NAME_OFFSET + 6] = 1;
	check(!playlist_display_name_from_block(other, name), "a wrong size fails");

	/* lengths */
	wide(long_name, "ABCDEFGHIJ", 70);
	playlist_display_name_to_block(block, long_name);
	check(playlist_display_name_from_block(block, name) && wcslen(name) == 31 && !wcsncmp(name, long_name, 31),
		"a long name is cut at 31");
	wide(long_name, "ABCDEFGHIJ", 31);
	playlist_display_name_to_block(block, long_name);
	check(playlist_display_name_from_block(block, name) && !wcscmp(name, long_name), "31 characters whole");
	wide(long_name, "ABCDEFGHIJ", 32);
	playlist_display_name_to_block(block, long_name);
	check(playlist_display_name_from_block(block, name) && wcslen(name) == 31, "32 characters cut to 31");
	playlist_display_name_to_block(block, L"X");
	check(playlist_display_name_from_block(block, name) && !wcscmp(name, L"X"), "one character");

	/* empty and NULL clear */
	fill(block);
	playlist_display_name_to_block(block, L"Abc");
	playlist_display_name_to_block(block, L"");
	check(!playlist_display_name_from_block(block, name), "an empty name is none");
	for (index = PLAYLIST_DISPLAY_NAME_OFFSET; index < PLAYLIST_DISPLAY_NAME_OFFSET + PLAYLIST_DISPLAY_NAME_BLOCK_SIZE; index++)
		if (block[index])
			break;
	check(index == PLAYLIST_DISPLAY_NAME_OFFSET + PLAYLIST_DISPLAY_NAME_BLOCK_SIZE, "empty clears the block to zeros");
	playlist_display_name_to_block(block, L"Abc");
	playlist_display_name_to_block(block, NULL);
	check(!playlist_display_name_from_block(block, name), "NULL clears");

	/* control character, wide character */
	playlist_display_name_to_block(block, L"AB\nCD");
	check(playlist_display_name_from_block(block, name) && !wcscmp(name, L"AB"), "a control character ends the name");
	{
		wchar_t emoji[3];

		emoji[0] = L'A';
		emoji[1] = (wchar_t)0x1F600;
		emoji[2] = 0;
		playlist_display_name_to_block(block, emoji);
		check(playlist_display_name_from_block(block, name) && !wcscmp(name, L"A?"), "a character past 16 bits is a question mark");
	}
	{
		wchar_t accent[3];

		accent[0] = (wchar_t)0x00E9;
		accent[1] = (wchar_t)0x4E2D;
		accent[2] = 0;
		playlist_display_name_to_block(block, accent);
		check(playlist_display_name_from_block(block, name) && name[0] == 0x00E9 && name[1] == 0x4E2D, "16-bit characters kept");
	}

	/* carrying */
	fill(other);
	memcpy(original, other, sizeof(other));
	memset(block, 0, sizeof(block));
	playlist_display_name_to_block(block, L"Carried Over");
	playlist_display_name_carry(other, block);
	check(playlist_display_name_from_block(other, name) && !wcscmp(name, L"Carried Over"), "carry: the name arrives");
	outside = 0;
	for (index = 0; index < SAVED_GAME_FILE_BLOCK_SIZE; index++)
	{
		int inside = index >= PLAYLIST_DISPLAY_NAME_OFFSET &&
			index < PLAYLIST_DISPLAY_NAME_OFFSET + PLAYLIST_DISPLAY_NAME_BLOCK_SIZE;

		if (!inside && other[index] != original[index])
			outside++;
	}
	check(outside == 0, "carry: the new bytes outside the block are untouched");
	memset(block, 0, sizeof(block));
	fill(other);
	playlist_display_name_carry(other, block);
	check(!playlist_display_name_from_block(other, name), "carry: an old block with none carries none");
	memset(block, 0, sizeof(block));
	playlist_display_name_to_block(block, L"Torn");
	block[PLAYLIST_DISPLAY_NAME_OFFSET + 9] ^= 0x10;
	memset(other, 0, sizeof(other));
	playlist_display_name_carry(other, block);
	check(!playlist_display_name_from_block(other, name), "carry: a torn old block carries none");
	memset(block, 0, sizeof(block));
	playlist_display_name_to_block(block, L"Y");
	playlist_display_name_to_block(block, L"");
	memset(other, 0, sizeof(other));
	playlist_display_name_carry(other, block);
	check(!playlist_display_name_from_block(other, name), "carry: an emptied block carries none");

	if (failures)
		return 1;
	printf("PASS\n");
	return 0;
}
