/*
AE_CATALOG.H

The AE menus' catalog files (ae_catalog.c), text in and text out; the files
themselves (the profile folder's paths) are the glue's (M4). Pure C, no
engine includes (unit test: port/linux/tests/ae_catalog_test.c).

Formats: UTF-8, one item a line, LF or CRLF, '#' starts a comment line:
- maps.txt, the map facts the files don't hold: file<TAB>name<TAB>players
  (2-8)<TAB>size (s, m, l or -)<TAB>tags (a comma list)<TAB>credits;
- favourites: one entry a line, "map:<file>" or "gametype:<name>";
- recent: the same entries, newest first, at most AE_CATALOG_RECENT of each
  kind (maps, gametypes);
- playlists: "[name]" lines, each followed by its map files, one a line.
(Map ratings and ae_gametypes.txt come with M4: plan P18.)

A line that doesn't fit its format is skipped, counted in skipped_lines and
passed to the log function if one is set (the first 16 of a catalog, each cut
to 120 bytes with its control characters as '?', then one line saying the
rest aren't logged); a field longer than its room is cut (at a character's
boundary), never written past it. A UTF-8 byte-order mark is ignored;
entries and map lines are trimmed of spaces and tabs at both ends.
struct ae_catalog is about 300 KB: keep it static (or allocated), never on
the stack. The unit is not reentrant: one catalog's calls at a time, and the
log function is one pointer for the whole process (the menus' thread).

For the glue (M4):
- writing a file: the writer into a buffer (sizes below), then a temporary
  file beside it, written, flushed (fsync where there is one) and renamed over
  the old one; never truncated in place. A writer's -1 (the text didn't fit)
  writes nothing. A crash then leaves the old file or the new one, whole.
- a map's facts.file is a lookup key, a file name as maps.txt gives it: the
  reader refuses '/', '\', ".." and control characters in it, but the glue
  still never builds a path from it without checking it again.
*/

#ifndef __AE_CATALOG_H
#define __AE_CATALOG_H

enum { AE_CATALOG_MAXIMUM_ITEMS = 512, AE_CATALOG_NAME = 64, AE_CATALOG_PLAYLISTS = 32, AE_CATALOG_RECENT = 10,
	AE_CATALOG_PLAYLIST_MAPS = 64 };
struct ae_map_facts { char file[AE_CATALOG_NAME], name[AE_CATALOG_NAME]; short players_min, players_max;
	char size;  /* 's', 'm', 'l' or 0 */  char tags[AE_CATALOG_NAME]; char credits[AE_CATALOG_NAME]; };
struct ae_playlist { char name[AE_CATALOG_NAME]; short count; char maps[AE_CATALOG_PLAYLIST_MAPS][AE_CATALOG_NAME]; };
struct ae_catalog
{
	short facts_count; struct ae_map_facts facts[AE_CATALOG_MAXIMUM_ITEMS];
	short favourite_count; char favourites[AE_CATALOG_MAXIMUM_ITEMS][AE_CATALOG_NAME];   /* "map:<file>" / "gametype:<name>" */
	short recent_count; char recent[AE_CATALOG_RECENT * 2][AE_CATALOG_NAME];             /* the same, newest first */
	short playlist_count; struct ae_playlist playlists[AE_CATALOG_PLAYLISTS];
	short skipped_lines;     /* corrupt lines skipped while reading (each also logged) */
};

/* the writers' largest texts, for the glue's buffers (with their NUL) */
enum
{
	AE_CATALOG_FAVOURITES_TEXT = 34 * 1024,   /* 512 entries of up to 63 bytes, a line each, and the heading */
	AE_CATALOG_RECENT_TEXT = 2 * 1024,        /* 20 entries */
	AE_CATALOG_PLAYLISTS_TEXT = 136 * 1024    /* 32 playlists of 64 maps */
};

/* where skipped lines are reported (the glue: the game's log); NULL: nowhere. Not thread-safe (one pointer) */
void ae_catalog_set_log(void (*log)(char const *what, char const *line));

/* text in, catalog out: each reader appends to the catalog; bad lines are skipped and counted. A map's facts, or a
playlist (by name), read again replace the earlier ones (a player's file over the shipped one). */
void ae_catalog_read_facts(struct ae_catalog *catalog, const char *text);       /* maps.txt */
void ae_catalog_read_favourites(struct ae_catalog *catalog, const char *text);
void ae_catalog_read_recent(struct ae_catalog *catalog, const char *text);
void ae_catalog_read_playlists(struct ae_catalog *catalog, const char *text);
/* writers: into buffer, returns length (or -1 if it doesn't fit) */
long ae_catalog_write_favourites(struct ae_catalog const *catalog, char *buffer, long size);
long ae_catalog_write_recent(struct ae_catalog const *catalog, char *buffer, long size);
long ae_catalog_write_playlists(struct ae_catalog const *catalog, char *buffer, long size);
void ae_catalog_recent_push(struct ae_catalog *catalog, const char *entry);   /* newest first, unique, capped */
/* the entry's new state: 1 a favourite, 0 not; -1 when it isn't an entry, or the list is full */
int ae_catalog_favourite_toggle(struct ae_catalog *catalog, const char *entry);
struct ae_map_facts const *ae_catalog_facts(struct ae_catalog const *catalog, const char *file);

#endif
