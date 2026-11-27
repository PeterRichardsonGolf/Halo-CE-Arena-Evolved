/*
AE_TAG_INJECT.C

(Arena Evolved, DEV MODE only for now) See ae_tag_inject.h. The profile is "ae_dev" in the data folder's
tag_injector folder. A map is opened by its level name: a Custom Edition map's file name without ".map" (in
maps\ce), or an MCC map's identity (<name>@mcc, <name>@ws<item id>). A map cannot be opened while a map of its kind
is played: the tag injector says why. A tag is taken by its number in its category's list, or by a part of its path.
*/

#include "cev/cev_names.h"

#include "cseries.h"
#include "tag_files/tag_groups.h"
#include "cev/tag_injector.h"
#include "ae_tag_loaded.h"
#include "cev_glue.h"
#include "mcc_maps.h"
#include "ae_tag_inject.h"
#include "ae_set_body.h"
#include "ae_item_sets_rules.h"
#include "randomizer.h"
#include "firing_range.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);

/* (port_tag_profiles.c) */
long port_tag_profiles_scan(void);
long port_tag_profiles_find(const char *folder);
long port_tag_profiles_create(const char *name);
long port_tag_profiles_tag_count(long profile);
int port_tag_profiles_delete(long profile);
long port_tag_profiles_tag_find(long profile, unsigned long group, const char *name);
const char *port_tag_profiles_tag_map(long profile, long tag);
unsigned long port_tag_profiles_tag_group(long profile, long tag);
const char *port_tag_profiles_tag_name(long profile, long tag);
int port_tag_profiles_set_option(long profile, const char *key, const char *value);

#define INJECT_PROFILE "ae_dev"

enum
{
	MAXIMUM_INJECT_ROOTS = 32,
	MAXIMUM_INJECT_HUDS = 8,
	MAXIMUM_INJECT_PATH = 256,
	MAXIMUM_LISTED = 200,
};

static struct
{
	/* the command turned the profile's tags on for the levels loaded next */
	boolean on;
	/* the loaded level has them */
	boolean loaded;
	/* the biped last taken ("" none) */
	char biped[MAXIMUM_INJECT_PATH];
	/* the hands last taken, as a set keeps them ("path#map"; "" none) */
	char hands[MAXIMUM_INJECT_PATH];
} inject = { FALSE, FALSE, "", "" };

static long inject_profile(
	boolean make)
{
	long profile;

	port_tag_profiles_scan();
	profile = port_tag_profiles_find(INJECT_PROFILE);
	if (profile < 0 && make)
		profile = port_tag_profiles_create(INJECT_PROFILE);

	return profile < 0 ? NONE : profile;
}

/* the map of the injector's list a level name is: an MCC map's identity, else a Custom Edition map's level name */
short ae_tag_inject_map_of_level(
	char const *level)
{
	if (strchr(level, '@'))
	{
		short rows;
		short row;

		tag_injector_maps_find();
		rows = mcc_maps_type_count(FALSE);
		for (row = 0; row < rows; row++)
		{
			short catalog = mcc_maps_type_index(row, FALSE);
			char const *level_name = catalog != NONE ? mcc_maps_level_name(catalog) : NULL;

			if (level_name && !_stricmp(level_name, level))
				return tag_injector_map_of_mcc(catalog);
		}

		return NONE;
	}
	if (!cev_maps_add(level))
		return NONE;
	tag_injector_maps_find();

	return tag_injector_map_named(level);
}

/* a category's tag by its number in the list, or the first whose path has the text (any case); NONE when none */
static long inject_tag_named(
	short category,
	long count,
	char const *text)
{
	char const *digit = text;
	long index;

	while (*digit >= '0' && *digit <= '9')
		digit++;
	if (text[0] && !*digit)
	{
		index = atol(text);

		return index < count ? index : NONE;
	}
	for (index = 0; index < count; index++)
	{
		char const *path = tag_injector_category_tag(category, index);
		size_t length = strlen(text);

		for (; path && *path; path++)
		{
			if (!_strnicmp(path, text, length))
				return index;
		}
	}

	return NONE;
}

