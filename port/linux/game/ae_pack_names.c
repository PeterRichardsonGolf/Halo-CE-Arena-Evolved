/*
AE_PACK_NAMES.C

A pack weapon with no picture of its own: its name as text where its picture would be (ae_pack_names.h).
*/

#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "cache/cache_files.h"
#include "game/game.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_draw.h"
#include "interface/hud_messaging.h"
#include "interface/weapon_hud_interface_definition.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "tag_files/tag_groups.h"

#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_killfeed_rules.h"
#include "ae_pack_format.h"
#include "ae_pack_names.h"
#include "ae_pack_names_rules.h"
#include "ae_weapon_pack.h"

/* hud_draw.c's own: a weapon HUD's overlay element's items (hud_draw_weapon_overlays draws them) */
struct weapon_hud_overlay_item
{
	struct hud_placement_definition placement;
	struct hud_color_definition colors;
	short frame_rate;
	short pad;
	short sequence_index;
	short type;
	long flags;
	long unused[14];
};

struct weapon_hud_overlay_definition
{
	struct tag_reference bitmap;
	struct tag_block items;
};

/* hud_draw.c's hud_weapon_overlay_flags: an item the game found bad; and its anchor bits of a corner */
#define OVERLAY_RUNTIME_INVALID_BIT 1
#define ANCHOR_RIGHT_BIT 0
#define ANCHOR_BOTTOM_BIT 1
/* the name's size: its em in HUD units (480 to the screen's height; a 1080p screen's 22.5 px), its capitals 0.7 of it */
#define NAME_EM_UNITS 10.0f
#define NAME_WORD_SIZE 32

/* the pack weapon (slot, weapon) whose definition this is, or whose HUD is this (hud_index), on the loaded map */
static boolean find_weapon(long weapon_definition_index, long hud_index, short *slot_out, short *weapon_out)
{
	short slot, weapon;

	for (slot = 1; slot <= AE_WEAPON_PACK_SLOTS; slot++)
	{
		if (!ae_weapon_pack_slot_added(slot))
			continue;
		for (weapon = 0; weapon < ae_weapon_pack_weapon_count(slot); weapon++)
		{
			long definition = ae_weapon_pack_weapon_definition(slot, weapon);

			if (definition == NONE)
				continue;
			if ((weapon_definition_index != NONE && definition == weapon_definition_index) ||
				(hud_index != NONE &&
					weapon_definition_get(definition)->weapon.interface_definition.hud_interface.index == hud_index))
			{
				*slot_out = slot;
				*weapon_out = weapon;
				return TRUE;
			}
		}
	}
	return FALSE;
}

/* the name of the pack weapon (slot, weapon) when it has no picture (its own HUD, no messaging icon) */
static boolean no_picture_name(short slot, short weapon, wchar_t *out, int size)
{
	long definition = ae_weapon_pack_weapon_definition(slot, weapon);
	long hud = definition != NONE ? weapon_definition_get(definition)->weapon.interface_definition.hud_interface.index : NONE;
	struct ae_pack_weapon_record const *entry = ae_weapon_pack_weapon_entry(slot, weapon);
	int sequence;

	if (hud == NONE || tag_get_group_tag(hud) != 'wphi' || !entry)
		return FALSE;
	sequence = ((struct weapon_hud_interface_definition *)tag_get('wphi', hud))->messaging_icon.sequence_index;
	if (!ae_pack_names_no_picture(1, ae_weapon_pack_tag_is_own(hud), sequence))
		return FALSE;
	return (boolean)(ae_pack_names_word(ae_weapon_pack_string(slot, entry->menu_name), out, size) > 0);
}

boolean ae_pack_names_text(long weapon_definition_index, wchar_t *out, int size)
{
	short slot, weapon;

	if (out && size > 0)
		out[0] = 0;
	if (!out || size < 1 || weapon_definition_index == NONE ||
		!find_weapon(weapon_definition_index, NONE, &slot, &weapon))
	{
		return FALSE;
	}
	return no_picture_name(slot, weapon, out, size);
}

/* the ink of the HUD's text: AE's ice on HUD STYLE AE (as its kill feed), the HUD's own text colour on Classic */
static void name_color(union real_argb_color *color)
{
	if (ae_hud_style_is_ae())
	{
		struct ae_kf_rgb ice = ae_kf_ice();

		color->alpha = 1.0f;
		color->red = ice.r / 255.0f;
		color->green = ice.g / 255.0f;
		color->blue = ice.b / 255.0f;
		return;
	}
	hud_get_text_color(color);
}

