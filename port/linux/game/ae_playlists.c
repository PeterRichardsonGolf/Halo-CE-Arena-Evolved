/*
AE_PLAYLISTS.C

Several named playlists (ae_playlists.h). Pure.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_playlists.h"
#include "ae_rotation_shuffle.h"

#define MIDDLE_DOT "\xC2\xB7"

static void copy(char *to, int size, char const *from)
{
	snprintf(to, (size_t)size, "%s", from ? from : "");
}

static int same_text_case(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
			return 0;
	return *a == *b;
}

void ae_playlists_init(struct ae_playlists *set)
{
	memset(set, 0, sizeof(*set));
	set->in_use = -1;
}

/* ---------- names */

int ae_playlists_name_check(struct ae_playlists const *set, char const *typed, int except, char *name, int size)
{
	char cleaned[128];
	int length = 0, index, start = 0, end;
	int reason = AE_PLAYLISTS_NAME_OK;

	if (size > 0)
		name[0] = 0;
	typed = typed ? typed : "";
	end = (int)strlen(typed);
	while (start < end && (typed[start] == ' ' || typed[start] == '\t'))
		start++;
	while (end > start && (typed[end - 1] == ' ' || typed[end - 1] == '\t' || typed[end - 1] == '\r' || typed[end - 1] == '\n'))
		end--;
	for (index = start; index < end && length < (int)sizeof(cleaned) - 1; index++)
		cleaned[length++] = (char)toupper((unsigned char)typed[index]);
	cleaned[length] = 0;
	if (length <= 0)
		reason = AE_PLAYLISTS_NAME_EMPTY;
	else if (length > AE_PLAYLISTS_NAME_MAX)
		reason = AE_PLAYLISTS_NAME_LONG;
	for (index = 0; reason == AE_PLAYLISTS_NAME_OK && index < length; index++)
	{
		unsigned char c = (unsigned char)cleaned[index];

		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '\'' || c == '&'))
			reason = AE_PLAYLISTS_NAME_CHARACTER;
	}
	if (reason == AE_PLAYLISTS_NAME_OK && !strcmp(cleaned, "OFF"))
		reason = AE_PLAYLISTS_NAME_OFF;
	if (reason == AE_PLAYLISTS_NAME_OK && set)
	{
		int found = ae_playlists_find(set, cleaned);

		if (found >= 0 && found != except)
			reason = AE_PLAYLISTS_NAME_TAKEN;
	}
	if (size > 0)
		snprintf(name, (size_t)size, "%s", cleaned);
	return reason;
}

void ae_playlists_name_message(int reason, char const *name, char *text, int size)
{
	switch (reason)
	{
	case AE_PLAYLISTS_NAME_EMPTY:
		snprintf(text, (size_t)size, "Type a name for the playlist.");
		break;
	case AE_PLAYLISTS_NAME_LONG:
		snprintf(text, (size_t)size, "A playlist's name holds %d characters at most.", (int)AE_PLAYLISTS_NAME_MAX);
		break;
	case AE_PLAYLISTS_NAME_CHARACTER:
		snprintf(text, (size_t)size, "A name takes letters, numbers, spaces and - ' & only.");
		break;
	case AE_PLAYLISTS_NAME_OFF:
		snprintf(text, (size_t)size, "OFF is the row that turns playlists off: pick another name.");
		break;
	case AE_PLAYLISTS_NAME_TAKEN:
		snprintf(text, (size_t)size, "A playlist is already called %s.", name ? name : "that");
		break;
	default:
		if (size > 0)
			text[0] = 0;
		break;
	}
}

int ae_playlists_find(struct ae_playlists const *set, char const *name)
{
	int index;

	for (index = 0; set && name && index < set->count; index++)
		if (same_text_case(set->lists[index].name, name))
			return index;
	return -1;
}

