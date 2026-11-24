/* ae_target_framerate_rules_test.c: TARGET FRAMERATE's choices, the earlier boolean's carry-over and the rate held */

#include <stdio.h>
#include <string.h>

#include "ae_target_framerate_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	static const char *const texts[] = { AE_TARGET_FRAMERATE_TEXTS };
	static const char *const words[] = { AE_TARGET_FRAMERATE_WORDS };
	int index;

	/* the registry: the texts and words line up, off first, DISPLAY last; the defaults are words of it */
	CHECK(sizeof(texts) / sizeof(texts[0]) == AE_TARGET_FRAMERATE_COUNT);
	CHECK(sizeof(words) / sizeof(words[0]) == AE_TARGET_FRAMERATE_COUNT);
	CHECK(!strcmp(texts[0], "OFF") && !strcmp(words[0], "off"));
	CHECK(!strcmp(texts[AE_TARGET_FRAMERATE_COUNT - 1], "DISPLAY") && !strcmp(words[AE_TARGET_FRAMERATE_COUNT - 1],
		"display"));
	for (index = 1; index < AE_TARGET_FRAMERATE_COUNT - 1; index++)
		CHECK(!strcmp(texts[index], words[index]));
	CHECK(ae_target_framerate_choice(AE_TARGET_FRAMERATE_DEFAULT) == 0);
	CHECK(ae_target_framerate_choice(AE_TARGET_FRAMERATE_SPLIT_DEFAULT) == 60);
	/* (the help says it lowers resolution and limits no frames, in the AE menus and the classic ones) */
	CHECK(strstr(AE_TARGET_FRAMERATE_HELP, "resolution") && strstr(AE_TARGET_FRAMERATE_HELP, "never limits"));
	CHECK(strstr(AE_TARGET_FRAMERATE_SPLIT_HELP, "resolution") && strstr(AE_TARGET_FRAMERATE_SPLIT_HELP, "never limits"));
	CHECK(strstr(AE_TARGET_FRAMERATE_CLASSIC_HELP, "resolution") && strstr(AE_TARGET_FRAMERATE_CLASSIC_HELP, "never limits"));
	CHECK(strstr(AE_TARGET_FRAMERATE_CLASSIC_SPLIT_HELP, "resolution") &&
		strstr(AE_TARGET_FRAMERATE_CLASSIC_SPLIT_HELP, "never limits"));

	/* the choices: each word, any case; anything else is off */
	CHECK(ae_target_framerate_choice("off") == 0);
	CHECK(ae_target_framerate_choice("60") == 60);
	CHECK(ae_target_framerate_choice("90") == 90);
	CHECK(ae_target_framerate_choice("120") == 120);
	CHECK(ae_target_framerate_choice("144") == 144);
	CHECK(ae_target_framerate_choice("display") == AE_TARGET_FRAMERATE_DISPLAY);
	CHECK(ae_target_framerate_choice("Display") == AE_TARGET_FRAMERATE_DISPLAY);
	CHECK(ae_target_framerate_choice("OFF") == 0);
	CHECK(ae_target_framerate_choice(NULL) == 0 && ae_target_framerate_choice("") == 0);
	CHECK(ae_target_framerate_choice("75") == 0 && ae_target_framerate_choice("true") == 0 &&
		ae_target_framerate_choice("60x") == 0 && ae_target_framerate_choice("6") == 0);

	/* the earlier boolean: true held 60 with V-SYNC on or off, so it carries over as 60; false as off */
	CHECK(!strcmp(ae_target_framerate_carry_over(1), "60"));
	CHECK(!strcmp(ae_target_framerate_carry_over(0), "off"));
	CHECK(ae_target_framerate_choice(ae_target_framerate_carry_over(1)) == 60);
	CHECK(ae_target_framerate_choice(ae_target_framerate_carry_over(0)) == 0);

	/* off is off whatever the display */
	CHECK(ae_target_framerate_hz(0, 1, 144.0f, 0) == 0.0f);
	CHECK(ae_target_framerate_hz(0, 0, 60.0f, -1) == 0.0f);
	/* V-SYNC on: never above the refresh */
	CHECK(ae_target_framerate_hz(60, 1, 144.0f, 0) == 60.0f);
	CHECK(ae_target_framerate_hz(120, 1, 144.0f, 0) == 120.0f);
	CHECK(ae_target_framerate_hz(144, 1, 144.0f, 0) == 144.0f);
	CHECK(ae_target_framerate_hz(144, 1, 60.0f, 0) == 60.0f);
	CHECK(ae_target_framerate_hz(120, 1, 100.0f, 500) == 100.0f);
	CHECK(ae_target_framerate_hz(60, 1, 50.0f, 0) == 50.0f);
	/* (the refresh not known: a 60 Hz display) */
	CHECK(ae_target_framerate_hz(120, 1, 0.0f, 0) == 60.0f);
	CHECK(ae_target_framerate_hz(60, 1, -1.0f, 0) == 60.0f);
	/* DISPLAY: the refresh, with V-SYNC on or off */
	CHECK(ae_target_framerate_hz(AE_TARGET_FRAMERATE_DISPLAY, 1, 165.0f, 0) == 165.0f);
	CHECK(ae_target_framerate_hz(AE_TARGET_FRAMERATE_DISPLAY, 0, 165.0f, 0) == 165.0f);
	CHECK(ae_target_framerate_hz(AE_TARGET_FRAMERATE_DISPLAY, 1, 0.0f, 0) == 60.0f);
	CHECK(ae_target_framerate_hz(AE_TARGET_FRAMERATE_DISPLAY, 0, 240.0f, 120) == 120.0f);
	/* V-SYNC off: never above FRAME RATE LIMIT's cap (0: twice the refresh; below 0: none) */
	CHECK(ae_target_framerate_hz(120, 0, 60.0f, 60) == 60.0f);
	CHECK(ae_target_framerate_hz(144, 0, 60.0f, 0) == 120.0f);
	CHECK(ae_target_framerate_hz(144, 0, 144.0f, 0) == 144.0f);
	CHECK(ae_target_framerate_hz(144, 0, 0.0f, 0) == 120.0f);
	CHECK(ae_target_framerate_hz(144, 0, 60.0f, -1) == 144.0f);
	CHECK(ae_target_framerate_hz(90, 0, 60.0f, 144) == 90.0f);

	if (failures)
		return 1;
	printf("ae_target_framerate_rules_test: all passed\n");
	return 0;
}
