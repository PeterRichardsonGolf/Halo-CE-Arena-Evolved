/*
AE_MAP_SHARE.H

Arena Evolved: OpenCE's map sharing (map_share.c, map_share_files.c) on
Arena Evolved's Custom Edition maps, which are ChupathingyCE's
(halo_map_families.h: <name>@ce, found in maps_ce and its older places, the
resource maps there or read in place from MCC). OpenCE's sharing is written
for its own Custom Edition code (custom_edition_cache.c, which Arena Evolved
does not have): these are the few functions of it that sharing calls, with
Arena Evolved's answers, and the names on the wire.

- A level name here is the game's (<name>@ce); in sharing's messages it is
  the protocol's (custom_maps\<name>), as in the game's settings
  (map_family_wire_name), so that OpenCE's machines and Arena Evolved's
  share with each other.
- The folders whose maps are named for their hashes (and shared), and that
  downloads go to: maps_ce (Arena Evolved's own) and custom_maps (OpenCE's,
  which Arena Evolved also reads). A map elsewhere (maps\ce, a name ending
  in @ce) is played, not shared.
- Resource maps resolve as the game loads them (map_family_resource): maps_ce
  first, its older places, then MCC's files read in place (game.mcc_use).
*/

#ifndef __AE_MAP_SHARE_H
#define __AE_MAP_SHARE_H

/* ---------- constants */

/* where downloads go (a path of the platform's) */
#define CUSTOM_EDITION_MAP_DIRECTORY "d:\\maps_ce\\"
/* a Custom Edition map's level name in the game's protocol, and its longest */
#define CUSTOM_EDITION_LEVEL_NAME_PREFIX "custom_maps\\"
#define CUSTOM_EDITION_MAXIMUM_LEVEL_NAME_LENGTH 63

/* the resource maps, as map_share.h's file kinds number them */
enum resource_map_type
{
	_resource_map_none,
	_resource_map_bitmaps,
	_resource_map_sounds,
	_resource_map_locale,

	NUMBER_OF_RESOURCE_MAP_TYPES
};

/* ---------- prototypes (OpenCE's custom_edition_cache.h / cache_file_formats.h names) */

/* a resource map's file name without .map ("bitmaps", "sounds", "loc") */
char const *resource_map_type_describe(
	enum resource_map_type type);
/* whether the level is a Custom Edition map's (<name>@ce) */
boolean custom_edition_level_name(
	char const *level_name);
/* the level's map file (MAP_SHARE_PATH_SIZE): map_family_find's, the game's
own resolution */
boolean custom_edition_map_file_path(
	char const *level_name,
	char *path);
/* the resource map of `type` the game reads (MAP_SHARE_PATH_SIZE):
map_family_resource's (maps_ce, its older places, then MCC's in place) */
boolean custom_edition_resource_map_file_path(
	short type,
	char *path);
/* the resource maps the level's map needs: a bit each (1 << type), from its
tag index (each tag held in one: bitmaps, sounds, strings and fonts) */
unsigned long custom_edition_map_resource_maps_needed(
	char const *level_name);
/* the checksum in the level's map's header (cache_files_map_version) */
unsigned long custom_edition_map_checksum(
	char const *level_name);
/* the Custom Edition map loaded (open), or NULL */
char const *custom_edition_cache_loaded_path(
	void);
/* whether this machine can play the level as the host's version
(`checksum`; 0 for any): its map, and the resource maps it needs; when not,
why in `message` */
boolean custom_edition_cache_present(
	char const *level_name,
	unsigned long checksum,
	char *message,
	long message_size);
/* the map lists read their folders again (Arena Evolved's do when shown) */
void custom_edition_maps_look_again(
	void);
/* whether Custom Edition maps can load: their tag cache was mapped */
boolean halo_custom_edition_tag_cache(
	void);

/* ---------- prototypes (Arena Evolved's) */

/* a level's map file's name (no folder, no @ce): one of a few buffers,
each good until a few more calls */
char const *map_share_level_file(
	char const *level_name);
/* a level name into a message (LEVEL_NAME_BYTES, the protocol's), and one
from a message, in place, as the game names it */
void map_share_level_to_wire(
	char *wire,
	char const *level_name);
void map_share_level_from_wire(
	char *level_name);
/* whether a folder (ending in its separator) is one whose maps are shared */
boolean map_share_folder_shared(
	char const *folder);
/* the downloads' folder made, if it is not there */
void map_share_folder_make(
	void);
/* whether the level's map is here but a resource map it needs is not */
boolean map_share_resource_maps_missing(
	char const *level_name);

/* map_families.c's map_family_find, a Custom Edition map not found under its
own name: one named for its hash in the shared folders (<file>_<hash>.map,
the only one of that name), or, for a name with a hash, that map under its
name without it, named for its hash now if its hash is that (a map the map
lists have not named yet). FALSE if neither */
boolean ae_map_share_find(
	short family,
	char const *file,
	char *path,
	long size);

/* map_share_files.c's: the map <folder><name>.map hashed now, on this
thread, and named for its hash (unless it is in use, or was tried before);
TRUE if it was renamed */
boolean map_share_name_now(
	char const *folder,
	char const *name);

#endif
