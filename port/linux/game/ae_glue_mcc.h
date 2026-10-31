/*
AE_GLUE_MCC.H

What the AE hook lines of MCC's Custom Edition files call in the game's units
(ae_hooks.txt lists those lines; notes/ae-hooks.md says where each goes):
map_families.c's resource lookup, ce_resources.c's refusal, the PC menus' map
pick, their functions and their status line. The detection and the reads are
port/linux/src/ae_mcc.h's. Plain types only.
*/

#ifndef __AE_GLUE_MCC_H
#define __AE_GLUE_MCC_H

#include "../src/ae_mcc.h"

struct widget_instance;

/* (menu_functions.c's map_list_choose, a multiplayer map chosen) when the map
is a Halo PC one (it needs Custom Edition's resource maps), the player's own
folders lack them, game.mcc_use is "ask" and MCC has them: the map is made
the one used last (so the list comes back on it), and the question opens in
the Map screen's place. 1 then (widget_deleted set); 0 to go on */
int ae_mcc_map_chosen(char const *map_name, short chosen, struct widget_instance *list, unsigned char *widget_deleted);
/* (menu_functions.c) the question's and Settings' functions, "ae mcc ...":
USE MCC FILES (answered, then the map picked as the Map screen's OK picks
it; the question's handler opens the gametypes), NOT NOW, NEVER ASK AGAIN,
BROWSE. 1; 0 when USE MCC FILES' map is no longer listed */
int ae_mcc_menu_event(char const *name, short controller);
/* whether the player's own folders (maps_ce, maps\ce, custom_maps) have all
three resource maps */
int ae_mcc_own_files_present(void);

#endif
