/*
AE_SCREEN_ITEM_SETS.H

MAPS & MODS' sixth page, ITEM SETS (ae_screen_item_sets.c): the sets of items taken out of maps on this PC (Content Evolved's tag
injector profiles), a set's items, and the way to take more out of a map. It is a screen of its own over the hub, as the WORKSHOP
page is: the hub's tab strip is drawn by the hub (ae_screen_maps_hub_strip), a click on another tab and LB / RB leave it for that
page. The rules are ae_item_sets_rules.h's, what it reads and does ae_item_sets_data.h's.
*/

#ifndef __AE_SCREEN_ITEM_SETS_H
#define __AE_SCREEN_ITEM_SETS_H

/* opens the page over the hub: staged (DEV MODE's `menu maps sets <scene>`: made-up data, no file touched) with the scene
(AE_IS_SCENE_*), else the real page (it comes back to the step it was left on); 0 when the stack is full */
int ae_screen_item_sets_open(int staged, int scene);
/* closes the page when it is the top screen (the hub staging another page) */
void ae_screen_item_sets_close(void);
/* the real page opened from the campaign lobby's picker on a campaign swap (kind 0: its file) or a set (kind 1: its folder); B comes back to
the picker. 0 when it did not open */
int ae_screen_item_sets_open_from(int kind, char const *id);

#endif
