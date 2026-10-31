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
enum { SETS = 32 };
static struct { const char *key, *value; } sets[SETS];

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
	{
		int index;

		for (index = 0; index < SETS && sets[index].key; index++)
		{
			if (!strcmp(key, sets[index].key))
				return sets[index].value;
		}
	}
	return "";
}

/* (phase B) any other setting a test sets */
static void set(const char *key, const char *now)
{
	int index;

	for (index = 0; index < SETS; index++)
	{
		if (!sets[index].key || !strcmp(sets[index].key, key))
		{
			sets[index].key = key;
			sets[index].value = now;
			return;
		}
	}
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
	struct ae_settings_facts pad = { 0, 0, value, NULL, 0, 0 }, keyboard = { 1, 0, value, NULL, 0, 0 },
		both = { 1, 1, value, NULL, 0, 0 };
	struct ae_settings_section sections[AE_SETTINGS_SECTIONS];
	short entries[AE_SETTINGS_ENTRIES], count, found, index;
	char text[400];

	/* the nine groups, in the top level's order; phase A built CONTROLS, VIEW, HUD and SYSTEM's MENUS, phase B the
	rest */
	CHECK(!strcmp(ae_settings_group(AE_SETTINGS_CONTROLS)->name, "CONTROLS"));
	CHECK(!strcmp(ae_settings_group(AE_SETTINGS_SYSTEM)->name, "SYSTEM"));
	CHECK(!ae_settings_group(AE_SETTINGS_GROUPS) && !ae_settings_group(-1));
	CHECK(ae_settings_group(AE_SETTINGS_CONTROLS)->built && ae_settings_group(AE_SETTINGS_VIEW)->built &&
		ae_settings_group(AE_SETTINGS_HUD)->built && ae_settings_group(AE_SETTINGS_VIDEO)->built);
	/* (0.3.7: SYSTEM holds MENUS alone, the way back to the classic menus; it needs a restart) */
	CHECK(ae_settings_group(AE_SETTINGS_SYSTEM)->built && ae_settings_row_of("display.arena_menus") >= 0 &&
		ae_settings_row(ae_settings_row_of("display.arena_menus"))->group == AE_SETTINGS_SYSTEM &&
		(ae_settings_row(ae_settings_row_of("display.arena_menus"))->flags & AE_SETTINGS_RESTART));
	/* (0.3.9: and OPEN GAMETYPES FOLDER, an action: A opens the folder, no value) */
	CHECK(ae_settings_row_of("action.gametypes_folder") >= 0 &&
		ae_settings_row(ae_settings_row_of("action.gametypes_folder"))->group == AE_SETTINGS_SYSTEM &&
		ae_settings_row(ae_settings_row_of("action.gametypes_folder"))->kind == AE_SETTINGS_SUBSCREEN &&
		ae_settings_value_count(ae_settings_row(ae_settings_row_of("action.gametypes_folder"))) == 0);
	/* (TEST RUN: SYSTEM's DEV section: the DEV MODE switch always, the TEST RUN screen's row only with DEV MODE on) */
	{
		struct ae_settings_row const *dev = ae_settings_row(ae_settings_row_of("system.dev_mode"));
		struct ae_settings_row const *run = ae_settings_row(ae_settings_row_of("action.test_run"));
		short before;

		CHECK(dev && run && dev->group == AE_SETTINGS_SYSTEM && run->group == AE_SETTINGS_SYSTEM);
		CHECK(dev->kind == AE_SETTINGS_TOGGLE && (dev->flags & AE_SETTINGS_THIS_PC) != 0 && dev->when == AE_SETTINGS_WHEN_DEV_SWITCH);
		/* (its pill shows with one player; no other SYSTEM row's does) */
		CHECK(dev->flags & AE_SETTINGS_PILL_ALWAYS);
		{
			short which;

			for (which = 0; which < ae_settings_row_count(); which++)
				CHECK(ae_settings_row(which) == dev || !(ae_settings_row(which)->flags & AE_SETTINGS_PILL_ALWAYS));
		}
		CHECK(strstr(dev->help, "THIS PC"));
		CHECK(run->kind == AE_SETTINGS_SUBSCREEN && run->when == AE_SETTINGS_DEV_MODE &&
			ae_settings_value_count(run) == 0);
		CHECK(!strcmp(dev->section, "DEV") && !strcmp(run->section, "DEV"));
		set("system.dev_mode", "false");
		before = rows_of(AE_SETTINGS_SYSTEM, &pad);
		CHECK(has_label(AE_SETTINGS_SYSTEM, &pad, "DEV MODE") && !has_label(AE_SETTINGS_SYSTEM, &pad, "TEST RUN"));
		set("system.dev_mode", "true");
		CHECK(has_label(AE_SETTINGS_SYSTEM, &pad, "DEV MODE") && has_label(AE_SETTINGS_SYSTEM, &pad, "TEST RUN"));
		CHECK(rows_of(AE_SETTINGS_SYSTEM, &pad) == before + 1);
		set("system.dev_mode", "false");
		/* (a release build hides the switch, unless DEV MODE is already on: it can be turned off again) */
		{
			struct ae_settings_facts release = pad;

			release.release_build = 1;
			CHECK(!has_label(AE_SETTINGS_SYSTEM, &release, "DEV MODE") && !has_label(AE_SETTINGS_SYSTEM, &release, "TEST RUN"));
			set("system.dev_mode", "true");
			CHECK(has_label(AE_SETTINGS_SYSTEM, &release, "DEV MODE") && has_label(AE_SETTINGS_SYSTEM, &release, "TEST RUN"));
			set("system.dev_mode", "false");
			CHECK(has_label(AE_SETTINGS_SYSTEM, &pad, "DEV MODE"));
		}
	}
	/* (0.3.9: the seven settings only the classic menus had) */
	{
		struct ae_settings_row const *sound = ae_settings_row(ae_settings_row_of("audio.enabled"));
		struct ae_settings_row const *disc = ae_settings_row(ae_settings_row_of("game.disc"));
		struct ae_settings_row const *mcc = ae_settings_row(ae_settings_row_of("game.mcc_use"));
		struct ae_settings_row const *health = ae_settings_row(ae_settings_row_of("game.health"));
		const char *keys[] = { "audio.enabled", "network.map_share_internet", "game.weapon_packs", "game.disc",
			"game.mcc_use", "game.fall_damage", "game.health" };
		short which;

		for (which = 0; which < 7; which++)
			CHECK(ae_settings_row_of(keys[which]) >= 0);
		CHECK(sound->group == AE_SETTINGS_AUDIO && (sound->flags & AE_SETTINGS_RESTART) &&
			!strcmp(ae_settings_value_of(sound, 0), "true"));
		CHECK(ae_settings_row(ae_settings_row_of("network.map_share_internet"))->group == AE_SETTINGS_ONLINE);
		CHECK(disc->group == AE_SETTINGS_SYSTEM && (disc->flags & AE_SETTINGS_RESTART));
		/* (its values are read when SYSTEM opens: STOCK is the empty text) */
		{
			const char *texts[] = { "STOCK", "NHE" }, *values[] = { "", "nhe" };

			CHECK(ae_settings_set_values("game.disc", texts, values, 2) && ae_settings_row_takes_empty(disc) &&
				ae_settings_value_index(disc, "nhe") == 1 && ae_settings_value_index(disc, "") == 0);
		}
		CHECK(ae_settings_value_count(mcc) == 3 && !strcmp(ae_settings_value_of(mcc, 1), "yes") &&
			!strcmp(ae_settings_value_text(mcc, 1), "ON"));
		CHECK(ae_settings_value_count(health) == 4 && !strcmp(ae_settings_value_of(health, 2), "halo2") &&
			!strcmp(ae_settings_value_text(health, 3), "HALO 3"));
		CHECK(!strcmp(ae_settings_row(ae_settings_row_of("game.health"))->section, "CAMPAIGN") &&
			!strcmp(ae_settings_row(ae_settings_row_of("game.fall_damage"))->section, "CAMPAIGN"));
	}
	CHECK(ae_settings_group(AE_SETTINGS_VIDEO)->whose == AE_SETTINGS_PC);

	/* rows.md's counts: CONTROLS 30 with everything shown (a pad-only player sees fewer; GYRO's other four only with
	GYRO AIMING on), VIEW 11, HUD 26 */
	zoom_mode = "both";
	mouse_zoom = "both";
	callouts = "items";
	glow = "true";
	/* (CONTROLS: MOUSE STYLE picks CLASSIC's two or MCC's three: 30 counts the classic ones and MCC's three) */
	/* (0.4.0: WHEEL ZOOM, the mouse's) */
	CHECK(rows_of(AE_SETTINGS_CONTROLS, &both) == 28);
	mouse_style = "mcc";
	CHECK(rows_of(AE_SETTINGS_CONTROLS, &both) == 29);
	CHECK(has_label(AE_SETTINGS_CONTROLS, &both, "MOUSE SENSITIVITY") &&
		!has_label(AE_SETTINGS_CONTROLS, &both, "HORIZONTAL SENSITIVITY"));
	/* (0.3.9: GYRO AIMING for every player, this PC's; its sensitivity and invert rows only with it on) */
	CHECK(has_label(AE_SETTINGS_CONTROLS, &pad, "GYRO AIMING") && !has_label(AE_SETTINGS_CONTROLS, &pad, "TURN SENSITIVITY"));
	CHECK(ae_settings_row(ae_settings_row_of("input.gyro_aim"))->flags & AE_SETTINGS_THIS_PC);
	mouse_style = "classic";
	/* (0.4.0: WEAPON COSMETICS, PISTOL COLOR and PISTOL TRIM; BACKPACK WEAPONS) */
	CHECK(rows_of(AE_SETTINGS_VIEW, &pad) == 14);
	/* (0.4.0: ALLY RETICLE, DEAD ALLY MARKERS) */
	CHECK(rows_of(AE_SETTINGS_HUD, &pad) == 25);
	/* a pad-only player: no MOUSE, no KEYBOARD, no DUALSENSE (no DualSense); ZOOM HOLD TIME only with BOTH */
	CHECK(!has_label(AE_SETTINGS_CONTROLS, &pad, "MOUSE STYLE") && !has_label(AE_SETTINGS_CONTROLS, &pad, "KEY BINDINGS"));
	CHECK(!has_label(AE_SETTINGS_CONTROLS, &pad, "ADAPTIVE TRIGGERS"));
	CHECK(has_label(AE_SETTINGS_CONTROLS, &pad, "ZOOM HOLD TIME"));
	zoom_mode = "toggle";
	CHECK(!has_label(AE_SETTINGS_CONTROLS, &pad, "ZOOM HOLD TIME"));
	CHECK(rows_of(AE_SETTINGS_CONTROLS, &pad) == 13);
	/* the rows that hang on another: GLOW INTENSITY, CLOCK LABELS, PANEL COLOR, CALLOUT DETAIL / VOICE */
	glow = "false";
	clock = "off";
	panel = "false";
	callouts = "off";
	CHECK(!has_label(AE_SETTINGS_VIEW, &pad, "GLOW INTENSITY") && !has_label(AE_SETTINGS_HUD, &pad, "CLOCK LABELS"));
	CHECK(!has_label(AE_SETTINGS_HUD, &pad, "PANEL COLOR") && !has_label(AE_SETTINGS_AUDIO, &pad, "CALLOUT VOICE"));
	/* (CALLOUTS on AUDIO since 0.3.9) */
	CHECK(rows_of(AE_SETTINGS_HUD, &pad) == 23 && !has_label(AE_SETTINGS_HUD, &pad, "CALLOUTS") &&
		has_label(AE_SETTINGS_AUDIO, &pad, "CALLOUTS"));
	clock = "down";
	panel = "true";

	/* the list: a header before each section's rows, the sections and their rows, steps past headers */
	count = ae_settings_entries(AE_SETTINGS_CONTROLS, &keyboard, entries, AE_SETTINGS_ENTRIES);
	CHECK(ae_settings_entry_is_header(entries[0]) && !strcmp(ae_settings_entry_section(entries[0]), "CONTROLLER"));
	CHECK(!strcmp(ae_settings_row(entries[1])->label, "BUTTON LAYOUT"));
	found = ae_settings_sections(entries, count, sections, AE_SETTINGS_SECTIONS);
	CHECK(found == 4 && !strcmp(sections[1].name, "GYRO") && !strcmp(sections[2].name, "MOUSE") &&
		!strcmp(sections[3].name, "KEYBOARD"));
	CHECK(sections[0].rows == 12 && sections[1].rows == 1 && sections[3].rows == 2);
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

	/* VIDEO (rows.md: 26 with everything shown, the two LOADING SCREEN rows with them since 0.7.0): WINDOW SIZE in RESOLUTION's place when WINDOWED, POSITION with
	PERFORMANCE on, the HDR rows with HDR on, HDR DISPLAY on an HDR display only */
	{
		struct ae_settings_facts hdr = { 0, 0, value, NULL, 1, 0 };
		struct ae_settings_row const *resolution = ae_settings_row(ae_settings_row_of("display.resolution"));
		static const char *const texts[] = { "NATIVE", "1920 X 1080" }, *const values[] = { "native", "1920x1080" };

		set("display.performance", "off");
		CHECK(rows_of(AE_SETTINGS_VIDEO, &pad) == 21);
		CHECK(has_label(AE_SETTINGS_VIDEO, &pad, "RESOLUTION") && !has_label(AE_SETTINGS_VIDEO, &pad, "WINDOW SIZE"));
		set("display.mode", "windowed");
		set("display.performance", "fps");
		set("display.hdr", "true");
		CHECK(!has_label(AE_SETTINGS_VIDEO, &pad, "RESOLUTION") && has_label(AE_SETTINGS_VIDEO, &pad, "WINDOW SIZE"));
		CHECK(rows_of(AE_SETTINGS_VIDEO, &pad) == 25 && rows_of(AE_SETTINGS_VIDEO, &hdr) == 26);
		set("display.mode", "fullscreen");
		CHECK(ae_settings_value_count(resolution) == 0);
		CHECK(ae_settings_set_values("display.resolution", texts, values, 2) && ae_settings_value_count(resolution) == 2);
		CHECK(!strcmp(ae_settings_value_text(resolution, 1), "1920 X 1080") && ae_settings_value_index(resolution,
			"1920x1080") == 1);
		CHECK(!ae_settings_set_values("display.fov", texts, values, 2));
		CHECK(!ae_settings_row_takes_empty(resolution));
	}

	/* AUDIO (13): INPUT DEVICE unless VOICE CHAT is OFF, PUSH TO TALK (a key row, no values) with PUSH TO TALK only;
	the volumes 0 to 10 as shown, 0 to 1 as written */
	{
		struct ae_settings_row const *master = ae_settings_row(ae_settings_row_of("audio.volume"));

		set("audio.voice_chat", "push_to_talk");
		CHECK(rows_of(AE_SETTINGS_AUDIO, &pad) == 15); /* (CALLOUTS off above: its row alone) */
		set("audio.voice_chat", "off");
		CHECK(rows_of(AE_SETTINGS_AUDIO, &pad) == 13 && !has_label(AE_SETTINGS_AUDIO, &pad, "INPUT DEVICE"));
		CHECK(ae_settings_row(ae_settings_row_of("controls.push_to_talk"))->kind == AE_SETTINGS_KEY &&
			!ae_settings_value_count(ae_settings_row(ae_settings_row_of("controls.push_to_talk"))));
		CHECK(ae_settings_value_count(master) == 11 && !strcmp(ae_settings_value_of(master, 10), "1") &&
			!strcmp(ae_settings_value_of(master, 4), "0.4") && !strcmp(ae_settings_value_text(master, 4), "4"));
		CHECK(ae_settings_value_index(master, "1.0") == 10 && ae_settings_value_index(master, "0.4") == 4);
		CHECK(ae_settings_row(ae_settings_row_of("audio.channels"))->flags & AE_SETTINGS_RESTART);
	}

	/* ONLINE (19 of rows.md's 22: GAME PORT, LAN ADDRESS and BROADCAST ADDRESSES wait for their own lists; and
	OpenCE's host SHIELD EFFECTS, HOSTING's; and INTERNET SHARING, MAPS's): INTERNET
	DOWNLOADS with DOWNLOAD MAPS on, UPLOAD LIMIT with HELP SEND MAPS on, the Discord rows unless DISCORD STATUS is OFF;
	COMMUNITY SERVICES ON / OFF writes yes / no */
	{
		struct ae_settings_row const *community = ae_settings_row(ae_settings_row_of("network.community_services"));

		set("network.map_download", "true");
		set("network.map_seed", "true");
		set("discord.presence", "full");
		CHECK(rows_of(AE_SETTINGS_ONLINE, &pad) == 21);
		set("network.map_download", "false");
		set("network.map_seed", "false");
		set("discord.presence", "off");
		CHECK(rows_of(AE_SETTINGS_ONLINE, &pad) == 17);
		CHECK(!has_label(AE_SETTINGS_ONLINE, &pad, "INTERNET DOWNLOADS") && !has_label(AE_SETTINGS_ONLINE, &pad,
			"DISCORD JOIN BUTTON"));
		CHECK(!strcmp(ae_settings_value_of(community, 0), "yes") && !strcmp(ae_settings_value_text(community, 1), "OFF"));
		CHECK(ae_settings_value_index(ae_settings_row(ae_settings_row_of("network.map_seed_upload")), "300") == 2);
	}

	/* ACCESSIBILITY (6; LOADING SCREEN went to VIDEO): the menus' rows this PC's (LANGUAGE needs a restart; English is the empty value), the links
	the HUD rows' own settings (row_of finds HUD's: where A goes) */
	{
		struct ae_settings_row const *language = ae_settings_row(ae_settings_row_of("game.language"));
		short entries_here[AE_SETTINGS_ENTRIES], here = ae_settings_entries(AE_SETTINGS_ACCESSIBILITY, &pad, entries_here,
			AE_SETTINGS_ENTRIES);

		CHECK(rows_of(AE_SETTINGS_ACCESSIBILITY, &pad) == 6 && here == 8);
		CHECK((language->flags & AE_SETTINGS_RESTART) && (language->flags & AE_SETTINGS_THIS_PC));
		CHECK(ae_settings_row_takes_empty(language) && ae_settings_value_index(language, "") == 0 &&
			ae_settings_value_index(language, "ja") == 5);
		CHECK(ae_settings_row(ae_settings_row_of("display.waypoint_size"))->group == AE_SETTINGS_HUD);
		CHECK(ae_settings_row(entries_here[6])->kind == AE_SETTINGS_LINK &&
			!strcmp(ae_settings_row(entries_here[6])->key, "display.player_name_scale"));
	}

	/* CHAT AND STREAMING (6, this PC's: the text chat is config.toml's for every player): the CENSOR rows with
	STREAMER MODE on */
	set("streamer.enabled", "true");
	CHECK(ae_settings_group(AE_SETTINGS_CHAT)->whose == AE_SETTINGS_PC && rows_of(AE_SETTINGS_CHAT, &pad) == 6);
	set("streamer.enabled", "false");
	CHECK(rows_of(AE_SETTINGS_CHAT, &pad) == 3 && !has_label(AE_SETTINGS_CHAT, &pad, "CENSOR SLURS"));
	CHECK(!strcmp(ae_settings_value_text(ae_settings_row(ae_settings_row_of("streamer.swearing")), 2),
		"STRONG AND WORSE"));
	/* every group built (phase B) */
	for (index = 0; index < AE_SETTINGS_GROUPS; index++)
		CHECK(ae_settings_group(index)->built);

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

	/* (0.3.9) the MOUSE sensitivities are number rows: 0.01 steps (Shift, Ctrl + wheel) and typed values */
	{
		struct ae_settings_row const *mcc = ae_settings_row(ae_settings_row_of("input.mouse_mcc_sensitivity"));
		struct ae_settings_row const *horizontal = ae_settings_row(ae_settings_row_of("input.mouse_sensitivity"));
		struct ae_settings_row const *vertical = ae_settings_row(ae_settings_row_of("input.mouse_vertical_sensitivity"));
		struct ae_settings_row const *zoom = ae_settings_row(ae_settings_row_of("input.mouse_zoom_scale"));
		struct ae_settings_row const *vehicle = ae_settings_row(ae_settings_row_of("input.mouse_vehicle_scale"));
		struct ae_settings_row const *hold = ae_settings_row(ae_settings_row_of("input.mouse_zoom_hold_time"));
		struct ae_settings_row const *fov = ae_settings_row(ae_settings_row_of("display.fov"));
		struct ae_settings_number range;
		char out[32];

		CHECK(ae_settings_row_number(mcc, &range) && range.minimum == 0.1 && range.maximum == 10.0 && !range.has_special);
		CHECK(ae_settings_row_number(horizontal, NULL) && ae_settings_row_number(zoom, NULL) &&
			ae_settings_row_number(vehicle, NULL));
		CHECK(ae_settings_row_number(vertical, &range) && range.has_special && range.special == 0.0);
		/* (not the others, nor the hold time: a time, not a sensitivity) */
		CHECK(!ae_settings_row_number(hold, NULL) && !ae_settings_row_number(fov, NULL) && !ae_settings_row_number(NULL,
			NULL));
		/* the text: two decimals only when the hundredths aren't 0 */
		ae_settings_number_text(1.37, out, sizeof(out));
		CHECK(!strcmp(out, "1.37"));
		ae_settings_number_text(1.4, out, sizeof(out));
		CHECK(!strcmp(out, "1.4"));
		ae_settings_number_text(10.0, out, sizeof(out));
		CHECK(!strcmp(out, "10.0"));
		ae_settings_number_text(0.05, out, sizeof(out));
		CHECK(!strcmp(out, "0.05"));
		ae_settings_number_text(2.999, out, sizeof(out));
		CHECK(!strcmp(out, "3.0"));
		/* shown: a step's own text, else the number; SAME stays SAME */
		ae_settings_number_shown(mcc, "1.37", out, sizeof(out));
		CHECK(!strcmp(out, "1.37"));
		ae_settings_number_shown(mcc, "1.4", out, sizeof(out));
		CHECK(!strcmp(out, "1.4"));
		ae_settings_number_shown(horizontal, "1", out, sizeof(out));
		CHECK(!strcmp(out, "1.0"));
		ae_settings_number_shown(horizontal, "1.33", out, sizeof(out));
		CHECK(!strcmp(out, "1.33"));
		ae_settings_number_shown(vertical, "0", out, sizeof(out));
		CHECK(!strcmp(out, "SAME"));
		ae_settings_number_shown(vertical, "0.0", out, sizeof(out));
		CHECK(!strcmp(out, "SAME"));
		/* (MCC's unset 0.0, outside the range: its nearest step as before) */
		ae_settings_number_shown(mcc, "0.0", out, sizeof(out));
		CHECK(!strcmp(out, "0.1"));
		/* typing: digits and one '.', two decimals, five characters */
		CHECK(ae_settings_number_typing_ok("") && ae_settings_number_typing_ok("1") && ae_settings_number_typing_ok("1.") &&
			ae_settings_number_typing_ok(".37") && ae_settings_number_typing_ok("10.00"));
		CHECK(!ae_settings_number_typing_ok("1.375") && !ae_settings_number_typing_ok("1..3") &&
			!ae_settings_number_typing_ok("1a") && !ae_settings_number_typing_ok("-1") &&
			!ae_settings_number_typing_ok("100.00") && !ae_settings_number_typing_ok(NULL));
		/* typed: kept to 0.01, clamped, a step's own text */
		CHECK(ae_settings_number_typed(mcc, "1.37", out, sizeof(out)) && !strcmp(out, "1.37"));
		CHECK(ae_settings_number_typed(mcc, "25", out, sizeof(out)) && !strcmp(out, "10.0"));
		CHECK(ae_settings_number_typed(mcc, "0.01", out, sizeof(out)) && !strcmp(out, "0.1"));
		CHECK(ae_settings_number_typed(mcc, "5.", out, sizeof(out)) && !strcmp(out, "5.0"));
		CHECK(ae_settings_number_typed(mcc, ".5", out, sizeof(out)) && !strcmp(out, "0.5"));
		CHECK(ae_settings_number_typed(horizontal, "1", out, sizeof(out)) && !strcmp(out, "1"));
		CHECK(ae_settings_number_typed(horizontal, "9", out, sizeof(out)) && !strcmp(out, "4"));
		CHECK(ae_settings_number_typed(zoom, "0.85", out, sizeof(out)) && !strcmp(out, "0.85"));
		CHECK(ae_settings_number_typed(vertical, "0", out, sizeof(out)) && !strcmp(out, "0"));
		CHECK(ae_settings_number_typed(vertical, "0.04", out, sizeof(out)) && !strcmp(out, "0.1"));
		CHECK(!ae_settings_number_typed(mcc, "", out, sizeof(out)) && !ae_settings_number_typed(mcc, ".", out,
			sizeof(out)) && !ae_settings_number_typed(fov, "90", out, sizeof(out)));
		/* fine steps: 0.01, held at the ends */
		CHECK(ae_settings_number_step(mcc, "1.4", 1, 1, out, sizeof(out)) && !strcmp(out, "1.41"));
		CHECK(ae_settings_number_step(mcc, "1.4", -1, 1, out, sizeof(out)) && !strcmp(out, "1.39"));
		CHECK(ae_settings_number_step(mcc, "1.39", 1, 1, out, sizeof(out)) && !strcmp(out, "1.4"));
		CHECK(!ae_settings_number_step(mcc, "10.0", 1, 1, out, sizeof(out)));
		CHECK(!ae_settings_number_step(mcc, "0.1", -1, 1, out, sizeof(out)));
		CHECK(ae_settings_number_step(horizontal, "1", 1, 1, out, sizeof(out)) && !strcmp(out, "1.01"));
		CHECK(ae_settings_number_step(horizontal, "1.01", -1, 1, out, sizeof(out)) && !strcmp(out, "1"));
		/* (from SAME: right the range's first, left held) */
		CHECK(ae_settings_number_step(vertical, "0", 1, 1, out, sizeof(out)) && !strcmp(out, "0.1"));
		CHECK(!ae_settings_number_step(vertical, "0", -1, 1, out, sizeof(out)));
		/* coarse steps: the next step past the value, round past the ends */
		CHECK(ae_settings_number_step(mcc, "1.37", 1, 0, out, sizeof(out)) && !strcmp(out, "1.4"));
		CHECK(ae_settings_number_step(mcc, "1.37", -1, 0, out, sizeof(out)) && !strcmp(out, "1.3"));
		CHECK(ae_settings_number_step(mcc, "1.4", 1, 0, out, sizeof(out)) && !strcmp(out, "1.5"));
		CHECK(ae_settings_number_step(mcc, "10.0", 1, 0, out, sizeof(out)) && !strcmp(out, "0.1"));
		CHECK(ae_settings_number_step(mcc, "0.1", -1, 0, out, sizeof(out)) && !strcmp(out, "10.0"));
		CHECK(ae_settings_number_step(horizontal, "1.37", 1, 0, out, sizeof(out)) && !strcmp(out, "1.5"));
		CHECK(ae_settings_number_step(horizontal, "1.37", -1, 0, out, sizeof(out)) && !strcmp(out, "1.25"));
		CHECK(ae_settings_number_step(vertical, "0.1", -1, 0, out, sizeof(out)) && !strcmp(out, "0"));
		CHECK(ae_settings_number_step(vertical, "4", 1, 0, out, sizeof(out)) && !strcmp(out, "0"));
		/* the slider: between the steps' places */
		CHECK(ae_settings_slider_at(mcc, "0.1") == 0.0f && ae_settings_slider_at(mcc, "10.0") == 1.0f);
		CHECK(ae_settings_slider_at(mcc, "1.35") > ae_settings_slider(mcc, 12) &&
			ae_settings_slider_at(mcc, "1.35") < ae_settings_slider(mcc, 13));
		CHECK(ae_settings_slider_at(horizontal, "1") == ae_settings_slider(horizontal, 6));
		CHECK(ae_settings_slider_at(vertical, "0") == 0.0f);
	}

	/* (0.7.0) FRAME RATE LIMIT: a whole-number row, typed 30 to 1000 besides its list */
	{
		struct ae_settings_row const *fps = ae_settings_row(ae_settings_row_of("display.max_fps"));
		struct ae_settings_whole range;
		struct ae_settings_whole_value values[32];
		char out[32];
		short count, index, typed_at = -1;

		CHECK(fps && ae_settings_row_whole(fps, &range) && range.minimum == 30 && range.maximum == 1000);
		CHECK(!ae_settings_row_whole(ae_settings_row(ae_settings_row_of("display.vsync")), NULL));
		/* shown: a list value's text, a typed value as itself */
		ae_settings_whole_shown(fps, "60", out, sizeof(out));
		CHECK(!strcmp(out, "60"));
		ae_settings_whole_shown(fps, "0", out, sizeof(out));
		CHECK(!strcmp(out, "AUTO"));
		ae_settings_whole_shown(fps, "-1", out, sizeof(out));
		CHECK(!strcmp(out, "NONE"));
		ae_settings_whole_shown(fps, "237", out, sizeof(out));
		CHECK(!strcmp(out, "237"));
		ae_settings_whole_shown(fps, "1000", out, sizeof(out));
		CHECK(!strcmp(out, "1000"));
		/* typing: digits, four at most; held to the range */
		CHECK(ae_settings_whole_typing_ok("") && ae_settings_whole_typing_ok("237") && ae_settings_whole_typing_ok("1000"));
		CHECK(!ae_settings_whole_typing_ok("12345") && !ae_settings_whole_typing_ok("1.5") &&
			!ae_settings_whole_typing_ok("-3") && !ae_settings_whole_typing_ok("2a") && !ae_settings_whole_typing_ok(NULL));
		CHECK(ae_settings_whole_typed(fps, "237", out, sizeof(out)) && !strcmp(out, "237"));
		CHECK(ae_settings_whole_typed(fps, "20", out, sizeof(out)) && !strcmp(out, "30"));
		CHECK(ae_settings_whole_typed(fps, "5000", out, sizeof(out)) && !strcmp(out, "1000"));
		CHECK(ae_settings_whole_typed(fps, "0", out, sizeof(out)) && !strcmp(out, "30"));
		CHECK(!ae_settings_whole_typed(fps, "", out, sizeof(out)) && !ae_settings_whole_typed(fps, "12a", out, sizeof(out)));
		/* steps go to the list's neighbours: 237 between 200 and 240, 500 between 360 and NONE */
		CHECK(ae_settings_whole_step(fps, "237", -1, out, sizeof(out)) && !strcmp(out, "200"));
		CHECK(ae_settings_whole_step(fps, "237", 1, out, sizeof(out)) && !strcmp(out, "240"));
		CHECK(ae_settings_whole_step(fps, "500", 1, out, sizeof(out)) && !strcmp(out, "-1"));
		CHECK(ae_settings_whole_step(fps, "500", -1, out, sizeof(out)) && !strcmp(out, "360"));
		CHECK(ae_settings_whole_step(fps, "60", 1, out, sizeof(out)) && !strcmp(out, "72"));
		CHECK(ae_settings_whole_step(fps, "30", -1, out, sizeof(out)) && !strcmp(out, "0"));
		CHECK(ae_settings_whole_step(fps, "0", 1, out, sizeof(out)) && !strcmp(out, "30"));
		CHECK(!ae_settings_whole_step(fps, "0", -1, out, sizeof(out)) && !ae_settings_whole_step(fps, "-1", 1, out,
			sizeof(out)));
		CHECK(ae_settings_whole_step(fps, "-1", -1, out, sizeof(out)) && !strcmp(out, "360"));
		/* ALL VALUES: the typed value in order, TYPE A NUMBER... last */
		count = ae_settings_whole_values(fps, "237", values, 32);
		CHECK(count >= 3 && values[count - 1].typed == 2 && !strcmp(values[count - 1].text, "TYPE A NUMBER..."));
		for (index = 0; index < count; index++)
			if (values[index].typed == 1)
				typed_at = index;
		CHECK(typed_at > 0 && !strcmp(values[typed_at].text, "237") && !strcmp(values[typed_at - 1].text, "200") &&
			!strcmp(values[typed_at + 1].text, "240"));
		count = ae_settings_whole_values(fps, "60", values, 32);
		for (index = 0, typed_at = -1; index < count; index++)
			if (values[index].typed == 1)
				typed_at = index;
		CHECK(typed_at < 0 && !strcmp(values[0].text, "AUTO") && !strcmp(values[count - 2].text, "NONE"));
		count = ae_settings_whole_values(fps, "700", values, 32);
		CHECK(!strcmp(values[count - 4].text, "360") && !strcmp(values[count - 3].text, "700") && values[count - 3].typed == 1 &&
			!strcmp(values[count - 2].text, "NONE") && !strcmp(values[count - 1].text, "TYPE A NUMBER..."));
	}

	if (failures)
	{
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("ae_settings_rows: ok\n");
	return 0;
}
