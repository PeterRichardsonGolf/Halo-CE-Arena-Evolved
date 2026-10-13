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
#define CUSTOM_EDITION_MAP_DIRECTORY ae_map_share_download_folder()
/* (the download being offered's folder: maps_ce, or an Xbox map's maps_shared; ae_map_share_all.c) */
char const *ae_map_share_download_folder(
	void);
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
/* the map file at path closed if the game has it open but not loaded
(checked, or chosen in the lobby), so that it can be renamed; FALSE if it is
the loaded map's */
boolean map_share_release_map(
	char const *path);
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

/* map sharing's safety rules (ae_map_share_rules.h; DamnationCE v0.3.41, CC0) */

/* whether the host's source is one to reach: internet play's always (by its
key), a LAN player's only at a local network's address (host byte order);
each address refused logged once per download (its serial) */
boolean ae_map_share_source_allowed(
	boolean internet,
	unsigned long address,
	long download);
/* a finished download that failed its check: its file at path (a downloads'
folder file, nothing else) removed, so that it is never kept or loadable */
void ae_map_share_discard_failed(
	char const *path);

/* ---------- sharing every map but the game's own (0.2.3): ae_map_share_all.c, its rules ae_map_share_rules.c */

/* whether a machine of this host's game runs this game (its join request's mark): ae_silent.c */
boolean ae_peer_is_ae(
	long machine_index);

/* the adapters above, for a community map in the Xbox's format: whether the level is one that is shared (a host:
of its list; a guest: of its host's list, or the game's map waited for), a host's file of it, its file's name with
its hash (<name>_<8 digits>), and a download of it checked and given its own name in maps_shared */
boolean ae_map_share_xbox_level(
	char const *level_name);
boolean ae_map_share_xbox_file_path(
	char const *level_name,
	char *path);
boolean ae_map_share_xbox_level_file(
	char const *level_name,
	char *name,
	long size);
boolean ae_map_share_xbox_present(
	char const *level_name,
	char *message,
	long message_size);
char const *ae_map_share_xbox_folder(
	void);

/* map_share.c's hooks, a guest's: whether a download may be asked for at all (the settings, the internet's);
whether its request goes out now (begun), and each frame what holds it: 0 nothing, 1 it waits (the host's list, the
player's answer), 2 the request goes out now, 3 it is refused (ae_map_share_refusal says why); whether a failed
download was one of GET ALL's (the game is not left); whether a download is GET ALL's (it holds no players, and is
kept while the game is on another map); whether an offered file is taken (the list's size and hash, the limits, the
player's yes) */
boolean ae_map_share_download_permitted(
	char const *level_name);
boolean ae_map_share_download_begun(
	char const *level_name);
int ae_map_share_download_gate(
	char const *level_name,
	boolean asking);
char const *ae_map_share_refusal(
	void);
boolean ae_map_share_download_failed(
	char const *level_name,
	char const *reason);
boolean ae_map_share_download_extra(
	char const *level_name);
/* (... and whether a download comes from the host alone, never the game's other players: an Xbox map's, GET ALL's) */
boolean ae_map_share_download_host_only(
	char const *level_name);
boolean ae_map_share_offer_allowed(
	char const *level_name,
	short kind,
	unsigned long size,
	byte const *hash);
/* ... a host's: whether a machine's request is answered (its rate, the internet's switch); whether a level that
is not the game's is of the list the machine was sent; whether this host sends its list's files; the hashed file of
the list with this hash (or NONE); whether a machine's download is not of the game's map (it is not held out of a
game that starts) */
boolean ae_map_share_host_allowed(
	long machine_index,
	char const *level_name);
boolean ae_map_share_host_lists(
	long machine_index,
	char const *level_name);
boolean ae_map_share_host_serving(
	void);
short ae_map_share_host_file(
	short found,
	byte const *hash,
	unsigned long address);
/* (map_share_files.c's: whether the hashed file at path keeps its place in the catalogue: the game's own, or one
being sent to a machine) */
boolean ae_map_share_file_kept(
	char const *path);
boolean ae_map_share_host_extra(
	long machine_index);
/* THE QUESTION ("download from this host?") AND ITS ANSWERS: what a menu's dialog calls (the F6 key and the text
line of ae_map_share_all.c are one caller, until there is one).
- ae_map_share_consent_question: TRUE while the question is open, with what to show: the host (its address, host
  byte order), how many maps this machine lacks, their bytes (0: not known), the most megabytes taken from one host,
  and whether the game cannot be played without a yes (then "no" means leaving).
- ae_map_share_consent_shown: called each frame the question is really on the screen. An answer is taken only if
  it was shown within the last half second and for at least a second, and only in the lobby or the menus.
- ae_map_share_consent_yes: the player's yes, for that host and that amount. FALSE (nothing changes) if the
  question is not open, was not shown long enough, or is now for another host or another amount: show it again.
  The yes covers that many bytes from that host; more is asked for again.
- ae_map_share_consent_no: the player's no. To the game's map it stands for this host (the download is refused and
  the game left); to GET ALL alone it only closes the question. */
struct ae_map_share_question
{
	unsigned long host;
	long maps;
	unsigned long long bytes;
	long limit_megabytes;
	boolean needed_to_play;
};
boolean ae_map_share_consent_question(
	struct ae_map_share_question *question);
void ae_map_share_consent_shown(
	void);
boolean ae_map_share_consent_yes(
	unsigned long host,
	unsigned long long bytes);
boolean ae_map_share_consent_no(
	unsigned long host);
/* whether the player said yes to this host (files and folders may be made) */
boolean ae_map_share_download_may_write(
	void);

/* ... both: a frame of the list (TRUE: it was one, handled or dropped); every frame; the screen's line */
boolean ae_map_share_handle_message(
	long machine_index,
	word const *message,
	word size,
	boolean from_stream);
void ae_map_share_all_update(
	void);
void ae_map_share_all_render(
	void);

/* cache_files.c's: a guest without the game's Xbox map asks its host for it (TRUE: taken as present, downloaded
before it is precached); a map's file is maps_shared's if no other folder has it */
boolean ae_map_share_played_map(
	char const *map_name);
void ae_map_share_xbox_path(
	char const *map_name,
	char *path,
	long size);
/* map_families.c's: a map loaded from maps_shared is a downloaded one (its scripts' tighter rules), in any run */
boolean ae_map_share_xbox_downloaded(
	char const *map_name);
/* custom_maps.c's: maps_shared's maps listed with the maps folder's */
void ae_map_share_scan_downloads(
	void (*scan)(char const *directory));

#endif
