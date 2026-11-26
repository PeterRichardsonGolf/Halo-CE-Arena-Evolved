/*
AE_MAP_DUPES_RULES.H

One map held in several copies (the Steam Workshop item, the stock or community file, our converted stand-in, the CE+ X
copy): which list entries are the same map, which copy the map pickers show for it, and the words that say what each copy
is. Pure code: port/linux/tests/ae_map_dupes_rules_test.c runs it.

Two entries are one map only by an explicit identity, never by a shown name (a Workshop collection's remakes share the
stock names): their file stems match, the stem being read only for a Workshop item with an old name
(ae_map_alias_rules.h), a converted copy (@smp, @pro), a plain community file and a Custom Edition file (@ce); and one
of the two is a Workshop item or a converted copy. A stock map, an MCC stock map, a CE+ X file, another map pack's file
and a Workshop item with no old name are never grouped.
The shown copy is the best by AE_MAP_DUPE_*: the Workshop item, else the community file, else the converted stand-in;
the earlier entry wins a tie.
*/

#ifndef AE_MAP_DUPES_RULES_H
#define AE_MAP_DUPES_RULES_H

enum
{
	AE_MAP_DUPE_WORKSHOP,
	AE_MAP_DUPE_STOCK,
	AE_MAP_DUPE_COMMUNITY,
	AE_MAP_DUPE_CONVERTED,
	AE_MAP_DUPE_CEX,
	/* MCC's own stock map (<map>@mcc) */
	AE_MAP_DUPE_MCC,
	/* a Custom Edition or Halo PC map (<map>@ce, @pc) */
	AE_MAP_DUPE_CUSTOM_EDITION,
	AE_MAP_DUPE_HALOMD,
	/* any other map pack's map */
	AE_MAP_DUPE_PACK,
	AE_MAP_DUPE_KINDS,
	AE_MAP_DUPE_NAME_SIZE = 64
};

struct ae_map_dupe_entry
{
	/* the game's list name (<file>, <file>@<pack>, a Workshop identity) */
	char const *list_name;
	/* the name shown */
	char const *name;
	/* AE_MAP_DUPE_* */
	int kind;
	/* 0: never grouped (a map Steam is still writing, MCC's own stock maps) */
	int groupable;
};

/* the kind of a list name: "@ws<digits>" Workshop, "@smp" / "@pro" converted, "@cex" CE+ X, "@mcc" MCC, "@ce" / "@pc"
Custom Edition, "@md" HaloMD, any other "@<pack>" a pack's, no "@": stock when stock is set, else community */
int ae_map_dupe_kind(char const *list_name, int stock);
/* the chip's label ("WORKSHOP", "STOCK", "COMMUNITY", "CONVERTED", "CE+ X", "MCC", "CE MAP", "HALOMD", "MAP PACK"); ""
out of range */
char const *ae_map_dupe_chip(int kind);
/* the plain line for the focused copy */
char const *ae_map_dupe_help(int kind);
/* the order of the kinds, 0 best */
int ae_map_dupe_rank(int kind);
/* the entries' groups: head[i] is the shown copy of i's map (i itself when alone), order[i] its place in its group, best
first from 0, versions[i] how many copies its map has */
void ae_map_dupes_group(struct ae_map_dupe_entry const *entries, int count, short *head, short *order, short *versions);

/* the facts (maps.txt) of one map hold for its copies in every family. The keys to look a list name up by, in order: attempt 0
the name itself, 1 the base name with @ce, 2 the base name alone; attempts 1 and 2 only for an @mcc or @pc copy (a Gearbox map
played from MCC or Halo PC), never for a pack's or a Workshop copy. 1 and the key, 0 when there is no such attempt */
int ae_map_facts_key(char const *list_name, int attempt, char *key, int size);
/* the copy of a map BTB lists when a PC has several, 0 best: the Custom Edition file (@ce), the plain file, MCC's (@mcc),
Halo PC's (@pc); 4 for any other name (never compared) */
int ae_map_btb_rank(char const *list_name);
/* whether two list names are the same base map (the name before the @, any case) */
int ae_map_same_base(char const *a, char const *b);

#endif
