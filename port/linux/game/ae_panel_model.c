/*
AE_PANEL_MODEL.C

What HUD STYLE AE's panels say (ae_panel_model.h; spec 14.15 parts 2, 3 and
5). Pure.
*/

#include <string.h>

#include "ae_panel_model.h"

enum
{
	STANDING_ROW_MAXIMUM = 20,      /* AR rounds 3 x 20 */
	LYING_MAXIMUM = 12,             /* lying art holds 2 rows of 6 */
	BUFF_SECONDS_MAXIMUM = 9 * 60 + 59
};

#define ROUNDS_HEAT_SHOWN 0.01f /* a rounds weapon's heat over which its heat bar shows */
#define NONE_ART (-1)

static real clamp01(real value)
{
	if (!(value > 0.0f))
		return 0.0f;
	return value > 1.0f ? 1.0f : value;
}

/* a number as text, zero-padded to `width` digits (width 0 = no padding), clamped to what fits */
static void put_number(wchar_t *text, long value, short width, long maximum)
{
	wchar_t digits[8];
	short count = 0, i;

	if (value < 0)
		value = 0;
	if (value > maximum)
		value = maximum;
	do
	{
		digits[count++] = (wchar_t)(L'0' + value % 10);
		value /= 10;
	} while (value > 0 && count < 7);
	while (count < width)
		digits[count++] = L'0';
	for (i = 0; i < count; i++)
		text[i] = digits[count - 1 - i];
	text[count] = 0;
}

static void put_string(wchar_t *text, const wchar_t *string, short capacity)
{
	short i;

	for (i = 0; i + 1 < capacity && string[i]; i++)
		text[i] = string[i];
	text[i] = 0;
}

/* "m:ss", seconds rounded up by the caller */
static void put_time(wchar_t *text, long seconds)
{
	if (seconds < 0)
		seconds = 0;
	if (seconds > BUFF_SECONDS_MAXIMUM)
		seconds = BUFF_SECONDS_MAXIMUM;
	text[0] = (wchar_t)(L'0' + seconds / 60);
	text[1] = L':';
	text[2] = (wchar_t)(L'0' + (seconds % 60) / 10);
	text[3] = (wchar_t)(L'0' + seconds % 10);
	text[4] = 0;
}

static boolean art_is_lying(short art)
{
	return art == _ae_art_sniper || art == _ae_art_rocket;
}

short ae_panel_model_rows_for(short art, short magazine, short rows[AE_PANEL_MAXIMUM_ROWS])
{
	short count, i, each, extra;

	for (i = 0; i < AE_PANEL_MAXIMUM_ROWS; i++)
		rows[i] = 0;
	if (magazine <= 0 || art == _ae_art_bar)
		return 0;
	if (art_is_lying(art))
	{
		if (magazine > LYING_MAXIMUM)
			return 0;
		/* rockets: up to 2 in one row; sniper rounds (and more rockets: the fuel rod's 3 + 2) in two rows */
		if (magazine == 1 || (art == _ae_art_rocket && magazine <= 2))
		{
			rows[0] = magazine;
			return 1;
		}
		rows[0] = (short)((magazine + 1) / 2);
		rows[1] = (short)(magazine / 2);
		return 2;
	}
	if (magazine > STANDING_ROW_MAXIMUM * AE_PANEL_MAXIMUM_ROWS)
		return 0;
	count = (short)((magazine + STANDING_ROW_MAXIMUM - 1) / STANDING_ROW_MAXIMUM);
	each = (short)(magazine / count);
	extra = (short)(magazine % count);
	for (i = 0; i < count; i++)
		rows[i] = (short)(each + (i < extra ? 1 : 0));
	return count;
}

/* the art a magazine of this size borrows when nothing names the weapon */
static short art_for_size(short magazine)
{
	if (magazine <= 2)
		return _ae_art_rocket;
	if (magazine <= 6)
		return _ae_art_sniper;
	if (magazine <= 16)
		return _ae_art_pistol;
	return _ae_art_ar;
}

static short art_for_hint(short hint)
{
	switch (hint)
	{
	case _ae_hint_assault_rifle: return _ae_art_ar;
	case _ae_hint_pistol: return _ae_art_pistol;
	case _ae_hint_shotgun: return _ae_art_shell;
	case _ae_hint_needler: return _ae_art_needle;
	case _ae_hint_sniper: return _ae_art_sniper;
	case _ae_hint_rocket: return _ae_art_rocket;
	case _ae_hint_battle_rifle: return _ae_art_ar;
	case _ae_hint_fuel_rod_rounds: return _ae_art_rocket;
	case _ae_hint_flamethrower: return _ae_art_bar;
	default: return NONE_ART;
	}
}

