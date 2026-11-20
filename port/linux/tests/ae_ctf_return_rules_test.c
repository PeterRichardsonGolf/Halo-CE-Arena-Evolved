#include <stdio.h>
#include "ae_ctf_return_rules.h"
#include "ae_variant_ext_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_variant_ext ext;

	/* the time a loose flag lies: Halo CE's 17 s in CE, 30 s in the other two; FLAG RESET TIME wins in all */
	CHECK(ae_ctf_return_seconds(AE_EXT_CTF_RETURN_CE, 0) == 17);
	CHECK(ae_ctf_return_seconds(AE_EXT_CTF_RETURN_H2, 0) == 30);
	CHECK(ae_ctf_return_seconds(AE_EXT_CTF_RETURN_STAND, 0) == 30);
	CHECK(ae_ctf_return_seconds(AE_EXT_CTF_RETURN_CE, 15) == 15);
	CHECK(ae_ctf_return_seconds(AE_EXT_CTF_RETURN_H2, 45) == 45);
	CHECK(ae_ctf_return_seconds(AE_EXT_CTF_RETURN_STAND, 10) == 10);
	/* (an unknown mode is CE's) */
	CHECK(ae_ctf_return_seconds(9, 0) == 17);

	/* a tick's count: H2 TIMER runs alone and stops while an enemy is by the flag; defenders do nothing there */
	CHECK(ae_ctf_return_step(AE_EXT_CTF_RETURN_H2, 0, 0) == 1);
	CHECK(ae_ctf_return_step(AE_EXT_CTF_RETURN_H2, 3, 0) == 1);
	CHECK(ae_ctf_return_step(AE_EXT_CTF_RETURN_H2, 0, 1) == 0);
	CHECK(ae_ctf_return_step(AE_EXT_CTF_RETURN_H2, 2, 1) == 0);
	/* STAND-BY: each defender by it (at most 2 count) adds 4; an enemy by it stops it */
	CHECK(ae_ctf_return_step(AE_EXT_CTF_RETURN_STAND, 0, 0) == 1);
	CHECK(ae_ctf_return_step(AE_EXT_CTF_RETURN_STAND, 1, 0) == 5);
	CHECK(ae_ctf_return_step(AE_EXT_CTF_RETURN_STAND, 2, 0) == 9);
	CHECK(ae_ctf_return_step(AE_EXT_CTF_RETURN_STAND, 6, 0) == 9);
	CHECK(ae_ctf_return_step(AE_EXT_CTF_RETURN_STAND, 2, 1) == 0);
	/* (CE keeps its own clock: no count) */
	CHECK(ae_ctf_return_step(AE_EXT_CTF_RETURN_CE, 2, 0) == 0);
	/* one defender sends a 30 s flag home in 6 s, two in a little over 3 */
	CHECK(30 * 30 / ae_ctf_return_step(AE_EXT_CTF_RETURN_STAND, 1, 0) == 180);

	/* whether a defender's touch sends the flag home: not in STAND-BY */
	CHECK(ae_ctf_return_touch_returns(AE_EXT_CTF_RETURN_CE));
	CHECK(ae_ctf_return_touch_returns(AE_EXT_CTF_RETURN_H2));
	CHECK(!ae_ctf_return_touch_returns(AE_EXT_CTF_RETURN_STAND));

	/* FLAG PICKUP HOLD: an enemy's flag is taken by the hold; the own team's flag stays a touch; TOUCH is Halo CE's */
	CHECK(ae_ctf_hold_pickup(AE_EXT_CTF_HOLD_PICKUP, 0, 1));
	CHECK(!ae_ctf_hold_pickup(AE_EXT_CTF_HOLD_PICKUP, 1, 1));
	CHECK(!ae_ctf_hold_pickup(0, 0, 1));
	CHECK(!ae_ctf_hold_pickup(AE_EXT_CTF_SUDDEN_DEATH | AE_EXT_CTF_NO_DRIVING, 0, 1));
	/* (no team: no hold) */
	CHECK(!ae_ctf_hold_pickup(AE_EXT_CTF_HOLD_PICKUP, -1, 1));
	ae_variant_ext_default(&ext);
	ext.ctf_flags = AE_EXT_CTF_HOLD_PICKUP | AE_EXT_CTF_SLOW_CARRIER;
	CHECK(ae_variant_ext_sanitize(&ext) && ext.ctf_flags == (AE_EXT_CTF_HOLD_PICKUP | AE_EXT_CTF_SLOW_CARRIER));

	/* SUDDEN DEATH: the game goes on while the objective is in play and for 5 s (150 ticks) after it last was */
	{
		long last = -1;

		/* (nothing in play when time runs out: it ends) */
		CHECK(!ae_sudden_death_hold(0, 1000, &last, 150));
		CHECK(ae_sudden_death_hold(1, 1001, &last, 150) && last == 1001);
		CHECK(ae_sudden_death_hold(0, 1100, &last, 150));
		CHECK(ae_sudden_death_hold(0, 1150, &last, 150));
		CHECK(!ae_sudden_death_hold(0, 1152, &last, 150));
		/* (in play again inside the grace: it starts again) */
		CHECK(ae_sudden_death_hold(1, 1160, &last, 150) && ae_sudden_death_hold(0, 1300, &last, 150));
		/* (a new game: a time before the last one forgets it) */
		CHECK(!ae_sudden_death_hold(0, 20, &last, 150) && last == -1);
	}

	/* the block keeps the byte, zero is CE (no block needed), out of range is CE again */
	ae_variant_ext_default(&ext);
	CHECK(ext.ctf_return == AE_EXT_CTF_RETURN_CE);
	ext.ctf_return = AE_EXT_CTF_RETURN_STAND;
	CHECK(!ae_variant_ext_is_default(&ext));
	CHECK(ae_variant_ext_sanitize(&ext) && ext.ctf_return == AE_EXT_CTF_RETURN_STAND);
	ext.ctf_return = AE_EXT_CTF_RETURN_COUNT;
	CHECK(!ae_variant_ext_sanitize(&ext) && ext.ctf_return == AE_EXT_CTF_RETURN_CE);

	if (failures)
		return 1;
	printf("ae_ctf_return_rules: ok\n");
	return 0;
}
