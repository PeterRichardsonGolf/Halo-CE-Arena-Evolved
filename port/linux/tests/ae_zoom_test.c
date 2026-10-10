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
			level1 = ae_zoom_bank_step(&bank, 1, HOLD_MS, down, 400, level1, 2, 0);
			CHECK(level1 == 0);
			ae_zoom_bank_set_mode(&bank, AE_ZOOM_BOTH);
			CHECK(bank.states[1].was_held == 0);
			level1 = ae_zoom_bank_step(&bank, 1, HOLD_MS, 0, 5000, level1, 2, 0); /* not a falling edge now */
			CHECK(level1 == 0);
			ae_zoom_bank_set_mode(&bank, AE_ZOOM_TOGGLE);
			CHECK(ae_zoom_bank_step(&bank, 0, HOLD_MS, down, 6000, -1, 2, 0) == -1);
		}
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
