/*
AE_WEAPON_PACK.H

port: weapon pack. The weapon packs this game carries (docs/weapon-packs.md),
read once at start (game_initialize) and kept by SLOT (ae_weapon_slots.h): a
gametype's universal_variant.weapon_pack is a mask of slots (game_engine.h; bit
0 = slot 1, the AE battle rifle, packs\ae_battle_rifle.aepack beside the
game's maps folder; slot 2 the SMG, 3 the silenced SMG, 4 the carbine). Every
named slot's pack is added to the map, in slot order.

A pack is an untrusted download: one that is missing, damaged or of another
pack is refused with one "weapon pack:" line in debug.txt, its slot stays
empty, and the game plays without it. A game with no packs folder logs
nothing. A loaded pack's file stays open (one handle) for its resources,
which are read when a map asks for them.

When a multiplayer map loads for a game whose gametype asks for a pack, the
pack is added to the map's tags (ae_weapon_pack_tags_loaded), after its own
tags have been checked as a map's are; a pack a map cannot take is refused
for that map with one line, and the game plays the stock fallback weapons.

Include after cseries.h.
*/

#ifndef __AE_WEAPON_PACK_H
#define __AE_WEAPON_PACK_H

#include "ae_pack_memory_rules.h"

struct ae_pack_weapon_record;

enum
{
	/* the slots are 1 to AE_WEAPON_PACK_SLOTS (ae_weapon_slots.h's AE_WEAPON_SLOT_COUNT; the mask's bit 0,
	_weapon_pack_battle_rifle, is slot 1) */
	AE_WEAPON_PACK_SLOTS = 7,
	AE_WEAPON_PACK_SHA256_SIZE = 32,
};

/* every slot's pack read and checked, each logged (game_initialize; once) */
void ae_weapon_pack_initialize(void);
/* every pack let go (game_dispose) */
void ae_weapon_pack_dispose(void);

/* whether the slot holds a pack that was read and accepted */
boolean ae_weapon_pack_slot_loaded(short slot);
/* the slot's pack's revision and SHA-256 (32 bytes; either may be NULL): TRUE, or FALSE (nothing written) when the
slot is empty */
boolean ae_weapon_pack_identity(short slot, unsigned int *revision, unsigned char *sha256);
/* how many weapons the slot's pack holds (0 when empty) */
short ae_weapon_pack_weapon_count(short slot);
/* the slot's pack's weapon entry (0 first: the one pack_loadout gives), or NULL; its strings through
ae_weapon_pack_string */
struct ae_pack_weapon_record const *ae_weapon_pack_weapon_entry(short slot, short weapon);
/* a string of the slot's pack (a weapon entry's key, label, menu name, ...): NULL for AE_PACK_NONE, an offset past
its STRINGS or an empty slot */
char const *ae_weapon_pack_string(short slot, unsigned int offset);

/* ---------- the pack on the loaded map */

enum
{
	AE_WEAPON_PACK_FINGERPRINT_SIZE = 8,
};

/* (scenario_tags_load) the pack the pending game's gametype asks for added to the multiplayer map whose tags
have just loaded and been checked, or refused for it (one line); scenario_tag_index: the map's scenario */
void ae_weapon_pack_tags_loaded(char const *map_name, long scenario_tag_index);
/* (scenario_tags_unload) the pack's tags let go and the map's own table back */
void ae_weapon_pack_tags_unloaded(void);
/* (cache_file_read) a read of the "map file" for a tag: TRUE if the tag is a pack's own, and then buffer has
size bytes from the pack (zeros for what it cannot give) and *complete is set */
boolean ae_weapon_pack_read(long tag_index, long offset, long size, void *buffer, boolean *complete);

/* the lowest slot whose pack is added to the loaded map, or 0: none was asked for, or each was refused (the
fallback: what the loadout and the map spawns key on) */
short ae_weapon_pack_added(void);
/* whether the vehicle damage pack (packs\ae_vehicle_damage.aepack, a pack of tags only: ae_vehicle_pack_rules.h)
is added to the loaded map: read at start beside the weapon packs, added after them to a multiplayer map with
vehicles when the game about to be played has VEHICLES DESTRUCTIBLE on; its tags are found by name */
boolean ae_weapon_pack_vehicle_pack_added(void);
/* whether SEARCH AND DESTROY's bomb pack is on the loaded map (ae_bomb_pack.c) */
boolean ae_weapon_pack_bomb_pack_added(void);
/* whether the slot's pack is added to the loaded map */
boolean ae_weapon_pack_slot_added(short slot);
/* the slots whose packs are added to the loaded map (ae_weapon_slots.h's mask) */
unsigned int ae_weapon_pack_added_mask(void);
/* the slot of the pack whose weapon this definition is on the loaded map, or 0 */
short ae_weapon_pack_definition_slot(long definition_index);
/* the import fingerprint of the slot's pack on this map (AE_WEAPON_PACK_FINGERPRINT_SIZE bytes: the first 8 of the
SHA-256 of the pack's SHA-256, the map's name, the tag count before it and each pack tag's tag index in pack
order): FALSE (nothing written) when that pack is not added */
boolean ae_weapon_pack_fingerprint(short slot, unsigned char *fingerprint);
/* the slot's pack's weapon's definition (its weapon tag) on the loaded map, or NONE: no such weapon, or the pack
is not added to this map */
long ae_weapon_pack_weapon_definition(short slot, short weapon);
/* whether a tag of the loaded map is one of the pack's own (added with it), not the map's: what may be retuned
for the pack's weapons without touching the map's (ae_burst_rifle.c) */
boolean ae_weapon_pack_tag_is_own(long tag_index);
/* (game_engine_give_loadout) a player was given a pack weapon as the loadout's primary (0) or secondary (1):
the first of each on a map is logged */
void ae_weapon_pack_loadout_given(short loadout_slot, long definition_index);

