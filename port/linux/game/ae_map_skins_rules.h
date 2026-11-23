/*
AE_MAP_SKINS_RULES.H

A map's own look for AE's weapons, as pure rules (port/linux/tests/ae_map_skins_rules_test.c runs them;
ae_weapon_pack.c applies them).

- THE DIGSITE MAPS: the maps of the Steam Workshop item 3477809506 (CE Digsite Multiplayer), named
  "<map>@ws3477809506". The map's name decides, so every machine does the same with no network change.
- A WEAPON'S DIGSITE LOOK: a pack may hold, beside a weapon, a second weapon tag of the same name with "_digsite"
  after it. On a Digsite map the weapon takes that tag's model, first-person model and first-person animations; its
  numbers, sounds, HUD and everything else stay its own.
- THE MAP'S OWN WEAPONS: a Workshop map keeps its own battle rifle and needler (a mod as its author made it), but a
  Digsite map's battle rifle is AE's SINGLESHOT BR (the weapon cache's singleshot_br: one round a pull, in CE+ X's
  battle rifle look): its placements, starts and lists give it (AE's battle rifle where the cache lacks it). A
  Digsite map's tag names are hidden, so its battle rifle is known by its bullet, "weapons\battle rifle\bullet".
*/

#ifndef AE_MAP_SKINS_RULES_H
#define AE_MAP_SKINS_RULES_H

#define AE_MAP_SKINS_DIGSITE_SUFFIX "@ws3477809506"
#define AE_MAP_SKINS_DIGSITE_LOOK "_digsite"
#define AE_MAP_SKINS_DIGSITE_RIFLE_BULLET "weapons\\battle rifle\\bullet"
/* the weapon cache's key of the weapon a Digsite map's own battle rifle becomes (tools/weapon_cache/registry.json) */
#define AE_MAP_SKINS_DIGSITE_RIFLE_KEY "singleshot_br"

/* whether the map (its name as the game has it, "<map>@ws<item>") is a Digsite map */
int ae_map_skins_rules_digsite(char const *map_name);
/* whether the map keeps its own battle rifles (needler 0) or needlers (needler 1) */
int ae_map_skins_rules_keeps_own(char const *map_name, int needler);
/* whether candidate is the Digsite look of the weapon tag named weapon (its name and "_digsite"; case aside) */
int ae_map_skins_rules_is_look(char const *weapon, char const *candidate);
/* whether a weapon whose first trigger fires this projectile is a Digsite map's battle rifle (case and slash aside) */
int ae_map_skins_rules_digsite_rifle_bullet(char const *projectile);

#endif