void ae_playlists_default_name(struct ae_playlists const *set, char *name, int size)
{
	int number;

	for (number = set->count + 1; number < 1000; number++)
	{
		char candidate[AE_PLAYLISTS_NAME_MAX + 1];

		snprintf(candidate, sizeof(candidate), "PLAYLIST %d", number);
		if (ae_playlists_find(set, candidate) < 0)
		{
			copy(name, size, candidate);
			return;
		}
	}
	copy(name, size, "PLAYLIST");
}

/* ---------- the set */

int ae_playlists_new(struct ae_playlists *set, char const *typed, int *reason)
{
	char name[AE_PLAYLISTS_NAME_MAX + 8];
	int why = ae_playlists_name_check(set, typed, -1, name, (int)sizeof(name));
	struct ae_playlist *list;

	if (reason)
		*reason = why;
	if (why != AE_PLAYLISTS_NAME_OK || set->count >= AE_PLAYLISTS_MAX)
		return -1;
	list = &set->lists[set->count];
	ae_playlist_init(list);
	copy(list->name, AE_PLAYLIST_NAME, name);
	set->next[set->count] = 0;
	return set->count++;
}

int ae_playlists_rename(struct ae_playlists *set, int index, char const *typed)
{
	char name[AE_PLAYLISTS_NAME_MAX + 8];
	int why;

	if (index < 0 || index >= set->count)
		return AE_PLAYLISTS_NAME_EMPTY;
	why = ae_playlists_name_check(set, typed, index, name, (int)sizeof(name));
	if (why == AE_PLAYLISTS_NAME_OK)
		copy(set->lists[index].name, AE_PLAYLIST_NAME, name);
	return why;
}

int ae_playlists_duplicate(struct ae_playlists *set, int index)
{
	int number;

	if (index < 0 || index >= set->count || set->count >= AE_PLAYLISTS_MAX)
		return -1;
	for (number = 2; number < 1000; number++)
	{
		char suffix[16], base[AE_PLAYLISTS_NAME_MAX + 1], candidate[AE_PLAYLISTS_NAME_MAX + 16];
		int room;

		snprintf(suffix, sizeof(suffix), " %d", number);
		room = AE_PLAYLISTS_NAME_MAX - (int)strlen(suffix);
		snprintf(base, sizeof(base), "%.*s", room, set->lists[index].name);
		while (strlen(base) > 0 && base[strlen(base) - 1] == ' ')
			base[strlen(base) - 1] = 0;
		snprintf(candidate, sizeof(candidate), "%s%s", base, suffix);
		if (ae_playlists_find(set, candidate) < 0)
		{
			int made = set->count++;

			set->lists[made] = set->lists[index];
			copy(set->lists[made].name, AE_PLAYLIST_NAME, candidate);
			set->next[made] = 0;
			return made;
		}
	}
	return -1;
}

void ae_playlists_delete(struct ae_playlists *set, int index)
{
	int at;

	if (index < 0 || index >= set->count)
		return;
	for (at = index; at + 1 < set->count; at++)
	{
		set->lists[at] = set->lists[at + 1];
		set->next[at] = set->next[at + 1];
	}
	set->count--;
	if (set->in_use == index)
		set->in_use = -1;
	else if (set->in_use > index)
		set->in_use--;
}

int ae_playlists_use(struct ae_playlists *set, int index)
{
	if (index < -1 || index >= set->count)
		return 0;
	if (set->in_use == index)
		return 0;
	set->in_use = index;
	return 1;
}

int ae_playlists_drop_empty(struct ae_playlists *set, int index)
{
	if (index < 0 || index >= set->count || set->lists[index].count > 0)
		return 0;
	ae_playlists_delete(set, index);
	return 1;
}

/* ---------- the file */

