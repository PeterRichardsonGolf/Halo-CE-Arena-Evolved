/*
AE_WEAPON_CACHE.H

The AE weapon cache in the engine (design: notes/plans/2026-11-05-weapon-cache-design.md, section 6.5): the id API
every consumer asks instead of a pack slot, plus the registry columns the consumers read. Interface freeze (T0): the
declarations only; ae_weapon_cache.c arrives with T2-2 (start) and T2-3 (attach), the lookups with T2-4. Until then
nothing includes this file.

IDS. A weapon, tag set, grenade type or the base layer is named by its permanent 32-bit id (ae_weapon_cache_format.h;
tools/weapon_cache/registry.json). Weapon ids 1-23 are today's slots 1-23. A CONFIG id names a variant of one weapon:
the pair (weapon id, config id), config 0 meaning the weapon itself.

DEFINITIONS. A weapon's definition is its 'weap' tag's index on the loaded map; a config's is its view tag's index
(appended after every cache tag, in config-id order, so it depends only on the cache and the map). Both map back to
(weapon id, config id). NONE where the entry is not available on this map: the stock fallback plays.

Include after cseries.h.
*/

#ifndef AE_WEAPON_CACHE_H
#define AE_WEAPON_CACHE_H

#include "ae_weapon_cache_format.h"
#include "ae_weapon_registry.h"
#include "ae_cache_config_rules.h"

struct ae_pack;

/* the state of the cache at start (ae_weapon_pack_initialize calls the opener) */
enum ae_weapon_cache_state
{
	AE_WEAPON_CACHE_ABSENT = 0, /* game.weapon_packs off, or no packs folder: nothing read, nothing refused */
	AE_WEAPON_CACHE_LOADED,
	AE_WEAPON_CACHE_MISSING, /* a packs folder without the cache file */
	AE_WEAPON_CACHE_STALE, /* another format, another schema, another serial or sha than this build's lock, damaged */
	AE_WEAPON_CACHE_DEV /* loaded and accepted as a dev cache: the right format and schema, not this build's lock */
};

/* a registry entry as the engine sees it (the DIRECTORY's, with its strings resolved) */
struct ae_weapon_cache_entry
{
	unsigned int id;
	unsigned int kind; /* AE_CACHE_KIND_* */
	unsigned int flags; /* AE_CACHE_ENTRY_* */
	char const *key; /* "battle_rifle" */
	char const *display; /* "BATTLE RIFLE" */
	unsigned int behaviour; /* weapons */
	unsigned int stock_fallback; /* weapons: ae_weapon_slots.h's ae_weapon_stock */
	unsigned int replaces_map_own; /* weapons: AE_CACHE_REPLACES_MAP_* */
	unsigned int legacy_slot; /* weapons: 1-24 or 0 */
	unsigned int ordinal; /* weapons: dense, DIRECTORY order from 0 (the item message index uses it) */
	unsigned int grenade_catalogue_id; /* grenades */
};

/* ---------- the consumer columns: ae_weapon_registry.h (the generated table; pure, so the rules units read it too) */

/* ---------- the cache */

/* AE's compiled tag schema's hash (tag_schema_hash.c; design 3.6), low word then high: what a cache is stamped with */
void tag_schema_hash(unsigned int *low, unsigned int *high);
/* each group's own hash, in tag order (tag_schema_hash.c): for finding where two builds' schemas differ */
void tag_schema_group_hashes(
	void (*report)(unsigned int group_tag, char const *definition_name, unsigned int low, unsigned int high));

/* the cache read at start (game_initialize, ae_weapon_pack_initialize calls it) and let go at the end */
void ae_weapon_cache_open(void);
void ae_weapon_cache_close(void);
/* the open cache's reader struct (NULL when none): its tags, directory, configs and the file for resource reads */
struct ae_pack *ae_weapon_cache_pack(void);

enum ae_weapon_cache_state ae_weapon_cache_state(void);
/* one line of why the cache is not loaded (the "update AE" text's source), NULL when it is */
char const *ae_weapon_cache_refusal(void);
/* the identity: the header's serial (1 for 0.7.6) and the first AE_CACHE_ID_SIZE bytes of its SHA-256; 0 / NULL
when no cache is loaded */
unsigned int ae_weapon_cache_serial(void);
unsigned char const *ae_weapon_cache_id(void);
/* the schema hash the open cache was stamped with: low word, *high the other (0 when none is loaded) */
unsigned int ae_weapon_cache_schema_hash(unsigned int *high);

