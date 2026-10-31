/*
AE_LAYOUT.H

AE map layout files applied (queue 113; the file's reader: ae_layout_rules.h). When a map loads (game.c's game_load,
an AE hook line right after scenario_load), the map's layout file, if the map pack has one beside the map
(<the map's folder>\layouts\<map file name, no .map>.txt) and it was made for this very file (its hash prefix), changes the
loaded scenario in memory: its netgame items (a new list in place of the map's), its placed vehicles (dropped), its
player spawns and netgame flags (new lists, queue 141). The
map file is never written. Every machine that has the file does the same, so a guest's item timers agree with the
host's items; a guest without it gets the host's items and vehicles as always (the host spawns them) and its own item
timers from the map (owner 2026-10-16: guests read the pack's file).

Since 0.3.5 a host's layout travels to its guests that run this game (ae_layout_share_rules.h, ae_map_share_all.c): a
guest plays its host's layout for the match (none when its host has none), and its own file only when its host sent no
word (an older host).
*/

#ifndef __AE_LAYOUT_H
#define __AE_LAYOUT_H

/* the loaded map's layout, if it has one: applied once to this load of the map (a replay of the map without a reload
keeps it), every step logged ("layout: ...") */
void ae_layout_apply(void);

/* whether the size bytes at the Xbox address lie in the applied layout's item list: a Custom Edition map's tags are read
only inside its tag cache (ce_map_checks.c ce_tags_pointer), and this list is read in place of the map's */
int ae_layout_memory(unsigned long address, long size);

/* (item_timers.c) the applied layout's name for an item of the scenario's list (name item <n>), into label: TRUE when it
has one */
boolean ae_layout_item_label(long equipment_index, char *label, long size);

/* (ae_pack_spawns.c, the item spawn's) the pack slot (ae_weapon_slots.h) a layout line put on an item of the
scenario's list (item smg at ...), or 0 */
short ae_layout_item_pack_slot(long equipment_index);

/* (game_engine.c's item spawn, AE hook) whether a netgame equipment entry's facing is a layout line's, so an Xbox
map's item turns by it as a Custom Edition map's does */
boolean ae_layout_equipment_faces(long equipment_flags);

/* (game_engine_bomb.c's jump multiplier, AE hook: biped_jump and the host's bound on a guest's movement) the applied
layout's jump line as a multiplier of the jump speed, 1 when the map has none */
real ae_layout_jump_multiplier(void);

/* (ae_bots_travel.c) the applied layout's bot lines (bot jump, bot avoid): the layout itself, NULL when the map has
none applied or no bot lines */
struct ae_layout;
struct ae_layout const *ae_layout_bot_hints(void);

/* (ae_bots_travel.c, debug.bot_hints) the first line of a layout file for the loaded map ("layout 1 <file> <hash>"),
into text: FALSE when the map has no file the layouts know */
boolean ae_layout_map_header(char *text, long size);

/* (object_types.c's placing of the scenario's objects, game_engine.c's vehicle sets: AE hooks) whether a vehicle
placement is placed in this gametype (a layout's vehicle line's game types; the map's own always), and the team it
counts for in a set of each team's (a vehicle line's team; else side, the nearest team's) */
boolean ae_layout_vehicle_placed(void const *placement);
short ae_layout_vehicle_side(void const *placement, short side);

/* (map sharing, a host's: ae_map_share_all.c) the layout file this machine applies to a level's map (level_name as
the game names it), read: TRUE with its text in text (size bytes; *length of them) when it has one that reads and was
made for that very map file; FALSE when it has none it would apply */
boolean ae_layout_host_text(char const *level_name, char *text, long size, long *length);

/* (map sharing, a guest's) its host's layout for a level whose map was just downloaded from it: kept on disk beside
the map as ae_layout_share_rules.h says, if it was made for the very map file now here */
void ae_layout_keep_host_text(char const *level_name, char const *text, long length);

#endif /* __AE_LAYOUT_H */
