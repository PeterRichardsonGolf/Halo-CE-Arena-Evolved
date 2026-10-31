/*
AE_MAP_KIND.H

(Arena Evolved) a map's kind by its identity, the name AE gives it in games, lists and the game record (at most a
record's map name, 127 characters):
- the Xbox's: its file name ("bloodgulch") or its scenario's path;
- Halo PC's families: custom_maps\<name>, maps_md\ and maps_pc\ names, <name>@ce, @pc, @md;
- a map pack's: <file>@<pack> (halo_map_families.h);
- MCC's stock multiplayer maps, read in place from MCC's halo1/maps: <name>@mcc, a stock name only
  (ae_mcc_stock_map_name);
- MCC Workshop maps: <name>@ws<item id> (Phase A's catalog), and GulchCE's package folders, mcc_maps\...
All are played on OpenCE's net 39; an MCC map's identity in a game is its name and the record's map version
(mcc_identity.c).
*/

#ifndef __AE_MAP_KIND_H
#define __AE_MAP_KIND_H

enum ae_map_kind
{
	AE_MAP_KIND_INVALID = -1,
	AE_MAP_KIND_XBOX,
	AE_MAP_KIND_CE,
	AE_MAP_KIND_PACK,
	AE_MAP_KIND_MCC_STOCK,
	AE_MAP_KIND_MCC_WORKSHOP,
};

/* an identity's kind; AE_MAP_KIND_INVALID for none, one too long (an MCC one of 64 characters or more: the loader's
level names hold 63), or a malformed MCC one ("a10@mcc", "x@ws12a", "../x@mcc") */
int ae_map_kind_of(const char *identity);
/* an MCC stock map's file name in MCC's halo1/maps (without .map, in lower case as MCC names it) for its identity,
<name>@mcc in any case (1); 0 for any other identity or a file that does not fit */
int ae_map_kind_mcc_stock_file(const char *identity, char *file, unsigned long size);

/* an MCC Workshop identity's parts, <name>@ws<item id>: its map's file name (without .map) and the item id (1); 0
for any other identity, or parts that do not fit */
int ae_map_kind_workshop_parts(const char *identity, char *name, unsigned long name_size, char *id, unsigned long id_size);

#endif
