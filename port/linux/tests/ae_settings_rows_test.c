/* ae_settings_rows_test.c: the SETTINGS model (ae_settings_rows.h): the groups, rows by rows.md, what shows when, the
sections, values by text, the KEEP / UNDO and RESTORE DEFAULTS texts. */

#include <stdio.h>
#include <string.h>

#include "ae_settings_rows.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a player's settings as a test has them */
static const char *mouse_style = "classic", *zoom_mode = "toggle", *mouse_zoom = "toggle", *clock = "down",
	*glow = "false", *panel = "true", *callouts = "off";

static const char *value(void *context, const char *key)
{
	(void)context;
	if (!strcmp(key, "input.mouse_style"))
		return mouse_style;
	if (!strcmp(key, "input.zoom_mode"))
		return zoom_mode;
	if (!strcmp(key, "input.mouse_zoom_mode"))
		return mouse_zoom;
	if (!strcmp(key, "display.match_clock"))
		return clock;
	if (!strcmp(key, "display.shield_glow"))
		return glow;
	if (!strcmp(key, "display.scoreboard_background"))
		return panel;
	if (!strcmp(key, "game.callouts"))
		return callouts;
	return "";
}

static short rows_of(short group, struct ae_settings_facts const *facts)
{
	short entries[AE_SETTINGS_ENTRIES], count = ae_settings_entries(group, facts, entries, AE_SETTINGS_ENTRIES), index,
		rows = 0;

	for (index = 0; index < count; index++)
		rows += !ae_settings_entry_is_header(entries[index]);
	return rows;
}

static int has_label(short group, struct ae_settings_facts const *facts, const char *label)
{
	short entries[AE_SETTINGS_ENTRIES], count = ae_settings_entries(group, facts, entries, AE_SETTINGS_ENTRIES), index;

	for (index = 0; index < count; index++)
	{
		if (!ae_settings_entry_is_header(entries[index]) && !strcmp(ae_settings_row(entries[index])->label, label))
			return 1;
	}
	return 0;
}

