/*
AE_PICKUP_RULES.H

AE PICKUPS (0.9.3, owner 2026-11-15): in an AE game the weapons lying on the map are AE's, pure (no engine includes;
port/linux/tests/ae_pickup_rules_test.c). ae_pack_spawns.c reads it at the item spawn, the host's alone: a guest only
gets the objects, so a guest's build and its weapon cache (the same cache every machine of an AE game has) need nothing
new. The players' starts are not touched (the CE magnum of a COMP start stays one).

Where: a game with the cache loaded whose gametype has its MAP SPAWNS bit (CREATE GAME's PACK WEAPONS ON MAP; a
server's ae_pickups rule) or a tournament game (ae_glue_social_game.c: every playlist but CLASSIC COMP, which plays the
maps' own weapons). A Workshop map is a mod as its author made it and keeps its weapons.

The owner's table (registry ids, tools/weapon_cache/registry.json):
- an assault rifle's spots, in the order the map first spawns them: COMBAT AR, SUPPRESSED AR, the map's own assault
  rifle, then again ("use all of them, sometimes there are multiple AR spawns");
- a sniper rifle: the BOLT SNIPER;
- a pistol, on a Halo 2 map (a map file h2d_* or h2_*: the demakes, which carry Halo 2's magnum): the SIDEARM; the CE
  magnum stays one on every other map;
- a Halo 2 map's own brute shot (seven of Halo 2's maps place one): AE's BRUTE SHOT;
- a needler: AE's NEEDLER (0.9.3, the owner's round: "needler are all stock on hangem high"). A gametype naming the
  NEEDLER has the map's references made the cache's already (ae_pack_rifle_rules.h): those spots are the cache's own and
  left alone; one that does not name it (AE BR TS names only its battle rifle) gets it here;
- the rest (rockets, shotgun, the plasma weapons, the flamethrower, the fuel rod) stays the map's own. The battle rifle
  is the cache's by the map's references where the gametype names it (no stock map places one).
- a layout line's pack weapon (item brute_shot at ...: ae_layout_rules.h) spawns there whether or not the gametype names
  it, so a demake's layout puts Halo 2's weapons back on the spots it gave a CE stand-in (H2 Containment: its BRs,
  carbines, brute shots and SMGs); any other game gets the stand-in.
*/

#ifndef __AE_PICKUP_RULES_H
#define __AE_PICKUP_RULES_H

#include <string.h>

#include "ae_weapon_slots.h"

enum
{
	/* the AE weapons the table gives (registry ids) */
	AE_PICKUP_COMBAT_AR = 12,
	AE_PICKUP_SUPPRESSED_AR = 5,
	AE_PICKUP_BOLT_SNIPER = 9,
	AE_PICKUP_SIDEARM = 13,
	AE_PICKUP_BRUTE_SHOT = 11,
	AE_PICKUP_NEEDLER = 8,
	/* the assault rifle spots' turn: COMBAT AR, SUPPRESSED AR, the map's own */
	AE_PICKUP_AR_TURN = 3,
	AE_PICKUP_NAME_MAXIMUM = 255
};

enum ae_pickup_kind
{
	AE_PICKUP_KIND_NONE = 0,
	AE_PICKUP_KIND_ASSAULT_RIFLE,
	AE_PICKUP_KIND_SNIPER_RIFLE,
	AE_PICKUP_KIND_PISTOL,
	AE_PICKUP_KIND_BRUTE_SHOT,
	AE_PICKUP_KIND_NEEDLER
};

/* the part of a path after its last backslash or slash, lowered into out (size bytes) */
static inline void ae_pickup_leaf(char const *path, char *out, unsigned int size)
{
	unsigned int length = 0, at;
	char const *leaf = path;

	if (!size)
		return;
	out[0] = 0;
	if (!path)
		return;
	for (at = 0; path[at]; at++)
		if (path[at] == '\\' || path[at] == '/')
			leaf = path + at + 1;
	for (; leaf[length] && length + 1 < size; length++)
		out[length] = (char)(leaf[length] >= 'A' && leaf[length] <= 'Z' ? leaf[length] + 32 : leaf[length]);
	out[length] = 0;
}