/* ---------- the look of the pack's weapons on the loaded map (hud.c, hud_messaging.c, xbox_textures.c) */

struct icon_hud_element_definition;

/* (hud_get_item_string) the item message string *string_index names: TRUE and *string set when it is a pack
weapon's pickup text (its weapon entry's: the weapon's hud_message_index was pointed past every map's list as
the pack was added); FALSE otherwise, and then *string_index the weapon's own index again where its entry names
no such text (the engine's own) */
boolean ae_weapon_pack_item_string(long *string_index, wchar_t const **string);
/* (render_state_bitmap) the swap / pickup prompt's icon: for a pack weapon's messaging icon, the icon its weapon
entry names (a bitmap and a sequence of sprites, R10 / R30), *bitmap_group_index set to that bitmap; an icon refused
for the map (one line) comes back with sequence NONE: none is drawn. Only a pack weapon whose HUD is the pack's own
tag; any other icon is returned as it is */
struct icon_hud_element_definition const *ae_weapon_pack_message_icon(struct icon_hud_element_definition const *icon,
	long *bitmap_group_index);
/* (the platform layer's texture cache, port/linux/src/ae_pack_textures.h) whether the pixels at address are a
bitmap of a pack tag flagged SMOOTH (R9): 1 (sampled linear with its mips, as the high-res HUD's textures), 2
(and METER COVERAGE: its green is its shapes' coverage), 0 not one */
long ae_weapon_pack_smooth_bitmap_at(unsigned long address, long width, long height);

/* ---------- the fallback: a map the pack is not added to */

struct game_variant;

/* (scenario_tags_load, its Custom Edition branch, which returns before ae_weapon_pack_tags_loaded is reached) a
Custom Edition map (or Halo PC's, HaloMD's) takes the packs as an Xbox map does, once its tags are made the Xbox's
and before the menus' tags are added; refused with the reason any map is ("missing <group> <name>") */
void ae_weapon_pack_tags_loaded_ce(char const *scenario_name, long scenario_tag_index);
/* (scenario_tags_load, its MCC branch, which returns before ae_weapon_pack_tags_loaded is reached) one of MCC's maps
(<name>@mcc, <name>@ws<item id>; GulchCE's mcc_maps levels too) takes the packs as a Custom Edition map does: its tags
were made the engine's as they loaded (mcc_cache.c), the validator's state is the map's
(ae_tag_validate_custom_edition_map), and the menus' tags are not added yet. The map goes by its identity */
void ae_weapon_pack_tags_loaded_mcc(char const *scenario_name, long scenario_tag_index);
/* (ae_glue_ce_maps.c's ae_ce_map_menu_memory, which ce_map_checks.c's ce_tags_pointer asks on a Custom Edition map)
whether size bytes at an Xbox address lie wholly in a pack's own tags on the loaded map (its arena, from the moment
it is made as the pack is added until it is let go; any slot's, the vehicle damage pack's too) */
boolean ae_weapon_pack_memory(unsigned long address, long size);
/* whether a bitmap (a bitmap tag's bitmaps element) is one of a pack's own */
boolean ae_weapon_pack_bitmap_is_own(void const *bitmap);
/* (xbox_texture_cache.c) a texture's common flags for its bitmap: a pack's own is laid out as the Xbox's on any map,
a Custom Edition map's too (whose own bitmaps are Halo PC's): ae_pack_memory_rules.h */
unsigned long ae_weapon_pack_texture_common(void const *bitmap, unsigned long common);
/* (game_engine_log_rules: every machine, at game start) the line the console and the log give when the game's
gametype names the slot and the loaded map did not get its pack ("BATTLE RIFLE unavailable on <map>: <reason>"):
TRUE and line written, or FALSE (the pack is on the map, or not asked for) */
boolean ae_weapon_pack_unavailable_line(short slot, char *line, long size);
/* (the PC lobby, on the host) appended to the lobby's panel text (count characters) as a line of its own for each
slot the variant names (the slot's display name in place of BATTLE RIFLE), for a game of this variant on this map
before it starts: "BATTLE RIFLE unavailable on <map>" where this machine knows the pack cannot be added (no pack
in the slot here, or the pack was refused for that map before, a Custom Edition map's as any); nothing otherwise */
void ae_weapon_pack_lobby_line(wchar_t *text, long count, char const *map_name, struct game_variant const *variant);

#endif
