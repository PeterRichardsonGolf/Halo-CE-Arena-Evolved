/*
AE_ITEM_SETS_RULES.C

ITEM SETS' rules (ae_item_sets_rules.h). Plain C.
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "ae_item_sets_rules.h"

#define SEPARATOR "\xE2\x80\xBA"
#define DOT "\xE2\x80\xA2"
#define ELLIPSIS "\xE2\x80\xA6"

static char const *const kind_names[AE_IS_KINDS] = { "WEAPONS", "AI", "EQUIPMENT", "VEHICLES", "BIPEDS", "SCENERY",
	"DOORS & DEVICES", "SKIES", "FIRST PERSON HANDS" };
static char const *const kind_blurbs[AE_IS_KINDS] = { "What players and AI carry", "Enemies and allies, and how they fight",
	"Grenades, power-ups and pickups", "Driven, flown or ridden", "The bodies players and AI move in",
	"Rocks, crates, trees and props", "Doors, lifts, switches and machines", "The sky around the map", "The arms a player sees" };
static char const *const kind_words[AE_IS_KINDS] = { "WEAPON", "AI", "EQUIPMENT", "VEHICLE", "BIPED", "SCENERY", "DEVICE", "SKY",
	"HANDS" };

static int same(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

static void copy(char *to, int size, char const *from)
{
	if (size < 1)
		return;
	snprintf(to, (size_t)size, "%s", from ? from : "");
}

char const *ae_is_kind_name(int kind)
{
	return kind >= 0 && kind < AE_IS_KINDS ? kind_names[kind] : "";
}

char const *ae_is_kind_blurb(int kind)
{
	return kind >= 0 && kind < AE_IS_KINDS ? kind_blurbs[kind] : "";
}

char const *ae_is_kind_word(int kind)
{
	return kind >= 0 && kind < AE_IS_KINDS ? kind_words[kind] : "";
}

static char const *last_slash(char const *path)
{
	char const *last = NULL;

	for (; path && *path; path++)
		if (*path == '\\' || *path == '/')
			last = path;
	return last;
}

void ae_is_item_name(char const *path, char *name, int size)
{
	char const *last = last_slash(path);

	copy(name, size, last ? last + 1 : path);
}

int ae_is_folder_count(char const *path)
{
	int count = 0;

	for (; path && *path; path++)
		if (*path == '\\' || *path == '/')
			count++;
	return count;
}

/* the nth folder of a path (0 first): its start and length */
static int folder_at(char const *path, int n, char const **start, int *length)
{
	char const *each = path, *from = path;
	int at = 0;

	for (; *each; each++)
	{
		if (*each == '\\' || *each == '/')
		{
			if (at == n)
			{
				*start = from;
				*length = (int)(each - from);
				return 1;
			}
			at++;
			from = each + 1;
		}
	}
	return 0;
}

static void append(char *to, int size, char const *text, int length)
{
	int used = (int)strlen(to);

	if (used + length >= size)
		length = size - used - 1;
	if (length > 0)
	{
		memcpy(to + used, text, (size_t)length);
		to[used + length] = 0;
	}
}

void ae_is_item_folders(char const *path, int keep, char *folders, int size)
{
	int count = ae_is_folder_count(path), each, first;
	char const *start;
	int length;

	if (size < 1)
		return;
	folders[0] = 0;
	if (count == 0)
		return;
	if (keep < 0 || keep >= count - 1)
	{
		for (each = 0; each < count; each++)
		{
			if (each)
				append(folders, size, " " SEPARATOR " ", 5);
			if (folder_at(path, each, &start, &length))
				append(folders, size, start, length);
		}
		return;
	}
	if (folder_at(path, 0, &start, &length))
		append(folders, size, start, length);
	append(folders, size, " " SEPARATOR " " ELLIPSIS, 9);
	first = count - keep;
	for (each = first; each < count; each++)
	{
		append(folders, size, " " SEPARATOR " ", 5);
		if (folder_at(path, each, &start, &length))
			append(folders, size, start, length);
	}
}

/* the character count of UTF-8 text */
static int glyphs(char const *text)
{
	int count = 0;

	for (; *text; text++)
		if (((unsigned char)*text & 0xC0) != 0x80)
			count++;
	return count;
}

