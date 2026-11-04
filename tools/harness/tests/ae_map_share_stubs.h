/*
AE_MAP_SHARE_STUBS.H

Arena Evolved: what upstream's map sharing tests (map_share_trust.c, which compiles map_share.c's real functions)
see of Arena Evolved's sharing of every map (port/linux/game/ae_map_share_all.c): none of it. Every machine is
allowed its requests, no level but the game's is listed, no download is one of GET ALL's, and map sharing's own
answer for a file stands. Arena Evolved's own tests are port/linux/tests/ae_map_share_rules_test.c and
tools/test_ae_map_share_all.py.
*/

#ifndef __AE_MAP_SHARE_STUBS_H
#define __AE_MAP_SHARE_STUBS_H

#define ae_map_share_host_allowed(machine_index, level_name) (TRUE)
#define ae_map_share_host_lists(machine_index, level_name) (FALSE)
#define ae_map_share_host_extra(machine_index) (FALSE)
#define ae_map_share_host_serving() (FALSE)
#define ae_map_share_host_file(found, hash, address) (found)
#define ae_map_share_file_kept(path) (FALSE)
#define ae_map_share_download_extra(level_name) (FALSE)
#define ae_map_share_download_host_only(level_name) (FALSE)
#define ae_map_share_xbox_level(level_name) (FALSE)
#define ae_map_share_discard_failed(path) ((void)0)
/* (layouts travel with maps, 0.3.5: no layout kept here) */
#define ae_map_share_layout_downloaded(level_name) ((void)0)
/* (ChupathingyCE's list of downloaded maps, map_families.c: none here) */
#define map_downloaded_mark(level_name) ((void)0)

#endif
