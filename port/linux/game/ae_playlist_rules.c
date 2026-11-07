/*
AE_PLAYLIST_RULES.C

See ae_playlist_rules.h.
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ae_playlist_rules.h"

int ae_pl_weight(
	int written)
{
	return written >= 1 && written <= AE_PL_WEIGHT_MAXIMUM ? written : 1;
}

int ae_pl_weight_parse(
	char const *text,
	int *weight)
{
	if (!text || text[0] < '1' || text[0] > '0' + AE_PL_WEIGHT_MAXIMUM || text[1])
		return 0;
	if (weight)
		*weight = text[0] - '0';
	return 1;
}

static unsigned long next_random(
	unsigned long *state)
{
	unsigned long x = *state & 0xFFFFFFFFUL;

	if (!x)
		x = 0x9E3779B9UL;
	x ^= (x << 13) & 0xFFFFFFFFUL;
	x ^= x >> 17;
	x ^= (x << 5) & 0xFFFFFFFFUL;
	*state = x ? x : 0x9E3779B9UL;
	return *state;
}

int ae_pl_sample(
	int const *weights,
	unsigned char const *eligible,
	int count,
	int wanted,
	unsigned long *state,
	int *out)
{
	unsigned char taken[AE_PL_ENTRIES] = { 0 };
	int picked = 0;

	if (count < 1 || count > AE_PL_ENTRIES || wanted < 1)
		return 0;
	while (picked < wanted)
	{
		unsigned long total = 0, roll;
		int index;

		for (index = 0; index < count; index++)
			if (eligible[index] && !taken[index])
				total += (unsigned long)ae_pl_weight(weights ? weights[index] : 1);
		if (!total)
			break;
		roll = next_random(state) % total;
		for (index = 0; index < count; index++)
		{
			unsigned long weight;

			if (!eligible[index] || taken[index])
				continue;
			weight = (unsigned long)ae_pl_weight(weights ? weights[index] : 1);
			if (roll < weight)
			{
				taken[index] = 1;
				out[picked++] = index;
				break;
			}
			roll -= weight;
		}
	}
	return picked;
}

static int same_text(
	char const *a,
	char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return !*a && !*b;
}

static int holds_text(
	char const *text,
	char const *query)
{
	size_t length = strlen(query);

	if (!length)
		return 0;
	for (; *text; text++)
	{
		size_t at;

		for (at = 0; at < length && text[at] && tolower((unsigned char)text[at]) == tolower((unsigned char)query[at]); at++)
		{
		}
		if (at == length)
			return 1;
	}
	return 0;
}

void ae_pl_history_push(
	struct ae_pl_history *history,
	char const *text)
{
	int index;

	if (history->count == AE_PL_HISTORY_MAXIMUM)
	{
		for (index = 1; index < AE_PL_HISTORY_MAXIMUM; index++)
			memcpy(history->text[index - 1], history->text[index], AE_PL_TEXT);
		history->count--;
	}
	snprintf(history->text[history->count++], AE_PL_TEXT, "%s", text);
}

int ae_pl_history_recent(
	struct ae_pl_history const *history,
	char const *text,
	int depth)
{
	int index;

	if (depth > AE_PL_HISTORY_MAXIMUM)
		depth = AE_PL_HISTORY_MAXIMUM;
	for (index = history->count - 1; index >= 0 && index >= history->count - depth; index--)
		if (same_text(history->text[index], text))
			return 1;
	return 0;
}

int ae_pl_match_entry(
	char const *const *map_names,
	char const *const *texts,
	int count,
	char const *query)
{
	int index, found = -1;

	while (query && *query == ' ')
		query++;
	if (!query || !*query)
		return -1;
	for (index = 0; index < count; index++)
		if (same_text(map_names[index], query))
			return index; /* (one map in several entries: the first) */
	for (index = 0; index < count; index++)
	{
		if (holds_text(map_names[index], query) || holds_text(texts[index], query))
		{
			if (found >= 0)
			{
				/* (the same map in several entries is one answer) */
				if (!same_text(map_names[found], map_names[index]))
					return -2;
				continue;
			}
			found = index;
		}
	}
	return found;
}

void ae_pl_people_reset(
	struct ae_pl_people *people)
{
	memset(people, 0, sizeof(*people));
}

