/*
AE_WEAPON_IDS.H

The weapon cache's ids as the slot-keyed consumers ask them (design 6.5; seam map S5): which weapon id a definition on the
loaded map is, the definition an id is, whether it is there and whether the game plays it. One answer with a cache (the
cache's placement, ae_weapon_cache.h) and without one (the loose slot packs, where id n is slot n). The consumers read the
rest of what they need from the registry's row for the id (ae_weapon_registry.h): the kill feed word and picture, bot
values, medal role, trigger family, stock fallback.

Include after cseries.h.
*/

#ifndef AE_WEAPON_IDS_H
#define AE_WEAPON_IDS_H

#include "ae_weapon_registry.h"

/* the weapon id (ids 1-24, the registry's) of the weapon, or of one of its config views, whose definition this is; 0 for a
definition that is not a cache weapon's */
unsigned int ae_weapon_id_of_definition(long definition_index);
/* the weapon cache's id of the weapon (or of one of its config views) whose definition this is, the ids past 24 too (the
Protomag, 25); 0 for a definition that is no cache weapon's or with no cache. Never index a slot array by it */
unsigned int ae_weapon_id_of_definition_any(long definition_index);
/* the registry row of that weapon (by the full id, so the ids past 24 too), NULL for none */
struct ae_weapon_registry_row const *ae_weapon_id_row_of_definition(long definition_index);
/* a weapon's definition (its weapon tag, config 0) on the loaded map, NONE when it is not there */
long ae_weapon_id_definition(unsigned int id);
/* whether the weapon is on the loaded map and usable (attached, not unavailable here) */
boolean ae_weapon_id_available(unsigned int id);
/* whether the game plays the weapon: available, and named by its gametype (DEV MODE's give and the wheel take any
available one: ae_weapon_id_available) */
boolean ae_weapon_id_played(unsigned int id);

/* whether a weapon tag of the loaded map is one the DEV wheel and DEV give list: every tag that is not the cache's own, and of
the cache's the weapons themselves (config 0), not a config's view or a tag the cache carries for another weapon. The
wheel lists the registry's weapons, once each */
boolean ae_weapon_ids_is_listed(long definition_index);
/* (game_engine_predict_resources) each weapon the gametype names, by id, as the map's weapons are predicted */
void ae_weapon_ids_predict_named(void);
/* (game_engine_log_rules, every machine at game start) one line each, in the log and the console, for each weapon the
gametype names and the loaded map did not get ("BATTLE RIFLE unavailable on <map>: <reason>") */
void ae_weapon_ids_unavailable_lines(void);
/* a weapon's definition for a give: the loadout's, the ground test aid's, a layout's or map spawn's weapon, DEV MODE's give
(what names it in the log). NONE when it is not on the map, or when the no-cache guard refuses it (a network game whose
selection does not use the cache: the stock weapon plays, ae_weapon_selection_game.h) */
long ae_weapon_ids_give_definition(unsigned int id, char const *what);

#endif
