/*
AE_ZOOM.C

The zoom action's modes (ae_zoom.h). Pure: no engine, no config, no clock.
*/

#include <ctype.h>
#include <stddef.h>

#include "ae_zoom.h"

enum
{
	PHASE_IDLE = 0,
	/* a press the machine acts on, down now */
	PHASE_ACTIVE,
	/* a press that was blocked, or whose zoom the game took away: nothing of it acts */
	PHASE_IGNORED
};

static int text_is(char const *text, char const *word)
{
	if (!text)
		return 0;
	for (; *word; word++, text++)
		if (tolower((unsigned char)*text) != *word)
			return 0;
	return *text == 0;
}

int ae_zoom_mode_from_text(char const *text)
{
	if (text_is(text, "hold"))
		return AE_ZOOM_HOLD;
	if (text_is(text, "both"))
		return AE_ZOOM_BOTH;
	return AE_ZOOM_TOGGLE;
}

long ae_zoom_hold_ms(double seconds)
{
	if (seconds != seconds) /* (not a number) */
		seconds = AE_ZOOM_HOLD_TIME_DEFAULT;
	if (seconds < AE_ZOOM_HOLD_TIME_MINIMUM)
		seconds = AE_ZOOM_HOLD_TIME_MINIMUM;
	if (seconds > AE_ZOOM_HOLD_TIME_MAXIMUM)
		seconds = AE_ZOOM_HOLD_TIME_MAXIMUM;
	return (long)(seconds * 1000.0 + 0.5);
}

void ae_zoom_reset(struct ae_zoom_state *state)
{
	state->phase = PHASE_IDLE;
	state->was_held = 0;
	state->press_ms = 0;
	state->start_level = -1;
	state->expected = -1;
}

/* (weapon_rotate_zoom_level in source/items/weapons.c, without its reload test, which is the caller's blocked) */
short ae_zoom_rotate(short level, short count)
{
	if (level >= 0 && level < count - 1)
		return (short)(level + 1);
	return level == count - 1 ? (short)-1 : (short)0;
}

short ae_zoom_step(struct ae_zoom_state *state, int mode, long hold_ms, int held, long now_ms, short level,
	short count, int blocked)
{
	int rising = held && !state->was_held;
	int falling = !held && state->was_held;

	state->was_held = held;
	/* the game changed the level behind a press that is still down (or whose release is yet to come) */
	if (state->phase == PHASE_ACTIVE && level != state->expected)
		state->phase = PHASE_IGNORED;

	if (rising)
	{
		state->phase = PHASE_IDLE;
		if (blocked)
		{
			state->phase = PHASE_IGNORED;
		}
		else if (mode == AE_ZOOM_TOGGLE)
		{
			level = ae_zoom_rotate(level, count);
		}
		else
		{
			state->phase = PHASE_ACTIVE;
			state->press_ms = now_ms;
			state->start_level = level;
			/* a press zooms in at once from unzoomed: a tap and a hold both end there; a zoomed press waits for its
			release to tell a tap (the next level) from a hold (back to unzoomed) */
			if (level < 0)
				level = ae_zoom_rotate(level, count);
		}
	}
	else if (falling)
	{
		if (state->phase == PHASE_ACTIVE)
		{
			if (mode == AE_ZOOM_HOLD)
				level = -1;
			else if (mode == AE_ZOOM_BOTH)
			{
				if (now_ms - state->press_ms > hold_ms)
					level = -1;
				else if (state->start_level >= 0 && !blocked)
					level = ae_zoom_rotate(level, count);
			}
		}
		state->phase = PHASE_IDLE;
	}
	state->expected = level;
	return level;
}

void ae_zoom_flags_note_held(unsigned long *flags, int held)
{
	if (held)
		*flags |= 1ul << AE_ZOOM_HELD_BIT;
	else
		*flags &= ~(1ul << AE_ZOOM_HELD_BIT);
}

void ae_zoom_flags_filter_press(int mode, unsigned long *flags, int press_bit)
{
	if (mode != AE_ZOOM_TOGGLE)
		*flags &= ~(1ul << press_bit);
}

void ae_zoom_bank_set_mode(struct ae_zoom_bank *bank, int mode)
{
	int index;

	if (mode == bank->mode)
		return;
	bank->mode = mode;
	for (index = 0; index < AE_ZOOM_PLAYERS; index++)
		ae_zoom_reset(&bank->states[index]);
}

short ae_zoom_bank_step(struct ae_zoom_bank *bank, int player, long hold_ms, unsigned long flags, long now_ms,
	short level, short count, int blocked)
{
	if (bank->mode == AE_ZOOM_TOGGLE || player < 0 || player >= AE_ZOOM_PLAYERS)
		return level;
	return ae_zoom_step(&bank->states[player], bank->mode, hold_ms, (int)((flags >> AE_ZOOM_HELD_BIT) & 1), now_ms,
		level, count, blocked);
}
