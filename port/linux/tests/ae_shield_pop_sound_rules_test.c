#include <stdio.h>

#include "ae_shield_pop_sound_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int logged = 0;

	/* MCC present: its sound, whether or not the map has the stock one */
	CHECK(ae_shield_pop_sound_pick(1, 1) == _ae_shield_pop_sound_mcc);
	CHECK(ae_shield_pop_sound_pick(1, 0) == _ae_shield_pop_sound_mcc);
	/* MCC missing (no install, missing file, bad read): the stock sound, the map's own */
	CHECK(ae_shield_pop_sound_pick(0, 1) == _ae_shield_pop_sound_stock);
	/* neither: nothing */
	CHECK(ae_shield_pop_sound_pick(0, 0) == _ae_shield_pop_sound_none);
	/* the fallback is logged once; the MCC sound and nothing never are */
	CHECK(ae_shield_pop_sound_log_fallback(_ae_shield_pop_sound_mcc, &logged) == 0 && logged == 0);
	CHECK(ae_shield_pop_sound_log_fallback(_ae_shield_pop_sound_none, &logged) == 0 && logged == 0);
	CHECK(ae_shield_pop_sound_log_fallback(_ae_shield_pop_sound_stock, &logged) == 1 && logged == 1);
	CHECK(ae_shield_pop_sound_log_fallback(_ae_shield_pop_sound_stock, &logged) == 0);
	CHECK(ae_shield_pop_sound_log_fallback(_ae_shield_pop_sound_stock, NULL) == 0);
	if (failures)
		return 1;
	printf("ae_shield_pop_sound_rules_test: ok\n");
	return 0;
}
