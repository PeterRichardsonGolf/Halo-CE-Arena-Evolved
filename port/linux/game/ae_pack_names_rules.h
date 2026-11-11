/*
AE_PACK_NAMES_RULES.H

port: weapon pack (0.7.6). The pure rules of a pack weapon that has no picture of its own (no licensed picture of it
exists): its pack names no message icon and its own HUD's messaging icon is none (sequence -1, the recipe's edit),
so its NAME, as text, stands where a picture would: the swap and pickup prompts (the game's own text fallback), the
kill feed and the HUD's weapon picture (ae_pack_names.c). No engine includes: port/linux/tests/
ae_pack_names_rules_test.c compiles these alone.
*/

#ifndef __AE_PACK_NAMES_RULES_H
#define __AE_PACK_NAMES_RULES_H

#include <wchar.h>

/* whether a weapon shows its name for a picture: a pack's weapon whose HUD is the pack's own tag and names no
messaging icon (a negative sequence). A map's own weapon, or a pack weapon on the map's HUD, keeps its picture */
int ae_pack_names_no_picture(int pack_weapon, int hud_is_pack_own, int messaging_sequence);

/* an overlay item's "show always" bit (hud_weapon.c's weapon_overlay_on_flags: flashing, empty, reload, default,
always) */
#define AE_PACK_NAMES_OVERLAY_ALWAYS_BIT 4

/* which overlay item of a no-picture pack weapon's HUD is its weapon picture: the first item, in the HUD's order (its
overlay elements in order, each element's items in order), that is always shown. types[e][i] is element e's item i's
type flags, counts[e] its item count. TRUE and *element_out, *item_out set when there is one; FALSE (both -1) when
none is: then nothing is replaced and every overlay is drawn as the game draws it */
int ae_pack_names_picture(short const *const *types, int const *counts, int elements, int *element_out, int *item_out);

/* the name as the HUD writes it: the menu name (UTF-8) in capitals, a byte past ASCII a '?', at most size - 1
characters, always ended; the characters written (0 for none) */
int ae_pack_names_word(char const *menu_name, wchar_t *out, int size);

#endif /* __AE_PACK_NAMES_RULES_H */
