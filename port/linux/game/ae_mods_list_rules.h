/*
AE_MODS_LIST_RULES.H

The mods list's pure rules: a mod's text made fit to draw, the kind of tag a mod's file replaces, a mod's state, the
move that puts a mod at a priority, and the files two mods both have. String and stdlib only, no engine headers.
*/

#ifndef AE_MODS_LIST_RULES_H
#define AE_MODS_LIST_RULES_H

#include <stddef.h>

/* the kinds of a mod's files, by their extensions: those the game plays from a mod (sounds, bitmaps, the two string
lists, models), and the rest, which the game never reads from a mod */
enum
{
	AE_MODS_GROUP_SOUND,
	AE_MODS_GROUP_BITMAP,
	AE_MODS_GROUP_TEXT,
	AE_MODS_GROUP_MODEL,
	AE_MODS_GROUP_OTHER,
	AE_MODS_GROUPS
};

/* a mod's state in the list */
enum
{
	/* on, and played on the loaded map */
	AE_MODS_STATE_ON,
	/* turned off */
	AE_MODS_STATE_OFF,
	/* on, played once a map that takes mods loads (or at the next scan of the mods) */
	AE_MODS_STATE_WAITS,
	/* not played whatever its switch says: the reason says why */
	AE_MODS_STATE_REFUSED
};

/* Copies `source` into `destination` (size bytes) fit to draw: control characters and broken UTF-8 become spaces,
runs of spaces one space, no space at either end; at most `characters` characters (a cut one ends with "..."); line
breaks kept when `lines` (one at a time). An empty or NULL source gives "". The length written. */
size_t ae_mods_text_clean(char *destination, size_t size, const char *source, int characters, int lines);
/* the group of a file by its tag path's extension (".sound", ".bitmap", ".unicode_string_list", ".string_list",
".gbxmodel", ".model"; any case), AE_MODS_GROUP_OTHER for any other */
int ae_mods_group_of(const char *key);
/* a mod's state: refused (the engine will not play it), off (turned off), else on when it was played at the last scan
of the mods and the loaded map plays mods now, else waits */
int ae_mods_state_of(int refused, int off, int played, int active);
/* how many places to move a mod at load position `position` (0 loads first) of `count` so that it gets priority
`priority` (1 plays over every other mod: it loads last); the priority is held to 1..count */
long ae_mods_priority_places(long count, long position, long priority);
/* the priority of the mod at load position `position` of `count` */
long ae_mods_priority_of(long count, long position);

/* A file of a mod that is played: its tag path (lower case, as the engine keeps it), its mod and that mod's load
position (later plays over earlier). */
struct ae_mods_claim
{
	const char *key;
	long mod;
	long position;
};

/* Two mods with files of the same tag paths: how many, and which mod's play (the one loaded later). */
struct ae_mods_overlap
{
	long first;
	long second;
	long files;
	long winner;
};

/* The pairs of mods that have files of the same tag paths, from `claims` (sorted in place by key); at most `most`
pairs into `overlaps`; how many pairs there are in all. A key three mods have counts once for each pair. */
int ae_mods_overlaps(struct ae_mods_claim *claims, long count, struct ae_mods_overlap *overlaps, int most);

#endif
