/*
AE_SEAT_PANEL.C

THE SEAT PANEL (ae_seat_panel.h). A client has all it shows: the host sends every player's unit, with the seat it rides, its
shields and health. A seat with no child unit in it is open. A rider with no player has no name: the role alone.
*/

#include <math.h>
#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"
#include "math/real_math.h"
#include "game/players.h"
#include "objects/objects.h"
#include "objects/object_types.h"
#include "units/units.h"
#include "units/unit_definitions.h"

#include "ae_hud_text.h"
#include "ae_marker_rules.h"
#include "ae_panel_draw.h"
#include "ae_panel_layout.h"
#include "ae_seat_panel.h"
#include "ae_seat_panel_rules.h"
#include "ae_wheel_draw.h"

#define SEAT_MAXIMUM 8
#define TEXT_PX 16.0f
#define TEXT_CAP 0.72f				/* of the text's size */
#define OPEN_FADE 0.45f
#define BAR_HEIGHT 10.0f			/* the rest x scale */
#define LABEL_TO_BAR 3.0f
#define BAR_TO_CROSS 4.0f
#define BETWEEN_ROWS 8.0f
#define CROSS_SIZE 11.0f
#define CROSS_SMALLEST 9.0f
#define PIP_WIDTH 11.0f
#define PIP_HEIGHT 7.0f
#define PIP_PITCH 14.0f
#define CROSS_TO_PIP 4.0f
#define LABEL_BLUE_RED (117.0f / 255.0f)
#define LABEL_BLUE_GREEN (186.0f / 255.0f)

struct seat
{
	short index;
	long occupant;				/* the child unit in it, or NONE: open */
};

/* the vehicle's seats but the driver's, each with who sits in it */
static short seats_of(long vehicle_index, struct seat *seats)
{
	struct unit_datum *vehicle = unit_try_and_get(vehicle_index);
	struct unit_definition const *definition;
	short count = 0;
	short seat;

	if (!vehicle)
		return 0;
	definition = unit_definition_get(vehicle->definition_index);
	for (seat = 0; seat < definition->unit.seats.count && count < SEAT_MAXIMUM; seat++)
	{
		struct unit_seat const *block = TAG_BLOCK_GET_ELEMENT(&definition->unit.seats, seat, struct unit_seat);
		long child;

		if (TEST_FLAG(block->flags, _unit_seat_driver_bit))
			continue;
		seats[count].index = seat;
		seats[count].occupant = NONE;
		for (child = vehicle->object.first_child_object_index; child != NONE; child = object_get(child)->object.next_object_index)
		{
			struct unit_datum *rider = unit_try_and_get(child);

			if (rider && rider->object.parent_object_index == vehicle_index && rider->unit.parent_seat_index == seat)
			{
				seats[count].occupant = child;
				break;
			}
		}
		count++;
	}
	return count;
}

long ae_seat_panel_vehicle(short local_player_index)
{
	struct seat seats[SEAT_MAXIMUM];
	long player_index = local_player_get_player_index(local_player_index);
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	struct unit_datum *unit = player && player->unit_index != NONE ? unit_try_and_get(player->unit_index) : NULL;
	struct unit_datum *vehicle;
	struct unit_definition const *definition;
	struct unit_seat const *seat;

	if (!unit || unit->object.parent_object_index == NONE || unit->unit.parent_seat_index == NONE ||
		!(vehicle = unit_try_and_get(unit->object.parent_object_index)))
	{
		return NONE;
	}
	definition = unit_definition_get(vehicle->definition_index);
	seat = TAG_BLOCK_GET_ELEMENT(&definition->unit.seats, unit->unit.parent_seat_index, struct unit_seat);
	if (!TEST_FLAG(seat->flags, _unit_seat_driver_bit) ||
		seats_of(unit->object.parent_object_index, seats) == 0)
	{
		return NONE;
	}
	return unit->object.parent_object_index;
}

