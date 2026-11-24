#include <stdio.h>
#include <string.h>
#include "ae_pad_notice_rules.h"
#include "ae_steam_pad_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int starts(const char *text, const char *prefix)
{
	return !strncmp(text, prefix, strlen(prefix));
}

static int step(struct ae_pad_battery_state *state, int power, int percent)
{
	return ae_pad_battery_rules_step(state, power, percent);
}

static void details(struct ae_pad_details *d, int world, int keyboard, const char *name, const char *mode, int power, int percent)
{
	ae_pad_details_build(d, world, keyboard, name, mode, power, percent);
}

int main(void)
{
	struct ae_pad_battery_state state;
	struct ae_pad_details d;
	char text[AE_PAD_NOTICE_TEXT];
	int set[4] = { 1, 1, 1, 1 };

	/* the battery notice: low at 15 % or less on the battery, once a drop */
	memset(&state, 0, sizeof(state));
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 80) == AE_PAD_NOTICE_NONE);
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 16) == AE_PAD_NOTICE_NONE);
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 15) == AE_PAD_NOTICE_LOW);
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 14) == AE_PAD_NOTICE_NONE);
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 12) == AE_PAD_NOTICE_NONE);
	/* (a level between the low and the re-arm one does not re-arm it) */
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 19) == AE_PAD_NOTICE_NONE && step(&state, AE_PAD_POWER_BATTERY, 10) == AE_PAD_NOTICE_NONE);
	/* (20 % or more re-arms it: the next drop says it again) */
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 20) == AE_PAD_NOTICE_NONE);
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 14) == AE_PAD_NOTICE_LOW);
	/* the first read of a low pad says it */
	memset(&state, 0, sizeof(state));
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 3) == AE_PAD_NOTICE_LOW);
	/* nothing when the level or the state is not known, or a wired pad has no battery */
	memset(&state, 0, sizeof(state));
	CHECK(step(&state, AE_PAD_POWER_BATTERY, -1) == AE_PAD_NOTICE_NONE && step(&state, AE_PAD_POWER_UNKNOWN, -1) == AE_PAD_NOTICE_NONE &&
		step(&state, AE_PAD_POWER_NO_BATTERY, -1) == AE_PAD_NOTICE_NONE && step(&state, AE_PAD_POWER_UNKNOWN, 5) == AE_PAD_NOTICE_NONE);
	/* the charging notice only after a low one, once a plug-in; a charge re-arms the low notice */
	memset(&state, 0, sizeof(state));
	CHECK(step(&state, AE_PAD_POWER_CHARGING, 14) == AE_PAD_NOTICE_NONE);
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 13) == AE_PAD_NOTICE_LOW);
	CHECK(step(&state, AE_PAD_POWER_CHARGING, 14) == AE_PAD_NOTICE_CHARGING);
	CHECK(step(&state, AE_PAD_POWER_CHARGING, 15) == AE_PAD_NOTICE_NONE);
	CHECK(step(&state, AE_PAD_POWER_CHARGED, 100) == AE_PAD_NOTICE_NONE);
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 14) == AE_PAD_NOTICE_LOW);
	CHECK(step(&state, AE_PAD_POWER_CHARGING, 14) == AE_PAD_NOTICE_CHARGING);
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 14) == AE_PAD_NOTICE_LOW);

	/* another pad on the port starts again: pad 5 low (told), then pad 6 low is told too; the same pad is not told twice; a pad
	pulled out and put back is a new start */
	memset(&state, 0, sizeof(state));
	CHECK(ae_pad_battery_rules_pad(&state, 5) && !ae_pad_battery_rules_pad(&state, 5));
	CHECK(step(&state, AE_PAD_POWER_BATTERY, 14) == AE_PAD_NOTICE_LOW && step(&state, AE_PAD_POWER_BATTERY, 13) == AE_PAD_NOTICE_NONE);
	CHECK(ae_pad_battery_rules_pad(&state, 6) && step(&state, AE_PAD_POWER_BATTERY, 14) == AE_PAD_NOTICE_LOW);
	/* (the first pad's low history does not give the second a charging notice either) */
	CHECK(ae_pad_battery_rules_pad(&state, 7) && step(&state, AE_PAD_POWER_CHARGING, 14) == AE_PAD_NOTICE_NONE);
	CHECK(ae_pad_battery_rules_pad(&state, 0) && ae_pad_battery_rules_pad(&state, 7) && step(&state, AE_PAD_POWER_BATTERY, 90) == AE_PAD_NOTICE_NONE);

	/* the notice's texts */
	CHECK(ae_pad_notice_text(AE_PAD_NOTICE_LOW, 0, 12, text) && !strcmp(text, "CONTROLLER BATTERY LOW 12 %"));
	CHECK(ae_pad_notice_text(AE_PAD_NOTICE_LOW, 1, 12, text) && !strcmp(text, "CONTROLLER BATTERY LOW 12 %"));
	CHECK(ae_pad_notice_text(AE_PAD_NOTICE_LOW, 2, 12, text) && !strcmp(text, "PLAYER 2: CONTROLLER BATTERY LOW 12 %"));
	CHECK(ae_pad_notice_text(AE_PAD_NOTICE_LOW, 0, -1, text) && !strcmp(text, "CONTROLLER BATTERY LOW"));
	CHECK(ae_pad_notice_text(AE_PAD_NOTICE_CHARGING, 0, 14, text) && !strcmp(text, "CONTROLLER CHARGING 14 %"));
	CHECK(ae_pad_notice_text(AE_PAD_NOTICE_CHARGING, 3, 14, text) && !strcmp(text, "PLAYER 3: CONTROLLER CHARGING 14 %"));
	CHECK(!ae_pad_notice_text(AE_PAD_NOTICE_NONE, 0, 14, text) && !text[0]);

	/* the notice's time: full for 6 s, the fade 0.4 s, then none */
	CHECK(ae_pad_notice_alpha(0) == 1.0 && ae_pad_notice_alpha(6000) == 1.0 && ae_pad_notice_alpha(6200) > 0.45 &&
		ae_pad_notice_alpha(6200) < 0.55 && ae_pad_notice_alpha(6400) == 0.0 && ae_pad_notice_alpha(9000) == 0.0);
	CHECK(ae_pad_notice_pulse(0, 0) == 0.0 && ae_pad_notice_pulse(250, 0) > 0.99 && ae_pad_notice_pulse(500, 0) < 0.01 &&
		ae_pad_notice_pulse(750, 0) > 0.99 && ae_pad_notice_pulse(1000, 0) == 0.0 && ae_pad_notice_pulse(250, 1) == 0.0);

	/* the details line: a Steam Controller read directly, by its connection and battery */
	details(&d, AE_STEAM_WORLD_DIRECT, 0, "", "PUCK", AE_PAD_POWER_BATTERY, 80);
	CHECK(d.count == 3 && !strcmp(d.part[0], "STEAM CONTROLLER") && !strcmp(d.part[1], "PUCK") && !strcmp(d.part[2], "BATTERY 80 %") && d.low == -1);
	details(&d, AE_STEAM_WORLD_DIRECT, 0, "", "BLUETOOTH", AE_PAD_POWER_BATTERY, 15);
	CHECK(d.count == 3 && !strcmp(d.part[1], "BLUETOOTH") && !strcmp(d.part[2], "BATTERY LOW 15 %") && d.low == 2);
	details(&d, AE_STEAM_WORLD_DIRECT, 0, "", "WIRED", AE_PAD_POWER_CHARGING, 80);
	CHECK(d.count == 3 && !strcmp(d.part[1], "WIRED") && !strcmp(d.part[2], "CHARGING 80 %") && d.low == -1);
	details(&d, AE_STEAM_WORLD_DIRECT, 0, "", "WIRED", AE_PAD_POWER_CHARGING, -1);
	CHECK(d.count == 3 && !strcmp(d.part[2], "CHARGING"));
	details(&d, AE_STEAM_WORLD_DIRECT, 0, "", "WIRED", AE_PAD_POWER_CHARGED, 100);
	CHECK(d.count == 3 && !strcmp(d.part[2], "CHARGED"));
	/* (no level known: the part is left out; a wired pad with no battery too) */
	details(&d, AE_STEAM_WORLD_DIRECT, 0, "", "PUCK", AE_PAD_POWER_BATTERY, -1);
	CHECK(d.count == 2 && !strcmp(d.part[1], "PUCK"));
	details(&d, AE_STEAM_WORLD_DIRECT, 0, "", "WIRED", AE_PAD_POWER_NO_BATTERY, -1);
	CHECK(d.count == 2);
	details(&d, AE_STEAM_WORLD_DIRECT, 0, "", NULL, AE_PAD_POWER_UNKNOWN, -1);
	CHECK(d.count == 1 && !strcmp(d.part[0], "STEAM CONTROLLER"));
	/* the worlds: Steam Input, a pad not known behind it, Linux without access */
	details(&d, AE_STEAM_WORLD_STEAM_INPUT, 0, "", NULL, AE_PAD_POWER_BATTERY, 50);
	CHECK(d.count == 2 && !strcmp(d.part[0], "STEAM CONTROLLER") && !strcmp(d.part[1], "THROUGH STEAM INPUT"));
	details(&d, AE_STEAM_WORLD_STEAM_INPUT_OTHER, 0, "", NULL, AE_PAD_POWER_UNKNOWN, -1);
	CHECK(d.count == 1 && !strcmp(d.part[0], "THROUGH STEAM INPUT"));
	details(&d, AE_STEAM_WORLD_NO_ACCESS, 0, "", NULL, AE_PAD_POWER_UNKNOWN, -1);
	CHECK(d.count == 2 && !strcmp(d.part[0], "STEAM CONTROLLER") && !strcmp(d.part[1], "AS MOUSE AND KEYS"));
	/* the keyboard, and another pad by its short name */
	details(&d, AE_STEAM_WORLD_NONE, 1, "", NULL, AE_PAD_POWER_UNKNOWN, -1);
	CHECK(d.count == 1 && !strcmp(d.part[0], "KEYBOARD AND MOUSE"));
	details(&d, AE_STEAM_WORLD_NONE, 0, "DUALSENSE", "WIRELESS", AE_PAD_POWER_BATTERY, 64);
	CHECK(d.count == 3 && !strcmp(d.part[0], "DUALSENSE") && !strcmp(d.part[1], "WIRELESS") && !strcmp(d.part[2], "BATTERY 64 %"));
	details(&d, AE_STEAM_WORLD_NONE, 0, "XBOX", "WIRED", AE_PAD_POWER_NO_BATTERY, -1);
	CHECK(d.count == 2 && !strcmp(d.part[1], "WIRED"));

	/* the backpack note: only with all four of L4, R4, L5 and R5 set */
	CHECK(ae_pad_backpack_note_shown(set));
	set[2] = 0;
	CHECK(!ae_pad_backpack_note_shown(set));
	CHECK(!strcmp(AE_PAD_BACKPACK_HEADING, "BACKPACK MODE") && starts(AE_PAD_BACKPACK_NOTE, "Holding L4, R4, L5, R5 and the Steam button turns the controller off. Hold them again until it wakes, or plug it in by USB-C."));

	/* the strings of the card and the notices */
	CHECK(!strcmp(AE_PAD_RAIL_FOOT_LABEL, "YOUR CONTROLLER") && !strcmp(AE_PAD_NOTICE_WORLD_NO_ACCESS_SETTINGS, "STEAM CONTROLLER IS ACTING AS A MOUSE"));
	CHECK(!strcmp(AE_PAD_NOTICE_WORLD_NO_ACCESS_MENU, "STEAM CONTROLLER IS ACTING AS A MOUSE: SEE SETTINGS > CONTROLS"));
	CHECK(starts(AE_PAD_NOTE_WORLD_STEAM_INPUT, "Steam Input drives your controller: Steam's controller layout sets its trackpads, L4, R4, L5, R5 and gyro."));
	CHECK(strstr(AE_PAD_NOTE_WORLD_STEAM_INPUT, "in Steam, the game's Properties > Controller, then Disable Steam Input.") != NULL);
	CHECK(starts(AE_PAD_NOTE_WORLD_NO_ACCESS, "Linux has not given the game access to your Steam Controller, so it acts as a mouse and keys."));
	CHECK(strstr(AE_PAD_NOTE_WORLD_NO_ACCESS, "GUIDE") == NULL && strstr(AE_PAD_NOTE_WORLD_NO_ACCESS, "then plug the controller in again.") != NULL &&
		strlen(AE_PAD_NOTE_WORLD_NO_ACCESS) > 0 && AE_PAD_NOTE_WORLD_NO_ACCESS[strlen(AE_PAD_NOTE_WORLD_NO_ACCESS) - 1] == '.' && !strcmp(AE_PAD_NOTE_NO_DUALSENSE, "DUALSENSE shows while one is plugged in."));

	if (failures)
		return 1;
	printf("ae_pad_notice_rules_test: all passed\n");
	return 0;
}