long ae_playlists_write(struct ae_playlists const *set, char *text, long size)
{
	long used = 0;
	int index;

#define PUT(...) \
	do { \
		int wrote = snprintf(text + used, (size_t)(size - used), __VA_ARGS__); \
		if (wrote < 0 || used + wrote >= size) \
			return -1; \
		used += wrote; \
	} while (0)
	if (size <= 0)
		return -1;
	text[0] = 0;
	PUT("# Arena Evolved playlists (CREATE GAME > PLAYLIST). One section a playlist.\n");
	PUT("# in use: %s\n", set->in_use >= 0 && set->in_use < set->count && set->lists[set->in_use].count > 0 ?
		set->lists[set->in_use].name : "OFF");
	for (index = 0; index < set->count; index++)
	{
		long wrote;

		if (set->lists[index].count <= 0)
			continue;
		wrote = ae_playlist_write_next(&set->lists[index], set->next[index], text + used, size - used);
		if (wrote < 0)
			return -1;
		used += wrote;
	}
#undef PUT
	return used;
}

static char *trim(char *text)
{
	char *end;

	while (*text == ' ' || *text == '\t' || *text == '\r')
		text++;
	end = text + strlen(text);
	while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
		*--end = 0;
	return text;
}

/* a line that begins a section: "# playlist:" */
static int section_start(char const *line, long length)
{
	char buffer[64];
	char *body;

	snprintf(buffer, sizeof(buffer), "%.*s", (int)(length < (long)sizeof(buffer) - 1 ? length : (long)sizeof(buffer) - 1), line);
	body = trim(buffer);
	if (*body != '#')
		return 0;
	body = trim(body + 1);
	return !strncmp(body, "playlist:", 9);
}

/* the text of a section holds an entry (a line that is not a comment) */
static int has_entries(char const *text)
{
	while (*text)
	{
		size_t length = strcspn(text, "\n"), at = 0;

		while (at < length && (text[at] == ' ' || text[at] == '\t' || text[at] == '\r'))
			at++;
		if (at < length && text[at] != '#')
			return 1;
		text += length + (text[length] == '\n');
	}
	return 0;
}

/* a name made storable and unique: capitals, the characters a name takes (others become spaces), cut to length */
static void tidy_name(struct ae_playlists const *set, char const *raw, char *name, int size)
{
	char cleaned[AE_PLAYLISTS_NAME_MAX + 1];
	int length = 0, index, number;

	for (index = 0; raw[index] && length < AE_PLAYLISTS_NAME_MAX; index++)
	{
		unsigned char c = (unsigned char)toupper((unsigned char)raw[index]);

		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '\'' || c == '&'))
			c = ' ';
		if (length == 0 && c == ' ')
			continue;
		cleaned[length++] = (char)c;
	}
	while (length > 0 && cleaned[length - 1] == ' ')
		length--;
	cleaned[length] = 0;
	if (!length || !strcmp(cleaned, "OFF"))
		snprintf(cleaned, sizeof(cleaned), "MY PLAYLIST");
	if (ae_playlists_find(set, cleaned) < 0)
	{
		copy(name, size, cleaned);
		return;
	}
	for (number = 2; number < 1000; number++)
	{
		char suffix[16], candidate[AE_PLAYLISTS_NAME_MAX + 16];

		snprintf(suffix, sizeof(suffix), " %d", number);
		snprintf(candidate, sizeof(candidate), "%.*s%s", AE_PLAYLISTS_NAME_MAX - (int)strlen(suffix), cleaned, suffix);
		if (ae_playlists_find(set, candidate) < 0)
		{
			copy(name, size, candidate);
			return;
		}
	}
	copy(name, size, cleaned);
}

/* a section's "# next: N" */
static int section_next(char const *text)
{
	while (*text)
	{
		size_t length = strcspn(text, "\n");
		char line[128], *body;

		snprintf(line, sizeof(line), "%.*s", (int)(length < sizeof(line) - 1 ? length : sizeof(line) - 1), text);
		body = trim(line);
		if (*body == '#')
		{
			body = trim(body + 1);
			if (!strncmp(body, "next:", 5))
			{
				int place = atoi(trim(body + 5));

				return place > 0 && place <= AE_PLAYLIST_ENTRIES ? place : 0;
			}
		}
		text += length + (text[length] == '\n');
	}
	return 0;
}