boolean ae_seat_panel_takes_hud(long hud_unit_index, long own_unit_index)
{
	struct unit_datum *hud_unit = unit_try_and_get(hud_unit_index);
	struct unit_datum *own = unit_try_and_get(own_unit_index);

	/* (the viewer's own seat's HUD stays: it has the driver's reticle) */
	return (boolean)(hud_unit && own && hud_unit_index != own_unit_index && own->object.parent_object_index != NONE &&
		hud_unit->object.parent_object_index == own->object.parent_object_index);
}

/* the panel's lines for a vehicle's seats: with more than three, the open seats of one kind are one line (the rule: ae_seat_roll) */
static short lines_of(long vehicle_index, struct seat const *seats, short count, struct ae_seat_line *lines)
{
	struct ae_seat_kind kinds[SEAT_MAXIMUM];
	short index;

	for (index = 0; index < count; index++)
	{
		kinds[index].gunner = unit_seat_is_gunner(vehicle_index, seats[index].index) ? 1 : 0;
		kinds[index].open = seats[index].occupant == NONE ? 1 : 0;
	}
	return (short)ae_seat_roll(count, kinds, lines);
}

short ae_seat_panel_count(long vehicle_index)
{
	struct seat seats[SEAT_MAXIMUM];
	struct ae_seat_line lines[SEAT_MAXIMUM];

	return lines_of(vehicle_index, seats, seats_of(vehicle_index, seats), lines);
}

static float text_measure(wchar_t const *text)
{
	return (float)ae_hud_text_measure(text, TEXT_PX);
}

static void seat_label(short view, struct seat const *seat, int open_seats, long vehicle_index, real x, real baseline)
{
	wchar_t const *role = unit_seat_is_gunner(vehicle_index, seat->index) ? L"GUNNER" : L"PASSENGER";
	wchar_t name[16] = L"";
	wchar_t text[48];
	real_argb_color blue;
	real_argb_color white;
	real role_width;
	real rgb[3];
	size_t role_length = wcslen(role);

	if (seat->occupant == NONE)
	{
		/* (OPEN, or OPEN x6 where six open seats of the kind are one line) */
		ae_seat_open_text(open_seats, name, NUMBEROF(name));
	}
	else
	{
		struct unit_datum *rider = unit_get(seat->occupant);
		struct player_datum *player = rider->unit.player_index != NONE ? player_try_and_get(rider->unit.player_index) : NULL;

		if (player)
		{
			wcsncpy(name, player->name, NUMBEROF(name) - 1);
			name[NUMBEROF(name) - 1] = 0;
		}
	}
	/* (an open line is never cut: its count is the point, and it fits the pitch) */
	ae_seat_name_clip(role, name, seat->occupant == NONE ? AE_SEAT_PITCH : AE_SEAT_BAR_WIDTH, text_measure, text,
		NUMBEROF(text));
	ae_panel_tone_rgb(_ae_tone_white, rgb);
	white.alpha = seat->occupant == NONE ? OPEN_FADE : 1.0f;
	white.red = rgb[0];
	white.green = rgb[1];
	white.blue = rgb[2];
	blue.alpha = white.alpha;
	blue.red = LABEL_BLUE_RED;
	blue.green = LABEL_BLUE_GREEN;
	blue.blue = 1.0f;
	/* (the role in the label's blue, the rest in white, on one baseline) */
	if (text[role_length])
	{
		role_width = ae_hud_text_measure(role, TEXT_PX);
		ae_hud_text_draw_ex(view, x + role_width, baseline, text + role_length, TEXT_PX, &white, _ae_text_outline);
	}
	ae_hud_text_draw_ex(view, x, baseline, role, TEXT_PX, &blue, _ae_text_outline);
}

static void lean_box(struct ae_panel_para *box, real x, real y, real width, real height)
{
	box->x0 = x;
	box->x1 = x + width;
	box->y0 = y;
	box->y1 = y + height;
	box->lean = AE_PANEL_LEAN;
}

