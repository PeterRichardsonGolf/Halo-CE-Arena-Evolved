#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_zoom.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define HOLD_MS 250

/* one weapon, one machine: events at timestamps; level is what the game holds between events */
struct rig
{
	struct ae_zoom_state state;
	int mode;
	short count; /* zoom_level_count of the weapon: 1 pistol (-1, 0), 2 sniper (-1, 0, 1) */
	short level;
	int blocked;
};

static void rig_start(struct rig *r, int mode, short count)
{
	memset(r, 0, sizeof(*r));
	ae_zoom_reset(&r->state);
	r->mode = mode;
	r->count = count;
	r->level = -1;
}

/* the game's tick: the button's state and the time */
static short tick(struct rig *r, int held, long now)
{
	r->level = ae_zoom_step(&r->state, r->mode, HOLD_MS, held, now, r->level, r->count, r->blocked);
	return r->level;
}

/* one tick with the extras */
static short tickx(struct rig *r, int held, long now, int wheel, int flash, int reset, short remembered, long key)
{
	struct ae_zoom_in in;

	memset(&in, 0, sizeof(in));
	in.mode = r->mode;
	in.hold_ms = HOLD_MS;
	in.held = held;
	in.now_ms = now;
	in.level = r->level;
	in.count = r->count;
	in.blocked = r->blocked;
	in.key = key;
	in.reset = reset;
	in.remembered = remembered;
	in.wheel = wheel;
	in.flash = flash;
	r->level = ae_zoom_step_ex(&r->state, &in);
	return r->level;
}
#define W(r, held, now, wheel) tickx(r, held, now, wheel, 0, AE_ZOOM_RESET_FIRST, -1, 7)
#define F(r, held, now) tickx(r, held, now, 0, 1, AE_ZOOM_RESET_FIRST, -1, 7)
#define T(r, held, now) tickx(r, held, now, 0, 0, AE_ZOOM_RESET_FIRST, -1, 7)

