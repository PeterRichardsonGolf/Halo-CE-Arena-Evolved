/*
AE_DEV_WHEEL.C

DEV MODE's WEAPON WHEEL (ae_dev_wheel.h). The pointer is the look's turn added up on a disc (the stick held to a
side runs it to the edge there; the mouse moves it as a cursor), so a stick and a mouse point the same way; the
slice it is in is the pick (ae_dev_wheel_rules.c: a pick sticks until the pointer clearly enters another slice; the
dead zone in the middle takes nothing).

The list is read when the wheel opens: every weapon tag in the map but its objectives' (the flag, the ball) and the
vehicles' own guns (the weapons a vehicle tag starts with), by the tag's last name (two of one name show their folder
too), up to 96, on pages of at most 8: HUMAN, COVENANT and the rest (AE / OTHER), a group of more than 8 on several.
LB / RB (the pad), the flashlight and action keys (Q / E) or the mouse wheel flip the pages. The pick is made in the next
game tick of the player (players.c), as the map's own weapons are made, with the weapon set and the map's remaps off.

HALO_DEV_WHEEL_SHOT="<page>,<x>,<y>" (tests, with DEV MODE only): the wheel opens at that page (counting from 0) with the
pointer there (the disc's, -1 to 1) and stays open after the button is let go of, for a screenshot.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cache/cache_files.h"
#include "game/players.h"
#include "input/input.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/objects.h"
#include "render/render.h"
#include "units/unit_definitions.h"
#include "units/units.h"
#include "units/vehicle_definitions.h"

#include "../include/ae_seats.h"
#include "../include/halo_keyboard.h"
#include "ae_dev_mode.h"
#include "ae_dev_wheel.h"
#include "ae_dev_wheel_rules.h"
#include "ae_hud_digits.h"
#include "ae_hud_text.h"
#include "ae_key_prompts.h"
#include "ae_style.h"
#include "../src/ae_platform.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void platform_log(char const *format, ...);

#define WHEEL_WEAPONS 96
#define WHEEL_NAME 40
#define WHEEL_PAGES 16
/* the hold that opens it (a tap is SWITCH WEAPON's own) */
#define WHEEL_OPEN_SECONDS 0.25f
/* the pointer's disc (radians of turn) */
#define WHEEL_DISC 0.20f
/* the names' size at k = 1 (they step down to fit their slice, never under the floor) */
#define WHEEL_TEXT 24.0f
#define WHEEL_TEXT_FLOOR 13.0f