void ae_is_middle_cut(char const *name, int chars, char *cut, int size)
{
	int length = (int)strlen(name), head, tail;

	if (glyphs(name) <= chars || chars < 5 || length >= 200)
	{
		copy(cut, size, name);
		return;
	}
	/* (names are ASCII: the engine's paths) */
	head = (chars - 1) / 2 + (chars - 1) % 2;
	tail = chars - 1 - head;
	if (size < head + tail + 4)
	{
		copy(cut, size, name);
		return;
	}
	memcpy(cut, name, (size_t)head);
	cut[head] = 0;
	append(cut, size, ELLIPSIS, 3);
	append(cut, size, name + length - tail, tail);
}

int ae_is_path_lines(char const *path, int chars, char lines[][96], int most)
{
	int count = ae_is_folder_count(path), each, used = 0, line = 0;
	char const *start;
	int length;

	if (most < 1)
		return 0;
	lines[0][0] = 0;
	/* (the folders then the item's own name as the last part) */
	for (each = 0; each <= count; each++)
	{
		char part[96];
		int piece;

		if (each < count && folder_at(path, each, &start, &length))
		{
			if (length > 90)
				length = 90;
			memcpy(part, start, (size_t)length);
			part[length] = 0;
		}
		else
			ae_is_item_name(path, part, sizeof(part));
		piece = glyphs(part) + (used ? 3 : 0);
		if (used && used + piece + 2 > chars && line + 1 < most)
		{
			append(lines[line], 96, " " SEPARATOR, 4);
			line++;
			lines[line][0] = 0;
			used = 0;
			piece = glyphs(part);
		}
		if (used)
			append(lines[line], 96, " " SEPARATOR " ", 5);
		append(lines[line], 96, part, (int)strlen(part));
		used += piece;
	}
	return line + 1;
}

int ae_is_set_name_ok(char const *name)
{
	int letters = 0;

	if (!name)
		return 0;
	for (; *name; name++)
	{
		unsigned char c = (unsigned char)*name;

		if (isalnum(c))
			letters++;
		else if (c != ' ' && c != '-' && c != '_')
			return 0;
	}
	return letters > 0;
}

static void size_text(unsigned long long bytes, char *text, int size)
{
	if (bytes >= 1024ull * 1024ull * 1024ull)
		snprintf(text, (size_t)size, "%llu.%llu GB", bytes / (1024ull * 1024ull * 1024ull),
			(bytes * 10ull / (1024ull * 1024ull * 1024ull)) % 10ull);
	else if (bytes >= 1024ull * 1024ull)
		snprintf(text, (size_t)size, "%llu MB", (bytes + 512ull * 1024ull) / (1024ull * 1024ull));
	else
		snprintf(text, (size_t)size, "%llu KB", (bytes + 512ull) / 1024ull);
}

void ae_is_set_line(long items, unsigned long long bytes, long maps, char *text, int size)
{
	char megabytes[24];

	if (items <= 0)
	{
		copy(text, size, "No items yet");
		return;
	}
	size_text(bytes, megabytes, sizeof(megabytes));
	snprintf(text, (size_t)size, "%ld item%s " DOT " %s " DOT " from %ld map%s", items, items == 1 ? "" : "s", megabytes, maps,
		maps == 1 ? "" : "s");
}

void ae_is_set_state(int playing, long items, unsigned long long bytes, char *text, int size)
{
	char megabytes[24];

	size_text(bytes, megabytes, sizeof(megabytes));
	snprintf(text, (size_t)size, "%s " DOT " %ld ITEM%s " DOT " %s", playing ? "PLAYING" : "OFF", items, items == 1 ? "" : "S",
		megabytes);
}

static int starts(char const *text, char const *prefix)
{
	return !strncmp(text, prefix, strlen(prefix));
}

void ae_is_reason_words(char const *engine, char *text, int size)
{
	if (!engine || !engine[0])
		copy(text, size, "");
	else if (starts(engine, "It has no first-person"))
		copy(text, size, "It has no first-person model or animations: its map never lets a player hold it, so it can't be added.");
	else if (starts(engine, "It names a tag this build cannot take out: "))
		snprintf(text, (size_t)size, "It uses a part this version can't take out: %s.", engine + strlen("It names a tag this build cannot take out: "));
	else if (starts(engine, "Make a profile"))
		copy(text, size, "Make a set first.");
	else if (starts(engine, "Out of memory"))
		copy(text, size, "Not enough memory.");
	else if (starts(engine, "The map cannot be read again"))
		copy(text, size, "The map can't be read again: open it again.");
	else if (starts(engine, "The profile cannot be written"))
		copy(text, size, "The set can't be written: the log says why.");
	else
		snprintf(text, (size_t)size, "It can't be added: %s", engine);
}

