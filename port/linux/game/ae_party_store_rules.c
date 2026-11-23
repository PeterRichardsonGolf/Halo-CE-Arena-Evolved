/* The party store (see the header). Pure code. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_party_store_rules.h"

static void clean(char *out, int size, char const *text)
{
	int at = 0;

	for (; *text && at < size - 1; text++)
		out[at++] = (char)(*text == '\t' || *text == '\n' || *text == '\r' ? ' ' : *text);
	out[at] = 0;
}

void ae_party_store_clear(struct ae_party_store *store)
{
	memset(store, 0, sizeof(*store));
}

/* the clock's next value; at its limit the records are renumbered 1 .. count in their order (the order is all `seen` is for) */
static long next_clock(struct ae_party_store *store)
{
	if (store->clock >= AE_STORE_CLOCK_MAX)
	{
		int done[AE_STORE_RECORDS], each, rank, pick;

		memset(done, 0, sizeof(done));
		for (rank = 1; rank <= store->count; rank++)
		{
			pick = -1;
			for (each = 0; each < store->count; each++)
				if (!done[each] && (pick < 0 || store->record[each].seen < store->record[pick].seen))
					pick = each;
			done[pick] = 1;
			store->record[pick].seen = rank;
		}
		store->clock = store->count;
	}
	return ++store->clock;
}

struct ae_party_record *ae_party_store_find(struct ae_party_store *store, char const *id)
{
	int at;

	for (at = 0; at < store->count; at++)
		if (!strcmp(store->record[at].id, id))
			return &store->record[at];
	return NULL;
}

/* the oldest record that is not saved, the one to drop: its index, -1 none */
static int oldest_unsaved(struct ae_party_store const *store)
{
	int at, best = -1;

	for (at = 0; at < store->count; at++)
		if (!store->record[at].saved && (best < 0 || store->record[at].seen < store->record[best].seen))
			best = at;
	return best;
}

struct ae_party_record *ae_party_store_touch(struct ae_party_store *store, char const *id, char const *name, int kind, int tier)
{
	struct ae_party_record *record;

	if (!id || !id[0])
		return NULL;
	record = ae_party_store_find(store, id);
	if (!record)
	{
		if (store->count >= AE_STORE_RECORDS)
		{
			int drop = oldest_unsaved(store);

			if (drop < 0)
				return NULL;
			memmove(&store->record[drop], &store->record[drop + 1], sizeof(store->record[0]) * (size_t)(store->count - drop - 1));
			store->count--;
		}
		record = &store->record[store->count++];
		memset(record, 0, sizeof(*record));
		clean(record->id, AE_PARTY_ID, id);
		record->kind = kind;
	}
	if (name && name[0])
		clean(record->name, AE_PARTY_NAME, name);
	if (tier > 0)
		record->tier = tier;
	record->seen = next_clock(store);
	return record;
}

static int saved_count(struct ae_party_store const *store)
{
	int at, saved = 0;

	for (at = 0; at < store->count; at++)
		saved += store->record[at].saved;
	return saved;
}

void ae_party_store_game(struct ae_party_store *store, char const *id, int together, int won)
{
	struct ae_party_record *record = ae_party_store_find(store, id);

	if (!record)
		return;
	if (together)
	{
		record->together_games++;
		record->together_wins += won != 0;
	}
	else
	{
		record->against_games++;
		record->against_wins += won != 0;
	}
	record->seen = next_clock(store);
	if (together && record->kind == AE_MEMBER_BOT && !record->saved && !record->blocked &&
		record->together_games >= AE_STORE_AUTO_FAVOURITE && saved_count(store) < AE_STORE_SAVED)
		record->saved = 1;
}

int ae_party_store_set_saved(struct ae_party_store *store, char const *id, int saved)
{
	struct ae_party_record *record = ae_party_store_find(store, id);

	if (!record || (saved && (record->blocked || (!record->saved && saved_count(store) >= AE_STORE_SAVED))))
		return 0;
	record->saved = saved != 0;
	return 1;
}

int ae_party_store_set_blocked(struct ae_party_store *store, char const *id, int blocked)
{
	struct ae_party_record *record = ae_party_store_find(store, id);

	if (!record)
		return 0;
	record->blocked = blocked != 0;
	if (blocked)
		record->saved = 0;
	return 1;
}

/* the records in newest-first order into out (those that pass the filter): a plain selection by `seen` */
static int newest_first(struct ae_party_store const *store, int saved_only, struct ae_party const *exclude,
	struct ae_party_record const **out, int max)
{
	int taken = 0;
	long above = 0x7FFFFFFF;

	while (taken < max)
	{
		struct ae_party_record const *best = NULL;
		int at;

		for (at = 0; at < store->count; at++)
		{
			struct ae_party_record const *record = &store->record[at];

			if (record->seen >= above || record->blocked || (saved_only && !record->saved) ||
				(exclude && ae_party_find_id(exclude, record->id) >= 0))
				continue;
			if (!best || record->seen > best->seen)
				best = record;
		}
		if (!best)
			break;
		out[taken++] = best;
		above = best->seen;
	}
	return taken;
}

