/*
AE_DOWNLOADED_MAPS_RULES.H

The names of the maps that came from elsewhere than the player's own folders (map_families.c's downloaded maps: their
scripts are held to tighter rules). A table that holds up to AE_DOWNLOADED_MAPS_MAXIMUM names and says so when it is
full: a full table never forgets a name and never drops one silently, it answers "downloaded" for every map instead (the
stricter rules), and the caller logs it. Pure: no files, no engine.
*/

#ifndef __AE_DOWNLOADED_MAPS_RULES_H
#define __AE_DOWNLOADED_MAPS_RULES_H

enum
{
#ifdef HALO_XBOX_CONSOLE
	/* (the console downloads none: only a game.downloaded_maps' names) */
	AE_DOWNLOADED_MAPS_MAXIMUM = 8,
#else
	AE_DOWNLOADED_MAPS_MAXIMUM = 65536,
#endif
	AE_DOWNLOADED_MAPS_NAME = 64
};

/* the table emptied (a list read again from its start) */
void ae_downloaded_maps_reset(void);
/* a name (a map's file, then "@ce" or the like; at most AE_DOWNLOADED_MAPS_NAME - 1 characters) added: 1 if it is in the
table now (new or there already), 0 if it was refused: the table is full (it is overflowed from then on) */
int ae_downloaded_maps_add(char const *name);
/* whether the table was full when a name came */
int ae_downloaded_maps_overflowed(void);
/* whether map_name (a path's last part after a backslash, any case) is a name of the table; always 1 once overflowed */
int ae_downloaded_maps_has(char const *map_name);
int ae_downloaded_maps_count(void);

/* whether a Custom Edition map's file name holds a character that makes it another file's alias (the "~" of an 8.3 short
name, which Windows opens as the long name's file): such a name is never found, listed or marked */
int ae_downloaded_maps_alias_name(char const *file);

/* the name a map goes by for the file that was opened: its last part less ".map" (any case), then "@ce" (as the downloaders
mark it). The game may open a file under a name other than the one asked for (a hashed name's base, a version's file), so the
scripts' rules are asked for this name as well as for the requested one. 1 with it in out, else 0 */
int ae_downloaded_maps_file_name(char const *path, char *out, int size);
/* whether a map is played by the downloaded maps' rules: asked for by name, or by its opened file's (path may be NULL) */
int ae_downloaded_maps_has_either(char const *requested, char const *opened_path);

/* a list's text a piece at a time (any piece sizes): each whole line a name, lines of AE_DOWNLOADED_MAPS_NAME characters
or more skipped; end takes the last line when the text did not end with a line feed */
struct ae_downloaded_maps_reader
{
	char line[AE_DOWNLOADED_MAPS_NAME];
	int length;
	int skipping;
};
void ae_downloaded_maps_reader_start(struct ae_downloaded_maps_reader *reader);
void ae_downloaded_maps_reader_feed(struct ae_downloaded_maps_reader *reader, char const *text, long size);
void ae_downloaded_maps_reader_end(struct ae_downloaded_maps_reader *reader);

#endif
