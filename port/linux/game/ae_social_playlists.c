/* The social playlists (see the header). Pure code. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ae_social_playlists.h"

static int same_text(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

static void copy_text(char *to, size_t size, char const *from)
{
	size_t length = strlen(from);

	if (length >= size)
		length = size - 1;
	memcpy(to, from, length);
	to[length] = 0;
}

static char *trim(char *text)
{
	char *end;

	while (*text == ' ' || *text == '\t')
		text++;
	end = text + strlen(text);
	while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
		*--end = 0;
	return text;
}

static int pack_map(char const *map)
{
	char const *at = strchr(map, '@');

	return at && same_text(at, "@smp");
}

/* a "# key: value" line: the key (one lowercase word) and the value; 0 when the line is not one */
static int key_line(char *line, char **key, char **value)
{
	char *p = line + 1, *colon;

	while (*p == ' ')
		p++;
	*key = p;
	while (*p >= 'a' && *p <= 'z')
		p++;
	if (p == *key || *p != ':')
		return 0;
	colon = p;
	*colon = 0;
	*value = trim(colon + 1);
	return 1;
}

static int shape_read(char const *text, int *teams, int *size)
{
	char *end;
	long first = strtol(text, &end, 10);

	if (end == text || first < 1)
		return 0;
	while (*end == ' ')
		end++;
	if (*end == 'x' || *end == 'X')
	{
		long second = strtol(end + 1, &end, 10);

		if (second < 1 || first > 8 || second > 16 || first * second > 16)
			return 0;
		*teams = (int)first;
		*size = (int)second;
		return 1;
	}
	if (first > 16)
		return 0;
	*teams = 0;
	*size = (int)first;
	return 1;
}

static int start_word(char const *word, size_t length)
{
	static char const *const words[AE_SOCIAL_STARTS] = { "br", "evo", "magnum", "protomag" };
	int each;

	for (each = 0; each < AE_SOCIAL_STARTS; each++)
		if (strlen(words[each]) == length && !strncmp(word, words[each], length))
			return each + 1;
	return AE_SOCIAL_START_NONE;
}

/* "br 1, evo 2": the starts a playlist mixes (an unknown word or a repeat is skipped; the weight 1 .. 5, none: 1) */
static void starts_read(struct ae_social_playlist *playlist, char *value)
{
	char *at = value;

	playlist->starts = 0;
	playlist->starts_given = 1;
	while (*at && playlist->starts < AE_SOCIAL_STARTS)
	{
		size_t length;
		int kind, each, seen = 0;
		long weight = 1;
		char *end;

		while (*at == ' ' || *at == ',' || *at == '\t')
			at++;
		length = 0;
		while (at[length] >= 'a' && at[length] <= 'z')
			length++;
		if (!length)
			break;
		kind = start_word(at, length);
		at += length;
		while (*at == ' ')
			at++;
		if (*at >= '0' && *at <= '9')
		{
			weight = strtol(at, &end, 10);
			at = end;
			if (weight < 1)
				weight = 1;
			if (weight > AE_SOCIAL_WEIGHT_MAX)
				weight = AE_SOCIAL_WEIGHT_MAX;
		}
		for (each = 0; each < playlist->starts; each++)
			seen |= playlist->start_kind[each] == kind;
		if (kind && !seen)
		{
			playlist->start_kind[playlist->starts] = (short)kind;
			playlist->start_weight[playlist->starts++] = (short)weight;
		}
	}
}

static void entry_read(struct ae_social_playlist *playlist, char *line)
{
	char *hash = strchr(line, '#'), *colon, *map, *gametype, *comment = "";
	struct ae_social_entry *entry;
	long weight = 2;

	if (hash)
	{
		*hash = 0;
		comment = hash + 1;
	}
	line = trim(line);
	colon = strchr(line, ':');
	if (!colon || playlist->count >= AE_SOCIAL_ENTRIES)
		return;
	*colon = 0;
	map = trim(line);
	gametype = trim(colon + 1);
	if (!strncmp(gametype, "file=", 5))
		gametype = trim(gametype + 5);
	if (!map[0] || !gametype[0] || strlen(map) >= AE_SOCIAL_MAP || strlen(gametype) >= AE_SOCIAL_GAMETYPE)
		return;
	{
		char *w = strstr(comment, "weight");

		if (w)
		{
			char *end;

			weight = strtol(w + 6, &end, 10);
			if (end == w + 6)
				weight = 2;
			if (weight < 1)
				weight = 1;
			if (weight > AE_SOCIAL_WEIGHT_MAX)
				weight = AE_SOCIAL_WEIGHT_MAX;
		}
	}
	entry = &playlist->entry[playlist->count++];
	memset(entry, 0, sizeof(*entry));
	copy_text(entry->map, sizeof(entry->map), map);
	copy_text(entry->gametype, sizeof(entry->gametype), gametype);
	entry->weight = (short)weight;
	entry->fallback = strstr(comment, "fallback") != NULL;
	entry->pack = (short)pack_map(entry->map);
}

