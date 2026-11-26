/*
AE_SCREEN_RANDOMIZER.H

The campaign randomizer's screens (ae_screen_randomizer.c), reached from the campaign lobby's RANDOMIZER row: the hub (the
switch, one door to each page, the saved setups, the item set), the four pages (AI REPLACEMENT, AI WEAPONS, WEAPON PICKUPS,
PLAYER LOADOUT), LOAD A SETUP, NAME THIS SETUP and ITEM SET. Every setting they write is a randomizer setting
(ae_randomizer_menus_rules.h); the randomizer plays only a campaign level alone or in split screen and Firefight games.
*/

#ifndef __AE_SCREEN_RANDOMIZER_H
#define __AE_SCREEN_RANDOMIZER_H

#include <stddef.h>

/* the hub over the stack; 0 when the stack is full */
int ae_screen_randomizer_open(void);
/* the lobby row's value ("OFF", "ENEMIES \xE2\x80\xA2 PICKUPS", "ON") */
void ae_screen_randomizer_lobby_value(char *text, size_t size);
/* (DEV MODE `menu randomizer <scene>`) a scene staged: hub, hub-off, ai, weapons, pickups, loadout, setups, delete, itemset, name */
int ae_screen_randomizer_open_staged(const char *scene);

#endif
