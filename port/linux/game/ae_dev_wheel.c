/*
AE_DEV_WHEEL.C

DEV MODE's WEAPON WHEEL (ae_dev_wheel.h). The pointer is the look's turn added up on a disc (the stick held to a
side runs it to the edge there; the mouse moves it as a cursor), so a stick and a mouse point the same way; the
weapon whose spoke is nearest its direction is the pick, once it is past a third of the disc.

The list is read when the wheel opens: every weapon tag in the map but its objectives' (the flag, the ball), by the
tag's last name (two of one name show their folder too), up to 96: past 32 on two or three rings, inner to outer,
the pointer's distance from the middle choosing the ring. The pick is made in the next game tick of the player
(players.c), as the map's own weapons are made, with the weapon set and the map's remaps off.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cache/cache_files.h"
#include "game/players.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/objects.h"
#include "render/render.h"
#include "units/units.h"

#include "ae_dev_mode.h"
#include "ae_dev_wheel.h"
#include "ae_hud_digits.h"
#include "ae_hud_text.h"

void platform_log(char const *format, ...);

#define WHEEL_WEAPONS 96
#define WHEEL_NAME 40
/* the hold that opens it (a tap is SWITCH WEAPON's own) */
#define WHEEL_OPEN_SECONDS 0.25f
/* the pointer's disc (radians of turn), and the part of it at the middle that points at nothing */
#define WHEEL_DISC 0.20f
#define WHEEL_DEAD 0.33f
/* the ring's radius and the names' size, in HUD units (480 to the view's height) */
#define WHEEL_RADIUS 150.0f
#define WHEEL_TEXT 13.0f
#define WHEEL_TEXT_PICKED 17.0f

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
	boolean has_pending;
	long pending;
} wheels[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

static boolean giving;

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

static void wheel_fill(short local_player_index)
{
	struct tag_iterator iterator;
	long tag_index;
	short count = 0, index, other;
	long all = 0;

	tag_iterator_new(&iterator, WEAPON_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		if (objective(tag_get_name(tag_index)))
			continue;
		all++;
		if (count < WHEEL_WEAPONS)
			wheels[local_player_index].definitions[count++] = tag_index;
	}
	if (all > count)
		platform_log("dev wheel: the map has %ld weapons, the wheel shows the first %d", all, (int)count);
	wheels[local_player_index].count = count;
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
	platform_log("dev wheel: open, %d weapons", (int)count);
}

/* the rings: up to WHEEL_RING names on one; more on up to three, inner to outer, the pointer's distance choosing */
#define WHEEL_RING 32

static short wheel_rings(short count)
{
	return (short)(count <= WHEEL_RING ? 1 : (count + WHEEL_RING - 1) / WHEEL_RING);
}

/* a ring's first name and count */
static void wheel_ring(short count, short ring, short *first, short *in_ring)
{
	short rings = wheel_rings(count), per = (short)((count + rings - 1) / rings);

	*first = (short)(ring * per);
	*in_ring = (short)MAX(0, MIN(per, count - *first));
}

/* the name nearest the pointer's direction on the ring its distance falls in, or NONE inside the middle (spokes
clockwise from the top) */
static short wheel_pick(short local_player_index)
{
	real x = wheels[local_player_index].x, y = wheels[local_player_index].y;
	real angle, length = (real)sqrt(x * x + y * y);
	short count = wheels[local_player_index].count, rings, ring, first, in_ring;

	if (count <= 0 || length < WHEEL_DISC * WHEEL_DEAD)
		return NONE;
	rings = wheel_rings(count);
	ring = (short)MIN(rings - 1, (long)((length / WHEEL_DISC - WHEEL_DEAD) / (1.0f - WHEEL_DEAD) * rings));
	wheel_ring(count, ring, &first, &in_ring);
	if (in_ring <= 0)
		return NONE;
	angle = (real)atan2(x, y);
	if (angle < 0.0f)
		angle += (real)(2.0 * M_PI);
	return (short)(first + (long)(angle / (real)(2.0 * M_PI) * in_ring + 0.5f) % in_ring);
}

/* where a name sits: its ring's radius (HUD units scaled by ppu) and its spoke's angle */
static void wheel_place(short count, short index, real radius, real *ring_radius, real *angle)
{
	short rings = wheel_rings(count), ring, first = 0, in_ring = 0;

	for (ring = 0; ring < rings; ring++)
	{
		wheel_ring(count, ring, &first, &in_ring);
		if (index < first + in_ring)
			break;
	}
	*ring_radius = rings == 1 ? radius : radius * (0.55f + 0.55f * ring / (rings - 1));
	*angle = in_ring > 0 ? (real)(2.0 * M_PI) * (index - first) / in_ring : 0.0f;
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
		if (!wheels[local_player_index].open && wheels[local_player_index].held_seconds >= WHEEL_OPEN_SECONDS)
		{
			wheels[local_player_index].open = TRUE;
			wheels[local_player_index].opened_this_hold = TRUE;
			wheels[local_player_index].x = wheels[local_player_index].y = 0.0f;
			wheels[local_player_index].picked = NONE;
			wheel_fill(local_player_index);
		}
	}
	else
	{
		if (wheels[local_player_index].open)
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
	{
		real x = wheels[local_player_index].x - *yaw, y = wheels[local_player_index].y + *pitch;
		real length = (real)sqrt(x * x + y * y);

		if (length > WHEEL_DISC)
		{
			x *= WHEEL_DISC / length;
			y *= WHEEL_DISC / length;
		}
		wheels[local_player_index].x = x;
		wheels[local_player_index].y = y;
		wheels[local_player_index].picked = wheel_pick(local_player_index);
	}
	*yaw = 0.0f;
	*pitch = 0.0f;
	*unit_flags &= ~(FLAG(_unit_control_weapon_primary_trigger_bit) | FLAG(_unit_control_weapon_secondary_trigger_bit) |
		FLAG(_unit_control_throw_grenade_bit) | FLAG(_unit_control_use_equipment_bit) | FLAG(_unit_control_action_bit) |
		FLAG(_unit_control_weapon_reload_bit) | FLAG(_unit_control_swap_weapons_bit));
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

void ae_dev_wheel_draw(short local_player_index)
{
	union real_argb_color dim, bright;
	real ppu, cx, cy, radius;
	short index, count;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS ||
		!wheels[local_player_index].open)
	{
		return;
	}
	count = wheels[local_player_index].count;
	ppu = ae_hud_pixels_per_unit();
	cx = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu / 2.0f;
	cy = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu / 2.0f;
	radius = WHEEL_RADIUS * ppu;
	dim.alpha = 0.80f;
	dim.red = 0.70f;
	dim.green = 0.78f;
	dim.blue = 0.86f;
	bright.alpha = 1.0f;
	bright.red = 214.0f / 255.0f;
	bright.green = 232.0f / 255.0f;
	bright.blue = 1.0f;
	ae_hud_backing_draw(local_player_index, cx, cy, radius * 1.6f, radius * 1.3f, 0.75f);
	if (count <= 0)
	{
		static wchar_t const none[] = L"NO WEAPONS";

		ae_hud_text_draw(local_player_index, cx - ae_hud_text_measure(none, WHEEL_TEXT * ppu) / 2.0f, cy, none,
			WHEEL_TEXT * ppu, &dim, TRUE);
		return;
	}
	if (wheels[local_player_index].picked != NONE)
	{
		real ring_radius, angle;

		wheel_place(count, wheels[local_player_index].picked, radius, &ring_radius, &angle);
		ae_hud_line(cx, cy, cx + (real)sin(angle) * ring_radius * 0.8f, cy - (real)cos(angle) * ring_radius * 0.8f,
			2.0f * ppu, &bright);
	}
	for (index = 0; index < count; index++)
	{
		real ring_radius, angle;
		boolean picked = index == wheels[local_player_index].picked;
		real px = (picked ? WHEEL_TEXT_PICKED : WHEEL_TEXT) * ppu * (wheel_rings(count) > 1 ? 0.85f : 1.0f);
		real width = ae_hud_text_measure(wheels[local_player_index].names[index], px);
		real x, y;

		wheel_place(count, index, radius, &ring_radius, &angle);
		x = cx + (real)sin(angle) * ring_radius;
		y = cy - (real)cos(angle) * ring_radius;

		ae_hud_text_draw(local_player_index, x - width / 2.0f, y + px * AE_HUD_TEXT_CAP / 2.0f,
			wheels[local_player_index].names[index], px, picked ? &bright : &dim, TRUE);
	}
}
