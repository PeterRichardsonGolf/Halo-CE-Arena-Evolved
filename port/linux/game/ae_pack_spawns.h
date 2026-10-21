/*
AE_PACK_SPAWNS.H

Weapon packs' weapons on the map's item spots (ae_weapon_slots.h's rules), at the item spawn (game_engine.c's
game_engine_update_item_spawn, an AE hook): the item an entry of the scenario's netgame equipment is about to spawn,
made a pack's weapon where

- a layout line put a pack weapon on that spot (item smg at ...: ae_layout_item_pack_slot), and the gametype names
  that pack and it is on the map: the spot's own fallback item (the pistol, the assault rifle) spawns otherwise;
- or the gametype's MAP SPAWNS (pack_loadout bit 4, a test switch only: owner 2026-10-21) is on: a spot that would
  spawn a stock pistol spawns the BR, one that would spawn an assault rifle the SMG.

Both only where the weapon set left the spot's weapon what it was (a SNIPING set's sniper rifle stays one). The spot's
respawn time, place and flags are the entry's own: only the definition changes. The item spawn is the host's alone
(or no network game's); every client gets the objects, with the pack's tags at the same indices.
*/

#ifndef __AE_PACK_SPAWNS_H
#define __AE_PACK_SPAWNS_H

/* the definition the netgame equipment entry spawns: definition_index (random_item's draw) or a pack's weapon */
long ae_pack_spawn_definition(long equipment_index, long definition_index);
/* (ae_weapon_pack.c, as the map's packs are added) a new map: the first replacements logged again */
void ae_pack_spawns_new_map(void);

#endif /* __AE_PACK_SPAWNS_H */