/* whether a map is a Halo 2 map: its file (after any folder) starts with h2d_ or h2_, in any case */
static inline int ae_pickup_map_is_halo2(char const *map_name)
{
	char leaf[AE_PICKUP_NAME_MAXIMUM + 1];

	ae_pickup_leaf(map_name, leaf, sizeof(leaf));
	return !strncmp(leaf, "h2d_", 4) || !strncmp(leaf, "h2_", 3);
}

/* what a weapon on the map is for the table: its stock kind (ae_weapon_slots.h's AE_WEAPON_STOCK_*, from the globals'
weapon list) and its tag's name (a Halo 2 map's own magnum and brute shot, and any map's needler, are no stock weapon of
the list) */
static inline enum ae_pickup_kind ae_pickup_kind_of(short stock, char const *tag_name, int halo2_map)
{
	char leaf[AE_PICKUP_NAME_MAXIMUM + 1];

	ae_pickup_leaf(tag_name, leaf, sizeof(leaf));
	if (halo2_map && !strstr(leaf, "ammo") && (strstr(leaf, "brute shot") || strstr(leaf, "brute_shot")))
		return AE_PICKUP_KIND_BRUTE_SHOT;
	if (halo2_map && (stock == AE_WEAPON_STOCK_PISTOL || (strstr(leaf, "magnum") && !strstr(leaf, "ammo"))))
		return AE_PICKUP_KIND_PISTOL;
	if (stock == AE_WEAPON_STOCK_ASSAULT_RIFLE)
		return AE_PICKUP_KIND_ASSAULT_RIFLE;
	if (stock == AE_WEAPON_STOCK_SNIPER_RIFLE)
		return AE_PICKUP_KIND_SNIPER_RIFLE;
	/* (a needler by its leaf's name, as the map's references are found (ae_pack_rifle_rules.h's
	ae_pack_rifle_name_is_needler): "needler", not the super needler) */
	if (strstr(leaf, "needler") && !strstr(leaf, "super"))
		return AE_PICKUP_KIND_NEEDLER;
	return AE_PICKUP_KIND_NONE;
}

/* whether the game's weapon set leaves a spot's weapon what it was, so the table may swap it: the remap gave the same
definition, or one of the same stock kind (a SNIPING set's sniper on a spot that was an assault rifle is the set's, not
a sniper spot) */
static inline int ae_pickup_set_keeps(short stock, short remapped_stock, int same_definition)
{
	return same_definition || (stock != AE_WEAPON_STOCK_NONE && stock == remapped_stock);
}

/* the AE weapon (registry id) a weapon of that kind spawns as, 0 for the map's own. turn is an assault rifle spot's
place in the map's order (0 the first spot) */
static inline unsigned int ae_pickup_weapon(enum ae_pickup_kind kind, unsigned int turn)
{
	switch (kind)
	{
	case AE_PICKUP_KIND_ASSAULT_RIFLE:
		return turn % AE_PICKUP_AR_TURN == 0 ? AE_PICKUP_COMBAT_AR :
			turn % AE_PICKUP_AR_TURN == 1 ? AE_PICKUP_SUPPRESSED_AR : 0;
	case AE_PICKUP_KIND_SNIPER_RIFLE:
		return AE_PICKUP_BOLT_SNIPER;
	case AE_PICKUP_KIND_PISTOL:
		return AE_PICKUP_SIDEARM;
	case AE_PICKUP_KIND_BRUTE_SHOT:
		return AE_PICKUP_BRUTE_SHOT;
	case AE_PICKUP_KIND_NEEDLER:
		return AE_PICKUP_NEEDLER;
	default:
		return 0;
	}
}

#endif /* __AE_PICKUP_RULES_H */
