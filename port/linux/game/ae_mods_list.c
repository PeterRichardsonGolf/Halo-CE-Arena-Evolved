/*
AE_MODS_LIST.C

The mods list (ae_mods_list.h): read from the engine's mods (through struct ae_mods_source), changed through the
engine's own load order, kept in load_order.toml.
*/

#include "ae_mods_list.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the engine's reasons for not playing a mod (port_mods.c's refusals), as the list says them */
static const char *refusal_text(const char *refusal)
{
	if (!refusal)
		return "";
	if (!strcmp(refusal, "made for a later build"))
		return "Made for a newer version of the game (its mod.toml's format).";
	if (!strcmp(refusal, "its list of maps names none"))
		return "Its mod.toml's list of maps names no map.";
	return refusal;
}

static int same_name(const char *first, const char *second)
{
	if (!first || !second)
		return 0;
	for (; *first && *second; first++, second++)
	{
		if (tolower((unsigned char)*first) != tolower((unsigned char)*second))
			return 0;
	}
	return *first == *second;
}

/* `text` with `more` added after a separator when it already says something */
static void note_add(char *text, size_t size, const char *more)
{
	size_t length = strlen(text);

	if (length + 2 < size)
		snprintf(text + length, size - length, "%s%s", length ? " " : "", more);
}

/* the mod's load position, or -1 */
static long position_of(const struct ae_mods_source *source, long index)
{
	long count = source->count(), position;

	for (position = 0; position < count; position++)
	{
		if (source->at(position) == index)
			return position;
	}
	return -1;
}

/* the list's item of an engine index, or NULL */
static struct ae_mods_item *item_of(struct ae_mods_list *list, const struct ae_mods_source *source, long index)
{
	long position = position_of(source, index);
	long row = position >= 0 ? ae_mods_priority_of(list->count, position) - 1 : -1;

	return row >= 0 && row < list->count ? &list->items[row] : NULL;
}

/* the files of the played mods: each one's kinds into its item, and the pairs of mods with files of the same tags */
static void files_read(const struct ae_mods_source *source, struct ae_mods_list *list)
{
	long count = source->files ? source->files() : 0, file, kept = 0;
	struct ae_mods_claim *claims;
	int pair;

	list->overlap_count = 0;
	if (count <= 0)
		return;
	claims = malloc((size_t)count * sizeof(*claims));
	for (file = 0; file < count; file++)
	{
		long mod = -1;
		const char *key = source->file_key(file, &mod);
		struct ae_mods_item *item = key ? item_of(list, source, mod) : NULL;

		if (!item)
			continue;
		item->files_known = 1;
		item->files[ae_mods_group_of(key)]++;
		/* (only the files the game plays can play over each other's) */
		if (claims && ae_mods_group_of(key) != AE_MODS_GROUP_OTHER)
		{
			claims[kept].key = key;
			claims[kept].mod = mod;
			claims[kept].position = item->position;
			kept++;
		}
	}
	if (!claims)
		return;
	list->overlap_count = ae_mods_overlaps(claims, kept, list->overlaps, AE_MODS_OVERLAPS_MOST);
	free(claims);
	for (pair = 0; pair < list->overlap_count && pair < AE_MODS_OVERLAPS_MOST; pair++)
	{
		const struct ae_mods_overlap *overlap = &list->overlaps[pair];
		struct ae_mods_item *first = item_of(list, source, overlap->first);
		struct ae_mods_item *second = item_of(list, source, overlap->second);
		char line[AE_MODS_REASON_CHARACTERS * 4];

		if (!first || !second)
			continue;
		snprintf(line, sizeof(line), "Shares %ld files with %s: %s's play.", overlap->files, second->name, second->name);
		note_add(first->note, sizeof(first->note), line);
		snprintf(line, sizeof(line), "Shares %ld files with %s: this mod's play.", overlap->files, first->name);
		note_add(second->note, sizeof(second->note), line);
	}
}

