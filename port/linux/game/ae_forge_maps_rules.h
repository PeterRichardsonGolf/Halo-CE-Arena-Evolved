/*
AE_FORGE_MAPS_RULES.H

The maps Forge saved, as MY MAPS lists them (ae_forge_maps.h): a name Forge's save box accepts, the facts a saved map's
text carries, the level name a map is played under, and the list's SHOW and SORT. Pure: no files, no engine, so it is
unit tested (port/linux/tests/ae_forge_maps_rules_test.c).
*/

#ifndef __AE_FORGE_MAPS_RULES_H
#define __AE_FORGE_MAPS_RULES_H

/* the longest name Forge's save box takes */
enum { AE_FORGE_MAP_NAME_MAX = 48 };

/* the file's cache version: Xbox (5, a stock map's copy with Forge's trailer), Custom Edition (609) or none the game plays */
enum { AE_FORGE_MAP_UNREADABLE, AE_FORGE_MAP_XBOX, AE_FORGE_MAP_CE };
enum { AE_FORGE_SHOW_ALL, AE_FORGE_SHOW_MADE, AE_FORGE_SHOW_DOWNLOADED, AE_FORGE_SHOWS };
enum { AE_FORGE_SORT_SAVED, AE_FORGE_SORT_NAME, AE_FORGE_SORT_BASE, AE_FORGE_SORTS };

struct ae_forge_maps_facts
{
	char name[64];  /* "Map Name:" */
	char base[64];  /* "Base Map:" */
	int objects;    /* "Objects Placed:" */
	int squads;     /* "AI Squads:" */
};

/* 1 when `name` is a name Forge's save box makes: 1 to 48 of a-z, 0-9 and _ */
int ae_forge_maps_name_valid(char const *name);

/* what a typed name becomes: lower case, a space or - an _, anything else left out, cut at 48; the length (0: nothing left) */
int ae_forge_maps_name_clean(char const *typed, char *name, int size);

/* the facts of a saved map's text ([OpenCE Forge Map], then its lines); 0 when the text is not one */
int ae_forge_maps_facts_read(char const *text, struct ae_forge_maps_facts *facts);

/* which kind a cache version is */
int ae_forge_maps_kind(long version);

/* the level name a map's file is played as: an Xbox cache by its file's name (the game's own maps' form), a Custom Edition
cache as <file>@ce; empty for an unreadable one */
void ae_forge_maps_level_name(int kind, char const *file, char *level, int size);

/* whether a map is listed under SHOW */
int ae_forge_maps_shown(int show, int downloaded);

/* the list's order: <0 when a is first. SAVED puts the later save first, NAME and BASE go A to Z (the name after the base) */
int ae_forge_maps_compare(int sort, char const *name_a, char const *base_a, unsigned long saved_a, char const *name_b,
	char const *base_b, unsigned long saved_b);

/* whether a name is one a stock map has: such a map's name is refused for a copy or a rename (the game's own file wins) */
int ae_forge_maps_name_is_stock(char const *name);

/* the picture a save carries (a 24 or 32 bit uncompressed BMP, Forge's 140 by 114) as RGBA, into rgba (capacity bytes); the
width and height; 0 when it is not one this reads or does not fit */
int ae_forge_maps_bmp_rgba(unsigned char const *bmp, long size, unsigned char *rgba, long capacity, int *width, int *height);

/* the career's FORGE TIME in words: "0 MIN" under a minute, "42 MIN", "2 H 14 MIN", "30 H"; seconds below 0 count as 0 */
void ae_forge_maps_duration_text(long seconds, char *text, int size);

#endif /* __AE_FORGE_MAPS_RULES_H */