static void section_take(struct ae_playlists *set, char const *text, long length)
{
	static char section[AE_PLAYLISTS_SECTION];
	struct ae_playlist list;
	char name[AE_PLAYLISTS_NAME_MAX + 8];

	if (set->count >= AE_PLAYLISTS_MAX)
		return;
	if (length > (long)sizeof(section) - 1)
		length = (long)sizeof(section) - 1;
	memcpy(section, text, (size_t)length);
	section[length] = 0;
	ae_playlist_init(&list);
	ae_playlist_read(&list, section);
	tidy_name(set, list.name, name, (int)sizeof(name));
	copy(list.name, AE_PLAYLIST_NAME, name);
	set->lists[set->count] = list;
	set->next[set->count] = (short)section_next(section);
	set->count++;
}

int ae_playlists_read(struct ae_playlists *set, char const *text)
{
	char in_use[AE_PLAYLIST_NAME] = "";
	char const *at = text, *section_begin = NULL;
	int index;

	ae_playlists_init(set);
	if (!text)
		return 0;
	/* (the lines before the first section: "# in use:", and games of a hand-made file, which are one playlist) */
	while (*at)
	{
		size_t length = strcspn(at, "\n");

		if (section_start(at, (long)length))
		{
			section_begin = at;
			break;
		}
		at += length + (at[length] == '\n');
	}
	{
		long prelude = section_begin ? (long)(section_begin - text) : (long)strlen(text);
		char const *scan = text;

		while (scan < text + prelude)
		{
			size_t length = strcspn(scan, "\n");
			char line[128], *body;

			snprintf(line, sizeof(line), "%.*s", (int)(length < sizeof(line) - 1 ? length : sizeof(line) - 1), scan);
			body = trim(line);
			if (*body == '#')
			{
				body = trim(body + 1);
				if (!strncmp(body, "in use:", 7))
					copy(in_use, (int)sizeof(in_use), trim(body + 7));
			}
			scan += length + (scan[length] == '\n');
		}
		if (prelude > 0)
		{
			static char head[AE_PLAYLISTS_SECTION];
			long take = prelude > (long)sizeof(head) - 1 ? (long)sizeof(head) - 1 : prelude;

			memcpy(head, text, (size_t)take);
			head[take] = 0;
			if (has_entries(head))
				section_take(set, head, take);
		}
	}
	while (section_begin && *section_begin)
	{
		char const *scan = section_begin;
		size_t first = strcspn(scan, "\n");

		scan += first + (scan[first] == '\n');
		while (*scan)
		{
			size_t length = strcspn(scan, "\n");

			if (section_start(scan, (long)length))
				break;
			scan += length + (scan[length] == '\n');
		}
		section_take(set, section_begin, (long)(scan - section_begin));
		section_begin = scan;
	}
	set->in_use = -1;
	if (in_use[0] && strcmp(in_use, "OFF"))
		for (index = 0; index < set->count; index++)
			if (same_text_case(set->lists[index].name, in_use) && set->lists[index].count > 0)
				set->in_use = index;
	return set->count;
}

int ae_playlists_migrate(struct ae_playlists *set, char const *legacy_text)
{
	static struct ae_playlists scratch;

	ae_playlists_init(set);
	if (!legacy_text || !has_entries(legacy_text))
		return 0;
	ae_playlists_read(&scratch, legacy_text);
	if (scratch.count <= 0 || scratch.lists[0].count <= 0)
		return 0;
	ae_playlists_init(set);
	set->lists[0] = scratch.lists[0];
	set->next[0] = 0;
	set->count = 1;
	set->in_use = -1;
	return 1;
}

/* ---------- what plays next */

void ae_playlist_run_reset(struct ae_playlist_run *run)
{
	memset(run, 0, sizeof(*run));
	run->current = -1;
	run->previous = -1;
}