long ae_mods_list_read(const struct ae_mods_source *source, struct ae_mods_list *list)
{
	long count, position;

	memset(list, 0, sizeof(*list));
	if (!source)
		return 0;
	count = source->count();
	if (count > AE_MODS_LIST_MOST)
		count = AE_MODS_LIST_MOST;
	list->count = count;
	list->active = source->active ? source->active() != 0 : 0;
	for (position = 0; position < count; position++)
	{
		long index = source->at(position);
		struct ae_mods_item *item = &list->items[ae_mods_priority_of(count, position) - 1];
		const char *id = "", *version = "", *authors = "", *description = "", *refusal = NULL;
		long map_count = 0, played_place = -1, file_count = 0, map;
		int flags = 0, problem;
		char text[AE_MODS_REASON_CHARACTERS * 4];

		item->position = position;
		item->priority = ae_mods_priority_of(count, position);
		ae_mods_text_clean(item->name, sizeof(item->name), source->folder(index), AE_MODS_NAME_CHARACTERS, 0);
		ae_mods_text_clean(item->title, sizeof(item->title), source->name(index), AE_MODS_TITLE_CHARACTERS, 0);
		if (!source->fields(index, &id, &version, &authors, &description, &refusal, &map_count, &played_place,
			&file_count, &flags))
		{
			continue;
		}
		ae_mods_text_clean(item->id, sizeof(item->id), id, AE_MODS_NAME_CHARACTERS, 0);
		ae_mods_text_clean(item->version, sizeof(item->version), version, AE_MODS_VERSION_CHARACTERS, 0);
		ae_mods_text_clean(item->authors, sizeof(item->authors), authors, AE_MODS_AUTHORS_CHARACTERS, 0);
		ae_mods_text_clean(item->description, sizeof(item->description), description, AE_MODS_DESCRIPTION_CHARACTERS,
			1);
		/* (the maps it is for, by their files' names) */
		text[0] = 0;
		for (map = 0; map < map_count && source->map; map++)
		{
			const char *name = source->map(index, map);
			size_t length = strlen(text);

			if (name && name[0] && length + strlen(name) + 3 < sizeof(text))
				snprintf(text + length, sizeof(text) - length, "%s%s", length ? ", " : "", name);
		}
		ae_mods_text_clean(item->maps, sizeof(item->maps), text, AE_MODS_MAPS_CHARACTERS, 0);
		item->on = !(flags & 1);
		item->manifest_damaged = (flags & 2) != 0;
		item->state = ae_mods_state_of(refusal != NULL, flags & 1, played_place >= 0, list->active);
		if (item->state == AE_MODS_STATE_REFUSED)
		{
			ae_mods_text_clean(item->reason, sizeof(item->reason), refusal_text(refusal), AE_MODS_REASON_CHARACTERS, 0);
		}
		else if (item->state == AE_MODS_STATE_WAITS)
		{
			ae_mods_text_clean(item->reason, sizeof(item->reason), played_place >= 0 ?
				"Plays once a map that takes mods loads." : "Plays when the mods are found again.",
				AE_MODS_REASON_CHARACTERS, 0);
		}
		if (item->manifest_damaged)
			note_add(item->note, sizeof(item->note), "Its mod.toml cannot be read: played without what it says.");
		/* (the engine's own line: another that is on says it cannot be played with it (both still play), or it loads
		on the wrong side of a mod it names) */
		text[0] = 0;
		problem = source->status ? source->status(index, text, sizeof(text)) : 0;
		if (problem == 1 || problem == 2)
		{
			char cleaned[AE_MODS_REASON_CHARACTERS * 4 + 1];

			ae_mods_text_clean(cleaned, sizeof(cleaned), text, AE_MODS_REASON_CHARACTERS, 0);
			note_add(item->note, sizeof(item->note), cleaned);
		}
	}
	files_read(source, list);
	return count;
}

long ae_mods_list_find(const struct ae_mods_source *source, const char *name)
{
	long count = source ? source->count() : 0, index;

	if (!name || !name[0])
		return -1;
	for (index = 0; index < count; index++)
	{
		if (same_name(source->folder(index), name))
			return index;
	}
	/* (then by mod.toml's id) */
	for (index = 0; index < count; index++)
	{
		const char *id = "", *version, *authors, *description, *refusal;
		long map_count, played_place, file_count;
		int flags;

		if (source->fields(index, &id, &version, &authors, &description, &refusal, &map_count, &played_place,
			&file_count, &flags) && same_name(id, name))
		{
			return index;
		}
	}
	return -1;
}

/* keeps and plays the changed load order: the answer of the change, said into `answer` after `what` */
static int keep(const struct ae_mods_source *source, const char *what, char *answer, size_t size)
{
	int kept = source->save();

	source->apply();
	if (!kept)
	{
		snprintf(answer, size, "%s; load_order.toml cannot be written: kept until the game quits", what);
		return AE_MODS_CHANGE_NOT_KEPT;
	}
	if (source->active && source->active())
	{
		snprintf(answer, size, "%s; playing now", what);
		return AE_MODS_CHANGE_NOW;
	}
	snprintf(answer, size, "%s; plays once a map that takes mods loads", what);
	return AE_MODS_CHANGE_NEXT_MAP;
}

