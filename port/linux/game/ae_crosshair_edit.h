/*
AE_CROSSHAIR_EDIT.H

CROSSHAIR v3's menu side (ae_crosshair_edit.c): the rows of HUD > RETICLE > EDIT CROSSHAIR and > WEAPONS read and written
as the player's saved crosshairs (display.crosshair_1 to _8), main (display.crosshair) and per-weapon table
(display.crosshair_weapons). The editor's rows have keys "xh.<field>" and the weapons' "xw.<weapon>": ae_screen_settings.c
routes them here instead of to a config setting. Every change is saved to the player's profile at once.
*/

#ifndef __AE_CROSSHAIR_EDIT_H
#define __AE_CROSSHAIR_EDIT_H

#include "ae_crosshair_rules.h"

/* a row key this module owns ("xh." or "xw.") */
int ae_crosshair_edit_owns(const char *key);
/* a row's value as the settings' text, set, and its default; 0 when the key is not one of its rows or the value refused */
int ae_crosshair_edit_get(short player, const char *key, char *text, int size);
int ae_crosshair_edit_set(short player, const char *key, const char *text);
int ae_crosshair_edit_default(const char *key, char *text, int size);
/* what an action row shows ("PRECISE", "TWIN CROSS", "2 OWN, 1 CUSTOM"); 0 when the key has none */
int ae_crosshair_edit_shown(short player, const char *key, char *text, int size);
/* the choices of the CROSSHAIR row and the weapons' rows, as the player's saved crosshairs are now */
void ae_crosshair_edit_lists(short player);

/* which saved crosshair the editor works on: the main one, else a free slot (a new one, not saved until a row changes), else
the first. Returns the slot 0 to 7 */
int ae_crosshair_edit_open(short player);
int ae_crosshair_edit_slot(short player);
/* the editor's crosshair (a new one when its slot is free); whether it is saved */
int ae_crosshair_edit_current(short player, struct ae_crosshair *xh);
/* the crosshair the preview shows for a weapon key (NULL key: the main one): 0 when it is the weapon's own reticle */
int ae_crosshair_edit_for_weapon(short player, const char *key, struct ae_crosshair *xh);
/* the player's saved crosshairs in use */
int ae_crosshair_edit_saved(short player);

/* the NAME row, the SHAPE picker's pick (1 to the library's count), DELETE (the main and any weapon using it fall back) */
int ae_crosshair_edit_name_set(short player, const char *typed);
int ae_crosshair_edit_shape_set(short player, int shape);
int ae_crosshair_edit_delete(short player);

/* the editor's whole slot as the profile keeps it (name, shape and every field; "" when the slot is free), and put back as it
was: UNDO and the changed count see NAME and SHAPE as well as the rows */
int ae_crosshair_edit_slot_text(short player, char *text, int size);
int ae_crosshair_edit_slot_restore(short player, const char *text);

/* the share code of the editor's crosshair; a pasted code: the result (AE_XH_CODE_), the crosshair it holds when OK, applied
to the editor's slot (name kept) or saved as a new one (the slot 0 to 7, or -1 when all 8 are used) */
void ae_crosshair_edit_code(short player, char *text, int size);
int ae_crosshair_edit_code_read(const char *text, struct ae_crosshair *xh, int *unknown_shape);
int ae_crosshair_edit_code_apply(short player, const struct ae_crosshair *xh);
int ae_crosshair_edit_code_save_new(short player, const struct ae_crosshair *xh);
/* ---------- (0.7.7) the CROSSHAIR screen's saved strip (ae_screen_crosshair.c) */
/* a slot's saved crosshair (1), or 0 when the slot is free */
int ae_crosshair_edit_slot_saved(short player, int slot, struct ae_crosshair *xh);
/* the main crosshair's slot (0 to 7), -1 for STOCK */
int ae_crosshair_edit_main(short player);
/* the editor works on this slot from now on (saved or free) */
int ae_crosshair_edit_select(short player, int slot);
/* NEW: a fresh default crosshair ("CROSSHAIR n") saved in the first free slot and edited; DUPLICATE: the edited one copied
there as "<NAME> 2" (the next number free among the saved names). The slot, or -1 when all 8 are used (or, DUPLICATE,
the edited one is not saved) */
int ae_crosshair_edit_new(short player);
int ae_crosshair_edit_duplicate(short player);
/* "OCTA" -> "OCTA 2", "OCTA 2" -> "OCTA 3", cut to fit 12 characters: the name a copy takes, taken: the names in use */
void ae_crosshair_edit_copy_name(const char *name, const char *const *taken, int taken_count, char *out, int size);
/* how many weapon rows name the slot (their keys in keys, up to maximum) */
int ae_crosshair_edit_weapons_using(short player, int slot, char (*keys)[AE_XH_WEAPON_KEY], int maximum);

/* the whole of a player's crosshairs as the profile keeps them (the 8 slots, the main one, the weapons' table): the screen
takes it as it opens, compares it for its changes and puts it back for UNDO */
struct ae_crosshair_edit_profile { char slot[AE_XH_SLOTS][192]; char main[16]; char weapons[1024]; };
void ae_crosshair_edit_profile_take(short player, struct ae_crosshair_edit_profile *profile);
int ae_crosshair_edit_profile_same(const struct ae_crosshair_edit_profile *a, const struct ae_crosshair_edit_profile *b);
int ae_crosshair_edit_profile_restore(short player, const struct ae_crosshair_edit_profile *profile);

/* a line about a crosshair: "TWIN CROSS, WHITE, INNER + OUTER LINES, OUTLINE" */
void ae_crosshair_edit_summary(const struct ae_crosshair *xh, char *text, int size);

#endif