int main(void)
{
	struct ae_settings_facts pad = { 0, 0, value, NULL }, keyboard = { 1, 0, value, NULL }, both = { 1, 1, value, NULL };
	struct ae_settings_section sections[AE_SETTINGS_SECTIONS];
	short entries[AE_SETTINGS_ENTRIES], count, found, index;
	char text[400];

	/* the nine groups, in the top level's order; phase A builds CONTROLS, VIEW and HUD */
	CHECK(!strcmp(ae_settings_group(AE_SETTINGS_CONTROLS)->name, "CONTROLS"));
	CHECK(!strcmp(ae_settings_group(AE_SETTINGS_SYSTEM)->name, "SYSTEM"));
	CHECK(!ae_settings_group(AE_SETTINGS_GROUPS) && !ae_settings_group(-1));
	CHECK(ae_settings_group(AE_SETTINGS_CONTROLS)->built && ae_settings_group(AE_SETTINGS_VIEW)->built &&
		ae_settings_group(AE_SETTINGS_HUD)->built && !ae_settings_group(AE_SETTINGS_VIDEO)->built);
	CHECK(ae_settings_group(AE_SETTINGS_VIDEO)->whose == AE_SETTINGS_PC);

	/* rows.md's counts: CONTROLS 29 with everything shown (a pad-only player sees fewer), VIEW 11, HUD 26 */
	zoom_mode = "both";
	mouse_zoom = "both";
	callouts = "items";
	glow = "true";
	/* (CONTROLS: MOUSE STYLE picks CLASSIC's two or MCC's three: 29 counts the classic ones and MCC's three) */
	CHECK(rows_of(AE_SETTINGS_CONTROLS, &both) == 26);
	mouse_style = "mcc";
	CHECK(rows_of(AE_SETTINGS_CONTROLS, &both) == 27);
	CHECK(has_label(AE_SETTINGS_CONTROLS, &both, "MOUSE SENSITIVITY") &&
		!has_label(AE_SETTINGS_CONTROLS, &both, "HORIZONTAL SENSITIVITY"));
	mouse_style = "classic";
	CHECK(rows_of(AE_SETTINGS_VIEW, &pad) == 11);
	CHECK(rows_of(AE_SETTINGS_HUD, &pad) == 26);
	/* a pad-only player: no MOUSE, no KEYBOARD, no DUALSENSE (no DualSense); ZOOM HOLD TIME only with BOTH */
	CHECK(!has_label(AE_SETTINGS_CONTROLS, &pad, "MOUSE STYLE") && !has_label(AE_SETTINGS_CONTROLS, &pad, "KEY BINDINGS"));
	CHECK(!has_label(AE_SETTINGS_CONTROLS, &pad, "ADAPTIVE TRIGGERS"));
	CHECK(has_label(AE_SETTINGS_CONTROLS, &pad, "ZOOM HOLD TIME"));
	zoom_mode = "toggle";
	CHECK(!has_label(AE_SETTINGS_CONTROLS, &pad, "ZOOM HOLD TIME"));
	CHECK(rows_of(AE_SETTINGS_CONTROLS, &pad) == 12);
	/* the rows that hang on another: GLOW INTENSITY, CLOCK LABELS, PANEL COLOR, CALLOUT DETAIL / VOICE */
	glow = "false";
	clock = "off";
	panel = "false";
	callouts = "off";
	CHECK(!has_label(AE_SETTINGS_VIEW, &pad, "GLOW INTENSITY") && !has_label(AE_SETTINGS_HUD, &pad, "CLOCK LABELS"));
	CHECK(!has_label(AE_SETTINGS_HUD, &pad, "PANEL COLOR") && !has_label(AE_SETTINGS_HUD, &pad, "CALLOUT VOICE"));
	CHECK(rows_of(AE_SETTINGS_HUD, &pad) == 22);
	clock = "down";
	panel = "true";

	/* the list: a header before each section's rows, the sections and their rows, steps past headers */
	count = ae_settings_entries(AE_SETTINGS_CONTROLS, &keyboard, entries, AE_SETTINGS_ENTRIES);
	CHECK(ae_settings_entry_is_header(entries[0]) && !strcmp(ae_settings_entry_section(entries[0]), "CONTROLLER"));
	CHECK(!strcmp(ae_settings_row(entries[1])->label, "BUTTON LAYOUT"));
	found = ae_settings_sections(entries, count, sections, AE_SETTINGS_SECTIONS);
	CHECK(found == 3 && !strcmp(sections[1].name, "MOUSE") && !strcmp(sections[2].name, "KEYBOARD"));
	CHECK(sections[0].rows == 12 && sections[2].rows == 2);
	CHECK(ae_settings_section_at(sections, found, (short)(sections[1].header + 1)) == 1);
	CHECK(ae_settings_row_step(entries, count, (short)(sections[1].header - 1), 1) == sections[1].header + 1);
	CHECK(ae_settings_row_step(entries, count, 1, -1) == 1);
	CHECK(ae_settings_row_step(entries, count, (short)(count - 1), 1) == count - 1);
	/* a short array: never past its end */
	CHECK(ae_settings_entries(AE_SETTINGS_CONTROLS, &keyboard, entries, 3) <= 3);

	/* values: by text, a number written another way, the nearest step; none */
	index = ae_settings_row_of("display.fov");
	CHECK(index >= 0);
	CHECK(ae_settings_value_index(ae_settings_row(index), "0") == 0);
	CHECK(ae_settings_value_index(ae_settings_row(index), "90.0") == 3);
	CHECK(!strcmp(ae_settings_value_text(ae_settings_row(index), 3), "90"));
	CHECK(ae_settings_value_index(ae_settings_row(index), "92") == 3);
	CHECK(ae_settings_value_index(ae_settings_row(ae_settings_row_of("input.zoom_mode")), "nonsense") == -1);
	CHECK(ae_settings_value_index(ae_settings_row(ae_settings_row_of("input.zoom_hold_time")), "0.25") == 3);
	CHECK(!strcmp(ae_settings_value_text(ae_settings_row(ae_settings_row_of("input.zoom_hold_time")), 3), "0.25 S"));
	CHECK(ae_settings_value_index(ae_settings_row(ae_settings_row_of("input.mouse_mcc_sensitivity")), "5.6723") == 56);
	CHECK(ae_settings_slider(ae_settings_row(index), 0) == 0.0f && ae_settings_slider(ae_settings_row(index), 14) == 1.0f);
	/* the voices, set at run time */
	{
		static const char *const voices[] = { "cori", "nhe", "" };
		struct ae_settings_row const *voice = ae_settings_row(ae_settings_row_of("game.callout_voice"));

		ae_settings_set_voices(voices, 3);
		CHECK(ae_settings_value_count(voice) == 2);
		CHECK(!strcmp(ae_settings_value_text(voice, 1), "NHE") && !strcmp(ae_settings_value_of(voice, 1), "nhe"));
		CHECK(ae_settings_value_index(voice, "cori") == 0);
	}

	/* the dialogs */
	{
		static const char *const labels[] = { "LOOK SENSITIVITY", "ZOOM", "VIBRATION", "LEGS", "FOV" };

		ae_settings_keep_body("CONTROLS", "Chief", labels, 3, text, sizeof(text));
		CHECK(!strcmp(text, "3 CONTROLS settings changed for Chief: LOOK SENSITIVITY, ZOOM and VIBRATION. They are in "
			"use already."));
		ae_settings_keep_body("CONTROLS", "Chief", labels, 1, text, sizeof(text));
		CHECK(!strcmp(text, "1 CONTROLS setting changed for Chief: LOOK SENSITIVITY. They are in use already."));
		ae_settings_keep_body("VIEW", "Chief", labels, 5, text, sizeof(text));
		CHECK(!strcmp(text, "5 VIEW settings changed for Chief: LOOK SENSITIVITY, ZOOM, VIBRATION and 2 more. They are in "
			"use already."));
		ae_settings_keep_body("VIDEO", NULL, labels, 2, text, sizeof(text));
		CHECK(!strcmp(text, "2 VIDEO settings changed on this PC: LOOK SENSITIVITY and ZOOM. They are in use already."));
		CHECK(ae_settings_restore_body("HUD", "Chief", 4, text, sizeof(text)) == 4);
		CHECK(!strcmp(text, "Every HUD setting for Chief goes back to its default: 4 settings change. Other players and "
			"this PC's settings stay as they are. You can still undo it as you leave."));
		CHECK(ae_settings_restore_body("HUD", "Chief", 0, text, sizeof(text)) == 0);
		CHECK(!strcmp(text, "HUD is already at its defaults."));
		ae_settings_restore_body("VIDEO", NULL, 1, text, sizeof(text));
		CHECK(!strcmp(text, "Every VIDEO setting on this PC goes back to its default: 1 setting changes. Players' own "
			"settings stay as they are."));
	}

	if (failures)
	{
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("ae_settings_rows: ok\n");
	return 0;
}
