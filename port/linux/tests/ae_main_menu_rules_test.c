#include <stdio.h>
#include <string.h>
#include "ae_main_menu_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	static const char *const main_labels[AE_MAIN_ROWS] = {
		"CAMPAIGN", "MULTIPLAYER", "HOST GAME", "SETTINGS", "MESSAGE OF THE DAY", "WHAT'S NEW",
		"FIREFIGHT", "FORGE", "THEATER", "FRIENDS"
	};
	static const char *const mp_labels[AE_MP_ROWS] = {
		"SERVER BROWSER", "MATCHMAKING", "JOIN BY ADDRESS", "JOIN LAN GAMES", "JOIN WITH INVITE", "HOST A SERVER"
	};
	int row;

	/* the rows in the owner's order (phase 1, layout A) */
	CHECK(AE_MAIN_ROWS == 10);
	CHECK(AE_MP_ROWS == 6);
	for (row = 0; row < AE_MAIN_ROWS; row++)
		CHECK(ae_main_row_label(row) && strcmp(ae_main_row_label(row), main_labels[row]) == 0);
	for (row = 0; row < AE_MP_ROWS; row++)
		CHECK(ae_mp_row_label(row) && strcmp(ae_mp_row_label(row), mp_labels[row]) == 0);
	/* out of range: no label */
	CHECK(ae_main_row_label(-1) == NULL);
	CHECK(ae_main_row_label(AE_MAIN_ROWS) == NULL);
	CHECK(ae_mp_row_label(AE_MP_ROWS) == NULL);

	/* the first launch starts on MULTIPLAYER; later launches on the last row used */
	CHECK(ae_main_first_focus(-1) == AE_MAIN_MULTIPLAYER);
	CHECK(ae_main_first_focus(AE_MAIN_ROWS) == AE_MAIN_MULTIPLAYER);
	CHECK(ae_main_first_focus(AE_MAIN_SETTINGS) == AE_MAIN_SETTINGS);
	CHECK(ae_main_first_focus(AE_MAIN_CAMPAIGN) == AE_MAIN_CAMPAIGN);
	CHECK(ae_main_first_focus(AE_MAIN_FORGE) == AE_MAIN_FORGE);

	/* COMING SOON: exactly the last four main rows */
	for (row = 0; row < AE_MAIN_ROWS; row++)
		CHECK(ae_main_row_coming_soon(row) == (row >= AE_MAIN_FIREFIGHT));
	CHECK(!ae_main_row_coming_soon(-1));
	CHECK(!ae_main_row_coming_soon(AE_MAIN_ROWS));

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
