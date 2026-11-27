/*
AE_SCREEN_FORGE.H

The doors into Content Evolved's Forge (ae_screen_forge.c): the main menu's FORGE row opens the
FORGE hub (CONTINUE the last map, NEW MAP, MY MAPS, ITEM SETS, CONTROLS) with the progress of Forge's tag pack, NEW MAP picks a
multiplayer map to start Forge on alone, and CONTROLS is FORGE KEYS, Forge's twelve actions and two pad speeds written to
Faber_data/forge_controls.ini as Forge's own screen writes it. The rules are ae_forge_doors_rules.h's. Forge is in the 64-bit
Linux build alone: any other build answers that FORGE is not a door (the main menu keeps it under COMING SOON).
*/

#ifndef __AE_SCREEN_FORGE_H
#define __AE_SCREEN_FORGE_H

/* 1 when this build has Forge: the main menu's FORGE row opens it; 0: the row waits under COMING SOON */
int ae_screen_forge_available(void);

/* the main menu row's value while Forge's pack is built: "40 %"; NULL otherwise */
const char *ae_screen_forge_badge(void);

/* opens the FORGE hub over the stack (it asks for Forge's tag pack when it is not ready); 0 when the stack is full or there
is no Forge */
int ae_screen_forge_open(void);

/* (DEV MODE `menu forge [hub|building|needs|maps|keys]`) hub: the real hub, as the FORGE row opens it; the others are made-up
scenes that start and write nothing: the hub with the pack being built (40 %) or without the game's maps, NEW MAP's list, FORGE KEYS; mymaps: MY MAPS read from this PC's saved maps (nothing is written until a map is renamed, copied or deleted) */
int ae_screen_forge_open_staged(char const *scene);

/* every screen of the doors closed, wherever they are (a Forge level leaving the menus) */
void ae_screen_forge_close(void);

#endif /* __AE_SCREEN_FORGE_H */