/* the engine's refusal of a mod, or NULL */
static const char *refusal_of(const struct ae_mods_source *source, long index)
{
	const char *id, *version, *authors, *description, *refusal = NULL;
	long map_count, played_place, file_count;
	int flags;

	if (!source->fields(index, &id, &version, &authors, &description, &refusal, &map_count, &played_place, &file_count,
		&flags))
	{
		return NULL;
	}
	return refusal;
}

int ae_mods_list_turn(const struct ae_mods_source *source, const char *name, int on, char *answer, size_t size)
{
	long index = ae_mods_list_find(source, name);
	char what[160];

	if (index < 0)
	{
		snprintf(answer, size, "no mod named %s", name ? name : "");
		return AE_MODS_CHANGE_REFUSED;
	}
	if (on && refusal_of(source, index))
	{
		snprintf(answer, size, "%s cannot be played: %s", source->folder(index), refusal_text(refusal_of(source, index)));
		return AE_MODS_CHANGE_REFUSED;
	}
	source->turn(index, on);
	snprintf(what, sizeof(what), "%s %s", source->folder(index), on ? "on" : "off");
	return keep(source, what, answer, size);
}

int ae_mods_list_order(const struct ae_mods_source *source, const char *name, long priority, char *answer, size_t size)
{
	long index = ae_mods_list_find(source, name);
	long count = source ? source->count() : 0, position, moved;
	char what[160];

	if (index < 0)
	{
		snprintf(answer, size, "no mod named %s", name ? name : "");
		return AE_MODS_CHANGE_REFUSED;
	}
	if (priority < 1 || priority > count)
	{
		snprintf(answer, size, "priority %ld is not 1 to %ld", priority, count);
		return AE_MODS_CHANGE_REFUSED;
	}
	position = position_of(source, index);
	moved = source->move(position, ae_mods_priority_places(count, position, priority));
	snprintf(what, sizeof(what), "%s at priority %ld of %ld", source->folder(index), ae_mods_priority_of(count, moved),
		count);
	return keep(source, what, answer, size);
}

void ae_mods_list_line(const struct ae_mods_list *list, char *text, size_t size)
{
	static const char *const states[] = { "on", "off", "waits", "refused" };
	long row, on = 0;
	size_t length;

	if (!size)
		return;
	for (row = 0; row < list->count; row++)
		on += list->items[row].state == AE_MODS_STATE_ON || list->items[row].state == AE_MODS_STATE_WAITS;
	snprintf(text, size, "%ld mod%s, %ld on%s", list->count, list->count == 1 ? "" : "s", on,
		list->count ? ":" : "");
	for (row = 0; row < list->count; row++)
	{
		char entry[96];

		snprintf(entry, sizeof(entry), "%s %s %ld %s", row ? "," : "", list->items[row].name, list->items[row].priority,
			states[list->items[row].state]);
		length = strlen(text);
		if (length + strlen(entry) + 4 >= size)
		{
			if (length + 5 < size)
				strcat(text, " ...");
			return;
		}
		strcat(text, entry);
	}
	length = strlen(text);
	if (list->overlap_count && length + 40 < size)
		snprintf(text + length, size - length, "; %d pair%s share files", list->overlap_count,
			list->overlap_count == 1 ? "" : "s");
}

int ae_mods_list_command(const struct ae_mods_source *source, const char *verb, const char *name, long priority,
	char *answer, size_t size)
{
	static struct ae_mods_list list;
	int change;

	if (!verb || !source)
	{
		snprintf(answer, size, "mods list | on <name> | off <name> | order <name> <priority>");
		return 0;
	}
	if (same_name(verb, "list"))
	{
		ae_mods_list_read(source, &list);
		ae_mods_list_line(&list, answer, size);
		return 1;
	}
	if (same_name(verb, "on") || same_name(verb, "off"))
		change = ae_mods_list_turn(source, name, same_name(verb, "on"), answer, size);
	else if (same_name(verb, "order"))
		change = ae_mods_list_order(source, name, priority, answer, size);
	else
	{
		snprintf(answer, size, "mods list | on <name> | off <name> | order <name> <priority>");
		return 0;
	}
	return change == AE_MODS_CHANGE_NOW || change == AE_MODS_CHANGE_NEXT_MAP;
}