void ae_panel_model_row_lit(const short *rows, short row_count, short lit, short out_lit[AE_PANEL_MAXIMUM_ROWS])
{
	short i, left = lit > 0 ? lit : 0;

	for (i = 0; i < AE_PANEL_MAXIMUM_ROWS; i++)
		out_lit[i] = 0;
	if (row_count > AE_PANEL_MAXIMUM_ROWS)
		row_count = AE_PANEL_MAXIMUM_ROWS;
	/* CE's order (seen on its part-spent assault rifle, 2026-10-15): the rounds fill row by row from the TOP row's
	left, so they go out from the BOTTOM row's right end, a row at a time */
	for (i = 0; i < row_count; i++)
	{
		short n = rows[i] > 0 ? rows[i] : 0;

		out_lit[i] = left < n ? left : n;
		left = (short)(left - out_lit[i]);
	}
}

static void build_ammo(const struct ae_panel_snapshot *in, struct ae_panel_model *out)
{
	short art;
	short magazine = in->rounds_per_magazine;

	if (in->in_seat)
	{
		short i, count = in->seat_row_count;

		if (count > AE_PANEL_MAXIMUM_SEAT_ROWS)
			count = AE_PANEL_MAXIMUM_SEAT_ROWS;
		if (count <= 0)
			return;
		out->weapon_kind = _ae_panel_seat;
		for (i = 0; i < count; i++)
		{
			short kind = in->seat_rows[i].kind;
			static const wchar_t *labels[NUMBER_OF_AE_SEAT_ROW_KINDS] = { L"HEAT", L"HEAT", L"BOMB", L"CANNON", L"MG" };

			if (kind < 0 || kind >= NUMBER_OF_AE_SEAT_ROW_KINDS)
				kind = _ae_seat_heat;
			out->seat[i].kind = kind;
			put_string(out->seat[i].label, labels[kind], 8);
			out->seat[i].fill = clamp01(in->seat_rows[i].fill);
			out->seat[i].red = in->seat_rows[i].overheated;
			out->seat[i].shell = kind == _ae_seat_cannon;
		}
		out->seat_count = count;
		return;
	}
	if (!in->has_weapon)
		return;

	out->grenade[0].shown = out->grenade[1].shown = TRUE;
	if (in->weapon_has_battery)
	{
		long percent = 100 - (long)((1.0f - clamp01(in->battery)) * 100.0f + 0.001f);

		out->weapon_kind = in->weapon_has_heat ? _ae_panel_battery : _ae_panel_battery_no_heat;
		put_number(out->number, percent, 3, 999);
		out->glyph = _ae_glyph_battery;
		out->art = _ae_art_bar;
		if (in->weapon_has_heat)
		{
			out->bar_shown = TRUE;
			out->bar_fill = clamp01(in->heat);
			out->bar_red = in->overheated;
		}
		return;
	}
	if (!in->weapon_uses_rounds && in->weapon_has_heat)
	{
		/* heat only: the bar, no number */
		out->weapon_kind = _ae_panel_battery;
		out->art = _ae_art_bar;
		out->bar_shown = TRUE;
		out->bar_fill = clamp01(in->heat);
		out->bar_red = in->overheated;
		return;
	}

	put_number(out->number, in->rounds_reserve, 3, 999);
	art = art_for_hint(in->weapon_type_hint);
	if (art == _ae_art_bar || magazine > STANDING_ROW_MAXIMUM * AE_PANEL_MAXIMUM_ROWS)
	{
		out->weapon_kind = _ae_panel_tank;
		out->art = _ae_art_bar;
		out->bar_shown = TRUE;
		out->bar_fill = magazine > 0 ? clamp01((real)in->rounds_loaded / (real)magazine) : 0.0f;
		return;
	}
	out->weapon_kind = _ae_panel_rounds;
	out->glyph = _ae_glyph_round;
	if (magazine <= 0)
	{
		out->art = _ae_art_ar;
		return;
	}
	if (art == NONE_ART || (art_is_lying(art) && magazine > LYING_MAXIMUM))
		art = art_for_size(magazine);
	out->art = art;
	out->lying = art_is_lying(art);
	out->row_count = ae_panel_model_rows_for(art, magazine, out->rows);
	out->lit = in->rounds_loaded < 0 ? 0 : (in->rounds_loaded > magazine ? magazine : in->rounds_loaded);
	ae_panel_model_row_lit(out->rows, out->row_count, out->lit, out->row_lit);
	out->rounds_red = in->low_ammo_flash_on && out->lit > 0;
	/* (a rounds weapon's heat bar only while it has heat: Custom Edition maps' weapons give off a little heat a round
	that never shows, and a bar that stays empty read as a battery's, owner 10-15) */
	if (in->weapon_has_heat && (in->heat > ROUNDS_HEAT_SHOWN || in->overheated))
	{
		out->bar_shown = TRUE;
		out->bar_fill = clamp01(in->heat);
		out->bar_red = in->overheated;
	}
}