static int person_of(
	struct ae_pl_people *people,
	long id,
	int add)
{
	int index;

	for (index = 0; index < people->count; index++)
		if (people->id[index] == id)
			return index;
	if (!add || people->count >= AE_PL_PEOPLE)
		return -1;
	people->id[people->count] = id;
	people->nominated[people->count] = -1;
	people->rtv[people->count] = 0;
	return people->count++;
}

int ae_pl_nominate(
	struct ae_pl_people *people,
	long id,
	int entry)
{
	int person;

	if (entry < 0)
		return 0;
	person = person_of(people, id, 1);
	if (person < 0)
		return 0;
	people->nominated[person] = entry;
	return 1;
}

int ae_pl_nominated(
	struct ae_pl_people const *people,
	int *out)
{
	int index, other, count = 0;

	for (index = 0; index < people->count; index++)
	{
		if (people->nominated[index] < 0)
			continue;
		for (other = 0; other < count && out[other] != people->nominated[index]; other++)
		{
		}
		if (other == count)
			out[count++] = people->nominated[index];
	}
	return count;
}

int ae_pl_rtv_ask(
	struct ae_pl_people *people,
	long id)
{
	int person = person_of(people, id, 1), index, asks = 0;

	if (person < 0)
		return -1;
	people->rtv[person] = 1;
	for (index = 0; index < people->count; index++)
		asks += people->rtv[index] ? 1 : 0;
	return asks;
}

int ae_pl_rtv_needed(
	int humans)
{
	return humans < 1 ? 0 : humans / 2 + 1;
}

int ae_pl_rtv_passed(
	struct ae_pl_people const *people,
	long const *present,
	int present_count)
{
	int index, other, asks = 0;

	if (present_count < 1)
		return 0;
	for (index = 0; index < present_count; index++)
		for (other = 0; other < people->count; other++)
			if (people->id[other] == present[index] && people->rtv[other])
			{
				asks++;
				break;
			}
	return asks >= ae_pl_rtv_needed(present_count);
}

int ae_pl_extend_allowed(
	int used)
{
	return used < 1;
}

int ae_pl_extend_minutes(
	int setting)
{
	return setting < 1 ? 1 : setting > 60 ? 60 : setting;
}

int ae_pl_audience(
	int needs_ae,
	int needs_mcc)
{
	return needs_mcc ? AE_AUDIENCE_AE_MCC : needs_ae ? AE_AUDIENCE_AE : AE_AUDIENCE_ANYONE;
}

char const *ae_pl_audience_word(
	int audience)
{
	return audience == AE_AUDIENCE_AE_MCC ? "ae+mcc" : audience == AE_AUDIENCE_AE ? "ae" : "all";
}

char const *ae_pl_audience_name(
	int audience)
{
	return audience == AE_AUDIENCE_AE_MCC ? "Arena Evolved with MCC's files" : audience == AE_AUDIENCE_AE ?
		"Arena Evolved" : "any Halo CE";
}

int ae_pl_can_play(
	int audience,
	int ae,
	int mcc)
{
	if (audience == AE_AUDIENCE_ANYONE)
		return 1;
	if (!ae)
		return 0;
	return audience == AE_AUDIENCE_AE || mcc != 0;
}

int ae_pl_next_playable(
	int const *audiences,
	unsigned char const *playable,
	int count,
	int start,
	int has_non_ae,
	int has_no_mcc)
{
	int step;

	for (step = 0; step < count; step++)
	{
		int index = (start + step) % count;

		if (!playable[index])
			continue;
		if (audiences[index] != AE_AUDIENCE_ANYONE && has_non_ae)
			continue;
		if (audiences[index] == AE_AUDIENCE_AE_MCC && has_no_mcc)
			continue;
		return index;
	}
	return -1;
}

int ae_pl_range_mask(
	int const *audiences,
	unsigned char const *playable,
	int count)
{
	int index, mask = 0;

	for (index = 0; index < count; index++)
		if (playable[index] && audiences[index] >= 0 && audiences[index] < AE_AUDIENCE_COUNT)
			mask |= 1 << audiences[index];
	return mask;
}

int ae_pl_range_mixed(
	int mask)
{
	return mask & (mask - 1) ? 1 : 0;
}

void ae_pl_refusal_text(
	int audience,
	char *text,
	int size)
{
	if (audience == AE_AUDIENCE_AE_MCC)
		snprintf(text, (size_t)size, "This game needs Arena Evolved with MCC's files on this PC.");
	else if (audience == AE_AUDIENCE_AE)
		snprintf(text, (size_t)size, "This game needs Arena Evolved 0.7.7 or later (arenaevolved.com).");
	else
		snprintf(text, (size_t)size, "This game cannot be joined.");
}

