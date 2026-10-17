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

/* (ae_bots_travel.c) the applied layout's bot lines (bot jump, bot avoid): the layout itself, NULL when the map has
none applied or no bot lines */
struct ae_layout;
struct ae_layout const *ae_layout_bot_hints(void);

/* (ae_bots_travel.c, debug.bot_hints) the first line of a layout file for the loaded map ("layout 1 <file> <hash>"),
into text: FALSE when the map has no file the layouts know */
boolean ae_layout_map_header(char *text, long size);

#endif /* __AE_LAYOUT_H */