static int playlist_valid(struct ae_social_playlist const *playlist)
{
	return playlist->id[0] && playlist->size >= 1 && playlist->count > 0;
}

int ae_social_playlists_parse(char const *text, struct ae_social_playlist *out, int max)
{
	int count = 0, open = 0;
	struct ae_social_playlist *current = NULL;

	while (*text)
	{
		char line[256], *key, *value, *use;
		size_t length = strcspn(text, "\n");

		if (length >= sizeof(line))
			length = sizeof(line) - 1;
		memcpy(line, text, length);
		line[length] = 0;
		text += strcspn(text, "\n");
		if (*text)
			text++;
		use = trim(line);
		if (!use[0])
			continue;
		if (use[0] == '#')
		{
			if (!key_line(use, &key, &value))
				continue;
			if (!strcmp(key, "social"))
			{
				if (open && !playlist_valid(current))
					count--;
				open = 0;
				if (count >= max || count >= AE_SOCIAL_PLAYLISTS)
					continue;
				current = &out[count++];
				memset(current, 0, sizeof(*current));
				copy_text(current->id, sizeof(current->id), value);
				current->party = 0;
				open = 1;
			}
			else if (open)
			{
				if (!strcmp(key, "label"))
					copy_text(current->label, sizeof(current->label), value);
				else if (!strcmp(key, "about"))
					copy_text(current->about, sizeof(current->about), value);
				else if (!strcmp(key, "teams") || !strcmp(key, "players"))
				{
					int teams, size;

					if (shape_read(value, &teams, &size))
					{
						current->teams = !strcmp(key, "players") ? 0 : teams;
						current->size = size;
					}
				}
				else if (!strcmp(key, "party"))
					current->party = atoi(value);
				else if (!strcmp(key, "kind"))
					current->kind = same_text(value, "ranked") ? AE_SOCIAL_KIND_RANKED :
						same_text(value, "comp") ? AE_SOCIAL_KIND_COMP : AE_SOCIAL_KIND_SOCIAL;
				else if (!strcmp(key, "weapons"))
					current->weapons_own = same_text(value, "map");
				else if (!strcmp(key, "shown"))
					current->hide_empty = same_text(value, "with maps");
				else if (!strcmp(key, "starts"))
					starts_read(current, value);
			}
			continue;
		}
		if (open)
			entry_read(current, use);
	}
	if (open && !playlist_valid(current))
		count--;
	/* a playlist's defaults: the label its id, the party its team's size (a free for all alone) */
	{
		int index;

		for (index = 0; index < count; index++)
		{
			if (!out[index].label[0])
				copy_text(out[index].label, sizeof(out[index].label), out[index].id);
			/* (a comp playlist with no starts line: the evo rifle and the CE magnum) */
			if (out[index].kind == AE_SOCIAL_KIND_COMP && !out[index].starts && !out[index].starts_given)
			{
				out[index].starts = 2;
				out[index].start_kind[0] = AE_SOCIAL_START_EVO;
				out[index].start_kind[1] = AE_SOCIAL_START_MAGNUM;
				out[index].start_weight[0] = out[index].start_weight[1] = 1;
			}
			if (out[index].party < 1 || out[index].party > out[index].size)
				out[index].party = out[index].teams ? out[index].size : 1;
		}
	}
	return count;
}

int ae_social_playlist_shown(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context)
{
	int index[AE_SOCIAL_ENTRIES];

	if (!playlist->hide_empty)
		return 1;
	return ae_social_playlist_candidates(playlist, present, context, index) > 0;
}

int ae_social_playlist_find(struct ae_social_playlist const *lists, int count, char const *id)
{
	int index;

	for (index = 0; index < count; index++)
		if (same_text(lists[index].id, id))
			return index;
	return -1;
}

int ae_social_playlist_seats(struct ae_social_playlist const *playlist)
{
	return playlist->teams ? playlist->teams * playlist->size : playlist->size;
}

/* whether the map of entry index already came before it in the list */
static int map_seen_before(struct ae_social_playlist const *playlist, int index)
{
	int before;

	for (before = 0; before < index; before++)
		if (same_text(playlist->entry[before].map, playlist->entry[index].map))
			return 1;
	return 0;
}