void ae_playlist_run_forget_pass(struct ae_playlist_run *run)
{
	run->pass_count = 0;
	run->pass_at = 0;
}

static int playable_at(unsigned char const *playable, int index)
{
	return !playable || playable[index];
}

/* a new pass of every game in a new order, the first not the game just before it (when there are two or more) */
static void make_pass(struct ae_playlist const *list, struct ae_playlist_run *run, unsigned long seed)
{
	char const *map[AE_PLAYLIST_ENTRIES], *gametype[AE_PLAYLIST_ENTRIES];
	int order[AE_PLAYLIST_ENTRIES], index;
	char const *previous_map = NULL, *previous_gametype = NULL;

	for (index = 0; index < list->count; index++)
	{
		map[index] = list->entries[index].map;
		gametype[index] = list->entries[index].gametype;
	}
	if (run->current >= 0 && run->current < list->count)
	{
		previous_map = list->entries[run->current].map;
		previous_gametype = list->entries[run->current].gametype;
	}
	ae_rotation_shuffle_order(list->count, map, gametype, previous_map, previous_gametype, seed, order);
	/* (the same game never twice in a row, even where a map has to repeat) */
	if (list->count > 1 && run->current >= 0 && order[0] == run->current)
	{
		int swap = order[0];

		order[0] = order[1];
		order[1] = swap;
	}
	for (index = 0; index < list->count; index++)
		run->pass[index] = (short)order[index];
	run->pass_count = (short)list->count;
	run->pass_at = 0;
}

static int advance_shuffle(struct ae_playlist const *list, struct ae_playlist_run *run, unsigned char const *playable,
	unsigned long seed)
{
	int pass_tries;

	for (pass_tries = 0; pass_tries < 2; pass_tries++)
	{
		if (run->pass_count != list->count || run->pass_at >= run->pass_count)
			make_pass(list, run, seed + (unsigned long)pass_tries * 7919ul);
		while (run->pass_at < run->pass_count)
		{
			int entry = run->pass[run->pass_at++];

			if (entry >= 0 && entry < list->count && playable_at(playable, entry))
				return entry;
		}
	}
	return -1;
}

int ae_playlist_run_advance(struct ae_playlist const *list, struct ae_playlist_run *run, int place,
	unsigned char const *playable, int after_game, unsigned long seed)
{
	int chosen = -1, index;

	if (list->count <= 0)
		return -1;
	if (run->current >= list->count)
		run->current = -1;
	switch (list->order)
	{
	case AE_PLAYLIST_ORDER_IN_TURN:
	{
		int start = run->current >= 0 ? run->current + 1 : place;

		start = ((start % list->count) + list->count) % list->count;
		for (index = 0; index < list->count; index++)
		{
			int entry = (start + index) % list->count;

			if (playable_at(playable, entry))
			{
				chosen = entry;
				break;
			}
		}
		break;
	}
	case AE_PLAYLIST_ORDER_SHUFFLE:
	{
		struct ae_playlist_run trial = *run;

		chosen = advance_shuffle(list, &trial, playable, seed);
		if (chosen >= 0)
			*run = trial;
		else
			return -1;
		break;
	}
	default:
	{
		struct ae_playlist copy_list = *list;
		int last = run->current;

		for (index = 0; index < copy_list.count; index++)
			if (!playable_at(playable, index))
				copy_list.entries[index].weight = 0;
		chosen = ae_playlist_pick(&copy_list, last, seed);
		if (chosen < 0 && copy_list.no_repeats)
		{
			copy_list.no_repeats = 0;
			chosen = ae_playlist_pick(&copy_list, last, seed);
		}
		break;
	}
	}
	if (chosen < 0)
		return -1;
	if (after_game && run->current >= 0)
		run->previous = run->current;
	else if (!after_game)
		run->previous = -1;
	run->current = (short)chosen;
	return chosen;
}