void ae_tag_inject_tags_loaded(
	char const *map_name)
{
	long roots[MAXIMUM_INJECT_ROOTS], root_tags[MAXIMUM_INJECT_ROOTS];
	short huds[MAXIMUM_INJECT_HUDS];
	long profile, hud_count, root_count;
	/* (the firing range's profile, a folder of the data folder's tag_injector: its weapons and AI laid out) */
	char const *range_profile;

	if (!map_name || !strcmp(map_name, "ui"))
		return;
	range_profile = firing_range_level_profile(map_name);
	if (!inject.on && !range_profile)
		return;
	/* (the randomizer's range branch injected the range's profile: a second load would overwrite the first one's table) */
	if (range_profile && ae_randomizer_range_injected())
		return;
	if (range_profile)
	{
		port_tag_profiles_scan();
		profile = port_tag_profiles_find(range_profile);
		profile = profile < 0 ? NONE : profile;
	}
	else
		profile = inject_profile(FALSE);
	if (profile == NONE)
		return;
	hud_count = tag_injector_tags_read(profile, huds, MAXIMUM_INJECT_HUDS);
	root_count = tag_injector_tags_load(profile, roots, root_tags, MAXIMUM_INJECT_ROOTS);
	if (range_profile)
		firing_range_tags_injected(profile, roots, root_tags, root_count);
	inject.loaded = tag_injector_tags_loaded();
	ae_tag_table_changed();
	platform_log("inject: %s: the profile's tags %s (%ld roots, %ld stock HUDs wanted)", map_name,
		inject.loaded ? "added" : "NOT added", root_count, hud_count);
}

void ae_tag_inject_tags_unloaded(
	void)
{
	if (!inject.loaded)
		return;
	inject.loaded = FALSE;
	tag_injector_tags_unloaded();
	ae_tag_table_changed();
}

boolean ae_tag_inject_read(
	long tag_index,
	long offset,
	long size,
	void *buffer)
{
	/* (whoever added the tags: this command's profile, or a level's props, a Forge session's or a game's shared ones, none of which set inject.loaded) */
	return tag_injector_tags_loaded() && tag_injector_read(tag_index, offset, size, buffer) != 0;
}

long ae_tag_inject_player_biped(
	long unit_definition_index)
{
	long tag_index;

	if (!inject.loaded || !inject.biped[0])
		return ae_set_body_biped(unit_definition_index);
	tag_index = tag_loaded('bipd', inject.biped);

	return tag_index != NONE ? tag_index : unit_definition_index;
}

