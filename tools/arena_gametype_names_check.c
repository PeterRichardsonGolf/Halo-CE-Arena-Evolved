/*
ARENA_GAMETYPE_NAMES_CHECK.C

A check of the seeded gametypes' names table (source/saved games/
arena_gametype_names.c), built with the game's flags by
tools/test_linux_port.py with that file alone (included here, for its
tables):

- every stored name at most 11 printable ASCII characters, upper case, and
  unique among the table's and the aliases' (letters' case aside);
- every display name at most 31 printable ASCII characters, upper case,
  unique, and every description present and at most 160 characters;
- every alias's successor in the table; an alias's info its successor's set,
  order, team and engine, its own stored name as display_name, no
  description, not special;
- the sets: AE 19, AE COMP 5, NHE 23, TRAINING 1 (48), each set's orders
  0..n-1 in the table's order, NHE's in its own order (TS 50 first,
  CTF WIZARD last), and the set names;
- lookups letters' case aside; an unknown name FALSE, displayed as it is;
  display names cut to the buffer given.

Prints the failures, or PASS.
*/

/* (the C library's own string functions, not cseries' stand-ins) */
#define BUILDING_CSERIES
#include "../source/saved games/arena_gametype_names.c"

#include <stdio.h>
#include <string.h>

/* (the C library's own, not the game's stand-ins for MSVC's formats,
port/linux/include/stdio.h) */
#undef printf

static int failures = 0;

static void fail(char const *what, char const *name)
{
	printf("FAIL: %s: %s\n", what, name);
	failures++;
}

/* an ASCII name as a stored one (wide, NUL-filled, 12 characters) */
static void wide(char const *name, wchar_t out[12])
{
	short index;

	for (index = 0; index < 12; index++)
		out[index] = 0;
	for (index = 0; index < 11 && name[index]; index++)
		out[index] = (wchar_t)(unsigned char)name[index];
}

static int same_ascii_wide(char const *a, wchar_t const *b)
{
	short index;

	for (index = 0; a[index] || b[index]; index++)
	{
		if ((wchar_t)(unsigned char)a[index] != b[index])
			return 0;
	}
	return 1;
}

static int upper_case_case_aside_equal(char const *a, char const *b)
{
	for (;; a++, b++)
	{
		char x = (char)(*a >= 'a' && *a <= 'z' ? *a - 'a' + 'A' : *a);
		char y = (char)(*b >= 'a' && *b <= 'z' ? *b - 'a' + 'A' : *b);

		if (x != y)
			return 0;
		if (!x)
			return 1;
	}
}

static void check_text(char const *what, char const *text, size_t maximum)
{
	size_t index;

	if (!text || !text[0] || strlen(text) > maximum)
	{
		fail(what, text ? text : "(none)");
		return;
	}
	for (index = 0; text[index]; index++)
	{
		if (text[index] < ' ' || text[index] > '~' || (text[index] >= 'a' && text[index] <= 'z'))
		{
			fail(what, text);
			return;
		}
	}
}

