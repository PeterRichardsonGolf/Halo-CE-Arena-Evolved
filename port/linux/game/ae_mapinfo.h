/*
AE_MAPINFO.H

The AE menus' map-file reader (ae_mapinfo.c): what a map picker shows about a
multiplayer map, read from the cache file itself: its family, name and build,
which game types its objects support, spawns per game type, equipment,
vehicles, BSPs and how far its play area spreads. It agrees with the tools'
Python reader (notes/map-metadata/mapmeta.py) and reads Xbox maps (version 5:
stock, and community builds such as Halo 1: NHE), Custom Edition (609, also
protected maps and OpenSauce's .yelo) and HaloMD's (7: Halo PC retail's); an
MCC map (13) is named but not read further (the port can't play it).

The core reads through a callback (ae_mapinfo_parse), so the game's side can
read through its own file layer; ae_mapinfo_read is a stdio wrapper (the unit
tests, tools). Every offset, pointer and count is checked against what was
read: a corrupt or cut map is refused, never read past.
*/

#ifndef __AE_MAPINFO_H
#define __AE_MAPINFO_H

enum ae_map_family { AE_MAP_UNKNOWN, AE_MAP_XBOX, AE_MAP_XBOX_COMMUNITY, AE_MAP_CUSTOM_EDITION, AE_MAP_HALOMD, AE_MAP_MCC };
enum { AE_MODE_CTF = 1, AE_MODE_SLAYER = 2, AE_MODE_ODDBALL = 4, AE_MODE_KING = 8, AE_MODE_RACE = 16 };
/* ae_mapinfo_parse's and ae_mapinfo_read's results */
enum
{
	AE_MAPINFO_OK = 0,
	/* not a cache file, or cut, or corrupt: family / name / build as far as read */
	AE_MAPINFO_UNREADABLE = 1,
	/* a cache file the port doesn't play (MCC's): family, name and build only */
	AE_MAPINFO_UNSUPPORTED = 2
};

struct ae_mapinfo
{
	int family;                 /* enum ae_map_family */
	/* the header's bytes as they are (a map's name up to 32 characters, its build up to 32), NUL-terminated: not
	checked as text, so the caller makes them safe to draw (control bytes, bytes 0x80-0xFF) */
	char name[33], build[33];
	int scenario_type;          /* 0 solo, 1 multiplayer, 2 ui */
	int modes;                  /* AE_MODE_*: the game types the map's objects support (mapmeta.py's rule) */
	int hills, race_checkpoints;
	int spawns[6];              /* by game type 0..5 as game_engine.h:95 (index 0 unused) */
	int equipment, vehicles, bsps;
	float play_extent;          /* spread of spawns + flags + equipment, world units */
	int protected_map;          /* CE scenario group relabelled (prot/devm) */
	int open_sauce;             /* an OpenSauce map (its header's 'yelo' block): listed, not refused */
};

/* reads size bytes at offset into buffer: nonzero if all of them were read */
typedef int (*ae_mapinfo_reader)(void *context, unsigned long offset, void *buffer, unsigned long size);

/* AE_MAPINFO_OK, _UNREADABLE or _UNSUPPORTED; file_size is the file's size in bytes */
int ae_mapinfo_parse(ae_mapinfo_reader read, void *context, unsigned long file_size, struct ae_mapinfo *info);
/* 0 on success; nonzero (and info->family / name as far as read) when the file is unreadable or not a map */
int ae_mapinfo_read(const char *path, struct ae_mapinfo *info);

#endif