boolean ae_tag_inject_command(
	int words,
	char const *verb,
	char const *first,
	char const *second,
	char *answer,
	unsigned int answer_size)
{
	char reason[160] = "";

	if (!strcmp(verb, "status"))
	{
		long profile = inject_profile(FALSE);

		snprintf(answer, answer_size, "inject %s, this level %s them; profile %s: %ld tags; map open: %s; biped '%s'",
			inject.on ? "on" : "off", inject.loaded ? "has" : "has not", INJECT_PROFILE,
			profile == NONE ? 0L : port_tag_profiles_tag_count(profile), tag_injector_map_opened() != NONE ? "yes" : "no",
			inject.biped);
		return TRUE;
	}
	if (!strcmp(verb, "on") || !strcmp(verb, "off"))
	{
		inject.on = !strcmp(verb, "on");
		snprintf(answer, answer_size, "inject %s from the next level loaded", verb);
		return TRUE;
	}
	if (!strcmp(verb, "close"))
	{
		tag_injector_map_close();
		snprintf(answer, answer_size, "no map is open");
		return TRUE;
	}
	if (!strcmp(verb, "clear"))
	{
		long profile = inject_profile(FALSE);

		if (inject.loaded)
		{
			snprintf(answer, answer_size, "this level has the profile's tags: inject off, load a level, then clear");
			return FALSE;
		}
		inject.biped[0] = 0;
		inject.hands[0] = 0;
		if (profile != NONE && !port_tag_profiles_delete(profile))
		{
			snprintf(answer, answer_size, "the profile could not be deleted");
			return FALSE;
		}
		snprintf(answer, answer_size, "the profile %s is empty", INJECT_PROFILE);
		return TRUE;
	}
	if (!strcmp(verb, "maps"))
	{
		tag_injector_maps_find();
		snprintf(answer, answer_size, "%d MCC maps listed; a Custom Edition map is opened by its file's name (maps\\ce)",
			(int)mcc_maps_type_count(FALSE));
		return TRUE;
	}
	if (!strcmp(verb, "open") && words >= 2)
	{
		short map;
		short category;
		int used;

		if (!tag_injector_maps_available(reason, sizeof(reason)))
		{
			snprintf(answer, answer_size, "no map can be opened: %s", reason);
			return FALSE;
		}
		map = ae_tag_inject_map_of_level(first);
		if (map == NONE || !tag_injector_map_open(map))
		{
			snprintf(answer, answer_size, "'%s' cannot be opened (the log says why)", first);
			return FALSE;
		}
		used = snprintf(answer, answer_size, "%s open:", first);
		for (category = 0; category < NUMBER_OF_TAG_INJECTOR_CATEGORIES && used > 0 && (unsigned int)used < answer_size;
			category++)
		{
			used += snprintf(answer + used, answer_size - (unsigned int)used, " %d %s %ld,", (int)category,
				tag_injector_category_name(category), tag_injector_category_count(category));
		}
		return TRUE;
	}
	if ((!strcmp(verb, "list") && words >= 2) || (!strcmp(verb, "take") && words >= 3))
	{
		short category = (short)atoi(first);
		long count;

		if (tag_injector_map_opened() == NONE)
		{
			snprintf(answer, answer_size, "no map is open: inject open <level>");
			return FALSE;
		}
		if (category < 0 || category >= NUMBER_OF_TAG_INJECTOR_CATEGORIES)
		{
			snprintf(answer, answer_size, "no category %d", (int)category);
			return FALSE;
		}
		count = tag_injector_category_count(category);
		if (!strcmp(verb, "list"))
		{
			long index;

			for (index = 0; index < count && index < MAXIMUM_LISTED; index++)
			{
				platform_log("inject: %s %ld: %s", tag_injector_category_name(category), index,
					tag_injector_category_tag(category, index));
			}
			snprintf(answer, answer_size, "%ld %s, the first is '%s' (all in the log)", count,
				tag_injector_category_name(category), count ? tag_injector_category_tag(category, 0) : "");
			return TRUE;
		}
		else
		{
			long index = inject_tag_named(category, count, second);
			long carried = 0;
			long profile = inject_profile(TRUE);
			char const *path;
			/* (the path copied before the tag is taken: taking it opens the map again, and the menu's own name is gone after) */
			char taken[256];

			if (index == NONE)
			{
				snprintf(answer, answer_size, "no tag '%s' among the %ld %s", second, count,
					tag_injector_category_name(category));
				return FALSE;
			}
			if (profile == NONE)
			{
				snprintf(answer, answer_size, "the profile cannot be made");
				return FALSE;
			}
			path = tag_injector_category_tag(category, index);
			snprintf(taken, sizeof(taken), "%s", path ? path : "");
			path = taken;
			if (!tag_injector_tag_add(profile, category, index, &carried, reason, sizeof(reason)) &&
				tag_injector_tag_in_profile(profile, category, index) == NONE)
			{
				snprintf(answer, answer_size, "'%s' was not taken: %s", path ? path : "?", reason);
				return FALSE;
			}
			if (tag_injector_category_group(category) == 'bipd' && path)
				snprintf(inject.biped, sizeof(inject.biped), "%s", path);
			if (tag_injector_category_group(category) == 'mod2' && path)
			{
				long entry = port_tag_profiles_tag_find(profile, 'mod2', path);
				long count = port_tag_profiles_tag_count(profile), each;
				size_t length = strlen(path);

				ae_is_hands_key(path, entry == NONE ? "" : port_tag_profiles_tag_map(profile, entry), inject.hands, sizeof(inject.hands));
				/* (a map's hands are kept by "path#map", tag_injector.c's menu_hands_entry_name: that entry's whole name is the key) */
				for (each = 0; entry == NONE && each < count; each++)
				{
					char const *name = port_tag_profiles_tag_name(profile, each);

					if (port_tag_profiles_tag_group(profile, each) == 'mod2' && name && !strncmp(name, path, length) && name[length] == '#')
					{
						snprintf(inject.hands, sizeof(inject.hands), "%s", name);
						break;
					}
				}
			}
			snprintf(answer, answer_size, "'%s' is in the profile (%ld tags, %ld carried); inject on, then load a level",
				path ? path : "?", port_tag_profiles_tag_count(profile), carried);
			return TRUE;
		}
	}
	if (!strcmp(verb, "body") && words >= 3)
	{
		long profile = inject_profile(FALSE);
		char const *hands = !strcmp(first, "taken") ? inject.hands : !strcmp(first, "campaign") ? "campaign" : "";

		/* (the profile is a set: its YOUR BODY is the biped last taken, the hands (own: its own, campaign: the campaign's, taken: the
		last hands taken) and the movement (spartan or own) the set's file keeps) */
		if (profile == NONE || !inject.biped[0] || (!strcmp(first, "taken") && !inject.hands[0]) ||
			(strcmp(second, "spartan") && strcmp(second, "own")))
		{
			snprintf(answer, answer_size, "inject body <own|campaign|taken> <spartan|own>: needs a biped taken (and hands for taken)");
			return FALSE;
		}
		if (!port_tag_profiles_set_option(profile, "player_biped", inject.biped) ||
			!port_tag_profiles_set_option(profile, "player_hands", hands) ||
			!port_tag_profiles_set_option(profile, "player_movement", !strcmp(second, "own") ? "own" : ""))
		{
			snprintf(answer, answer_size, "the profile's file cannot be written");
			return FALSE;
		}
		snprintf(answer, answer_size, "the set %s plays as %s, hands %s, movement %s", INJECT_PROFILE, inject.biped,
			hands[0] ? hands : "its own", second);
		return TRUE;
	}
	snprintf(answer, answer_size, "inject status|maps|open <level>|list <category>|take <category> <index>|body <own|campaign|taken> <spartan|own>|close|on|off|clear");

	return FALSE;
}