void ae_social_playlist_counts(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context,
	struct ae_social_counts *counts)
{
	int index;

	memset(counts, 0, sizeof(*counts));
	counts->workshop = playlist->count > 0;
	for (index = 0; index < playlist->count; index++)
	{
		struct ae_social_entry const *entry = &playlist->entry[index];
		char const *at = strchr(entry->map, '@');
		int here;

		if (!at || strncmp(at, "@ws", 3))
			counts->workshop = 0;

		if (map_seen_before(playlist, index))
			continue;
		here = present(context, entry->map) ? 1 : 0;
		if (entry->pack)
		{
			counts->pack_total++;
			counts->pack_present += here;
		}
		else
		{
			counts->stock_total++;
			counts->stock_present += here;
		}
	}
	/* (the rule counts the pack maps of entries that are not fallbacks: a pack map is never one) */
	counts->stock_in_use = counts->pack_total > 0 && counts->pack_present < AE_SOCIAL_PACK_MINIMUM;
}

int ae_social_playlist_candidates(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context,
	int *indexes)
{
	int index, count = 0, pack_here = 0, any = 0;
	int here[AE_SOCIAL_ENTRIES];

	for (index = 0; index < playlist->count; index++)
		here[index] = present(context, playlist->entry[index].map) ? 1 : 0;
	for (index = 0; index < playlist->count; index++)
	{
		if (!here[index])
			continue;
		any = 1;
		/* (a map counts once, whatever its entries) */
		if (!playlist->entry[index].fallback && !map_seen_before(playlist, index))
			pack_here++;
	}
	for (index = 0; index < playlist->count; index++)
	{
		struct ae_social_entry const *entry = &playlist->entry[index];

		if (!here[index])
			continue;
		/* (the fallbacks play only while fewer than the minimum pack maps are here) */
		if (entry->fallback && pack_here >= AE_SOCIAL_PACK_MINIMUM)
			continue;
		indexes[count++] = index;
	}
	/* (a playlist whose entries are all fallbacks, or none left: any present entry) */
	if (!count && any)
		for (index = 0; index < playlist->count; index++)
			if (here[index])
				indexes[count++] = index;
	return count;
}

int ae_social_playlist_pick(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context, int last,
	unsigned long random)
{
	int candidates[AE_SOCIAL_ENTRIES], count, index, usable[AE_SOCIAL_ENTRIES], usable_count = 0, distinct = 0;
	unsigned long total = 0;

	count = ae_social_playlist_candidates(playlist, present, context, candidates);
	if (!count)
		return -1;
	for (index = 0; index < count; index++)
	{
		int other, seen = 0;

		for (other = 0; other < index; other++)
			if (same_text(playlist->entry[candidates[other]].map, playlist->entry[candidates[index]].map))
				seen = 1;
		distinct += !seen;
	}
	for (index = 0; index < count; index++)
	{
		if (distinct >= 2 && last >= 0 && last < playlist->count &&
			same_text(playlist->entry[candidates[index]].map, playlist->entry[last].map))
			continue;
		usable[usable_count++] = candidates[index];
		total += (unsigned long)playlist->entry[candidates[index]].weight;
	}
	if (!usable_count)
		return candidates[0];
	random %= total;
	for (index = 0; index < usable_count; index++)
	{
		unsigned long weight = (unsigned long)playlist->entry[usable[index]].weight;

		if (random < weight)
			return usable[index];
		random -= weight;
	}
	return usable[usable_count - 1];
}

int ae_social_playlist_start_pick(struct ae_social_playlist const *playlist, unsigned supported, unsigned long random)
{
	unsigned long total = 0;
	int each;

	for (each = 0; each < playlist->starts; each++)
		if (supported & (1u << playlist->start_kind[each]))
			total += (unsigned long)playlist->start_weight[each];
	if (!total)
		return AE_SOCIAL_START_NONE;
	random %= total;
	for (each = 0; each < playlist->starts; each++)
	{
		unsigned long weight;

		if (!(supported & (1u << playlist->start_kind[each])))
			continue;
		weight = (unsigned long)playlist->start_weight[each];
		if (random < weight)
			return playlist->start_kind[each];
		random -= weight;
	}
	return AE_SOCIAL_START_NONE;
}

char const *ae_social_start_label(int start)
{
	return start == AE_SOCIAL_START_BR ? "BATTLE RIFLE" : start == AE_SOCIAL_START_EVO ? "EVO RIFLE" :
		start == AE_SOCIAL_START_MAGNUM ? "PISTOL" :
		start == AE_SOCIAL_START_PROTOMAG ? "PROTOMAG" : "";
}
