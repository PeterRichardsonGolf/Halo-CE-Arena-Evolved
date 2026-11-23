#include <stdio.h>
#include <string.h>
#include "ae_round_card_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char text[64];
	long slot[4] = { 0, 3, 5, 9 }, kills[4], deaths[4], side[4] = { 0, 0, 1, 1 };

	/* the verdict, per viewer */
	CHECK(ae_round_card_verdict(0, 1, 0, 0) == AE_ROUND_CARD_WON);
	CHECK(ae_round_card_verdict(0, 1, 1, 4) == AE_ROUND_CARD_LOST);
	CHECK(ae_round_card_verdict(0, 1, -1, -1) == AE_ROUND_CARD_LOST);
	CHECK(ae_round_card_verdict(AE_ROUND_WIRE_RESULT_TIE, 1, 0, 0) == AE_ROUND_CARD_TIED);
	CHECK(ae_round_card_verdict(AE_ROUND_WIRE_RESULT_NONE, 1, 0, 0) == AE_ROUND_CARD_NONE);
	CHECK(ae_round_card_verdict(7, 0, 0, 7) == AE_ROUND_CARD_WON);
	CHECK(ae_round_card_verdict(7, 0, 0, 6) == AE_ROUND_CARD_LOST);
	CHECK(ae_round_card_verdict(7, 0, 1, -1) == AE_ROUND_CARD_LOST);

	/* the reason */
	CHECK(ae_round_card_reason(1, 0, 1, 0, 0) == AE_ROUND_REASON_BLUE_ELIMINATED);
	CHECK(ae_round_card_reason(1, 1, 1, 0, 0) == AE_ROUND_REASON_RED_ELIMINATED);
	CHECK(ae_round_card_reason(1, 1, 1, 1, 0) == AE_ROUND_REASON_EXTERMINATION);
	CHECK(ae_round_card_reason(1, 4, 0, 0, 0) == AE_ROUND_REASON_LAST_STANDING);
	CHECK(ae_round_card_reason(0, -1, 1, 0, 1) == AE_ROUND_REASON_TIME_RAN_OUT);
	CHECK(ae_round_card_reason(0, -1, 1, 0, 0) == AE_ROUND_REASON_NONE);
	CHECK(!strcmp(ae_round_card_reason_text(AE_ROUND_REASON_BOMB_DETONATED), "BOMB DETONATED"));
	CHECK(!strcmp(ae_round_card_reason_text(AE_ROUND_REASON_LEVEL_ON_KILLS), "LEVEL ON KILLS"));
	CHECK(ae_round_card_reason_text(AE_ROUND_REASON_NONE) == NULL && ae_round_card_reason_text(99) == NULL);

	/* the MVP: kills, then deaths, then the winners' side, then the lowest slot; nobody with a kill: none */
	kills[0] = 0; kills[1] = 0; kills[2] = 0; kills[3] = 0;
	deaths[0] = deaths[1] = deaths[2] = deaths[3] = 1;
	CHECK(ae_round_card_mvp(4, slot, kills, deaths, side, 0) == -1);
	kills[1] = 3; kills[2] = 2;
	CHECK(ae_round_card_mvp(4, slot, kills, deaths, side, 1) == 1);
	kills[2] = 3;
	deaths[2] = 0;
	CHECK(ae_round_card_mvp(4, slot, kills, deaths, side, 0) == 2);
	deaths[2] = 1;
	CHECK(ae_round_card_mvp(4, slot, kills, deaths, side, 1) == 2);
	CHECK(ae_round_card_mvp(4, slot, kills, deaths, side, 0) == 1);
	CHECK(ae_round_card_mvp(4, slot, kills, deaths, side, -1) == 1);
	side[1] = 1; slot[1] = 9; slot[2] = 5;
	CHECK(ae_round_card_mvp(4, slot, kills, deaths, side, -1) == 2);
	CHECK(ae_round_card_mvp(0, slot, kills, deaths, side, 0) == -1);

	/* the words */
	ae_round_card_footer(4, 7, 3, text, sizeof(text));
	CHECK(!strcmp(text, "NEXT  \xC2\xB7  ROUND 4 OF 7"));
	ae_round_card_footer(4, 0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "NEXT  \xC2\xB7  ROUND 4"));
	ae_round_card_kills_text(3, text, sizeof(text));
	CHECK(!strcmp(text, "3 kills"));
	ae_round_card_kills_text(1, text, sizeof(text));
	CHECK(!strcmp(text, "1 kill"));

	/* the fade: in over 0.15 s, held, out over the last 0.25 s, nothing outside */
	CHECK(ae_round_card_alpha(-0.1f) == 0.0f && ae_round_card_alpha(0.0f) == 0.0f);
	CHECK(ae_round_card_alpha(0.075f) > 0.9f && ae_round_card_alpha(0.075f) < 1.0f);
	CHECK(ae_round_card_alpha(0.15f) == 1.0f && ae_round_card_alpha(1.5f) == 1.0f && ae_round_card_alpha(2.75f) == 1.0f);
	CHECK(ae_round_card_alpha(2.875f) > 0.0f && ae_round_card_alpha(2.875f) < 0.1f);
	CHECK(ae_round_card_alpha(3.0f) == 0.0f && ae_round_card_alpha(9.0f) == 0.0f);
	{
		float last = 0.0f, t;
		int up = 1;

		for (t = 0.0f; t < 0.15f; t += 0.01f)
		{
			float a = ae_round_card_alpha(t);

			up = up && a >= last && a <= 1.0f;
			last = a;
		}
		CHECK(up);
	}

	/* which builds read the tail */
	CHECK(ae_round_build_has_card("Arena Evolved 0.7.9-beta"));
	CHECK(ae_round_build_has_card("Arena Evolved 0.7.10"));
	CHECK(ae_round_build_has_card("Arena Evolved 0.8.0-beta"));
	CHECK(ae_round_build_has_card("Arena Evolved 1.0.0"));
	CHECK(!ae_round_build_has_card("Arena Evolved 0.7.8-beta"));
	CHECK(ae_round_build_has_card("Arena Evolved 0.7.8-beta-dev"));
	CHECK(!ae_round_build_has_card("Arena Evolved 0.7.7-beta-dev"));
	CHECK(!ae_round_build_has_card("Arena Evolved 0.6.9"));
	CHECK(!ae_round_build_has_card("OpenCE 1.2.3") && !ae_round_build_has_card("") && !ae_round_build_has_card(NULL));
	CHECK(!ae_round_build_has_card("Arena Evolved 0.7"));

	if (failures)
		return 1;
	printf("ae_round_card_rules_test: ok\n");
	return 0;
}
