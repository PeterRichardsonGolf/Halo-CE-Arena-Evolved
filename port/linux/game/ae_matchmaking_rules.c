/*
AE_MATCHMAKING_RULES.C

See ae_matchmaking_rules.h.
*/

#include <stddef.h>
#include <string.h>

#include "ae_matchmaking_rules.h"

/* a field of the line copied (cut to the size), the line's text from *at to the next '|' or the end */
static void field_copy(const char **at, const char *end, char *out, size_t size)
{
	const char *bar = *at;
	size_t length;

	while (bar < end && *bar != '|')
		bar++;
	length = (size_t)(bar - *at);
	if (length >= size)
		length = size - 1;
	memcpy(out, *at, length);
	out[length] = 0;
	*at = bar < end ? bar + 1 : end;
}

static int line_parse(const char *line, const char *end, struct ae_mm_playlist *p)
{
	const char *at = line, *word;
	int bars = 0;

	for (word = line; word < end; word++)
		bars += *word == '|';
	if (bars < 3)
		return 0;
	memset(p, 0, sizeof(*p));
	field_copy(&at, end, p->id, sizeof(p->id));
	field_copy(&at, end, p->label, sizeof(p->label));
	field_copy(&at, end, p->description, sizeof(p->description));
	/* (the addresses: space separated, the rest of the line) */
	while (at < end && p->address_count < AE_MM_PLAYLIST_ADDRESSES)
	{
		size_t length;

		while (at < end && (*at == ' ' || *at == '\t' || *at == '|'))
			at++;
		word = at;
		while (at < end && *at != ' ' && *at != '\t' && *at != '|')
			at++;
		length = (size_t)(at - word);
		if (!length)
			continue;
		if (length >= sizeof(p->addresses[0]))
			length = sizeof(p->addresses[0]) - 1;
		memcpy(p->addresses[p->address_count], word, length);
		p->addresses[p->address_count][length] = 0;
		p->address_count++;
	}
	return p->id[0] && p->label[0] && p->address_count > 0;
}

int ae_mm_playlists_parse(const char *text, struct ae_mm_playlist *out, int maximum)
{
	int count = 0;

	while (text && *text && count < maximum)
	{
		const char *end = strchr(text, '\n'), *next;

		if (!end)
			end = text + strlen(text);
		next = *end ? end + 1 : end;
		while (end > text && (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'))
			end--;
		while (text < end && (*text == ' ' || *text == '\t'))
			text++;
		if (text < end && *text != '#' && line_parse(text, end, &out[count]))
			count++;
		text = next;
	}
	return count;
}

int ae_mm_playlist_pick(struct ae_mm_playlist const *p, struct ae_mm_server_seen const *seen, int seen_count, int tried_mask,
	int party)
{
	int index, entry;

	if (!p)
		return -1;
	for (index = 0; index < p->address_count; index++)
	{
		struct ae_mm_server_seen const *found = NULL;

		if (tried_mask & (1 << index))
			continue;
		for (entry = 0; entry < seen_count && !found; entry++)
			if (strcmp(seen[entry].address, p->addresses[index]) == 0)
				found = &seen[entry];
		if (!found)
			return index;
		if (found->reachable && found->players + party <= found->max_players)
			return index;
	}
	return -1;
}