int main(void)
{
	short const expected_counts[NUMBER_OF_ARENA_GAMETYPE_SETS] = { 0, 19, 5, 23, 1 };
	char const *const expected_set_names[NUMBER_OF_ARENA_GAMETYPE_SETS] = { "", "AE", "AE COMP", "NHE", "TRAINING" };
	short counts[NUMBER_OF_ARENA_GAMETYPE_SETS] = { 0 };
	short index;
	short other;
	struct arena_gametype_info info;
	wchar_t name[12];
	wchar_t display[40];

	/* the table */
	if (arena_gametype_catalog_count() != 48)
		fail("catalog count is not 48", "");
	for (index = 0; index < (short)NUMBEROF(arena_gametype_names); index++)
	{
		struct arena_gametype_name const *entry = &arena_gametype_names[index];

		check_text("stored name", entry->stored_name, ARENA_STORED_NAME_MAXIMUM);
		check_text("display name", entry->display_name, ARENA_DISPLAY_NAME_MAXIMUM);
		if (!entry->description || !entry->description[0] || strlen(entry->description) > 160)
			fail("description", entry->stored_name);
		if (entry->set <= _arena_gametype_set_none || entry->set >= NUMBER_OF_ARENA_GAMETYPE_SETS)
			fail("set", entry->stored_name);
		if (entry->game_engine < first_usable_game_engine_index || entry->game_engine > last_usable_game_engine_index)
			fail("game engine", entry->stored_name);
		if (strncmp(entry->display_name, entry->team ? "TEAM " : "FFA ", entry->team ? 5 : 4))
			fail("display name does not say TEAM or FFA as the entry", entry->display_name);
		if (entry->special && entry->set != _arena_gametype_set_ae)
			fail("special outside AE", entry->stored_name);
		if (arena_gametype_catalog_stored_name(index) != entry->stored_name)
			fail("catalog stored name", entry->stored_name);
		for (other = 0; other < index; other++)
		{
			if (upper_case_case_aside_equal(entry->stored_name, arena_gametype_names[other].stored_name))
				fail("stored name twice", entry->stored_name);
			if (!strcmp(entry->display_name, arena_gametype_names[other].display_name))
				fail("display name twice", entry->display_name);
		}
		for (other = 0; other < (short)NUMBEROF(arena_gametype_aliases); other++)
		{
			if (upper_case_case_aside_equal(entry->stored_name, arena_gametype_aliases[other].stored_name))
				fail("stored name is also an alias", entry->stored_name);
		}

		/* its info, and its order in its set */
		wide(entry->stored_name, name);
		if (!arena_gametype_info(name, &info) || info.set != entry->set || info.team != entry->team ||
			info.special != entry->special || info.game_engine != entry->game_engine ||
			info.display_name != entry->display_name || info.description != entry->description)
		{
			fail("info", entry->stored_name);
		}
		if (entry->set > 0 && entry->set < NUMBER_OF_ARENA_GAMETYPE_SETS && info.order != counts[entry->set]++)
			fail("order", entry->stored_name);
		arena_gametype_display_name(name, display, NUMBEROF(display));
		if (!same_ascii_wide(entry->display_name, display))
			fail("display name lookup", entry->stored_name);
	}
	for (index = 0; index < NUMBER_OF_ARENA_GAMETYPE_SETS; index++)
	{
		if (counts[index] != expected_counts[index])
			fail("set count", arena_gametype_set_name(index));
		if (strcmp(arena_gametype_set_name(index), expected_set_names[index]))
			fail("set name", expected_set_names[index]);
	}
	if (strcmp(arena_gametype_set_name(-1), "") || strcmp(arena_gametype_set_name(NUMBER_OF_ARENA_GAMETYPE_SETS), ""))
		fail("set name out of range", "");
	wide("TS 50", name);
	if (!arena_gametype_info(name, &info) || info.set != _arena_gametype_set_nhe || info.order != 0)
		fail("NHE's order starts at TS 50", "");
	wide("CTF WIZARD", name);
	if (!arena_gametype_info(name, &info) || info.order != 22)
		fail("NHE's order ends at CTF WIZARD", "");

	/* the aliases */
	for (index = 0; index < (short)NUMBEROF(arena_gametype_aliases); index++)
	{
		struct arena_gametype_alias const *alias = &arena_gametype_aliases[index];
		struct arena_gametype_info successor;
		wchar_t successor_name[12];

		check_text("alias", alias->stored_name, ARENA_STORED_NAME_MAXIMUM);
		for (other = 0; other < index; other++)
		{
			if (upper_case_case_aside_equal(alias->stored_name, arena_gametype_aliases[other].stored_name))
				fail("alias twice", alias->stored_name);
		}
		wide(alias->successor, successor_name);
		wide(alias->stored_name, name);
		if (!arena_gametype_info(successor_name, &successor) || arena_gametype_name_index(successor_name) == NONE)
		{
			fail("alias's successor not in the table", alias->stored_name);
			continue;
		}
		if (!arena_gametype_info(name, &info) || info.set != successor.set || info.order != successor.order ||
			info.team != successor.team || info.game_engine != successor.game_engine || info.special ||
			info.description != NULL || !info.display_name || strcmp(info.display_name, alias->stored_name))
		{
			fail("alias info", alias->stored_name);
		}
		arena_gametype_display_name(name, display, NUMBEROF(display));
		if (!same_ascii_wide(alias->stored_name, display))
			fail("alias displays its stored name", alias->stored_name);
	}

	/* lookups */
	wide("ae comp ts", name);
	if (!arena_gametype_info(name, &info) || strcmp(info.display_name, "TEAM AE COMP SLAYER"))
		fail("lookup with letters' case aside", "ae comp ts");
	wide("nhe train", name);
	if (!arena_gametype_info(name, &info) || info.set != _arena_gametype_set_training ||
		strcmp(info.display_name, "NHE TRAIN"))
	{
		fail("alias lookup with letters' case aside", "nhe train");
	}
	wide("My Slayer", name);
	info.set = 99;
	if (arena_gametype_info(name, &info) || info.set != 99)
		fail("unknown name", "My Slayer");
	arena_gametype_display_name(name, display, NUMBEROF(display));
	if (!same_ascii_wide("My Slayer", display))
		fail("unknown name displayed as it is", "My Slayer");
	/* (a full 11 characters with no NUL in the variant's 12th place) */
	wide("CTF 3C 7S R", name);
	name[11] = 'X';
	if (!arena_gametype_info(name, NULL))
		fail("11-character name", "CTF 3C 7S R");
	arena_gametype_display_name(name, display, 5);
	if (!same_ascii_wide("TEAM", display))
		fail("display name cut to the buffer", "CTF 3C 7S R");
	if (arena_gametype_info(NULL, &info))
		fail("NULL name", "");

	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("PASS: %d gametypes, %d aliases\n", (int)NUMBEROF(arena_gametype_names), (int)NUMBEROF(arena_gametype_aliases));
	return 0;
}
