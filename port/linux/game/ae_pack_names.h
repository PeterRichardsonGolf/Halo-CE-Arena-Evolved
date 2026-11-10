/*
AE_PACK_NAMES.H

port: weapon pack (0.7.6). A pack weapon with no picture of its own shows its NAME as text where its picture would
be (ae_pack_names_rules.h says which weapons): the kill feed's weapon (ae_killfeed.c) and the HUD's weapon picture
(the HUD's first always-shown overlay item; hooks in hud_weapon.c and hud_draw.c). Every other overlay item is drawn
as the game draws it. The swap and pickup prompts need nothing here: HUD STYLE AE's
prompt writes the weapon's name, and the game's own prompt writes its text when the HUD names no messaging icon.

A pack opts in with its recipe: no "icon" in its "pack" and its own HUD's messaging_icon sequence set to -1
(docs/weapon-packs.md). No pack format change.

Include after cseries.h.
*/

#ifndef __AE_PACK_NAMES_H
#define __AE_PACK_NAMES_H

#include <wchar.h>

struct hud_absolute_placement_definition;
struct hud_placement_definition;

/* TRUE and the weapon's name (capitals) written when the weapon tag is a pack weapon with no picture; FALSE (nothing
written but an empty word) for any other */
boolean ae_pack_names_text(long weapon_definition_index, wchar_t *out, int size);

/* (render_weapon_hud, around the game's drawing of a HUD's overlay element) the element about to be drawn and done
with: for a no-picture pack weapon's own HUD, the element that holds its weapon picture (ae_pack_names_picture: the
HUD's first always-shown overlay item) is marked */
void ae_pack_names_overlays_begin(short local_player_index, long hud_index, short element_index);
void ae_pack_names_overlays_end(void);

/* (hud_draw_weapon_overlays, an item about to be drawn) TRUE when the item is the marked element's weapon picture: the
weapon's name is drawn in its place and the game draws nothing of it; FALSE (nothing drawn) for every other item, which
the game draws as it is */
boolean ae_pack_names_overlay_item(struct hud_absolute_placement_definition const *absolute_placement,
	struct hud_placement_definition const *placement, long item_index, boolean in_multiplayer);

#endif /* __AE_PACK_NAMES_H */
