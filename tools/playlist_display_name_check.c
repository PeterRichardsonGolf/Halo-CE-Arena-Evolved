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
- a foreign block with 32 filled units (valid signature) reads as 31 characters and a NUL, nothing written
  past them; DEL, a C1 control and a surrogate end a name;
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

/* (the game's memory functions, which this check does not link) */
long csmemcmp(const void *p1, const void *p2, unsigned long size)
{
	unsigned char const *a = p1;
	unsigned char const *b = p2;

	while (size--)
	{
		if (*a != *b)
			return *a < *b ? -1 : 1;
		a++, b++;
	}
	return 0;
}

void *csmemset(void *buffer, long c, unsigned long size)
{
	unsigned char *bytes = buffer;

	while (size--)
		*bytes++ = (unsigned char)c;
	return buffer;
}

void *csmemcpy(void *destination, const void *source, unsigned long size)
{
	unsigned char *to = destination;
	unsigned char const *from = source;

	while (size--)
		*to++ = *from++;
	return destination;
}

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

/* (the game's wchar_t may be 16 bits: the C library's wide functions cannot be used) */
static int wlen(wchar_t const *text)
{
	int length = 0;

	while (text[length])
		length++;
	return length;
}

static int wsame(wchar_t const *a, wchar_t const *b)
{
	int index = 0;

	while (a[index] && a[index] == b[index])
		index++;
	return a[index] == b[index];
}

static int wprefix(wchar_t const *a, wchar_t const *b, int count)
{
	int index;

	for (index = 0; index < count; index++)
		if (a[index] != b[index])
			return 0;
	return 1;
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
	check(playlist_display_name_from_block(block, name) && wsame(name, L"Big Team Battle"), "round trip");
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
	check(playlist_display_name_from_block(block, name) && wlen(name) == 31 && wprefix(name, long_name, 31),
		"a long name is cut at 31");
	wide(long_name, "ABCDEFGHIJ", 31);
	playlist_display_name_to_block(block, long_name);
	check(playlist_display_name_from_block(block, name) && wsame(name, long_name), "31 characters whole");
	wide(long_name, "ABCDEFGHIJ", 32);
	playlist_display_name_to_block(block, long_name);
	check(playlist_display_name_from_block(block, name) && wlen(name) == 31, "32 characters cut to 31");
	playlist_display_name_to_block(block, L"X");
	check(playlist_display_name_from_block(block, name) && wsame(name, L"X"), "one character");

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
	check(playlist_display_name_from_block(block, name) && wsame(name, L"AB"), "a control character ends the name");
	if (sizeof(wchar_t) > 2)
	{
		wchar_t emoji[3];

		emoji[0] = L'A';
		emoji[1] = (wchar_t)0x1F600;
		emoji[2] = 0;
		playlist_display_name_to_block(block, emoji);
		check(playlist_display_name_from_block(block, name) && wsame(name, L"A?"), "a character past 16 bits is a question mark");
	}
	{
		wchar_t accent[3];

		accent[0] = (wchar_t)0x00E9;
		accent[1] = (wchar_t)0x4E2D;
		accent[2] = 0;
		playlist_display_name_to_block(block, accent);
		check(playlist_display_name_from_block(block, name) && name[0] == 0x00E9 && name[1] == 0x4E2D, "16-bit characters kept");
	}

	/* a foreign block with all 32 units filled (a valid signature proves nothing): never more than 31 characters
	and the NUL, nothing written past them */
	{
		byte *data = block + PLAYLIST_DISPLAY_NAME_OFFSET;
		wchar_t guarded[PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 2];
		int unit;

		memset(block, 0, sizeof(block));
		memcpy(data, "AEDN", 4);
		data[4] = 1;
		data[6] = 64;
		for (unit = 0; unit < 32; unit++)
		{
			data[8 + 2 * unit] = (byte)('A' + unit % 26);
			data[9 + 2 * unit] = 0;
		}
		saved_game_file_generate_checksum(data, 72, (struct _XCALCSIG_SIGNATURE *)(data + 72));
		for (unit = 0; unit < PLAYLIST_DISPLAY_NAME_MAXIMUM_LENGTH + 2; unit++)
			guarded[unit] = (wchar_t)0x5A5A;
		check(playlist_display_name_from_block(block, guarded) && wlen(guarded) == 31 &&
			guarded[31] == 0 && guarded[32] == (wchar_t)0x5A5A, "32 filled units: 31 characters, a NUL, nothing past");
	}

	/* DEL, a C1 control and a surrogate end a name */
	{
		wchar_t odd[4];

		odd[0] = L'A';
		odd[1] = (wchar_t)0x7F;
		odd[2] = L'B';
		odd[3] = 0;
		playlist_display_name_to_block(block, odd);
		check(playlist_display_name_from_block(block, name) && wsame(name, L"A"), "DEL ends the name");
		odd[1] = (wchar_t)0x85;
		playlist_display_name_to_block(block, odd);
		check(playlist_display_name_from_block(block, name) && wsame(name, L"A"), "a C1 control ends the name");
		odd[1] = (wchar_t)0xD83D;
		playlist_display_name_to_block(block, odd);
		check(playlist_display_name_from_block(block, name) && wsame(name, L"A"), "a surrogate ends the name");
	}

	/* carrying */
	fill(other);
	memcpy(original, other, sizeof(other));
	memset(block, 0, sizeof(block));
	playlist_display_name_to_block(block, L"Carried Over");
	playlist_display_name_carry(other, block);
	check(playlist_display_name_from_block(other, name) && wsame(name, L"Carried Over"), "carry: the name arrives");
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
