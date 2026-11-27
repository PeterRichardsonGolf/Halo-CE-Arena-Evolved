/*
AE_SCREEN_ITEM_PICKER.H

The campaign lobby's ITEM SETS (ae_screen_item_picker.c): RULES' row, and the picker it opens: NONE, the campaign swaps, and the sets that play with
the randomizer. A pick writes what ae_item_swaps_rules.h's ae_is_pick_writes says, through the item-sets table (ae_item_sets_data.h). Include after
cseries.h.
*/

#ifndef __AE_SCREEN_ITEM_PICKER_H
#define __AE_SCREEN_ITEM_PICKER_H

struct ae_is_backend;

/* opens the picker over the lobby: the engine's data, or (demo) DEV MODE's made-up table; waits_scene: the randomizer off, so the sets wait;
mcc_mission: the campaign picked is MCC's, so the sets wait. 0 when the stack is full */
int ae_screen_item_picker_open(int demo, int waits_scene, int mcc_mission);
/* the lobby's row: whether it is shown (this PC has a set with an item on; never in a challenge or an online co-op lobby), its value, and whether
a campaign swap plays (the RANDOMIZER row waits then). demo: DEV MODE's made-up table; mcc_mission: the campaign picked is MCC's */
int ae_screen_item_picker_lobby(int demo, int mcc_mission, char *value, int size, int *swap_playing);
/* the made-up table the staged scenes share: a fresh one (NULL), or the one a page is using */
void ae_screen_item_picker_demo(struct ae_is_backend const *be);

#endif