/* the DIRECTORY entry of an id, or NULL (the id is not in this cache) */
struct ae_weapon_cache_entry const *ae_weapon_cache_entry(unsigned int id);
/* the weapon's definition (config 0), or its config's view, on the loaded map; NONE when it is not available here or no
cache is attached */
long ae_weapon_cache_definition(unsigned int weapon_id, unsigned int config_id);
/* the (weapon id, config id) a definition index is: TRUE, or FALSE for a definition that is not the cache's. Both
the weapon's own index and every view index of it map to its weapon id */
boolean ae_weapon_cache_of_definition(long definition_index, unsigned int *weapon_id, unsigned int *config_id);
/* whether an entry is available on the loaded map (attached, not unavailable, not retired, its part present) */
boolean ae_weapon_cache_available(unsigned int id);
/* why an entry is not available on this map ("missing first-person hands node", ...), NULL when it is */
char const *ae_weapon_cache_unavailable_reason(unsigned int id);
/* whether the tag is one of the cache's own on the loaded map (any of its tags and views) */
boolean ae_weapon_cache_is_own(long tag_index);
/* TAG SETS (vehicle damage AE_CACHE_ID_VEHICLE_DAMAGE, the bomb AE_CACHE_ID_BOMB): attached with the cache on every map the
cache goes on, used under the game's rules. attached: the cache is on this map and has the set. in use: attached and the game
asked for it when the cache attached (VEHICLES DESTRUCTIBLE on a map with vehicles; a bomb game). set_tag: the map's tag index of
the cache tag of this group named `name` inside the set (ae_cache_sets_rules.h: the ae\packs\<key>\ first part left out, case
aside), looked up in the set's DIRECTORY closure and root list; NONE where there is none */
boolean ae_weapon_cache_set_attached(unsigned int set_id);
boolean ae_weapon_cache_set_in_use(unsigned int set_id);
long ae_weapon_cache_set_tag(unsigned int set_id, long group_tag, char const *name);

/* the tags of a grenade type's DIRECTORY entry (by the catalogue's id), as the loaded map's tag indices */
enum ae_cache_grenade_tag
{
	AE_CACHE_GRENADE_PROJECTILE,
	AE_CACHE_GRENADE_EQUIPMENT,
	AE_CACHE_GRENADE_COLLECTION,
	AE_CACHE_GRENADE_HUD,
	AE_CACHE_GRENADE_FIELD_EFFECT
};
long ae_weapon_cache_grenade_tag(unsigned int catalogue_id, enum ae_cache_grenade_tag which);
/* a grenade type's DIRECTORY entry by the grenade catalogue's id, or NULL */
struct ae_weapon_cache_entry const *ae_weapon_cache_grenade(unsigned int catalogue_id);

/* the config view a weapon is made with in the game in play (ae_cache_config_rules.h): for a loadout slot (AE_CACHE_CONFIG_PRIMARY
/ _SECONDARY) the START pair that names the weapon with a config, else the game's weapon SET (casual, comp, BTB) config for it,
else 0 (the weapon itself); AE_CACHE_CONFIG_MAP_COPY (-1) for the map's copies and layouts, which no START pair reaches. Always a
config that has a view on the loaded map, else 0. Then ae_weapon_cache_definition(weapon_id, config) is the tag to make */
unsigned int ae_weapon_cache_config_for(unsigned int weapon_id, int loadout_slot);
/* the same for the map's copies (layouts, map spawns): ae_weapon_cache_config_for(weapon_id, AE_CACHE_CONFIG_MAP_COPY) */
unsigned int ae_weapon_cache_map_config(unsigned int weapon_id);

/* the item message index of a weapon's pickup text (its ammo text is one past): first + 2 x its ordinal */
long ae_weapon_cache_item_string_index(unsigned int weapon_id);

#endif /* AE_WEAPON_CACHE_H */
