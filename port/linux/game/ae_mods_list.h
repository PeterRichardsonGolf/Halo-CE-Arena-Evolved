/*
AE_MODS_LIST.H

The mods list: the mods this machine has (the data root's mods folder, port_mods.c), each with what its mod.toml says
(cleaned to draw), what it changes, its switch, its priority and its state; turning a mod on or off and moving it, kept
in the mods folder's load_order.toml (the engine's own store) and played at once when the loaded map takes mods. The
engine is reached through struct ae_mods_source (ae_mods_list_engine.c gives the game's; the unit tests give made-up
mods folders), so this file has no engine header.
*/

#ifndef AE_MODS_LIST_H
#define AE_MODS_LIST_H

#include "ae_mods_list_rules.h"

#define AE_MODS_LIST_MOST 256
/* the longest each text is drawn, in characters */
#define AE_MODS_NAME_CHARACTERS 63
#define AE_MODS_TITLE_CHARACTERS 48
#define AE_MODS_AUTHORS_CHARACTERS 64
#define AE_MODS_VERSION_CHARACTERS 16
#define AE_MODS_DESCRIPTION_CHARACTERS 240
#define AE_MODS_MAPS_CHARACTERS 120
#define AE_MODS_REASON_CHARACTERS 160
/* the pairs of mods with files of the same tags that the list keeps */
#define AE_MODS_OVERLAPS_MOST 32

/* what the engine gives (port_mods.c's and mods.c's functions, or a test's) */
struct ae_mods_source
{
	/* the mods found, by index; the index at a load position (0 loads first) */
	long (*count)(void);
	long (*at)(long position);
	const char *(*folder)(long index);
	/* mod.toml's name, else the folder's */
	const char *(*name)(long index);
	/* mod.toml's other fields; refusal NULL unless the engine will not play it; played_place -1 unless the last scan
	played it; flags 1 turned off, 2 mod.toml could not be read, 4 Xbox channel order; 0 if there is no such mod */
	int (*fields)(long index, const char **id, const char **version, const char **authors, const char **description,
		const char **refusal, long *map_count, long *played_place, long *file_count, int *flags);
	const char *(*map)(long index, long map);
	/* the engine's own line of what is wrong with a mod (port_mods_status: 1 out of order, 2 another says it cannot be
	played with it, 3 cannot be played) */
	int (*status)(long index, char *text, unsigned long size);
	/* the files of the mods played at the last scan: their tag paths and mods */
	long (*files)(void);
	const char *(*file_key)(long file, long *mod);
	/* the menu's changes: move (its new load position), turn, keep (0: load_order.toml cannot be written), play them
	(scans the mods again, and plays them on the loaded map when it takes mods) */
	long (*move)(long position, long places);
	void (*turn)(long index, int on);
	int (*save)(void);
	void (*apply)(void);
	/* whether the loaded map plays the mods now */
	int (*active)(void);
};

struct ae_mods_item
{
	/* its folder: what the list and the commands name it by */
	char name[AE_MODS_NAME_CHARACTERS * 4 + 1];
	/* mod.toml's name, id, version, authors and description, cleaned (empty: not said) */
	char title[AE_MODS_TITLE_CHARACTERS * 4 + 1];
	char id[AE_MODS_NAME_CHARACTERS * 4 + 1];
	char version[AE_MODS_VERSION_CHARACTERS * 4 + 1];
	char authors[AE_MODS_AUTHORS_CHARACTERS * 4 + 1];
	char description[AE_MODS_DESCRIPTION_CHARACTERS * 4 + 1];
	/* the maps it is for ("" every map) */
	char maps[AE_MODS_MAPS_CHARACTERS * 4 + 1];
	/* the switch, its priority (1 plays over every other mod) and its load position (0 loads first) */
	int on;
	long priority;
	long position;
	/* AE_MODS_STATE_*, and why when refused or waiting (else empty) */
	int state;
	char reason[AE_MODS_REASON_CHARACTERS * 4 + 1];
	/* what else to say: mod.toml unreadable, another mod says it cannot be played with it, it loads on the wrong side
	of a mod it names, it shares files with another (empty: nothing) */
	char note[AE_MODS_REASON_CHARACTERS * 4 + 1];
	/* its files by kind when played (files_known), from the last scan of the mods */
	int files_known;
	long files[AE_MODS_GROUPS];
	/* mod.toml could not be read: played without what it says */
	int manifest_damaged;
};

struct ae_mods_list
{
	long count;
	/* in load order, the last loaded (priority 1) first, as the list is drawn */
	struct ae_mods_item items[AE_MODS_LIST_MOST];
	/* whether the loaded map plays the mods now */
	int active;
	/* the pairs of played mods with files of the same tags (their engine indexes), and how many pairs in all */
	int overlap_count;
	struct ae_mods_overlap overlaps[AE_MODS_OVERLAPS_MOST];
	long overlap_index[AE_MODS_OVERLAPS_MOST][2];
};

/* the answers of a change */
enum
{
	/* kept and played at once */
	AE_MODS_CHANGE_NOW,
	/* kept; plays when a map that takes mods loads */
	AE_MODS_CHANGE_NEXT_MAP,
	/* made, but load_order.toml cannot be written: kept until the game quits */
	AE_MODS_CHANGE_NOT_KEPT,
	/* refused: the mod cannot be played, or there is no such mod */
	AE_MODS_CHANGE_REFUSED,
	AE_MODS_CHANGE_UNKNOWN
};

/* the list as the engine has it now (the last scan of the mods); how many mods */
long ae_mods_list_read(const struct ae_mods_source *source, struct ae_mods_list *list);
/* the engine index of the mod named (its folder or its id, any case), or -1 */
long ae_mods_list_find(const struct ae_mods_source *source, const char *name);
/* turns a mod on or off, or moves it to a priority (1 plays over every other), keeps it and plays it: an
AE_MODS_CHANGE_* answer and a line saying it into `answer` */
int ae_mods_list_turn(const struct ae_mods_source *source, const char *name, int on, char *answer, size_t size);
int ae_mods_list_order(const struct ae_mods_source *source, const char *name, long priority, char *answer, size_t size);
/* one line of the whole list, the top first ("2 mods, 1 on: hd_audio 1 on, german 2 off"), cut to fit */
void ae_mods_list_line(const struct ae_mods_list *list, char *text, size_t size);

/* DEV MODE's "mods list | on <name> | off <name> | order <name> <priority>" (ae_dev_cmd.c): one answer line; whether
it did what it was asked */
int ae_mods_list_command(const struct ae_mods_source *source, const char *verb, const char *name, long priority,
	char *answer, size_t size);

/* the game's engine (ae_mods_list_engine.c) */
const struct ae_mods_source *ae_mods_list_engine(void);

#endif