int ae_playlist_run_tag(struct ae_playlist_run const *run, int entry)
{
	if (entry < 0)
		return AE_PLAYLIST_TAG_NONE;
	if (entry == run->current)
		return AE_PLAYLIST_TAG_UP_NEXT;
	if (entry == run->previous)
		return AE_PLAYLIST_TAG_PLAYED;
	return AE_PLAYLIST_TAG_NONE;
}

void ae_playlist_run_removed(struct ae_playlist_run *run, int entry)
{
	short *place[2] = { &run->current, &run->previous };
	int each;

	for (each = 0; each < 2; each++)
	{
		if (*place[each] == entry)
			*place[each] = -1;
		else if (*place[each] > entry)
			(*place[each])--;
	}
	ae_playlist_run_forget_pass(run);
}

void ae_playlist_run_moved(struct ae_playlist_run *run, int from, int to)
{
	short *place[2] = { &run->current, &run->previous };
	int each;

	for (each = 0; each < 2; each++)
	{
		short at = *place[each];

		if (at < 0)
			continue;
		if (at == from)
			*place[each] = (short)to;
		else if (from < to && at > from && at <= to)
			*place[each] = (short)(at - 1);
		else if (from > to && at >= to && at < from)
			*place[each] = (short)(at + 1);
	}
	ae_playlist_run_forget_pass(run);
}

char const *ae_playlist_order_label(int order)
{
	switch (order)
	{
	case AE_PLAYLIST_ORDER_IN_TURN: return "IN TURN";
	case AE_PLAYLIST_ORDER_SHUFFLE: return "SHUFFLE";
	default: return "RANDOM";
	}
}

void ae_playlist_row_text(struct ae_playlist const *list, char *text, int size)
{
	snprintf(text, (size_t)size, "%s  " MIDDLE_DOT "  %s", list->name, ae_playlist_order_label(list->order));
}

void ae_playlist_board_text(struct ae_playlist const *list, char *text, int size)
{
	snprintf(text, (size_t)size, "%d GAME%s  " MIDDLE_DOT "  %s", (int)list->count, list->count == 1 ? "" : "S",
		ae_playlist_order_label(list->order));
}

/* ---------- HOST A SERVER */

int ae_playlist_host_rotation(struct ae_playlist const *list, char *line, int size, struct ae_playlist_host *host)
{
	int used = 0, games = 0, index;

	if (size <= 0)
		return -1;
	line[0] = 0;
	if (host)
	{
		host->shuffle = list->order != AE_PLAYLIST_ORDER_IN_TURN;
		host->map_vote = list->map_vote ? 1 : 0;
		host->map_vote_choices = AE_PLAYLIST_VOTE_GAMES;
		host->vote_no_repeat = list->no_repeats ? 2 : 0;
	}
	for (index = 0; index < list->count; index++)
	{
		struct ae_playlist_entry const *entry = &list->entries[index];
		char text[AE_PLAYLIST_MAP + AE_PLAYLIST_GAMETYPE + 32];
		int wrote;

		if (strchr(entry->map, ',') || strchr(entry->gametype, ','))
			continue;
		if (list->order == AE_PLAYLIST_ORDER_RANDOM)
			snprintf(text, sizeof(text), "%s:file=%s +weight=%d", entry->map, entry->gametype, (int)entry->weight);
		else
			snprintf(text, sizeof(text), "%s:file=%s", entry->map, entry->gametype);
		wrote = snprintf(line + used, (size_t)(size - used), "%s%s", games ? ", " : "", text);
		if (wrote < 0 || used + wrote >= size)
			return -1;
		used += wrote;
		games++;
	}
	return games;
}

void ae_playlist_host_summary(struct ae_playlist const *list, char *text, int size)
{
	snprintf(text, (size_t)size, "Playlist: %s, %d game%s, %s", list->name, (int)list->count, list->count == 1 ? "" : "s",
		list->order == AE_PLAYLIST_ORDER_IN_TURN ? "in turn" : list->order == AE_PLAYLIST_ORDER_SHUFFLE ? "shuffled" :
		"picked at random");
}
