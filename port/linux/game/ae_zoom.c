/*
AE_ZOOM.C

The zoom action's modes (ae_zoom.h). Pure: no engine, no config, no clock.
*/

#include <ctype.h>
#include <stddef.h>
#include <string.h>

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
	state->promoted = 0;
	state->tap_valid = 0;
	state->tap_release_ms = 0;
	state->tap_key = 0;
}

/* (weapon_rotate_zoom_level in source/items/weapons.c, without its reload test, which is the caller's blocked) */
short ae_zoom_rotate(short level, short count)
{
	if (level >= 0 && level < count - 1)
		return (short)(level + 1);
	return level == count - 1 ? (short)-1 : (short)0;
}

int ae_zoom_reset_from_text(char const *text)
{
	return text_is(text, "last") ? AE_ZOOM_RESET_LAST : AE_ZOOM_RESET_FIRST;
}

short ae_zoom_clamp_level(short level, short count)
{
	if (count < 1)
		return -1;
	if (level < 0)
		return 0;
	return level > count - 1 ? (short)(count - 1) : level;
}

/* the level a press from unzoomed zooms to (the hold part): the first, or the last used (remembered) one */
static short plain_start(struct ae_zoom_in const *in)
{
	return in->reset == AE_ZOOM_RESET_LAST ? ae_zoom_clamp_level(in->remembered, in->count) : ae_zoom_clamp_level(0, in->count);
}

short ae_zoom_step_ex(struct ae_zoom_state *state, struct ae_zoom_in const *in)
{
	int mode = in->mode;
	short level = in->level;
	short count = in->count;
	long now_ms = in->now_ms;
	int rising = in->held && !state->was_held;
	int falling = !in->held && state->was_held;

	state->was_held = in->held;
	/* the game changed the level behind a press that is still down (or whose release is yet to come) */
	if (state->phase == PHASE_ACTIVE && level != state->expected)
		state->phase = PHASE_IGNORED;

	if (rising)
	{
		int double_press = state->tap_valid && state->tap_key == in->key && now_ms - state->tap_release_ms <= in->hold_ms;

		state->phase = PHASE_IDLE;
		state->tap_valid = 0;
		state->promoted = 0;
		if (in->blocked)
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
			/* a press zooms in at once from unzoomed. HOLD: to the level the hold starts at: the second after a tap
			(a double press) when the weapon has one, else the first or the last used. BOTH: to the first, which a tap
			and a hold both end at; a hold then goes on to the last used one (promoted, below); a zoomed press waits
			for its release to tell a tap (the next level) from a hold (back to unzoomed) */
			if (level < 0 && count > 0)
			{
				if (mode == AE_ZOOM_HOLD)
					level = double_press ? ae_zoom_clamp_level(1, count) : plain_start(in);
				else
					level = ae_zoom_rotate(level, count);
			}
		}
	}
	else if (falling)
	{
		if (state->phase == PHASE_ACTIVE)
		{
			int hold = state->promoted || now_ms - state->press_ms > in->hold_ms;

			if (mode == AE_ZOOM_HOLD)
			{
				level = -1;
				/* a tap, not a hold: the next press, soon enough, is a double press */
				if (!hold)
				{
					state->tap_valid = 1;
					state->tap_release_ms = now_ms;
					state->tap_key = in->key;
				}
			}
			else if (mode == AE_ZOOM_BOTH)
			{
				if (hold)
					level = -1;
				else if (state->start_level >= 0 && !in->blocked)
					level = ae_zoom_rotate(level, count);
			}
		}
		state->phase = PHASE_IDLE;
	}

	/* held, in HOLD and BOTH: the level steps (the wheel up or down, the flashlight on to the next, the first after the
	last), which makes the press a hold; and a BOTH press from unzoomed becomes one after the tap time, going on to
	the level the hold starts at */
	if (in->held && state->phase == PHASE_ACTIVE && level >= 0 && !falling)
	{
		if (in->wheel || in->flash)
		{
			state->promoted = 1;
			if (in->wheel > 0)
				level = ae_zoom_clamp_level((short)(level + 1), count);
			else if (in->wheel < 0)
				level = ae_zoom_clamp_level((short)(level - 1), count);
			if (in->flash)
				if (count > 0)
				level = (short)((level + 1) % count);
		}
		if (mode == AE_ZOOM_BOTH && state->start_level < 0 && !state->promoted && now_ms - state->press_ms > in->hold_ms)
		{
			state->promoted = 1;
			level = plain_start(in);
		}
	}
	state->expected = level;
	return level;
}

