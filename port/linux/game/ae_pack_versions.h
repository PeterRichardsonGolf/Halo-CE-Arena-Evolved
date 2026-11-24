/*
AE_PACK_VERSIONS.H

A pack map of another version (rc6; the rules and where the copies go: ae_pack_versions_rules.h).

The guest: cache_files_map_present notes the network game's pack map and the host's version (ae_pack_versions_note).
When the player's own maps_<pack> file is another version, ae_pack_versions_join plays the host's copy if it is here,
else asks map sharing for it (ae_map_share_played_pack_version), which puts it in its version's folder
(ae_pack_versions_download_path) and keeps AE_PACK_VERSIONS_KEPT copies of the map (ae_pack_versions_arrived). While
this machine is a joined guest of that game, map_family_xbox_path gives the copy's path (ae_pack_versions_xbox_path);
every other load is the player's own file. Every fetch and use is logged with both versions ("map sharing: ...").
*/

#ifndef AE_PACK_VERSIONS_H
#define AE_PACK_VERSIONS_H

/* (cache_files.c, a guest's check of a network game's map) the game's map and the host's version (0 for any): a pack
map's copy is played for that game only */
void ae_pack_versions_note(char const *map_name, unsigned long version);

/* (cache_files.c) the game's pack map is here in another version (mine): 1 when the host's copy is here or map sharing
fetches it, so the join goes on; else 0, and message (the other version's words) gets the line that says why no copy
is fetched */
int ae_pack_versions_join(char const *map_name, unsigned long version, unsigned long mine, char *message, long size);

/* (delta_maps.c, the map identity check) the host's copy of the game's pack map is being fetched for this join: the
check waits for it (it would hash the player's own file of another version) */
int ae_pack_versions_fetching(char const *map_name);

/* (map_families.c map_family_xbox_path) the game's pack map while this machine is a joined guest of the game noted,
its own file another version: path is the host's copy, 1; else 0, path as it was */
int ae_pack_versions_xbox_path(char const *map_name, char *path, long size);

/* (ae_map_share_all.c) a downloaded pack map whose own file is here: its copy's path (the folders made), by the
downloaded file's fingerprint; 0 when the map is not the game noted */
int ae_pack_versions_download_path(char const *level_name, unsigned long fingerprint, char *path, long size);

/* (ae_map_share_all.c) the copy moved in: the oldest copies of the map beyond the kept number removed */
void ae_pack_versions_arrived(char const *level_name, unsigned long fingerprint);

/* the fingerprint of the map file at path (map_pack_fingerprint's), 0 for none */
unsigned long ae_pack_versions_fingerprint(char const *path);

/* (ae_maps_hub_data.c, MAPS & MODS) the bytes of every copy kept */
unsigned long long ae_pack_versions_total_bytes(void);

#endif