static struct
{
	boolean held;
	boolean open;
	boolean opened_this_hold;
	real held_seconds;
	real x, y;
	short count;
	short picked;
	long definitions[WHEEL_WEAPONS];
	wchar_t names[WHEEL_WEAPONS][WHEEL_NAME];
	/* the pages: where each starts in the list (grouped by page group), its size, its group */
	short pages, page;
	short page_first[WHEEL_PAGES], page_size[WHEEL_PAGES], page_group[WHEEL_PAGES];
	/* the page buttons as last seen (a press flips once) */
	unsigned char down[4];
	boolean has_pending;
	long pending;
} wheels[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

static boolean giving;
/* the mouse wheel's turns since the last read (xinput_sdl.c's event) */
static volatile float mouse_turns;
/* HALO_DEV_WHEEL_SHOT, read once: -2 not yet, -1 none */
static int shot_page = -2;
static float shot_x, shot_y;

void ae_dev_wheel_note_turns(float turns)
{
	mouse_turns += turns;
}

static boolean shot_wanted(void)
{
	if (shot_page == -2)
	{
		char const *text = getenv("HALO_DEV_WHEEL_SHOT");

		shot_page = -1;
		if (text && sscanf(text, "%d,%f,%f", &shot_page, &shot_x, &shot_y) != 3)
			shot_page = -1;
	}
	return shot_page >= 0;
}

/* the tag's name from its last backslash; with folder, from the one before it */
static void name_of(long definition_index, boolean folder, wchar_t *out)
{
	char const *name = tag_get_name(definition_index);
	char const *start = name;
	char const *last = NULL, *before = NULL, *at;
	int length = 0;

	if (!name)
		name = start = "?";
	for (at = name; *at; at++)
	{
		if (*at == '\\')
		{
			before = last;
			last = at;
		}
	}
	if (folder && before)
		start = before + 1;
	else if (last)
		start = last + 1;
	for (at = start; *at && length < WHEEL_NAME - 1; at++)
	{
		char c = *at == '_' ? ' ' : *at == '\\' ? '/' : *at;

		out[length++] = (wchar_t)(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
	}
	out[length] = 0;
}

/* (the build's wchar_t is two bytes: no wcscmp) */
static boolean same_name(wchar_t const *a, wchar_t const *b)
{
	while (*a && *a == *b)
	{
		a++;
		b++;
	}
	return *a == *b;
}

static boolean objective(char const *name)
{
	char const *last = name ? strrchr(name, '\\') : NULL;

	last = last ? last + 1 : name;
	return !last || !strcmp(last, "flag") || !strcmp(last, "ball") || strstr(last, "oddball") != NULL;
}

/* the weapons a vehicle starts with (its guns: the warthog's, the scorpion's, the ghost's): no one carries them */
#define WHEEL_VEHICLE_GUNS 64

/* (units.c's unit_initial_weapon, which is private there: a reference and 5 longs, 0x24) */
struct wheel_initial_weapon
{
	struct tag_reference weapon;
	long unused[5];
};

static short vehicle_guns(long *guns)
{
	struct tag_iterator iterator;
	long tag_index;
	short count = 0;

	tag_iterator_new(&iterator, VEHICLE_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		struct unit_definition *definition = vehicle_definition_get(tag_index);
		short index;

		for (index = 0; index < definition->unit.initial_weapons.count; index++)
		{
			struct wheel_initial_weapon *initial = TAG_BLOCK_GET_ELEMENT(&definition->unit.initial_weapons, index,
				struct wheel_initial_weapon);

			if (initial->weapon.index != NONE && count < WHEEL_VEHICLE_GUNS)
				guns[count++] = initial->weapon.index;
		}
	}
	return count;
}

static boolean one_of(long const *list, short count, long value)
{
	short index;

	for (index = 0; index < count; index++)
	{
		if (list[index] == value)
			return TRUE;
	}
	return FALSE;
}

static void wheel_fill(short local_player_index)
{
	struct tag_iterator iterator;
	long tag_index, guns[WHEEL_VEHICLE_GUNS], found[WHEEL_WEAPONS];
	short gun_count = vehicle_guns(guns), count = 0, index, other, group, pages, page;
	long all = 0;
	short offset = 0;

	tag_iterator_new(&iterator, WEAPON_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		if (objective(tag_get_name(tag_index)) || one_of(guns, gun_count, tag_index))
			continue;
		all++;
		if (count < WHEEL_WEAPONS)
			found[count++] = tag_index;
	}
	if (all > count)
		platform_log("dev wheel: the map has %ld weapons, the wheel shows the first %d", all, (int)count);
	wheels[local_player_index].count = count;
	/* (grouped: human, covenant, the rest, each in the map's order; a group's pages of at most 8) */
	wheels[local_player_index].pages = 0;
	for (group = 0; group < AE_DEV_WHEEL_CATEGORIES; group++)
	{
		short in_group = 0;

		for (index = 0; index < count; index++)
		{
			if (ae_dev_wheel_category(tag_get_name(found[index])) == group)
				wheels[local_player_index].definitions[offset + in_group++] = found[index];
		}
		pages = ae_dev_wheel_pages(in_group);
		for (page = 0; page < pages && wheels[local_player_index].pages < WHEEL_PAGES; page++)
		{
			short at = wheels[local_player_index].pages++, first = 0, before;

			for (before = 0; before < page; before++)
				first = (short)(first + ae_dev_wheel_page_size(in_group, before));
			wheels[local_player_index].page_first[at] = (short)(offset + first);
			wheels[local_player_index].page_size[at] = ae_dev_wheel_page_size(in_group, page);
			wheels[local_player_index].page_group[at] = group;
		}
		offset = (short)(offset + in_group);
	}
	for (index = 0; index < count; index++)
	{
		boolean twin = FALSE;
		wchar_t mine[WHEEL_NAME], theirs[WHEEL_NAME];

		name_of(wheels[local_player_index].definitions[index], FALSE, mine);
		for (other = 0; other < count && !twin; other++)
		{
			if (other == index)
				continue;
			name_of(wheels[local_player_index].definitions[other], FALSE, theirs);
			twin = same_name(mine, theirs);
		}
		name_of(wheels[local_player_index].definitions[index], twin, wheels[local_player_index].names[index]);
	}
	wheels[local_player_index].page = 0;
	platform_log("dev wheel: open, %d weapons on %d pages (%d vehicle guns left out)", (int)count,
		(int)wheels[local_player_index].pages, (int)gun_count);
}

/* the slice of the page the pointer is in (sticky), as the list's index of its weapon; NONE inside the dead zone */
static short wheel_pick(short local_player_index)
{
	short page = wheels[local_player_index].page, slice;

	if (wheels[local_player_index].pages <= 0)
		return NONE;
	slice = ae_dev_wheel_pick(wheels[local_player_index].page_size[page], wheels[local_player_index].x / WHEEL_DISC,
		wheels[local_player_index].y / WHEEL_DISC, wheels[local_player_index].picked >= 0 ?
		(short)(wheels[local_player_index].picked - wheels[local_player_index].page_first[page]) : -1);
	return slice < 0 ? NONE : (short)(wheels[local_player_index].page_first[page] + slice);
}

/* the page buttons: LB / RB on the pad, the flashlight and action keys (Q / E) on the keyboard, the mouse wheel */
static void wheel_pages_flip(short local_player_index, boolean opening)
{
	struct gamepad_state const *pad = input_get_gamepad_state(local_player_index);
	unsigned long long keys = 0;
	unsigned char now[4];
	float turns = mouse_turns;
	short flip = 0, index;

	if (local_player_index == ae_key_prompts_keyboard_player())
		keys = halo_keyboard_actions((short)ae_seats_keyboard_controller());
	now[0] = (unsigned char)(pad && pad->buttons[_gamepad_analog_button_white] != 0);
	now[1] = (unsigned char)(pad && pad->buttons[_gamepad_analog_button_black] != 0);
	now[2] = (unsigned char)((keys & (1ULL << HALO_KEYBOARD_FLASHLIGHT)) != 0);
	now[3] = (unsigned char)((keys & (1ULL << HALO_KEYBOARD_ACTION)) != 0);
	mouse_turns = 0.0f;
	if (!opening)
	{
		for (index = 0; index < 4; index++)
		{
			if (now[index] && !wheels[local_player_index].down[index])
				flip = (short)(flip + (index % 2 ? 1 : -1));
		}
		/* (a notch of the wheel away from the user is up: the page before) */
		if (turns >= 0.5f)
			flip = (short)(flip - 1);
		else if (turns <= -0.5f)
			flip = (short)(flip + 1);
	}
	for (index = 0; index < 4; index++)
		wheels[local_player_index].down[index] = now[index];
	if (flip && wheels[local_player_index].pages > 1)
	{
		short pages = wheels[local_player_index].pages;

		wheels[local_player_index].page = (short)(((wheels[local_player_index].page + (flip > 0 ? 1 : -1)) % pages + pages) % pages);
		wheels[local_player_index].picked = NONE;
	}
}


void ae_dev_wheel_note_button(short local_player_index, int held, unsigned long *player_flags, unsigned long rotate_bit)
{
	boolean was;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return;
	was = wheels[local_player_index].held;
	wheels[local_player_index].held = held != 0;
	if (!ae_dev_mode_on())
		return;
	/* (the switch on the press waits: a tap switches as it is let go of, a hold that opened the wheel never) */
	*player_flags &= ~rotate_bit;
	if (held && !was)
		wheels[local_player_index].opened_this_hold = FALSE;
	else if (!held && was && !wheels[local_player_index].opened_this_hold && !wheels[local_player_index].open)
		*player_flags |= rotate_bit;
}


void ae_dev_wheel_input(short local_player_index, real time_delta_sec, real *yaw, real *pitch, unsigned long *unit_flags)
{
	long player_index;
	struct player_datum *player;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return;
	player_index = local_player_get_player_index(local_player_index);
	player = player_index != NONE ? player_get(player_index) : NULL;
	if (!ae_dev_mode_on() || !player || player->unit_index == NONE ||
		object_get(player->unit_index)->object.parent_object_index != NONE)
	{
		wheels[local_player_index].open = FALSE;
		wheels[local_player_index].held_seconds = 0.0f;
		return;
	}
	if (wheels[local_player_index].held)
	{
		wheels[local_player_index].held_seconds += time_delta_sec;
		if (!wheels[local_player_index].open && wheels[local_player_index].held_seconds >= (shot_wanted() ? 0.05f : WHEEL_OPEN_SECONDS))
		{
			wheels[local_player_index].open = TRUE;
			wheels[local_player_index].opened_this_hold = TRUE;
			wheels[local_player_index].x = wheels[local_player_index].y = 0.0f;
			wheels[local_player_index].picked = NONE;
			wheel_fill(local_player_index);
			wheel_pages_flip(local_player_index, TRUE);
			if (shot_wanted() && shot_page < wheels[local_player_index].pages)
				wheels[local_player_index].page = (short)shot_page;
		}
	}
	else
	{
		if (wheels[local_player_index].open && !shot_wanted())
		{
			short picked = wheel_pick(local_player_index);

			wheels[local_player_index].open = FALSE;
			if (picked != NONE)
			{
				wheels[local_player_index].pending = wheels[local_player_index].definitions[picked];
				wheels[local_player_index].has_pending = TRUE;
			}
			platform_log("dev wheel: let go, %s", picked != NONE ? tag_get_name(wheels[local_player_index].definitions[picked]) :
				"nothing picked");
		}
		wheels[local_player_index].held_seconds = 0.0f;
	}
	if (!wheels[local_player_index].open)
		return;
	wheel_pages_flip(local_player_index, FALSE);
	{
		real x = wheels[local_player_index].x - *yaw, y = wheels[local_player_index].y + *pitch;
		real length = (real)sqrt(x * x + y * y);

		if (length > WHEEL_DISC)
		{
			x *= WHEEL_DISC / length;
			y *= WHEEL_DISC / length;
		}
		if (shot_wanted())
		{
			x = shot_x * WHEEL_DISC;
			y = shot_y * WHEEL_DISC;
		}
		wheels[local_player_index].x = x;
		wheels[local_player_index].y = y;
		wheels[local_player_index].picked = wheel_pick(local_player_index);
	}
	*yaw = 0.0f;
	*pitch = 0.0f;
	*unit_flags &= ~(FLAG(_unit_control_weapon_primary_trigger_bit) | FLAG(_unit_control_weapon_secondary_trigger_bit) |
		FLAG(_unit_control_throw_grenade_bit) | FLAG(_unit_control_use_equipment_bit) | FLAG(_unit_control_action_bit) |
		FLAG(_unit_control_weapon_reload_bit) | FLAG(_unit_control_swap_weapons_bit) |
		FLAG(_unit_control_integrated_light_bit));
}

boolean ae_dev_wheel_giving(void)
{
	return giving;
}

void ae_dev_wheel_apply(long player_index)
{
	struct player_datum *player = player_get(player_index);
	short local_player_index = player->local_player_index;
	long definition_index, unit_index = player->unit_index, weapon_index;
	struct object_placement_data placement_data;
	struct unit_datum *unit;
	short slot, held = 0;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS ||
		!wheels[local_player_index].has_pending)
	{
		return;
	}
	definition_index = wheels[local_player_index].pending;
	wheels[local_player_index].has_pending = FALSE;
	if (!ae_dev_mode_on() || unit_index == NONE || definition_index == NONE)
		return;
	unit = unit_get(unit_index);
	if (unit->object.parent_object_index != NONE || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return;
	/* (two held: the one in hand goes to the floor for it) */
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
		held += unit->unit.weapon_object_indices[slot] != NONE;
	if (held >= 2)
		unit_drop_current_weapon(unit_index, TRUE);
	object_placement_data_new(&placement_data, definition_index, NONE);
	placement_data.position = unit->object.position;
	giving = TRUE;
	weapon_index = object_new(&placement_data);
	giving = FALSE;
	if (weapon_index == NONE)
	{
		platform_log("dev wheel: %s could not be made", tag_get_name(definition_index));
		return;
	}
	if (!unit_add_weapon_to_inventory(unit_index, weapon_index, _unit_add_weapon_starting))
	{
		object_delete(weapon_index);
		platform_log("dev wheel: %s could not be held", tag_get_name(definition_index));
		return;
	}
	platform_log("dev wheel: gave %s", tag_get_name(definition_index));
}

/* ---------- the drawing: shapes in the HUD pass (screen pixels of the view, mockups wheel-pause) */

#define WHEEL_SEGMENTS 64

static real_argb_color shade(unsigned int rgba, real scale)
{
	real_argb_color color;

	color.alpha = (real)(rgba & 0xFFu) / 255.0f * scale;
	color.red = (real)((rgba >> 24) & 0xFFu) / 255.0f;
	color.green = (real)((rgba >> 16) & 0xFFu) / 255.0f;
	color.blue = (real)((rgba >> 8) & 0xFFu) / 255.0f;
	return color;
}

static void point_at(real cx, real cy, real radius, real angle, real *x, real *y)
{
	*x = cx + radius * (real)sin(angle);
	*y = cy - radius * (real)cos(angle);
}

/* a ring's part between two radii and two angles (clockwise from the top), its radial edges drawn in by gap / 2 across
(constant width) when gap is above 0; inner 0 is a fan */
static void sector(real cx, real cy, real inner, real outer, real a0, real a1, real gap, short segments,
	real_argb_color const *color)
{
	short step;
	real inset_in = gap > 0.0f && inner > gap / 2.0f ? (real)asin(gap / 2.0f / inner) : 0.0f;
	real inset_out = gap > 0.0f ? (real)asin(gap / 2.0f / outer) : 0.0f;

	for (step = 0; step < segments; step++)
	{
		real t0 = (real)step / segments, t1 = (real)(step + 1) / segments;
		real quad[4][2];

		point_at(cx, cy, outer, a0 + inset_out + (a1 - a0 - 2.0f * inset_out) * t0, &quad[0][0], &quad[0][1]);
		point_at(cx, cy, outer, a0 + inset_out + (a1 - a0 - 2.0f * inset_out) * t1, &quad[1][0], &quad[1][1]);
		point_at(cx, cy, inner, a0 + inset_in + (a1 - a0 - 2.0f * inset_in) * t1, &quad[2][0], &quad[2][1]);
		point_at(cx, cy, inner, a0 + inset_in + (a1 - a0 - 2.0f * inset_in) * t0, &quad[3][0], &quad[3][1]);
		ae_hud_fill_quad_color((real const (*)[2])quad, color);
	}
}

static void box(real x0, real y0, real x1, real y1, real_argb_color const *color)
{
	real quad[4][2] = { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };

	ae_hud_fill_quad_color((real const (*)[2])quad, color);
}

/* a round chip: the half discs at the ends of a bar; outline > 0: its rim only, that wide */
static void pill(real x0, real y0, real x1, real y1, real outline, real_argb_color const *color)
{
	real radius = (y1 - y0) / 2.0f, cy = (y0 + y1) / 2.0f;
	real pi = 3.14159265f;

	if (outline <= 0.0f)
	{
		box(x0 + radius, y0, x1 - radius, y1, color);
		sector(x0 + radius, cy, 0.0f, radius, pi, 2.0f * pi, 0.0f, WHEEL_SEGMENTS / 4, color);
		sector(x1 - radius, cy, 0.0f, radius, 0.0f, pi, 0.0f, WHEEL_SEGMENTS / 4, color);
		return;
	}
	box(x0 + radius, y0, x1 - radius, y0 + outline, color);
	box(x0 + radius, y1 - outline, x1 - radius, y1, color);
	sector(x0 + radius, cy, radius - outline, radius, pi, 2.0f * pi, 0.0f, WHEEL_SEGMENTS / 4, color);
	sector(x1 - radius, cy, radius - outline, radius, 0.0f, pi, 0.0f, WHEEL_SEGMENTS / 4, color);
}

static void widen(char const *text, wchar_t *out, short size)
{
	short index;

	for (index = 0; text[index] && index < size - 1; index++)
		out[index] = (wchar_t)text[index];
	out[index] = 0;
}

/* a name set on one or two lines inside its slice: the largest size (down to the floor) whose text box keeps its
margin from the slice's edges; lines is 1 or 2 (the break at the space nearest the middle) */
struct wheel_text
{
	wchar_t line[2][WHEEL_NAME];
	short lines;
	real px, x, y;
};

static void wheel_text_fit(real cx, real cy, real k, short n, short slice, wchar_t const *name, struct wheel_text *out)
{
	static real const sizes[] = { WHEEL_TEXT, 22.0f, 20.0f, 18.0f };
	real mid = (real)(2.0 * M_PI) * slice / n, ring = (AE_DEV_WHEEL_IN + AE_DEV_WHEEL_OUT) / 2.0f * k;
	real bx = (real)sin(mid) * ring, by = -(real)cos(mid) * ring;
	short size, variant, length = 0, split = -1, at;

	while (name[length])
		length++;
	/* (the break: the space nearest the middle) */
	for (at = 0; at < length; at++)
	{
		if (name[at] == L' ' && (split < 0 || abs(at - length / 2) < abs(split - length / 2)))
			split = at;
	}
	for (size = 0; size < 4; size++)
	{
		real px = sizes[size] * k < WHEEL_TEXT_FLOOR ? WHEEL_TEXT_FLOOR : sizes[size] * k;

		for (variant = 0; variant < (split >= 0 ? 2 : 1); variant++)
		{
			real width, height, lh = px * 1.12f;
			wchar_t first[WHEEL_NAME], second[WHEEL_NAME];
			short lines = (short)(variant + 1);
			boolean fits = TRUE;
			real margin = AE_DEV_WHEEL_MARGIN * k, dx, dy;

			if (variant == 0)
			{
				memcpy(first, name, (size_t)(length + 1) * sizeof(wchar_t));
				second[0] = 0;
				width = ae_hud_text_measure(first, px);
			}
			else
			{
				real w2;

				memcpy(first, name, (size_t)split * sizeof(wchar_t));
				first[split] = 0;
				memcpy(second, name + split + 1, (size_t)(length - split) * sizeof(wchar_t));
				width = ae_hud_text_measure(first, px);
				w2 = ae_hud_text_measure(second, px);
				if (w2 > width)
					width = w2;
			}
			height = lh * lines;
			for (dx = -1.0f; dx <= 1.0f && fits; dx += 1.0f)
			{
				for (dy = -1.0f; dy <= 1.0f && fits; dy += 1.0f)
				{
					if (ae_dev_wheel_margin(k, n, slice, bx + dx * width / 2.0f, by + dy * height / 2.0f) < margin)
						fits = FALSE;
				}
			}
			if (fits || (size == 3 && variant == (split >= 0 ? 1 : 0)))
			{
				memcpy(out->line[0], first, sizeof(first[0]) * WHEEL_NAME);
				memcpy(out->line[1], second, sizeof(second[0]) * WHEEL_NAME);
				out->lines = lines;
				out->px = px;
				out->x = cx + bx;
				out->y = cy + by - height / 2.0f;
				return;
			}
		}
	}
}

void ae_dev_wheel_draw(short local_player_index)
{
	real_argb_color back, rim, slice_color, pick_color, accent, dead, ink, dark, ice, pill_fill, pill_line, cap, hint_color;
	real ppu, width, height, k, cx, cy, tab_h, pi = 3.14159265f;
	short index, count, page, first, n;
	boolean keyboard_scheme;
	static char const *const tabs[AE_DEV_WHEEL_CATEGORIES] = { "HUMAN", "COVENANT", "AE / OTHER" };

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS ||
		!wheels[local_player_index].open)
	{
		return;
	}
	count = wheels[local_player_index].count;
	ppu = ae_hud_pixels_per_unit();
	width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu;
	height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu;
	k = ae_dev_wheel_scale(height);
	cx = width / 2.0f;
	cy = height / 2.0f + (k >= 0.99f ? -20.0f : 20.0f);
	back = shade(0x0A0E12A8u, 1.0f);
	rim = shade(AE_COLOR_RULE, 1.0f);
	slice_color = shade(0x202830EEu, 1.0f);
	pick_color = shade(AE_COLOR_SELECTION, 1.0f);
	accent = shade(AE_COLOR_ACCENT, 1.0f);
	dead = shade(0x0E1318EBu, 1.0f);
	ice = shade(0xB8D6FFFFu, 1.0f);
	dark = shade(0x0A0E12E6u, 1.0f);
	pill_fill = shade(0x0A0E1296u, 1.0f);
	pill_line = shade(0xC8D6E050u, 1.0f);
	cap = shade(AE_COLOR_KEY_CAP, 1.0f);
	ink = shade(AE_COLOR_TITLE, 1.0f);
	hint_color = shade(AE_COLOR_TITLE, 1.0f);
	keyboard_scheme = local_player_index == ae_key_prompts_keyboard_player() && ae_platform_keyboard_last_used();
	page = wheels[local_player_index].page;
	n = count > 0 && wheels[local_player_index].pages > 0 ? wheels[local_player_index].page_size[page] : 0;
	first = n > 0 ? wheels[local_player_index].page_first[page] : 0;

	/* the backing disc, its rim, the slices (the pick's plate with its accent rim), the dead zone */
	ae_hud_quads_begin();
	sector(cx, cy, 0.0f, AE_DEV_WHEEL_BACK * k, 0.0f, 2.0f * pi, 0.0f, WHEEL_SEGMENTS, &back);
	sector(cx, cy, AE_DEV_WHEEL_BACK * k - 2.0f, AE_DEV_WHEEL_BACK * k, 0.0f, 2.0f * pi, 0.0f, WHEEL_SEGMENTS, &rim);
	for (index = 0; index < n; index++)
	{
		real a0 = 2.0f * pi * ((real)index - 0.5f) / n, a1 = 2.0f * pi * ((real)index + 0.5f) / n;
		boolean on = first + index == wheels[local_player_index].picked;
		short segments = (short)(n == 1 ? WHEEL_SEGMENTS : WHEEL_SEGMENTS / n + 4);

		sector(cx, cy, AE_DEV_WHEEL_IN * k, AE_DEV_WHEEL_OUT * k, a0, a1, n == 1 ? 0.0f : AE_DEV_WHEEL_GAP, segments,
			on ? &pick_color : &slice_color);
		if (on)
			sector(cx, cy, (AE_DEV_WHEEL_OUT - 5.0f) * k, AE_DEV_WHEEL_OUT * k, a0, a1, n == 1 ? 0.0f : AE_DEV_WHEEL_GAP,
				segments, &accent);
	}
	sector(cx, cy, 0.0f, AE_DEV_WHEEL_DEAD * k, 0.0f, 2.0f * pi, 0.0f, WHEEL_SEGMENTS, &dead);
	sector(cx, cy, AE_DEV_WHEEL_DEAD * k - 1.0f, AE_DEV_WHEEL_DEAD * k, 0.0f, 2.0f * pi, 0.0f, WHEEL_SEGMENTS, &rim);
	ae_hud_quads_end();

	if (n <= 0)
	{
		static wchar_t const none[] = L"NO WEAPONS";
		real px = (WHEEL_TEXT < WHEEL_TEXT * k ? WHEEL_TEXT : WHEEL_TEXT * k);

		ae_hud_text_draw(local_player_index, cx - ae_hud_text_measure(none, px) / 2.0f, cy + px * AE_HUD_TEXT_CAP / 2.0f, none,
			px, &ink, TRUE);
		return;
	}
	/* the names */
	for (index = 0; index < n; index++)
	{
		struct wheel_text text;
		boolean on = first + index == wheels[local_player_index].picked;
		real_argb_color selected = shade(AE_COLOR_SELECTION_TEXT, 1.0f), *color = on ? &selected : &ink;
		short line;

		wheel_text_fit(cx, cy, k, n, index, wheels[local_player_index].names[first + index], &text);
		for (line = 0; line < text.lines; line++)
		{
			real lh = text.px * 1.12f;
			real w = ae_hud_text_measure(text.line[line], text.px);

			ae_hud_text_draw(local_player_index, text.x - w / 2.0f, text.y + lh * line + lh * 0.5f + text.px * AE_HUD_TEXT_CAP / 2.0f,
				text.line[line], text.px, color, !on);
		}
	}
	{
		static wchar_t const cancel[] = L"CANCEL";
		real px = WHEEL_TEXT_FLOOR > 17.0f * k ? WHEEL_TEXT_FLOOR : 17.0f * k;
		real_argb_color muted = shade(AE_COLOR_MUTED, 1.0f);

		ae_hud_text_draw(local_player_index, cx - ae_hud_text_measure(cancel, px) / 2.0f, cy + px * AE_HUD_TEXT_CAP / 2.0f, cancel,
			px, &muted, FALSE);
	}
	/* the pointer */
	ae_hud_quads_begin();
	{
		real px = cx + wheels[local_player_index].x / WHEEL_DISC * AE_DEV_WHEEL_OUT * k;
		real py = cy - wheels[local_player_index].y / WHEEL_DISC * AE_DEV_WHEEL_OUT * k;
		real r = 7.0f * k < 6.0f ? 6.0f : 7.0f * k;

		sector(px, py, 0.0f, r + 2.0f, 0.0f, 2.0f * pi, 0.0f, 16, &dark);
		sector(px, py, 0.0f, r, 0.0f, 2.0f * pi, 0.0f, 16, &ice);
	}
	/* the page tabs: the groups that have weapons, the shown page's lit (several pages of a group: its "1 / 2"), the
	buttons that flip them at the ends */
	tab_h = 40.0f * k;
	{
		short group, shown[AE_DEV_WHEEL_CATEGORIES], tab_count = 0, in_group, at;
		real px = WHEEL_TEXT_FLOOR > 20.0f * k ? WHEEL_TEXT_FLOOR : 20.0f * k, x, ty, total = 0.0f, gap = 8.0f * k;
		real widths[AE_DEV_WHEEL_CATEGORIES], cap_w, cap_h = 28.0f * k < 22.0f ? 22.0f : 28.0f * k;
		wchar_t labels[AE_DEV_WHEEL_CATEGORIES][24], left[3], right[3];
		short active = wheels[local_player_index].page_group[page];

		for (group = 0; group < AE_DEV_WHEEL_CATEGORIES; group++)
		{
			in_group = 0;
			for (at = 0; at < wheels[local_player_index].pages; at++)
				in_group = (short)(in_group + (wheels[local_player_index].page_group[at] == group));
			if (!in_group)
				continue;
			widen(tabs[group], labels[tab_count], 24);
			if (group == active && in_group > 1)
			{
				short before = 0, len = 0;

				for (at = 0; at < page; at++)
					before = (short)(before + (wheels[local_player_index].page_group[at] == group));
				while (labels[tab_count][len])
					len++;
				labels[tab_count][len++] = L' ';
				labels[tab_count][len++] = (wchar_t)(L'1' + before);
				labels[tab_count][len++] = L'/';
				labels[tab_count][len++] = (wchar_t)(L'0' + in_group);
				labels[tab_count][len] = 0;
			}
			shown[tab_count] = group;
			widths[tab_count] = ae_hud_text_measure(labels[tab_count], px) + 40.0f * k;
			total += widths[tab_count] + (tab_count ? gap : 0.0f);
			tab_count++;
		}
		widen(keyboard_scheme ? "Q" : "LB", left, 3);
		widen(keyboard_scheme ? "E" : "RB", right, 3);
		cap_w = (keyboard_scheme ? 26.0f : 36.0f) * k;
		ty = cy - AE_DEV_WHEEL_BACK * k - 8.0f - tab_h / 2.0f;
		x = cx - total / 2.0f;
		/* (the chips first, the texts after) */
		for (at = 0; at < tab_count; at++)
		{
			boolean on = shown[at] == active;

			if (on)
				pill(x, ty - tab_h / 2.0f, x + widths[at], ty + tab_h / 2.0f, 0.0f, &pick_color);
			else
			{
				pill(x, ty - tab_h / 2.0f, x + widths[at], ty + tab_h / 2.0f, 0.0f, &pill_fill);
				pill(x, ty - tab_h / 2.0f, x + widths[at], ty + tab_h / 2.0f, 2.0f * k < 1.0f ? 1.0f : 2.0f * k, &pill_line);
			}
			x += widths[at] + gap;
		}
		if (wheels[local_player_index].pages > 1)
		{
			box(cx - total / 2.0f - 8.0f * k - cap_w, ty - cap_h / 2.0f, cx - total / 2.0f - 8.0f * k, ty + cap_h / 2.0f, &cap);
			box(cx + total / 2.0f + 8.0f * k, ty - cap_h / 2.0f, cx + total / 2.0f + 8.0f * k + cap_w, ty + cap_h / 2.0f, &cap);
		}
		ae_hud_quads_end();
		x = cx - total / 2.0f;
		for (at = 0; at < tab_count; at++)
		{
			boolean on = shown[at] == active;
			real_argb_color selected = shade(AE_COLOR_SELECTION_TEXT, 1.0f), text = shade(AE_COLOR_TEXT, 1.0f);

			ae_hud_text_draw(local_player_index, x + widths[at] / 2.0f - ae_hud_text_measure(labels[at], px) / 2.0f,
				ty + px * AE_HUD_TEXT_CAP / 2.0f, labels[at], px, on ? &selected : &text, !on);
			x += widths[at] + gap;
		}
		if (wheels[local_player_index].pages > 1)
		{
			real_argb_color key_ink = shade(AE_COLOR_KEY_CAP_TEXT, 1.0f);
			real cpx = WHEEL_TEXT_FLOOR - 1.0f > 13.0f * k ? WHEEL_TEXT_FLOOR - 1.0f : 13.0f * k;

			ae_hud_text_draw(local_player_index, cx - total / 2.0f - 8.0f * k - cap_w / 2.0f - ae_hud_text_measure(left, cpx) / 2.0f,
				ty + cpx * AE_HUD_TEXT_CAP / 2.0f, left, cpx, &key_ink, FALSE);
			ae_hud_text_draw(local_player_index, cx + total / 2.0f + 8.0f * k + cap_w / 2.0f - ae_hud_text_measure(right, cpx) / 2.0f,
				ty + cpx * AE_HUD_TEXT_CAP / 2.0f, right, cpx, &key_ink, FALSE);
		}
	}
	/* (the hint: in a whole view only) */
	if (height >= 800.0f)
	{
		static wchar_t const hint[] = L"Point, then let go of SWITCH WEAPON to take it.  The centre takes nothing.";
		real px = 20.0f;

		ae_hud_text_draw(local_player_index, cx - ae_hud_text_measure(hint, px) / 2.0f,
			cy + AE_DEV_WHEEL_BACK * k + 8.0f + px, hint, px, &hint_color, TRUE);
	}
}