short ae_zoom_step(struct ae_zoom_state *state, int mode, long hold_ms, int held, long now_ms, short level,
	short count, int blocked)
{
	struct ae_zoom_in in;

	memset(&in, 0, sizeof(in));
	in.mode = mode;
	in.hold_ms = hold_ms;
	in.held = held;
	in.now_ms = now_ms;
	in.level = level;
	in.count = count;
	in.blocked = blocked;
	return ae_zoom_step_ex(state, &in);
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

static void player_let_go(struct ae_zoom_bank *bank, int player)
{
	ae_zoom_reset(&bank->states[player]);
	bank->flash_latch[player] = 0;
	bank->flash_pending[player] = 0;
	bank->flash_was_down[player] = 0;
}

void ae_zoom_bank_set_player(struct ae_zoom_bank *bank, int player, int mode, int reset)
{
	if (player < 0 || player >= AE_ZOOM_PLAYERS)
		return;
	bank->resets[player] = reset;
	if (mode == bank->modes[player])
		return;
	bank->modes[player] = mode;
	player_let_go(bank, player);
}

void ae_zoom_bank_set_mode(struct ae_zoom_bank *bank, int mode)
{
	int index;

	for (index = 0; index < AE_ZOOM_PLAYERS; index++)
	{
		if (mode == bank->modes[index])
			continue;
		bank->modes[index] = mode;
		player_let_go(bank, index);
	}
}

int ae_zoom_bank_mode(struct ae_zoom_bank const *bank, int player)
{
	return player >= 0 && player < AE_ZOOM_PLAYERS ? bank->modes[player] : AE_ZOOM_TOGGLE;
}

void ae_zoom_bank_set_reset(struct ae_zoom_bank *bank, int reset)
{
	int index;

	for (index = 0; index < AE_ZOOM_PLAYERS; index++)
		bank->resets[index] = reset;
}

void ae_zoom_bank_remember(struct ae_zoom_bank *bank, int player, long weapon, short level)
{
	int slot;

	if (player < 0 || player >= AE_ZOOM_PLAYERS || level < 0)
		return;
	for (slot = 0; slot < AE_ZOOM_MEMORY; slot++)
	{
		if (bank->memory_level[player][slot] > 0 && bank->memory_weapon[player][slot] == weapon)
			break;
	}
	if (slot == AE_ZOOM_MEMORY)
	{
		slot = bank->memory_next[player];
		bank->memory_next[player] = (slot + 1) % AE_ZOOM_MEMORY;
	}
	bank->memory_weapon[player][slot] = weapon;
	bank->memory_level[player][slot] = (short)(level + 1);
}

short ae_zoom_bank_recall(struct ae_zoom_bank const *bank, int player, long weapon)
{
	int slot;

	if (player < 0 || player >= AE_ZOOM_PLAYERS)
		return -1;
	for (slot = 0; slot < AE_ZOOM_MEMORY; slot++)
	{
		if (bank->memory_level[player][slot] > 0 && bank->memory_weapon[player][slot] == weapon)
			return (short)(bank->memory_level[player][slot] - 1);
	}
	return -1;
}

void ae_zoom_bank_filter_flash(struct ae_zoom_bank *bank, int player, unsigned long const *player_flags,
	unsigned long *unit_flags, int light_bit)
{
	int down, rising;

	if (player < 0 || player >= AE_ZOOM_PLAYERS)
		return;
	bank->flash_pending[player] = 0;
	down = (int)((*player_flags >> AE_ZOOM_FLASH_BIT) & 1);
	rising = down && !bank->flash_was_down[player];
	bank->flash_was_down[player] = down;
	if (bank->modes[player] == AE_ZOOM_TOGGLE || !down)
	{
		bank->flash_latch[player] = 0;
		return;
	}
	/* a press that comes with the zoom held is the zoom's; one held before it is the light's */
	if (rising && ((*player_flags >> AE_ZOOM_HELD_BIT) & 1))
	{
		bank->flash_latch[player] = 1;
		bank->flash_pending[player] = 1;
	}
	if (bank->flash_latch[player])
		*unit_flags &= ~(1ul << light_bit);
}

short ae_zoom_bank_step_ex(struct ae_zoom_bank *bank, int player, long hold_ms, unsigned long flags, long now_ms,
	short level, short count, int blocked, long weapon, int wheel)
{
	struct ae_zoom_in in;

	if (player < 0 || player >= AE_ZOOM_PLAYERS || bank->modes[player] == AE_ZOOM_TOGGLE)
		return level;
	memset(&in, 0, sizeof(in));
	in.mode = bank->modes[player];
	in.hold_ms = hold_ms;
	in.held = (int)((flags >> AE_ZOOM_HELD_BIT) & 1);
	in.now_ms = now_ms;
	in.level = level;
	in.count = count;
	in.blocked = blocked;
	in.key = weapon;
	in.reset = bank->resets[player];
	in.remembered = ae_zoom_bank_recall(bank, player, weapon);
	in.wheel = wheel;
	in.flash = bank->flash_pending[player];
	bank->flash_pending[player] = 0;
	/* the level a press ends on is the last one used (not the first one a press zooms to at once) */
	if (!in.held && bank->states[player].was_held)
		ae_zoom_bank_remember(bank, player, weapon, level);
	level = ae_zoom_step_ex(&bank->states[player], &in);
	if (!in.held && level >= 0)
		ae_zoom_bank_remember(bank, player, weapon, level);
	return level;
}

short ae_zoom_bank_step(struct ae_zoom_bank *bank, int player, long hold_ms, unsigned long flags, long now_ms,
	short level, short count, int blocked)
{
	if (player < 0 || player >= AE_ZOOM_PLAYERS || bank->modes[player] == AE_ZOOM_TOGGLE)
		return level;
	return ae_zoom_step(&bank->states[player], bank->modes[player], hold_ms, (int)((flags >> AE_ZOOM_HELD_BIT) & 1), now_ms,
		level, count, blocked);
}

int ae_zoom_wheel_wanted(int mode, unsigned long flags, short count, int blocked, short level)
{
	return mode != AE_ZOOM_TOGGLE && ((flags >> AE_ZOOM_HELD_BIT) & 1) && count > 0 && !blocked && level >= 0;
}

int ae_zoom_wheel_free(int wheel_zoom, short count, int blocked)
{
	return wheel_zoom && count > 0 && !blocked;
}

short ae_zoom_wheel_step(short level, short count, int wheel)
{
	if (count <= 0)
		return -1;
	if (wheel > 0)
		return level < 0 ? 0 : ae_zoom_clamp_level((short)(level + 1), count);
	if (wheel < 0)
		return level <= 0 ? -1 : ae_zoom_clamp_level((short)(level - 1), count);
	return level;
}

void ae_zoom_bank_forget(struct ae_zoom_bank *bank)
{
	int index;

	for (index = 0; index < AE_ZOOM_PLAYERS; index++)
	{
		int slot;

		ae_zoom_reset(&bank->states[index]);
		bank->flash_latch[index] = 0;
		bank->flash_pending[index] = 0;
		bank->flash_was_down[index] = 0;
		bank->memory_next[index] = 0;
		for (slot = 0; slot < AE_ZOOM_MEMORY; slot++)
		{
			bank->memory_weapon[index][slot] = 0;
			bank->memory_level[index][slot] = 0;
		}
	}
}