/* the element being drawn now (between begin and end), and the picture of the HUD it is of */
static struct
{
	boolean active;
	short local_player_index;
	long hud_index;
	short element_index;
	int picture_element;
	int picture_item;
	wchar_t name[NAME_WORD_SIZE];
} drawing;

/* the HUD's picture: its overlay elements' item types, read into a small table (the HUD's own, at most this many) */
#define PICTURE_ELEMENTS 8
#define PICTURE_ITEMS 16

static boolean find_picture(long hud_index, int *element_out, int *item_out)
{
	struct weapon_hud_interface_definition *definition = (struct weapon_hud_interface_definition *)tag_get('wphi', hud_index);
	short types[PICTURE_ELEMENTS][PICTURE_ITEMS];
	short const *rows[PICTURE_ELEMENTS];
	int counts[PICTURE_ELEMENTS];
	int elements = definition->overlays.count < PICTURE_ELEMENTS ? (int)definition->overlays.count : PICTURE_ELEMENTS;
	int element, item;

	for (element = 0; element < elements; element++)
	{
		struct weapon_hud_overlays_element_copy
		{
			/* (hud_weapon.c's weapon_hud_overlays_element, as tag_schema_effects.c checks it: 0x68 bytes) */
			short state_type;
			short runtime_flags;
			short use_on_map_type;
			short pad;
			long unused[7];
			struct weapon_hud_overlay_definition overlays;
			long unused2[10];
		} const *row = TAG_BLOCK_GET_ELEMENT(&definition->overlays, element, struct weapon_hud_overlays_element_copy);

		counts[element] = row->overlays.items.count < PICTURE_ITEMS ? (int)row->overlays.items.count : PICTURE_ITEMS;
		for (item = 0; item < counts[element]; item++)
			types[element][item] = TAG_BLOCK_GET_ELEMENT(&row->overlays.items, item, struct weapon_hud_overlay_item)->type;
		rows[element] = types[element];
	}
	return (boolean)ae_pack_names_picture(rows, counts, elements, element_out, item_out);
}

void ae_pack_names_overlays_begin(short local_player_index, long hud_index, short element_index)
{
	short slot, weapon;

	drawing.active = FALSE;
	if (hud_index == NONE || !find_weapon(NONE, hud_index, &slot, &weapon) ||
		!no_picture_name(slot, weapon, drawing.name, NAME_WORD_SIZE))
	{
		return;
	}
	/* (read each time: a few items, and a tag index means another HUD on another map) */
	drawing.hud_index = hud_index;
	if (!find_picture(hud_index, &drawing.picture_element, &drawing.picture_item))
		return;
	drawing.local_player_index = local_player_index;
	drawing.element_index = element_index;
	drawing.active = (boolean)(drawing.picture_element == element_index);
}

void ae_pack_names_overlays_end(void)
{
	drawing.active = FALSE;
}

boolean ae_pack_names_overlay_item(struct hud_absolute_placement_definition const *absolute_placement,
	struct hud_placement_definition const *placement, long item_index, boolean in_multiplayer)
{
	union real_argb_color color;
	point2d point;
	short corner;
	real ppu, px, x, baseline;

	if (!drawing.active || item_index != drawing.picture_item || !absolute_placement || !placement)
		return FALSE;
	ppu = ae_hud_pixels_per_unit();
	px = NAME_EM_UNITS * ppu;
	name_color(&color);
	/* (no bitmap: the point is the picture's anchored corner, its right edge on a right-hand corner, its bottom on a
	bottom one; the name keeps to that corner, so it stays on screen as the picture did) */
	hud_calculate_point(drawing.local_player_index, absolute_placement, placement, NULL, in_multiplayer, 0.0f, &point);
	corner = absolute_placement->corner;
	x = (real)point.x * ppu;
	baseline = (real)point.y * ppu + AE_HUD_TEXT_CAP * px;
	if (corner < _hud_anchor_center && TEST_FLAG(corner, ANCHOR_RIGHT_BIT))
		x -= ae_hud_text_measure(drawing.name, px);
	if (corner < _hud_anchor_center && TEST_FLAG(corner, ANCHOR_BOTTOM_BIT))
		baseline = (real)point.y * ppu;
	ae_hud_text_draw(drawing.local_player_index, x, baseline, drawing.name, px, &color, TRUE);
	return TRUE;
}