long ae_panel_camo_seconds(long ticks)
{
	return ticks > 0 ? (ticks + 29) / 30 : 0;
}

static void add_buff(struct ae_panel_model *out, const wchar_t *name, long seconds)
{
	short i = out->buff_count;

	if (i >= AE_PANEL_MAXIMUM_BUFFS)
		return;
	put_string(out->buff[i].name, name, 12);
	put_time(out->buff[i].time, seconds);
	out->buff[i].time_red = seconds <= AE_PANEL_BUFF_RED_SECONDS;
	out->buff_count++;
}

void ae_panel_model_build(const struct ae_panel_snapshot *in, struct ae_panel_model *out)
{
	short i;
	real health_cells;

	memset(out, 0, sizeof(*out));
	out->weapon_kind = _ae_panel_none;
	out->glyph = _ae_glyph_none;
	out->art = _ae_art_bar;
	build_ammo(in, out);

	if (out->grenade[0].shown)
		for (i = 0; i < 2; i++)
		{
			short count = in->grenades[i] < 0 ? 0 : in->grenades[i];

			put_number(out->grenade[i].count, count, 0, 99);
			out->grenade[i].zero = count == 0;
			out->grenade[i].armed = in->armed_grenade == i;
		}

	out->shield_fill = clamp01(in->shield);
	out->shield_trail = clamp01(in->shield_trail);
	out->shield_trail_alpha = clamp01(in->shield_trail_alpha);
	out->overshield1 = clamp01(in->shield - 1.0f);
	out->overshield2 = clamp01(in->shield - 2.0f);
	out->shield_red = in->shield_low || in->shield_down;
	out->shield_hidden_blink = out->shield_red && !in->shield_blink_on && !in->reduced_motion;

	health_cells = clamp01(in->health) * (real)AE_PANEL_HEALTH_CELLS - 0.0001f;
	out->health_cells_lit = 0;
	while (out->health_cells_lit < AE_PANEL_HEALTH_CELLS && (real)out->health_cells_lit < health_cells)
		out->health_cells_lit++;
	out->health_red = in->health_low;

	if (in->overshield_seconds > 0.0f)
	{
		real time = in->overshield_seconds > (real)BUFF_SECONDS_MAXIMUM ? (real)BUFF_SECONDS_MAXIMUM :
			in->overshield_seconds;
		long seconds = (long)time;

		if ((real)seconds < time)
			seconds++;
		add_buff(out, L"OVERSHIELD", seconds);
	}
	if (in->camo_ticks > 0)
		add_buff(out, L"CAMO", ae_panel_camo_seconds(in->camo_ticks));
}

void ae_panel_trail_step(struct ae_panel_trail *state, real shield, long now, real *trail, real *alpha)
{
	real current = clamp01(shield);
	long age;

	*trail = current;
	*alpha = 0.0f;
	if (current > state->last + 0.0001f || !state->active)
	{
		/* (rising, or no hit under way: none; a drop from here starts one) */
		if (current < state->last - 0.005f && state->last <= 1.0f)
		{
			state->active = TRUE;
			state->trail = state->last;
			state->hit_tick = now;
		}
		else
			state->active = FALSE;
	}
	else if (current < state->last - 0.005f && now - state->hit_tick >= AE_PANEL_TRAIL_HOLD)
	{
		/* (another hit once the first's hold is over: a new trail from where the shield was) */
		state->trail = state->last;
		state->hit_tick = now;
	}
	state->last = current;
	if (!state->active)
		return;
	age = now - state->hit_tick;
	if (age >= AE_PANEL_TRAIL_HOLD + AE_PANEL_TRAIL_FADE || state->trail <= current)
	{
		state->active = FALSE;
		return;
	}
	*trail = state->trail;
	*alpha = age < AE_PANEL_TRAIL_HOLD ? 1.0f :
		1.0f - (real)(age - AE_PANEL_TRAIL_HOLD) / (real)AE_PANEL_TRAIL_FADE;
}

real ae_panel_script_blink_fade(boolean blinking, long tick, boolean reduced_motion)
{
	if (!blinking)
		return 1.0f;
	if (tick < 0)
		tick = 0;
	if (reduced_motion)
		return tick % AE_PANEL_SCRIPT_BLINK_CALM_TICKS < AE_PANEL_SCRIPT_BLINK_TICKS ? AE_PANEL_SCRIPT_BLINK_DIM : 1.0f;
	return (tick / AE_PANEL_SCRIPT_BLINK_TICKS) % 2 == 0 ? 1.0f : AE_PANEL_SCRIPT_BLINK_DIM;
}

boolean ae_panel_zoomed_number_is_readout(short state_type)
{
	return (boolean)(state_type == AE_PANEL_WEAPON_NUMBER_DISTANCE || state_type == AE_PANEL_WEAPON_NUMBER_ELEVATION);
}
