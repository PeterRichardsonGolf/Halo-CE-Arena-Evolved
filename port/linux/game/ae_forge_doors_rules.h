/*
AE_FORGE_DOORS_RULES.H

The doors into Content Evolved's Forge (rc9, ae_screen_forge.c): what the FORGE hub's rows say while Forge's tag pack is
built or cannot be, the title of a map as the hub shows it, and the FORGE KEYS screen's rules: a key the platform layer
names as a Windows virtual key (the numbers Forge's own binds and forge_controls.ini hold), a controller button as
Forge's, and the steps of the pad's speeds. Pure: no engine includes (unit test: port/linux/tests/ae_forge_doors_rules_test.c).
*/

#ifndef __AE_FORGE_DOORS_RULES_H
#define __AE_FORGE_DOORS_RULES_H

#include <stddef.h>

/* ---------- the pack Forge's palette is built from */

enum ae_forge_pack_state
{
	AE_FORGE_PACK_READY,
	AE_FORGE_PACK_BUILDING,
	AE_FORGE_PACK_NEEDS_MAPS
};

/* the pack's state from what the tag pack module says: ready; writing (or queued: maps_wanted > 0); failed, or none of its
maps on this PC (maps_wanted 0) */
int ae_forge_doors_pack_state(int ready, int writing, int failed, int maps_wanted);
/* the percent of the maps read, 0 to 100 */
int ae_forge_doors_percent(int done, int wanted);
/* the row's value while it waits: "GETTING READY 40 %", "NEEDS THE GAME'S MAPS"; "" when ready */
void ae_forge_doors_pack_reason(int state, int percent, char *text, size_t size);
/* the card's line: "6 OF 17 MAPS READ"; "" when none are wanted */
void ae_forge_doors_pack_progress(int done, int wanted, char *text, size_t size);

/* ---------- the hub */

enum ae_forge_hub_row
{
	AE_FORGE_HUB_CONTINUE,
	AE_FORGE_HUB_NEW_MAP,
	AE_FORGE_HUB_MY_MAPS,
	AE_FORGE_HUB_ITEM_SETS,
	AE_FORGE_HUB_CONTROLS,
	AE_FORGE_HUB_ROWS
};

/* whether a hub row waits on the pack: CONTINUE and NEW MAP do; MY MAPS, ITEM SETS and CONTROLS work without it */
int ae_forge_doors_row_needs_pack(int row);
/* a map's name as the hub shows it: the level path's leaf, in capitals, without a "@ce" mark ("levels\test\bloodgulch\bloodgulch"
is BLOODGULCH); "" for none */
void ae_forge_doors_map_title(char const *path, char *text, size_t size);

/* ---------- FORGE KEYS (CE's twelve actions, two pad speeds) */

enum { AE_FORGE_KEYS_ACTIONS = 12 };

/* an action's label and its one-line help, as the screen shows them (CE's order); NULL out of range */
char const *ae_forge_keys_label(int action);
char const *ae_forge_keys_help(int action);

/* the Windows virtual key of an SDL scancode, 0 for a key Forge cannot read or that the screen keeps (Escape, Backspace,
Delete: cancel and clear) */
int ae_forge_keys_vk_of_scancode(int scancode);
/* a mouse button (SDL's: 1 left, 2 middle, 3 right) as a virtual key, 0 for another */
int ae_forge_keys_vk_of_mouse(int button);
/* a controller button (the platform layer's CONTROLLER_BUTTON_*: A 0 .. right stick 15) as Forge's, 0 out of range */
int ae_forge_keys_gp_of_button(int button);


/* the pad speeds (1.0 = 100 %): 25 % steps from 25 % to 250 %; dir -1 / +1; the value is first taken to the nearest step */
float ae_forge_keys_pad_step(float value, int direction);
int ae_forge_keys_pad_percent(float value);

/* a binding taken from another action (CE: a key or a button is on one action of its column): the line the screen shows */
void ae_forge_keys_taken_text(char const *shown, char const *from_label, char *text, size_t size);

#endif /* __AE_FORGE_DOORS_RULES_H */