void ae_is_failed_words(char const *failed, char *text, int size)
{
	if (!failed || !failed[0])
		copy(text, size, "");
	else if (same(failed, "too large"))
		copy(text, size, "The last level couldn't play it, so it was turned off: with the items before it, it was more than a level can add.");
	else if (same(failed, "missing"))
		copy(text, size, "Its file is missing from the set's folder, so it was turned off.");
	else if (same(failed, "unreadable"))
		copy(text, size, "Its file couldn't be read, so it was turned off.");
	else if (same(failed, "unplayable"))
		copy(text, size, "This version can't play it, so it was turned off.");
	else if (same(failed, "no memory"))
		copy(text, size, "The last level had no memory for it, so it was turned off.");
	else if (same(failed, "not holdable"))
		copy(text, size, "A weapon it adds has no first-person model or animations: no player can hold it. It was turned off.");
	else if (same(failed, "bitmaps"))
		copy(text, size, "A picture it uses can't be drawn by this version, so it was turned off.");
	else if (same(failed, "models"))
		copy(text, size, "A model it uses can't be made for this version, so it was turned off.");
	else
		copy(text, size, "The last level couldn't play it, so it was turned off.");
}

void ae_is_failed_todo(char const *failed, char *text, int size)
{
	if (failed && same(failed, "too large"))
		copy(text, size, "Turn off items this set can do without, then A turns it on to try again. Y removes it.");
	else
		copy(text, size, "A turns it on to try again. Y removes it from the set.");
}

int ae_is_plays_in(int game, int randomizer_on)
{
	return game == AE_IS_GAME_CAMPAIGN_ALONE && randomizer_on != 0;
}

char const *ae_is_plays_words(int randomizer_on)
{
	return randomizer_on ? "While it plays, the randomizer can use its items in the campaign levels you play alone. Changes play from the next level." :
		"It plays through the randomizer, which is off: turn it on in the campaign's setup. Changes play from the next level.";
}

char const *ae_is_others_words(void)
{
	return "Multiplayer, online and co-op games never use it.";
}

static void hint(struct ae_is_hint *hints, int *count, int which, char const *label)
{
	if (*count < AE_IS_HINTS_MOST)
	{
		hints[*count].hint = which;
		hints[*count].label = label;
		(*count)++;
	}
}

int ae_is_hints(int step, int playing, int has_items, struct ae_is_hint hints[AE_IS_HINTS_MOST])
{
	int count = 0;

	switch (step)
	{
	case AE_IS_STEP_SETS:
		if (has_items >= 0)
		{
			hint(hints, &count, AE_IS_HINT_OPTIONS, "Options");
			hint(hints, &count, playing ? AE_IS_HINT_STOP : AE_IS_HINT_PLAY, playing ? "Stop playing" : "Play this set");
		}
		hint(hints, &count, AE_IS_HINT_NEW, "New set");
		break;
	case AE_IS_STEP_CONTENTS:
		hint(hints, &count, AE_IS_HINT_ON_OFF, "On / off");
		hint(hints, &count, AE_IS_HINT_REMOVE, "Remove");
		hint(hints, &count, AE_IS_HINT_ADD_ITEMS, "Add items");
		break;
	case AE_IS_STEP_MAPS:
		hint(hints, &count, AE_IS_HINT_OPEN_MAP, "Open the map");
		hint(hints, &count, AE_IS_HINT_SEARCH, "Search");
		hint(hints, &count, AE_IS_HINT_PAGE, "Page");
		break;
	case AE_IS_STEP_KINDS:
		hint(hints, &count, AE_IS_HINT_OPEN, "Open");
		hint(hints, &count, AE_IS_HINT_CLOSE_MAP, "Close the map");
		return count;
	case AE_IS_STEP_ITEMS:
		if (has_items > 0)
			hint(hints, &count, AE_IS_HINT_REMOVE, "Remove");
		else
			hint(hints, &count, AE_IS_HINT_ADD, "Add to set");
		hint(hints, &count, AE_IS_HINT_SEARCH, "Search");
		hint(hints, &count, AE_IS_HINT_PAGE, "Page");
		break;
	}
	hint(hints, &count, AE_IS_HINT_BACK, "Back");
	return count;
}
