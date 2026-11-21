/* AE SEARCH AND DESTROY's pure rules (port/linux/game/ae_bomb_snd_rules.h): the gametype bit, a loose bomb out of reach (a pit),
BOMB DROP's bit and its hold */
#include <stdio.h>

#include "ae_bomb_snd_rules.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	AE_CHECK(ae_bomb_snd_flag(0x80) && !ae_bomb_snd_flag(0x7F));
	AE_CHECK(ae_bomb_snd_flag_set(0x03, 1) == 0x83 && ae_bomb_snd_flag_set(0x83, 0) == 0x03);
	/* (on the floor, up on a ledge, a short drop below the lowest spawn: in reach) */
	AE_CHECK(!ae_bomb_snd_out_of_reach(-0.2f, -0.2f, 6.0f));
	AE_CHECK(!ae_bomb_snd_out_of_reach(8.0f, -0.2f, 6.0f));
	AE_CHECK(!ae_bomb_snd_out_of_reach(-5.0f, -0.2f, 6.0f));
	/* (in a pit past a fall that kills) */
	AE_CHECK(ae_bomb_snd_out_of_reach(-7.0f, -0.2f, 6.0f));
	/* BOMB DROP: bit 6, apart from SEARCH AND DESTROY's and Chupa's bits 0-1 */
	AE_CHECK(ae_bomb_snd_drop_flag(0x40) && !ae_bomb_snd_drop_flag(0x83) && (AE_BOMB_SND_DROP_FLAG & AE_BOMB_SND_FLAG) == 0);
	{
		int held = 0, drop = 0, drops = 0, tick;

		/* (held with nothing in reach: one drop, at the hold's length, none after it while still held) */
		for (tick = 1; tick <= AE_BOMB_SND_DROP_HOLD_TICKS + 10; tick++)
		{
			held = ae_bomb_snd_drop_hold_next(held, 1, 1, &drop);
			drops += drop;
			if (drop)
				AE_CHECK(tick == AE_BOMB_SND_DROP_HOLD_TICKS);
		}
		AE_CHECK(drops == 1);
		/* (let go, or something in reach, starts it over) */
		held = ae_bomb_snd_drop_hold_next(AE_BOMB_SND_DROP_HOLD_TICKS - 1, 0, 1, &drop);
		AE_CHECK(held == 0 && !drop);
		held = ae_bomb_snd_drop_hold_next(AE_BOMB_SND_DROP_HOLD_TICKS - 1, 1, 0, &drop);
		AE_CHECK(held == 0 && !drop);
	}
	if (failures)
		return 1;
	printf("ae_bomb_snd_rules: all passed\n");
	return 0;
}