static void seat_draw(short view, struct seat const *seat, int open_seats, long vehicle_index, real x, real y, real scale)
{
	real baseline = y + TEXT_PX * TEXT_CAP;
	real bar_top = baseline + LABEL_TO_BAR * scale;
	real bar_height = BAR_HEIGHT * scale;
	struct ae_panel_para bar;

	seat_label(view, seat, open_seats, vehicle_index, x, baseline);
	lean_box(&bar, x, bar_top, AE_SEAT_BAR_WIDTH, bar_height);
	if (seat->occupant == NONE)
	{
		ae_panel_draw_set_fade(OPEN_FADE);
		ae_panel_draw_bar(&bar, scale, 0.0f, _ae_tone_blue, _ae_tone_blue, 0.0f, 0.0f);
		ae_panel_draw_set_fade(1.0f);
	}
	else
	{
		struct object_datum *rider = object_get(seat->occupant);
		real cross = MAX(CROSS_SIZE * scale, CROSS_SMALLEST);
		real row = bar_top + bar_height + BAR_TO_CROSS * scale;
		real pip_x = x + cross + CROSS_TO_PIP * scale;
		real pip_y = row + (cross - PIP_HEIGHT * scale) / 2.0f;
		int lit = ae_seat_pips_lit(rider->object.body_vitality);
		short tone = ae_seat_pips_red(lit) ? _ae_tone_red : _ae_tone_blue;
		int pip;

		ae_panel_draw_bar(&bar, scale, MIN(MAX(rider->object.shield_vitality, 0.0f), 1.0f), _ae_tone_blue, _ae_tone_blue, 0.0f, 0.0f);
		ae_panel_draw_cross(x, row, cross, tone);
		for (pip = 0; pip < AE_SEAT_PIPS; pip++)
		{
			struct ae_panel_para cell;

			lean_box(&cell, pip_x + (real)pip * PIP_PITCH * scale, pip_y, PIP_WIDTH * scale, PIP_HEIGHT * scale);
			ae_panel_draw_cell(&cell, (boolean)(pip < lit), tone);
		}
	}
}

void ae_seat_panel_draw(short local_player_index, long vehicle_index, real left, real top, real scale, real view_width, real box[4])
{
	struct seat seats[SEAT_MAXIMUM];
	struct ae_seat_line lines[SEAT_MAXIMUM];
	short count = lines_of(vehicle_index, seats, seats_of(vehicle_index, seats), lines);
	real row_height = TEXT_PX * TEXT_CAP + (LABEL_TO_BAR + BAR_HEIGHT + BAR_TO_CROSS + BETWEEN_ROWS) * scale +
		MAX(CROSS_SIZE * scale, CROSS_SMALLEST);
	int columns = ae_seat_columns(count, scale, left, view_width);
	int rows = count ? (count + columns - 1) / columns : 0;
	real keep_x, keep_y, keep_radius, keep_limit, keep_word[4];
	short index;

	box[0] = left;
	box[1] = top;
	box[2] = left + (real)MIN(columns, MAX(count, 1)) * AE_SEAT_PITCH - (AE_SEAT_PITCH - AE_SEAT_BAR_WIDTH);
	box[3] = top + (real)rows * row_height - BETWEEN_ROWS * scale;
	if (ae_wheel_text_keepout(local_player_index, &keep_x, &keep_y, &keep_radius, keep_word, &keep_limit))
	{
		float row_box[4];
		float word[4];

		for (index = 0; index < 4; index++)
		{
			row_box[index] = (float)box[index];
			word[index] = (float)keep_word[index];
		}
		if (ae_row_hidden_by_wheel(row_box, (float)keep_x, (float)keep_y, (float)keep_radius, word, (float)keep_limit))
			return;
	}
	for (index = 0; index < count; index++)
	{
		float x, y;

		ae_seat_cell(index, columns, (float)left, (float)top, (float)row_height, &x, &y);
		seat_draw(local_player_index, &seats[lines[index].seat], lines[index].open, vehicle_index, (real)x, (real)y, scale);
	}
}
