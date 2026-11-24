/*
AE_SCREEN_MAPS_HUB.H

AE's MAPS & MODS screen (ae_screen_maps_hub.c): one screen, five pages under a tab strip (LB / RB, Q / E, a click): INSTALLED
(every map this PC has), REQUIRED FILES (what Custom Edition and MCC maps need), BROWSE CE MAPS (the community map archive),
WORKSHOP (the Steam Workshop's maps) and MODS (the mods and the modded disc). Opened from the main menu and from SETTINGS >
SYSTEM > FILES. The rules (the pages, what each offers, the footers) are ae_maps_hub_rules.h's, the lists ae_maps_hub_data.h's.
*/

#ifndef __AE_SCREEN_MAPS_HUB_H
#define __AE_SCREEN_MAPS_HUB_H

#include "ae_maps_hub_rules.h"

/* opens MAPS & MODS over the stack on that page (AE_HUB_TAB_*; -1: the page used last); 0 when the stack is full */
int ae_screen_maps_hub_open(int tab);

/* (DEV MODE `menu maps [<page> [<state>]]`) opened on a page in a state (AE_HUB_STATE_*), every page drawn from made-up data:
no files are read or changed and nothing is asked of the network. An open MAPS & MODS is replaced */
int ae_screen_maps_hub_open_staged(int tab, int state);

/* the main menu row's value: "DOWNLOADING n" while the archive installs a map, NULL otherwise */
const char *ae_screen_maps_hub_badge(void);

/* the WORKSHOP page's door: ae_screen_steam.c draws the Workshop list over the hub with the hub's tab strip (drawn by this, at
the flow's frame), a click on another tab leaves that page for the tab, and LB / RB (the step) do the same */
struct ae_flow_screen;
struct ae_pointer;
void ae_screen_maps_hub_strip(struct ae_flow_screen *flow, int tab);
int ae_screen_maps_hub_strip_pointer(struct ae_pointer const *pointer);
void ae_screen_maps_hub_step(int direction);

#endif
