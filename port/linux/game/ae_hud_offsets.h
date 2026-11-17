/*
AE_HUD_OFFSETS.H

DEV MODE's hud command: the weapon HUD's elements moved, scaled or hidden where the game draws them, for placing a weapon
pack's HUD pieces (the numbers and math: ae_hud_offsets_rules.h). The HUD tag is never changed: a weapon's element
(hud_weapon.c's render_weapon_hud draws the statics, meters, numbers and overlays of the weapon's own HUD tag; the elements
count on in that order) has its placement changed for the one draw and put back as it was. The numbers are saved on request in
hud_offsets.txt in the config folder, read at the first draw. The elements are those of the weapon HUD tag: the HUD STYLE
CE draws them; HUD STYLE AE draws its own HUD instead.
*/

#ifndef __AE_HUD_OFFSETS_H
#define __AE_HUD_OFFSETS_H

#include "cseries.h"
#include "ae_hud_offsets_rules.h"

struct hud_placement_definition;
struct tag_reference;

enum
{
	AE_HUD_STATIC = 0,
	AE_HUD_METER,
	AE_HUD_NUMBER,
	AE_HUD_OVERLAY
};

/* (hud_weapon.c, around the render of a held weapon's HUD) the weapon whose HUD is drawn (its definition tag, its HUD tag and the
local player), then none (NONE) */
void ae_hud_offsets_weapon(long weapon_definition_index, long hud_index, short local_player_index);
/* (render_weapon_hud, around the game's draw of one element) the element about to be drawn and done with: kind (above), its
number in the weapon's HUD, its placement (none for an overlay: its items' come through ae_hud_offsets_overlay_item), the
bitmap it names (or none) and the HUD's anchor corner. The element is moved as its numbers say until the end */
void ae_hud_offsets_begin(long hud_index, short kind, short element_index, struct hud_placement_definition *placement,
	struct tag_reference const *bitmap, short corner);
void ae_hud_offsets_end(void);
/* (hud_draw_weapon_overlays, an overlay item about to be drawn) the item's placement moved as the element's numbers say */
void ae_hud_offsets_overlay_item(struct hud_placement_definition const *placement);

/* what the last draw of the first local player's weapon HUD had */
struct ae_hud_seen
{
	int index;
	short kind;
	char bitmap[40];
};
/* the weapon the elements were seen for (its key), "" before any draw; and how many elements, and element i */
void ae_hud_offsets_seen_key(char *key, int size);
int ae_hud_offsets_seen_count(void);
int ae_hud_offsets_seen_get(int i, struct ae_hud_seen *out);
char const *ae_hud_offsets_kind_name(short kind);

/* a weapon's element numbers (1 when the table has some); set (1, or 0 when refused), cleared (an element, or -1: all of the
weapon's), and kept in hud_offsets.txt (1 when written) */
int ae_hud_offsets_get(char const *key, int index, struct ae_hud_element *out);
int ae_hud_offsets_set(char const *key, struct ae_hud_element const *element);
int ae_hud_offsets_clear(char const *key, int index);
int ae_hud_offsets_save(void);

#endif