int ae_party_store_recent(struct ae_party_store const *store, struct ae_party const *exclude, struct ae_party_record const **out, int max)
{
	return newest_first(store, 0, exclude, out, max);
}

int ae_party_store_saved(struct ae_party_store const *store, struct ae_party_record const **out, int max)
{
	return newest_first(store, 1, NULL, out, max);
}

void ae_party_store_member(struct ae_party_record const *record, struct ae_party_member *member)
{
	memset(member, 0, sizeof(*member));
	snprintf(member->id, sizeof(member->id), "%s", record->id);
	snprintf(member->name, sizeof(member->name), "%s", record->name);
	member->kind = record->kind;
	member->seats = 1;
	member->tier = record->tier;
	member->saved = record->saved;
	member->ping_ms = -1;
	member->together_games = record->together_games;
	member->together_wins = record->together_wins;
	member->against_games = record->against_games;
	member->against_wins = record->against_wins;
}

int ae_party_store_write(struct ae_party_store const *store, char *text, int size)
{
	int at, used = 0;

	if (size < 1)
		return -1;
	text[0] = 0;
	for (at = 0; at < store->count; at++)
	{
		struct ae_party_record const *r = &store->record[at];
		int wrote = snprintf(text + used, (size_t)(size - used), "%s\t%s\t%d\t%d\t%d\t%d\t%ld\t%d\t%d\t%d\t%d\n", r->id, r->name,
			r->kind, r->saved, r->blocked, r->tier, r->seen, r->together_games, r->together_wins, r->against_games, r->against_wins);

		if (wrote < 0 || used + wrote >= size)
			return -1;
		used += wrote;
	}
	return used;
}

int ae_party_store_read(struct ae_party_store *store, char const *text)
{
	int each, saved;

	ae_party_store_clear(store);
	while (*text)
	{
		char line[256], *field[11], *at;
		size_t length = strcspn(text, "\r\n");
		int count = 0;

		snprintf(line, sizeof(line), "%.*s", (int)(length < sizeof(line) - 1 ? length : sizeof(line) - 1), text);
		text += length;
		while (*text == '\r' || *text == '\n')
			text++;
		at = line;
		field[count++] = at;
		for (; *at && count < 11; at++)
			if (*at == '\t')
			{
				*at = 0;
				field[count++] = at + 1;
			}
		if (count == 11 && field[0][0] && !ae_party_store_find(store, field[0]))
		{
			struct ae_party_record r;
			struct ae_party_record *slot;

			memset(&r, 0, sizeof(r));
			clean(r.id, AE_PARTY_ID, field[0]);
			clean(r.name, AE_PARTY_NAME, field[1]);
			r.kind = atoi(field[2]) == AE_MEMBER_BOT ? AE_MEMBER_BOT : AE_MEMBER_FRIEND;
			r.saved = atoi(field[3]) != 0;
			r.blocked = atoi(field[4]) != 0;
			r.tier = atoi(field[5]);
			r.seen = atol(field[6]);
			if (r.seen < 0)
				r.seen = 0;
			if (r.seen > AE_STORE_CLOCK_MAX)
				r.seen = AE_STORE_CLOCK_MAX;
			r.together_games = atoi(field[7]);
			r.together_wins = atoi(field[8]);
			r.against_games = atoi(field[9]);
			r.against_wins = atoi(field[10]);
			if (r.saved && saved_count(store) >= AE_STORE_SAVED)
			{
				/* (a ninth favourite: the older of it and the oldest saved one is not a favourite) */
				struct ae_party_record *oldest_saved = NULL;

				for (each = 0; each < store->count; each++)
					if (store->record[each].saved && (!oldest_saved || store->record[each].seen < oldest_saved->seen))
						oldest_saved = &store->record[each];
				if (oldest_saved && oldest_saved->seen < r.seen)
					oldest_saved->saved = 0;
				else
					r.saved = 0;
			}
			if (store->count < AE_STORE_RECORDS)
				slot = &store->record[store->count++];
			else
			{
				/* (full: the newest records are kept; a newer line takes the place of the oldest one that is not saved) */
				int oldest = oldest_unsaved(store);

				if (oldest < 0 || store->record[oldest].seen >= r.seen)
					continue;
				slot = &store->record[oldest];
			}
			*slot = r;
		}
	}
	/* (at most AE_STORE_SAVED favourites: the newest keep it) */
	for (;;)
	{
		struct ae_party_record *oldest_saved = NULL;

		saved = 0;
		for (each = 0; each < store->count; each++)
			if (store->record[each].saved)
			{
				saved++;
				if (!oldest_saved || store->record[each].seen < oldest_saved->seen)
					oldest_saved = &store->record[each];
			}
		if (saved <= AE_STORE_SAVED)
			break;
		oldest_saved->saved = 0;
	}
	for (each = 0; each < store->count; each++)
		if (store->record[each].seen > store->clock)
			store->clock = store->record[each].seen;
	return store->count;
}