/* ---------- the map check */

static int same_text_case(
	char const *a,
	char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return !*a && !*b;
}

int ae_pl_map_table_find(
	struct ae_pl_map_table const *table,
	char const *identity)
{
	int index;

	if (!identity)
		return -1;
	for (index = 0; index < table->count; index++)
		if (same_text_case(table->name[index], identity))
			return index;
	return -1;
}

int ae_pl_map_table_slot(
	struct ae_pl_map_table *table,
	char const *identity)
{
	int slot;

	if (!identity || !identity[0] || strlen(identity) >= AE_PL_MAP_NAME)
		return -1;
	slot = ae_pl_map_table_find(table, identity);
	if (slot >= 0)
		return slot;
	if (table->count >= AE_PL_MAPS)
		return -1;
	snprintf(table->name[table->count], AE_PL_MAP_NAME, "%s", identity);
	return table->count++;
}

int ae_pl_map_table_same(
	struct ae_pl_map_table const *a,
	struct ae_pl_map_table const *b)
{
	int index;

	if (a->count != b->count)
		return 0;
	for (index = 0; index < a->count; index++)
		if (strcmp(a->name[index], b->name[index]))
			return 0;
	return 1;
}

int ae_pl_ask_messages(
	int count)
{
	return count < 1 ? 0 : (count + AE_PL_ASK_PER_MESSAGE - 1) / AE_PL_ASK_PER_MESSAGE;
}

void ae_pl_holding_reset(
	struct ae_pl_holding *holding)
{
	memset(holding, 0, sizeof(*holding));
}

void ae_pl_bits_set(
	unsigned char *bits,
	int slot)
{
	if (slot >= 0 && slot < AE_PL_MAPS)
		bits[slot / 8] |= (unsigned char)(1 << (slot % 8));
}

void ae_pl_holding_set(
	struct ae_pl_holding *holding,
	int has_mcc,
	unsigned char const *bits,
	int count)
{
	int slot;

	ae_pl_holding_reset(holding);
	holding->told = 1;
	holding->has_mcc = has_mcc ? 1 : 0;
	holding->bits_told = 1;
	for (slot = 0; slot < count && slot < AE_PL_MAPS; slot++)
		if (bits[slot / 8] & (1 << (slot % 8)))
			ae_pl_bits_set(holding->bits, slot);
}

void ae_pl_holding_set_join(
	struct ae_pl_holding *holding,
	int has_mcc)
{
	ae_pl_holding_reset(holding);
	holding->told = 1;
	holding->has_mcc = has_mcc ? 1 : 0;
}

int ae_pl_holding_state(
	struct ae_pl_holding const *holding,
	int slot)
{
	if (!holding->told)
		return -1;
	if (!holding->has_mcc)
		return 0;
	if (!holding->bits_told)
		return -1;
	if (slot < 0 || slot >= AE_PL_MAPS)
		return 1;
	return (holding->bits[slot / 8] >> (slot % 8)) & 1;
}

int ae_pl_missing(
	struct ae_pl_holding const *holding,
	int slot)
{
	if (ae_pl_holding_state(holding, slot) != 0)
		return AE_MISSING_NOTHING;
	if (holding->has_mcc && !holding->bits_told)
		return AE_MISSING_NOTHING;
	return holding->has_mcc ? AE_MISSING_MAP : AE_MISSING_MCC;
}

void ae_pl_missing_text(
	int missing,
	char const *identity,
	char *text,
	int size)
{
	char const *at = identity ? strstr(identity, "@ws") : NULL;
	char const *id = at ? at + 3 : NULL;

	if (missing == AE_MISSING_MCC)
		snprintf(text, (size_t)size, "This game needs MCC: install Halo: The Master Chief Collection (Steam) on this PC.");
	else if (missing == AE_MISSING_MAP && id && *id)
		snprintf(text, (size_t)size, "Subscribe to %.*s (item %s) in the Steam Workshop.", (int)(at - identity), identity, id);
	else if (missing == AE_MISSING_MAP)
		snprintf(text, (size_t)size, "This game needs %.40s, which is not in this PC's MCC files.", identity ? identity : "a map");
	else
		snprintf(text, (size_t)size, "This game cannot be joined.");
}