int main(void)
{
	struct rig r;

	/* CE's rotation, the model of the toggle mode */
	CHECK(ae_zoom_rotate(-1, 1) == 0 && ae_zoom_rotate(0, 1) == -1);
	CHECK(ae_zoom_rotate(-1, 2) == 0 && ae_zoom_rotate(0, 2) == 1 && ae_zoom_rotate(1, 2) == -1);
	CHECK(ae_zoom_rotate(-1, 0) == -1);

	/* TOGGLE: each press steps, release does nothing; held does not repeat */
	rig_start(&r, AE_ZOOM_TOGGLE, 2);
	CHECK(tick(&r, 0, 0) == -1);
	CHECK(tick(&r, 1, 100) == 0);
	CHECK(tick(&r, 1, 200) == 0);
	CHECK(tick(&r, 0, 900) == 0);
	CHECK(tick(&r, 1, 1000) == 1);
	CHECK(tick(&r, 0, 1010) == 1);
	CHECK(tick(&r, 1, 1100) == -1);
	CHECK(tick(&r, 0, 1110) == -1);
	rig_start(&r, AE_ZOOM_TOGGLE, 1);
	CHECK(tick(&r, 1, 0) == 0);
	CHECK(tick(&r, 0, 10) == 0);
	CHECK(tick(&r, 1, 20) == -1);

	/* HOLD, sniper: press zooms to the first level only, release returns; never level 1 */
	rig_start(&r, AE_ZOOM_HOLD, 2);
	CHECK(tick(&r, 1, 0) == 0);
	CHECK(tick(&r, 1, 5000) == 0);
	CHECK(tick(&r, 0, 5010) == -1);
	CHECK(tick(&r, 1, 6000) == 0); /* a second press: first level again, not the second */
	CHECK(tick(&r, 0, 6100) == -1);
	/* HOLD, pistol */
	rig_start(&r, AE_ZOOM_HOLD, 1);
	CHECK(tick(&r, 1, 0) == 0);
	CHECK(tick(&r, 0, 50) == -1);
	/* a weapon that does not zoom stays out */
	rig_start(&r, AE_ZOOM_HOLD, 0);
	CHECK(tick(&r, 1, 0) == -1 && tick(&r, 0, 50) == -1);

	/* BOTH, pistol: a tap zooms in, the next tap out */
	rig_start(&r, AE_ZOOM_BOTH, 1);
	CHECK(tick(&r, 1, 0) == 0);
	CHECK(tick(&r, 0, 100) == 0); /* tapped: stays */
	CHECK(tick(&r, 1, 1000) == 0);
	CHECK(tick(&r, 0, 1100) == -1); /* tapped again: out */
	/* BOTH, sniper: tap, tap = second level, tap = out */
	rig_start(&r, AE_ZOOM_BOTH, 2);
	CHECK(tick(&r, 1, 0) == 0);
	CHECK(tick(&r, 0, 80) == 0);
	CHECK(tick(&r, 1, 500) == 0);
	CHECK(tick(&r, 0, 600) == 1);
	CHECK(tick(&r, 1, 1000) == 1);
	CHECK(tick(&r, 0, 1050) == -1);
	/* BOTH: a hold from unzoomed zooms (to the first level) and the release returns */
	rig_start(&r, AE_ZOOM_BOTH, 2);
	CHECK(tick(&r, 1, 0) == 0);
	CHECK(tick(&r, 1, 300) == 0);
	CHECK(tick(&r, 1, 2000) == 0);
	CHECK(tick(&r, 0, 2010) == -1);
	/* BOTH: a hold while zoomed by taps keeps the level, the release returns to unzoomed */
	rig_start(&r, AE_ZOOM_BOTH, 2);
	tick(&r, 1, 0); tick(&r, 0, 50); /* level 0 */
	tick(&r, 1, 500); tick(&r, 0, 550); /* level 1 */
	CHECK(r.level == 1);
	CHECK(tick(&r, 1, 1000) == 1);
	CHECK(tick(&r, 1, 1400) == 1);
	CHECK(tick(&r, 1, 3000) == 1);
	CHECK(tick(&r, 0, 3010) == -1);
	/* the same at level 0 */
	rig_start(&r, AE_ZOOM_BOTH, 2);
	tick(&r, 1, 0); tick(&r, 0, 50);
	CHECK(tick(&r, 1, 1000) == 0 && tick(&r, 1, 1600) == 0 && tick(&r, 0, 1700) == -1);
	/* the tap time edge: exactly 250 ms is a tap, 251 a hold */
	rig_start(&r, AE_ZOOM_BOTH, 1);
	tick(&r, 1, 0);
	CHECK(tick(&r, 0, 250) == 0);
	rig_start(&r, AE_ZOOM_BOTH, 1);
	tick(&r, 1, 0);
	CHECK(tick(&r, 0, 251) == -1);
	/* and at a zoomed start: 250 steps, 251 returns */
	rig_start(&r, AE_ZOOM_BOTH, 2);
	tick(&r, 1, 0); tick(&r, 0, 10);
	tick(&r, 1, 1000);
	CHECK(tick(&r, 0, 1250) == 1);
	rig_start(&r, AE_ZOOM_BOTH, 2);
	tick(&r, 1, 0); tick(&r, 0, 10);
	tick(&r, 1, 1000);
	CHECK(tick(&r, 0, 1251) == -1);

	/* forced unzoom mid-hold (the game sets -1): the release neither re-zooms nor steps */
	{
		int mode;
		for (mode = AE_ZOOM_HOLD; mode <= AE_ZOOM_BOTH; mode++)
		{
			rig_start(&r, mode, 2);
			tick(&r, 1, 0);
			r.level = -1; /* weapon switch, reload, melee, damage, vehicle, death */
			CHECK(tick(&r, 1, 100) == -1);
			CHECK(tick(&r, 1, 900) == -1);
			CHECK(tick(&r, 0, 1000) == -1);
			/* a press after it is a fresh press */
			CHECK(tick(&r, 1, 2000) == 0);
			CHECK(tick(&r, 0, 2050 + (mode == AE_ZOOM_HOLD ? 0 : 400)) == -1);
		}
		/* forced while zoomed by taps, button then pressed and released as a tap: a fresh tap from unzoomed */
		rig_start(&r, AE_ZOOM_BOTH, 2);
		tick(&r, 1, 0); tick(&r, 0, 50);
		r.level = -1;
		CHECK(tick(&r, 0, 100) == -1);
		CHECK(tick(&r, 1, 200) == 0 && tick(&r, 0, 250) == 0);
		/* forced while a tap is pending at level 1: the release does not step */
		rig_start(&r, AE_ZOOM_BOTH, 2);
		tick(&r, 1, 0); tick(&r, 0, 50); tick(&r, 1, 500); tick(&r, 0, 550);
		tick(&r, 1, 1000);
		r.level = -1;
		CHECK(tick(&r, 0, 1100) == -1);
	}

	/* a press while blocked (reloading, no camera control) does nothing, and its release neither */
	{
		int mode;

		for (mode = AE_ZOOM_HOLD; mode <= AE_ZOOM_BOTH; mode++)
		{
			rig_start(&r, mode, 2);
			r.blocked = 1;
			CHECK(tick(&r, 1, 0) == -1);
			r.blocked = 0;
			CHECK(tick(&r, 1, 100) == -1); /* the press began blocked: not zooming now */
			CHECK(tick(&r, 0, 2000) == -1);
			CHECK(tick(&r, 1, 3000) == 0);
			/* a zoomed press, blocked at its release (reloading began): a tap does not step */
			rig_start(&r, mode, 2);
			tick(&r, 1, 0); tick(&r, 0, 50);
			tick(&r, 1, 1000);
			r.blocked = 1;
			CHECK(tick(&r, 0, 1100) == (mode == AE_ZOOM_HOLD ? -1 : 0));
		}
	}

	/* a weapon that does not zoom (count 0), BOTH: taps and holds leave it unzoomed */
	rig_start(&r, AE_ZOOM_BOTH, 0);
	CHECK(tick(&r, 1, 0) == -1 && tick(&r, 0, 50) == -1);
	CHECK(tick(&r, 1, 1000) == -1 && tick(&r, 0, 2000) == -1);

	/* the engine moves the level to another non-negative value mid-press (a different weapon's level): the press ends */
	{
		int mode;

		for (mode = AE_ZOOM_HOLD; mode <= AE_ZOOM_BOTH; mode++)
		{
			rig_start(&r, mode, 2);
			tick(&r, 1, 0);
			r.level = 1;
			CHECK(tick(&r, 1, 100) == 1);
			CHECK(tick(&r, 0, 2000) == 1); /* released: neither unzoomed nor stepped */
		}
	}

	/* the glue's parts: the held bit, the press filter, one machine for each local player, the mode change */
	{
		unsigned long flags = 0x25ul; /* engine bits 0, 2 and 5 */
		struct ae_zoom_bank bank;

		ae_zoom_flags_note_held(&flags, 1);
		CHECK(flags == (0x25ul | (1ul << 12)) && AE_ZOOM_HELD_BIT == 12);
		ae_zoom_flags_note_held(&flags, 0);
		CHECK(flags == 0x25ul);
		/* TOGGLE leaves the press bit (2); HOLD and BOTH take it out and nothing else */
		ae_zoom_flags_filter_press(AE_ZOOM_TOGGLE, &flags, 2);
		CHECK(flags == 0x25ul);
		ae_zoom_flags_filter_press(AE_ZOOM_HOLD, &flags, 2);
		CHECK(flags == 0x21ul);
		flags = 0x25ul;
		ae_zoom_flags_filter_press(AE_ZOOM_BOTH, &flags, 2);
		CHECK(flags == 0x21ul);

		memset(&bank, 0, sizeof(bank));
		ae_zoom_bank_set_mode(&bank, AE_ZOOM_HOLD);
		/* player 0 holds, player 1 does not: only player 0 zooms; then 1 presses while 0 releases */
		{
			unsigned long down = 1ul << 12;
			short level0 = -1, level1 = -1;

			level0 = ae_zoom_bank_step(&bank, 0, HOLD_MS, down, 0, level0, 2, 0);
			level1 = ae_zoom_bank_step(&bank, 1, HOLD_MS, 0, 0, level1, 2, 0);
			CHECK(level0 == 0 && level1 == -1);
			level0 = ae_zoom_bank_step(&bank, 0, HOLD_MS, 0, 100, level0, 2, 0);
			level1 = ae_zoom_bank_step(&bank, 1, HOLD_MS, down, 100, level1, 2, 0);
			CHECK(level0 == -1 && level1 == 0);
			/* a player out of range, and the TOGGLE mode, return the level untouched */
			CHECK(ae_zoom_bank_step(&bank, 4, HOLD_MS, down, 200, 1, 2, 0) == 1);
			CHECK(ae_zoom_bank_step(&bank, -1, HOLD_MS, down, 200, 1, 2, 0) == 1);
			/* the same mode again changes nothing: player 1's press goes on */
			ae_zoom_bank_set_mode(&bank, AE_ZOOM_HOLD);
			level1 = ae_zoom_bank_step(&bank, 1, HOLD_MS, 0, 300, level1, 2, 0);
			CHECK(level1 == -1);
			/* a change of mode while pressed lets go of the press: a release afterwards does nothing */
			level1 = ae_zoom_bank_step(&bank, 1, HOLD_MS, down, 900, level1, 2, 0);
			CHECK(level1 == 0);
			ae_zoom_bank_set_mode(&bank, AE_ZOOM_BOTH);
			CHECK(bank.states[1].was_held == 0);
			level1 = ae_zoom_bank_step(&bank, 1, HOLD_MS, 0, 5000, level1, 2, 0); /* not a falling edge now */
			CHECK(level1 == 0);
			ae_zoom_bank_set_mode(&bank, AE_ZOOM_TOGGLE);
			CHECK(ae_zoom_bank_step(&bank, 0, HOLD_MS, down, 6000, -1, 2, 0) == -1);
		}
		/* (0.3.7) each player their own mode, set every tick as the glue does (player 1 on HOLD with the mouse, player 2
		on TOGGLE with a pad): player 1's held button stays one press (zoomed while held, out once let go of), and
		player 2's mode never lets go of it */
		{
			unsigned long down = 1ul << 12;
			short level0 = -1, level1 = 1, tick;

			memset(&bank, 0, sizeof(bank));
			for (tick = 0; tick < 6; tick++)
			{
				ae_zoom_bank_set_player(&bank, 0, AE_ZOOM_HOLD, AE_ZOOM_RESET_FIRST);
				ae_zoom_bank_set_player(&bank, 1, AE_ZOOM_TOGGLE, AE_ZOOM_RESET_LAST);
				level0 = ae_zoom_bank_step(&bank, 0, HOLD_MS, down, tick * 33L, level0, 2, 0);
				CHECK(ae_zoom_bank_step(&bank, 1, HOLD_MS, down, tick * 33L, level1, 2, 0) == level1);
				CHECK(level0 == 0 && bank.states[0].was_held == 1);
			}
			ae_zoom_bank_set_player(&bank, 0, AE_ZOOM_HOLD, AE_ZOOM_RESET_FIRST);
			ae_zoom_bank_set_player(&bank, 1, AE_ZOOM_TOGGLE, AE_ZOOM_RESET_LAST);
			level0 = ae_zoom_bank_step(&bank, 0, HOLD_MS, 0, 300, level0, 2, 0);
			CHECK(level0 == -1);
			CHECK(ae_zoom_bank_mode(&bank, 0) == AE_ZOOM_HOLD && ae_zoom_bank_mode(&bank, 1) == AE_ZOOM_TOGGLE);
			CHECK(bank.resets[0] == AE_ZOOM_RESET_FIRST && bank.resets[1] == AE_ZOOM_RESET_LAST);
			CHECK(ae_zoom_bank_mode(&bank, 4) == AE_ZOOM_TOGGLE);
			/* a player's own mode change lets go of their press only */
			level0 = ae_zoom_bank_step(&bank, 0, HOLD_MS, down, 400, level0, 2, 0);
			ae_zoom_bank_set_player(&bank, 1, AE_ZOOM_BOTH, AE_ZOOM_RESET_LAST);
			CHECK(bank.states[0].was_held == 1);
			ae_zoom_bank_set_player(&bank, 0, AE_ZOOM_BOTH, AE_ZOOM_RESET_FIRST);
			CHECK(bank.states[0].was_held == 0);
		}
	}


	/* ---- the zoom level input: the wheel and the flashlight, while the zoom is held */
	/* HOLD, sniper (3 levels would be 3; here 2 levels: -1, 0, 1): wheel up steps, clamps at the last, down steps back and never below the first, never unzooms */
	rig_start(&r, AE_ZOOM_HOLD, 2);
	CHECK(T(&r, 1, 0) == 0);
	CHECK(W(&r, 1, 50, -1) == 0); /* down at the first: stays */
	CHECK(W(&r, 1, 100, 1) == 1);
	CHECK(W(&r, 1, 150, 1) == 1); /* up at the last: stays */
	CHECK(W(&r, 1, 200, -1) == 0);
	CHECK(W(&r, 1, 250, -1) == 0);
	CHECK(T(&r, 1, 300) == 0);
	CHECK(T(&r, 0, 400) == -1); /* the release unzooms */
	/* a 4-level weapon: steps one level each */
	rig_start(&r, AE_ZOOM_HOLD, 4);
	T(&r, 1, 0);
	CHECK(W(&r, 1, 10, 1) == 1 && W(&r, 1, 20, 1) == 2 && W(&r, 1, 30, 1) == 3 && W(&r, 1, 40, 1) == 3);
	CHECK(W(&r, 1, 50, -1) == 2);
	/* the flashlight steps to the next level and wraps to the first after the last */
	rig_start(&r, AE_ZOOM_HOLD, 3);
	T(&r, 1, 0);
	CHECK(F(&r, 1, 10) == 1 && F(&r, 1, 20) == 2 && F(&r, 1, 30) == 0 && F(&r, 1, 40) == 1);
	CHECK(T(&r, 0, 100) == -1);
	/* the pistol (one level): every extra is a no-op, in both modes */
	rig_start(&r, AE_ZOOM_HOLD, 1);
	T(&r, 1, 0);
	CHECK(W(&r, 1, 10, 1) == 0 && W(&r, 1, 20, -1) == 0 && F(&r, 1, 30) == 0);
	CHECK(T(&r, 0, 100) == -1);
	rig_start(&r, AE_ZOOM_BOTH, 1);
	T(&r, 1, 0);
	CHECK(W(&r, 1, 10, 1) == 0 && F(&r, 1, 30) == 0);
	CHECK(T(&r, 0, 50) == -1); /* a step makes it a hold: released within the tap time, it still unzooms */
	/* BOTH: a step from unzoomed makes the press a hold, in the tap time or after; a step from a zoomed state too */
	rig_start(&r, AE_ZOOM_BOTH, 2);
	T(&r, 1, 0);
	CHECK(W(&r, 1, 100, 1) == 1);
	CHECK(T(&r, 0, 150) == -1);
	rig_start(&r, AE_ZOOM_BOTH, 2);
	T(&r, 1, 0); T(&r, 0, 50); /* a tap: level 0 */
	T(&r, 1, 1000);
	CHECK(F(&r, 1, 1050) == 1);
	CHECK(T(&r, 0, 1100) == -1);
	/* steps are nothing outside a press the machine acts on: unzoomed, a blocked press, a level the game took away */
	rig_start(&r, AE_ZOOM_HOLD, 2);
	CHECK(W(&r, 0, 0, 1) == -1 && F(&r, 0, 10) == -1);
	r.blocked = 1;
	T(&r, 1, 100);
	r.blocked = 0;
	CHECK(W(&r, 1, 110, 1) == -1 && F(&r, 1, 120) == -1);
	T(&r, 0, 130);
	T(&r, 1, 200);
	r.level = -1; /* forced */
	CHECK(W(&r, 1, 210, 1) == -1);
	/* the second-level steps still start from the level the press is at: BOTH, zoomed by taps, hold, wheel down */
	rig_start(&r, AE_ZOOM_BOTH, 2);
	T(&r, 1, 0); T(&r, 0, 50); T(&r, 1, 500); T(&r, 0, 550); /* level 1 */
	T(&r, 1, 1000);
	CHECK(W(&r, 1, 1100, -1) == 0);
	CHECK(T(&r, 0, 1200) == -1);

	/* ---- double press and hold (HOLD only): a tap, then a press within the tap time of its release, starts at the second level */
	rig_start(&r, AE_ZOOM_HOLD, 2);
	T(&r, 1, 0); T(&r, 0, 100); /* a tap */
	CHECK(T(&r, 1, 350) == 1); /* 250 ms after the release: a double press */
	CHECK(T(&r, 1, 2000) == 1);
	CHECK(T(&r, 0, 2100) == -1);
	CHECK(T(&r, 1, 2200) == 0); /* the next press is plain again (the long one was no tap) */
	rig_start(&r, AE_ZOOM_HOLD, 2);
	T(&r, 1, 0); T(&r, 0, 100);
	CHECK(T(&r, 1, 351) == 0); /* 251 ms: too late */
	/* the first press must be a tap: 250 ms is, 251 is not */
	rig_start(&r, AE_ZOOM_HOLD, 2);
	T(&r, 1, 0); T(&r, 0, 250);
	CHECK(T(&r, 1, 300) == 1);
	rig_start(&r, AE_ZOOM_HOLD, 2);
	T(&r, 1, 0); T(&r, 0, 251);
	CHECK(T(&r, 1, 300) == 0);
	/* a double press is used once: tap, tap-hold, release, press again is plain */
	rig_start(&r, AE_ZOOM_HOLD, 2);
	T(&r, 1, 0); T(&r, 0, 50); T(&r, 1, 100);
	CHECK(r.level == 1);
	T(&r, 0, 150); /* released within the tap time: a tap again */
	CHECK(T(&r, 1, 200) == 1);
	/* another weapon in between: no double press */
	rig_start(&r, AE_ZOOM_HOLD, 2);
	tickx(&r, 1, 0, 0, 0, AE_ZOOM_RESET_FIRST, -1, 7); tickx(&r, 0, 50, 0, 0, AE_ZOOM_RESET_FIRST, -1, 7);
	CHECK(tickx(&r, 1, 100, 0, 0, AE_ZOOM_RESET_FIRST, -1, 8) == 0);
	/* the pistol: the double press is the first level */
	rig_start(&r, AE_ZOOM_HOLD, 1);
	T(&r, 1, 0); T(&r, 0, 50);
	CHECK(T(&r, 1, 100) == 0);
	/* BOTH keeps its meaning: tap, tap toggles (first level, then the second), TOGGLE has nothing of it */
	rig_start(&r, AE_ZOOM_BOTH, 2);
	T(&r, 1, 0); T(&r, 0, 50);
	CHECK(T(&r, 1, 100) == 0);
	CHECK(T(&r, 0, 150) == 1);
	rig_start(&r, AE_ZOOM_TOGGLE, 2);
	{
		struct ae_zoom_bank bank;
		unsigned long unit = 0xfful;
		unsigned long pflags = (1ul << AE_ZOOM_HELD_BIT) | (1ul << AE_ZOOM_FLASH_BIT);

		memset(&bank, 0, sizeof(bank));
		ae_zoom_bank_set_mode(&bank, AE_ZOOM_TOGGLE);
		/* no effect in TOGGLE: the step returns the level, the flashlight stays, nothing is queued */
		ae_zoom_bank_filter_flash(&bank, 0, &pflags, &unit, 3);
		CHECK(unit == 0xfful);
		CHECK(ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, pflags, 0, 0, 2, 0, 5, 1) == 0);
		CHECK(ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, pflags, 10, 1, 2, 0, 5, -1) == 1);
	}

	/* ---- zoom reset: first or last, a hold from unzoomed */
	rig_start(&r, AE_ZOOM_HOLD, 2);
	CHECK(tickx(&r, 1, 0, 0, 0, AE_ZOOM_RESET_FIRST, 1, 7) == 0);
	tickx(&r, 0, 1000, 0, 0, AE_ZOOM_RESET_FIRST, 1, 7);
	CHECK(tickx(&r, 1, 2000, 0, 0, AE_ZOOM_RESET_LAST, 1, 7) == 1);
	tickx(&r, 0, 3000, 0, 0, AE_ZOOM_RESET_LAST, 1, 7);
	CHECK(tickx(&r, 1, 4000, 0, 0, AE_ZOOM_RESET_LAST, -1, 7) == 0); /* none remembered */
	tickx(&r, 0, 5000, 0, 0, AE_ZOOM_RESET_LAST, -1, 7);
	CHECK(tickx(&r, 1, 6000, 0, 0, AE_ZOOM_RESET_LAST, 5, 7) == 1); /* pinned to the weapon's levels */
	/* last, a 1-level weapon: always its only level */
	rig_start(&r, AE_ZOOM_HOLD, 1);
	CHECK(tickx(&r, 1, 0, 0, 0, AE_ZOOM_RESET_LAST, 3, 7) == 0);
	/* BOTH, last: a tap is the first level; a hold goes on to the last one used after the tap time; released before it, a tap */
	rig_start(&r, AE_ZOOM_BOTH, 2);
	CHECK(tickx(&r, 1, 0, 0, 0, AE_ZOOM_RESET_LAST, 1, 7) == 0);
	CHECK(tickx(&r, 1, 250, 0, 0, AE_ZOOM_RESET_LAST, 1, 7) == 0);
	CHECK(tickx(&r, 1, 251, 0, 0, AE_ZOOM_RESET_LAST, 1, 7) == 1);
	CHECK(tickx(&r, 0, 600, 0, 0, AE_ZOOM_RESET_LAST, 1, 7) == -1);
	rig_start(&r, AE_ZOOM_BOTH, 2);
	tickx(&r, 1, 0, 0, 0, AE_ZOOM_RESET_LAST, 1, 7);
	CHECK(tickx(&r, 0, 100, 0, 0, AE_ZOOM_RESET_LAST, 1, 7) == 0);
	/* BOTH, first: the hold stays at the first level */
	rig_start(&r, AE_ZOOM_BOTH, 2);
	tickx(&r, 1, 0, 0, 0, AE_ZOOM_RESET_FIRST, 1, 7);
	CHECK(tickx(&r, 1, 500, 0, 0, AE_ZOOM_RESET_FIRST, 1, 7) == 0);
	/* last with a double press in HOLD: the second level wins over the remembered */
	rig_start(&r, AE_ZOOM_HOLD, 3);
	tickx(&r, 1, 0, 0, 0, AE_ZOOM_RESET_LAST, 2, 7); tickx(&r, 0, 50, 0, 0, AE_ZOOM_RESET_LAST, 2, 7);
	CHECK(tickx(&r, 1, 100, 0, 0, AE_ZOOM_RESET_LAST, 2, 7) == 1);
	CHECK(ae_zoom_reset_from_text("last") == AE_ZOOM_RESET_LAST && ae_zoom_reset_from_text("LAST") == AE_ZOOM_RESET_LAST);
	CHECK(ae_zoom_reset_from_text("first") == AE_ZOOM_RESET_FIRST && ae_zoom_reset_from_text("x") == AE_ZOOM_RESET_FIRST
		&& ae_zoom_reset_from_text(NULL) == AE_ZOOM_RESET_FIRST);
	CHECK(ae_zoom_clamp_level(0, 0) == -1 && ae_zoom_clamp_level(-1, 2) == 0 && ae_zoom_clamp_level(9, 2) == 1);

	/* ---- the bank: memory for each player and weapon, the flashlight latch, the wheel */
	{
		struct ae_zoom_bank bank;
		unsigned long held = 1ul << AE_ZOOM_HELD_BIT;
		short a, b, c;
		long t = 0;

		memset(&bank, 0, sizeof(bank));
		ae_zoom_bank_set_mode(&bank, AE_ZOOM_HOLD);
		ae_zoom_bank_set_reset(&bank, AE_ZOOM_RESET_LAST);
		/* player 0, weapon 100: zoom, wheel up to level 1, release: remembered 1 */
		a = ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, held, t += 1000, -1, 2, 0, 100, 0);
		CHECK(a == 0);
		a = ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, held, t += 10, a, 2, 0, 100, 1);
		CHECK(a == 1);
		a = ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, 0, t += 600, a, 2, 0, 100, 0);
		CHECK(a == -1 && ae_zoom_bank_recall(&bank, 0, 100) == 1);
		/* the same player, another weapon: none; another player, the same weapon: none */
		CHECK(ae_zoom_bank_recall(&bank, 0, 200) == -1 && ae_zoom_bank_recall(&bank, 1, 100) == -1);
		/* player 1 holds weapon 200 and stays at the first; player 0's next hold starts at 1 */
		b = ae_zoom_bank_step_ex(&bank, 1, HOLD_MS, held, t += 1000, -1, 2, 0, 200, 0);
		a = ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, held, t, -1, 2, 0, 100, 0);
		CHECK(b == 0 && a == 1);
		b = ae_zoom_bank_step_ex(&bank, 1, HOLD_MS, 0, t += 600, b, 2, 0, 200, 0);
		a = ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, 0, t, a, 2, 0, 100, 0);
		CHECK(b == -1 && a == -1 && ae_zoom_bank_recall(&bank, 1, 200) == 0 && ae_zoom_bank_recall(&bank, 0, 100) == 1);
		/* the wheel goes to the player it is given to only */
		a = ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, held, t += 1000, -1, 2, 0, 100, 0);
		c = ae_zoom_bank_step_ex(&bank, 1, HOLD_MS, held, t, -1, 2, 0, 200, 0);
		CHECK(a == 1 && c == 0);
		a = ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, held, t += 10, a, 2, 0, 100, -1);
		c = ae_zoom_bank_step_ex(&bank, 1, HOLD_MS, held, t, c, 2, 0, 200, 0);
		CHECK(a == 0 && c == 0);
		/* five weapons: the oldest is forgotten, the rest stay */
		{
			long w;
			for (w = 300; w < 305; w++)
				ae_zoom_bank_remember(&bank, 2, w, 1);
			CHECK(ae_zoom_bank_recall(&bank, 2, 300) == -1 && ae_zoom_bank_recall(&bank, 2, 304) == 1);
			ae_zoom_bank_remember(&bank, 2, 302, 0);
			CHECK(ae_zoom_bank_recall(&bank, 2, 302) == 0 && ae_zoom_bank_recall(&bank, 2, 303) == 1);
		}
		/* a mode change forgets nothing it must not, but lets go of the machines */
		ae_zoom_bank_set_mode(&bank, AE_ZOOM_BOTH);
		CHECK(bank.states[0].was_held == 0);
	}
	{
		struct ae_zoom_bank bank;
		unsigned long light = 1ul << 3;
		unsigned long unit = 0xfful;
		unsigned long pf = (1ul << AE_ZOOM_HELD_BIT) | (1ul << AE_ZOOM_FLASH_BIT);
		unsigned long pf_nozoom = 1ul << AE_ZOOM_FLASH_BIT;

		memset(&bank, 0, sizeof(bank));
		ae_zoom_bank_set_mode(&bank, AE_ZOOM_HOLD);
		/* zoom up, flashlight pressed: the light bit is taken (player 0), player 1's is not */
		ae_zoom_bank_filter_flash(&bank, 0, &pf, &unit, 3);
		CHECK(!(unit & light) && (unit & ~light) == (0xfful & ~light));
		unit = 0xfful;
		ae_zoom_bank_filter_flash(&bank, 1, &pf_nozoom, &unit, 3);
		CHECK(unit == 0xfful);
		/* the press queued one step, once */
		CHECK(bank.flash_pending[0] == 1 && bank.flash_pending[1] == 0);
		{
			short l = ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, pf, 0, -1, 3, 0, 9, 0);
			CHECK(l == 1); /* the press (first level) and the queued step */
			l = ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, pf, 10, l, 3, 0, 9, 0);
			CHECK(l == 1 && bank.flash_pending[0] == 0);
		}
		/* still held with the zoom let go of: the light stays taken until the flashlight is let go of; then it works */
		pf = (1ul << AE_ZOOM_FLASH_BIT);
		unit = 0xfful;
		ae_zoom_bank_filter_flash(&bank, 0, &pf, &unit, 3);
		CHECK(!(unit & light));
		pf = 0;
		unit = 0xfful;
		ae_zoom_bank_filter_flash(&bank, 0, &pf, &unit, 3);
		CHECK(unit == 0xfful);
		pf = 1ul << AE_ZOOM_FLASH_BIT;
		ae_zoom_bank_filter_flash(&bank, 0, &pf, &unit, 3);
		CHECK(unit == 0xfful); /* a press without zoom is the light's */
		/* held first, then the zoom: the flashlight is the light's until let go of (no step) */
		pf = 0;
		ae_zoom_bank_filter_flash(&bank, 0, &pf, &unit, 3);
		pf = 1ul << AE_ZOOM_FLASH_BIT;
		ae_zoom_bank_filter_flash(&bank, 0, &pf, &unit, 3);
		pf = (1ul << AE_ZOOM_HELD_BIT) | (1ul << AE_ZOOM_FLASH_BIT);
		unit = 0xfful;
		ae_zoom_bank_filter_flash(&bank, 0, &pf, &unit, 3);
		CHECK(bank.flash_pending[0] == 0 && (unit & light));
	}

	/* the wheel is the zoom's only while a held zoom can act (a menu's zeroed blob, no zoomable weapon, reloading, TOGGLE) */
	{
		unsigned long down = 1ul << AE_ZOOM_HELD_BIT;

		CHECK(ae_zoom_wheel_wanted(AE_ZOOM_HOLD, down, 2, 0, 0) && ae_zoom_wheel_wanted(AE_ZOOM_BOTH, down, 1, 0, 0));
		CHECK(!ae_zoom_wheel_wanted(AE_ZOOM_HOLD, 0, 2, 0, 0)); /* a pause menu zeroed the blob */
		CHECK(!ae_zoom_wheel_wanted(AE_ZOOM_HOLD, down, 0, 0, -1)); /* a weapon that does not zoom */
		CHECK(!ae_zoom_wheel_wanted(AE_ZOOM_HOLD, down, 2, 1, 0)); /* reloading or zoom blocked */
		CHECK(!ae_zoom_wheel_wanted(AE_ZOOM_HOLD, down, 2, 0, -1)); /* not zoomed (taken off by the game) */
		CHECK(!ae_zoom_wheel_wanted(AE_ZOOM_TOGGLE, down, 2, 0, 0));
	}
	/* WHEEL ZOOM: the wheel by itself is the zoom's for a weapon that zooms and may act, in any mode; up steps in
	(unzoomed to the first, the last stays), down out (the first to unzoomed); a weapon that does not zoom stays out */
	{
		struct ae_zoom_bank bank;

		CHECK(ae_zoom_wheel_free(1, 2, 0) && ae_zoom_wheel_free(1, 1, 0));
		CHECK(!ae_zoom_wheel_free(0, 2, 0) && !ae_zoom_wheel_free(1, 0, 0) && !ae_zoom_wheel_free(1, 2, 1));
		CHECK(ae_zoom_wheel_step(-1, 2, 1) == 0 && ae_zoom_wheel_step(0, 2, 1) == 1 && ae_zoom_wheel_step(1, 2, 1) == 1);
		CHECK(ae_zoom_wheel_step(1, 2, -1) == 0 && ae_zoom_wheel_step(0, 2, -1) == -1 && ae_zoom_wheel_step(-1, 2, -1) == -1);
		CHECK(ae_zoom_wheel_step(-1, 1, 1) == 0 && ae_zoom_wheel_step(0, 1, 1) == 0);
		CHECK(ae_zoom_wheel_step(-1, 0, 1) == -1 && ae_zoom_wheel_step(0, 2, 0) == 0);
		/* HOLD and BOTH with the zoom not held leave a level the wheel set (their machines are idle) */
		memset(&bank, 0, sizeof(bank));
		ae_zoom_bank_set_mode(&bank, AE_ZOOM_HOLD);
		CHECK(ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, 0, 0, 0, 2, 0, 100, 0) == 0);
		CHECK(ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, 0, 50, 1, 2, 0, 100, 0) == 1);
		ae_zoom_bank_set_mode(&bank, AE_ZOOM_BOTH);
		CHECK(ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, 0, 100, 1, 2, 0, 100, 0) == 1);
	}
	/* a new game forgets the last levels, the presses and the latches, and keeps the mode and the reset */
	{
		struct ae_zoom_bank bank;
		unsigned long held = 1ul << AE_ZOOM_HELD_BIT;

		memset(&bank, 0, sizeof(bank));
		ae_zoom_bank_set_mode(&bank, AE_ZOOM_HOLD);
		ae_zoom_bank_set_reset(&bank, AE_ZOOM_RESET_LAST);
		ae_zoom_bank_remember(&bank, 0, 100, 1);
		ae_zoom_bank_remember(&bank, 2, 100, 1);
		ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, held, 0, -1, 2, 0, 100, 0);
		CHECK(bank.states[0].was_held == 1);
		ae_zoom_bank_forget(&bank);
		CHECK(ae_zoom_bank_recall(&bank, 0, 100) == -1 && ae_zoom_bank_recall(&bank, 2, 100) == -1);
		CHECK(bank.states[0].was_held == 0 && bank.modes[0] == AE_ZOOM_HOLD && bank.resets[3] == AE_ZOOM_RESET_LAST);
		/* the first weapon of the new game (the same object index) starts at the first level */
		CHECK(ae_zoom_bank_step_ex(&bank, 0, HOLD_MS, held, 100, -1, 2, 0, 100, 0) == 0);
		/* a mode change with the flashlight down: the next tick's press is not seen as new */
		ae_zoom_bank_set_mode(&bank, AE_ZOOM_BOTH);
		CHECK(bank.flash_was_down[0] == 0);
	}

	/* the text */
	CHECK(ae_zoom_mode_from_text("toggle") == AE_ZOOM_TOGGLE);
	CHECK(ae_zoom_mode_from_text("hold") == AE_ZOOM_HOLD);
	CHECK(ae_zoom_mode_from_text("BOTH") == AE_ZOOM_BOTH);
	CHECK(ae_zoom_mode_from_text("") == AE_ZOOM_TOGGLE);
	CHECK(ae_zoom_mode_from_text("holdx") == AE_ZOOM_TOGGLE);
	CHECK(ae_zoom_mode_from_text(NULL) == AE_ZOOM_TOGGLE);
	CHECK(ae_zoom_hold_ms(0.25) == 250 && ae_zoom_hold_ms(0.0) == 100 && ae_zoom_hold_ms(5.0) == 1000);
	CHECK(ae_zoom_hold_ms(NAN) == 250);

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_zoom_test: ok\n");
	return failures != 0;
}
