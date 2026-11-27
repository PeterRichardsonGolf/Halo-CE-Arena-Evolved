#include <stdio.h>
#include "ae_record_focus.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define ALL ((1ul << AE_RF_BOXES) - 1)

int main(void)
{
	int ids[AE_RF_BOXES], count;
	unsigned long has;

	/* the boxes of a page, in reading order */
	count = ae_rf_list(1, ALL, ids);
	CHECK(count == 5 && ids[0] == AE_RF_ACCURACY && ids[1] == AE_RF_KILL_KINDS && ids[2] == AE_RF_ASSISTS &&
		ids[3] == AE_RF_DAMAGE && ids[4] == AE_RF_PER_GAME);
	count = ae_rf_list(2, ALL, ids);
	CHECK(count == 5 && ids[0] == AE_RF_OBJECTIVES && ids[2] == AE_RF_MEDALS && ids[3] == AE_RF_MAPS && ids[4] == AE_RF_WEAPONS);
	/* (the overview: a player's LAST GAME, CAREER TOTALS, HIGHLIGHTS over the strip) */
	has = (1ul << AE_RF_LAST) | (1ul << AE_RF_TOTALS) | (1ul << AE_RF_HIGHLIGHTS) | (1ul << AE_RF_VERSUS);
	count = ae_rf_list(0, has, ids);
	CHECK(count == 4 && ids[0] == AE_RF_LAST && ids[1] == AE_RF_TOTALS && ids[2] == AE_RF_HIGHLIGHTS && ids[3] == AE_RF_VERSUS);
	CHECK(ae_rf_move(0, has, AE_RF_LAST, AE_RF_RIGHT) == AE_RF_TOTALS && ae_rf_move(0, has, AE_RF_TOTALS, AE_RF_RIGHT) == AE_RF_HIGHLIGHTS);
	/* (one's own rivals: the strip under, a stop of its own) */
	has = (1ul << AE_RF_LAST) | (1ul << AE_RF_TOTALS) | (1ul << AE_RF_RIVALS);
	CHECK(ae_rf_move(0, has, AE_RF_TOTALS, AE_RF_DOWN) == AE_RF_RIVALS && ae_rf_move(0, has, AE_RF_RIVALS, AE_RF_UP) == AE_RF_TOTALS);
	/* only the boxes that have details are stops */
	has = (1ul << AE_RF_KINDS) | (1ul << AE_RF_WEAPONS);
	CHECK(ae_rf_list(2, has, ids) == 2 && ids[0] == AE_RF_KINDS && ids[1] == AE_RF_WEAPONS);
	CHECK(ae_rf_list(1, has, ids) == 0 && ae_rf_list(0, 0, ids) == 0);
	/* moving: right along the top row, down to the row under, never past an edge, no wrap */
	CHECK(ae_rf_move(1, ALL, AE_RF_ACCURACY, AE_RF_RIGHT) == AE_RF_KILL_KINDS);
	CHECK(ae_rf_move(1, ALL, AE_RF_KILL_KINDS, AE_RF_RIGHT) == AE_RF_ASSISTS);
	CHECK(ae_rf_move(1, ALL, AE_RF_ASSISTS, AE_RF_RIGHT) == AE_RF_ASSISTS);
	CHECK(ae_rf_move(1, ALL, AE_RF_ACCURACY, AE_RF_LEFT) == AE_RF_ACCURACY);
	CHECK(ae_rf_move(1, ALL, AE_RF_ACCURACY, AE_RF_DOWN) == AE_RF_DAMAGE);
	CHECK(ae_rf_move(1, ALL, AE_RF_KILL_KINDS, AE_RF_DOWN) == AE_RF_DAMAGE);
	CHECK(ae_rf_move(1, ALL, AE_RF_ASSISTS, AE_RF_DOWN) == AE_RF_PER_GAME);
	CHECK(ae_rf_move(1, ALL, AE_RF_DAMAGE, AE_RF_RIGHT) == AE_RF_PER_GAME);
	CHECK(ae_rf_move(1, ALL, AE_RF_PER_GAME, AE_RF_UP) == AE_RF_ASSISTS);
	CHECK(ae_rf_move(1, ALL, AE_RF_DAMAGE, AE_RF_UP) == AE_RF_ACCURACY || ae_rf_move(1, ALL, AE_RF_DAMAGE, AE_RF_UP) == AE_RF_KILL_KINDS);
	CHECK(ae_rf_move(1, ALL, AE_RF_DAMAGE, AE_RF_DOWN) == AE_RF_DAMAGE);
	/* a box with no details is skipped: from KINDS down to the nearest below when MAPS is gone */
	has = ALL & ~(1ul << AE_RF_MAPS);
	CHECK(ae_rf_move(2, has, AE_RF_KINDS, AE_RF_DOWN) == AE_RF_WEAPONS);
	CHECK(ae_rf_move(2, has, AE_RF_OBJECTIVES, AE_RF_DOWN) == AE_RF_WEAPONS || ae_rf_move(2, has, AE_RF_OBJECTIVES, AE_RF_DOWN) == AE_RF_OBJECTIVES);
	/* the first box when the focus is on none or on a box that is not a stop; none with no stop */
	CHECK(ae_rf_move(2, ALL, AE_RF_NONE, AE_RF_RIGHT) == AE_RF_OBJECTIVES);
	CHECK(ae_rf_move(2, ALL, AE_RF_ACCURACY, AE_RF_DOWN) == AE_RF_OBJECTIVES);
	CHECK(ae_rf_move(2, 0, AE_RF_MEDALS, AE_RF_DOWN) == AE_RF_NONE);
	/* a lone box stays put */
	CHECK(ae_rf_move(0, 1ul << AE_RF_LAST, AE_RF_LAST, AE_RF_RIGHT) == AE_RF_LAST && ae_rf_move(0, 1ul << AE_RF_LAST, AE_RF_LAST, AE_RF_DOWN) == AE_RF_LAST);
	/* the overview's two: the last game above the strip */
	has = (1ul << AE_RF_LAST) | (1ul << AE_RF_VERSUS);
	CHECK(ae_rf_move(0, has, AE_RF_LAST, AE_RF_DOWN) == AE_RF_VERSUS && ae_rf_move(0, has, AE_RF_VERSUS, AE_RF_UP) == AE_RF_LAST);
	/* keeping the focus over a page or bot change */
	CHECK(ae_rf_keep(2, ALL, AE_RF_MEDALS) == AE_RF_MEDALS);
	CHECK(ae_rf_keep(2, ALL & ~(1ul << AE_RF_MEDALS), AE_RF_MEDALS) == AE_RF_OBJECTIVES);
	CHECK(ae_rf_keep(1, ALL, AE_RF_MEDALS) == AE_RF_ACCURACY && ae_rf_keep(1, 0, AE_RF_MEDALS) == AE_RF_NONE);
	/* names, pages, rects */
	CHECK(ae_rf_page_of(AE_RF_WEAPONS) == 2 && ae_rf_page_of(AE_RF_NONE) == -1 && ae_rf_name(AE_RF_MAPS)[0] == 'B');
	CHECK(ae_rf_rect_of(AE_RF_MEDALS).x == 1344 && ae_rf_rect_of(99).w == 0);
	if (failures)
		return 1;
	printf("ae_record_focus_test: ok\n");
	return 0;
}
