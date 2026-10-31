/* ae_key_bindings_test.c: the KEY BINDINGS model (ae_key_bindings.h): the actions and sections, the list, a setting's
two bindings read and written, a binding moved from another action, the dialogs' texts. */

#include <stdio.h>
#include <string.h>

#include "ae_key_bindings.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void actions_and_list(void)
{
	short entries[AE_KEYS_ENTRIES], count, place;

	CHECK(ae_keys_action_count() == 32 && ae_keys_action_count() <= AE_KEYS_ACTIONS);
	CHECK(ae_keys_action_of("controls.jump") == 4 && !strcmp(ae_keys_action(4)->label, "JUMP"));
	CHECK(ae_keys_action_of("controls.chat_all") == -1 && ae_keys_action_of("controls.controller_layout") == -1);
	/* (the classic menus' 21 in 4 sections: 6, 7, 5, 3; CHAT LOG only with ChupathingyCE's chat on) */
	count = ae_keys_entries(0, entries, AE_KEYS_ENTRIES);
	CHECK(count == 21 + 4);
	CHECK(ae_keys_entry_is_header(entries[0]) && ae_keys_section_at(entries, count, 0) == 0);
	CHECK(ae_keys_section_rows(entries, count, 0) == 6 && ae_keys_section_rows(entries, count, 1) == 7);
	CHECK(ae_keys_section_rows(entries, count, 2) == 5 && ae_keys_section_rows(entries, count, 3) == 3);
	CHECK(ae_keys_section_place(entries, count, 1) == 7 && ae_keys_section_place(entries, count, 3) == 21);
	CHECK(!strcmp(ae_keys_section_name(3), "CHAT"));
	CHECK(ae_keys_entries(1, entries, AE_KEYS_ENTRIES) == 22 + 4);
	/* (DEV MODE's three cameras and TEST RUN's seven verdict keys in ACTIONS, only with it on; with the chat too, all
	of them in 4 sections) */
	count = ae_keys_entries(AE_KEYS_WITH_DEV_MODE, entries, AE_KEYS_ENTRIES);
	CHECK(count == 31 + 4 && ae_keys_section_rows(entries, count, 2) == 15);
	CHECK(ae_keys_entries(AE_KEYS_WITH_DEV_MODE | AE_KEYS_WITH_CHUPA_CHAT, entries, AE_KEYS_ENTRIES) == 32 + 4);
	CHECK(ae_keys_action_of("controls.dev_free_camera") == 24);
	count = ae_keys_entries(0, entries, AE_KEYS_ENTRIES);
	/* (steps skip headers; the ends stay) */
	CHECK(ae_keys_row_step(entries, count, 6, 1) == 8);
	CHECK(ae_keys_row_step(entries, count, 8, -1) == 6);
	CHECK(ae_keys_row_step(entries, count, 1, -1) == 1);
	place = ae_keys_row_step(entries, count, (short)(count - 1), 1);
	CHECK(place == count - 1);
	/* (a short list is cut, never overrun) */
	CHECK(ae_keys_entries(0, entries, 3) == 3);
}

static void test_actions_listed(void)
{
	static const char *const keys[] = { "controls.test_pass", "controls.test_fail", "controls.test_note",
		"controls.test_skip", "controls.test_reload", "controls.test_back", "controls.test_hide_card" };
	short index;

	for (index = 0; index < 7; index++)
	{
		short action = ae_keys_action_of(keys[index]);

		CHECK(action == 25 + index);
		CHECK(action >= 0 && !strncmp(ae_keys_action(action)->label, "TEST ", 5));
		CHECK(action >= 0 && (ae_keys_action(action)->only_with & AE_KEYS_WITH_DEV_MODE));
	}
}

static void parse_and_join(void)
{
	char names[AE_KEYS_SLOTS][AE_KEYS_NAME], text[80];

	ae_keys_parse("Left Ctrl, C", names);
	CHECK(!strcmp(names[0], "Left Ctrl") && !strcmp(names[1], "C"));
	ae_keys_parse("  Mouse Right ,G , H", names);
	CHECK(!strcmp(names[0], "Mouse Right") && !strcmp(names[1], "G"));
	ae_keys_parse("", names);
	CHECK(!names[0][0] && !names[1][0]);
	ae_keys_parse(", C", names);
	CHECK(!names[0][0] && !strcmp(names[1], "C"));
	ae_keys_parse("Space", names);
	CHECK(!strcmp(names[0], "Space") && !names[1][0]);
	ae_keys_parse("An input name far longer than any key's name is", names);
	CHECK(strlen(names[0]) == AE_KEYS_NAME - 1);
	snprintf(names[0], AE_KEYS_NAME, "F");
	snprintf(names[1], AE_KEYS_NAME, "Mouse 4");
	ae_keys_join(names, text, sizeof(text));
	CHECK(!strcmp(text, "F, Mouse 4"));
	names[0][0] = 0;
	ae_keys_join(names, text, sizeof(text));
	CHECK(!strcmp(text, "Mouse 4"));
	names[1][0] = 0;
	ae_keys_join(names, text, sizeof(text));
	CHECK(!strcmp(text, ""));
}

static void binding(void)
{
	struct ae_keys keys;
	short jump = ae_keys_action_of("controls.jump"), light = ae_keys_action_of("controls.flashlight");
	short weapon = ae_keys_action_of("controls.switch_weapon"), from;
	char text[160];

	memset(&keys, 0, sizeof(keys));
	ae_keys_parse("Space", keys.names[jump]);
	ae_keys_parse("Q, G", keys.names[light]);
	ae_keys_parse("Wheel, 1", keys.names[weapon]);
	CHECK(ae_keys_overlap("g", "G") && ae_keys_overlap("Wheel", "wheel up") && ae_keys_overlap("Wheel Down", "Wheel"));
	CHECK(!ae_keys_overlap("Wheel Up", "Wheel Down") && !ae_keys_overlap("", "") && !ae_keys_overlap("G", "H"));
	/* G on JUMP: FLASHLIGHT loses it, keeps Q */
	CHECK(ae_keys_bind(&keys, jump, 0, "G", &from) == 1 && from == light);
	CHECK(!strcmp(keys.names[jump][0], "G") && !strcmp(keys.names[light][0], "Q") && !keys.names[light][1][0]);
	CHECK(ae_keys_has_any(&keys, light));
	/* Q too: FLASHLIGHT has none */
	CHECK(ae_keys_bind(&keys, jump, 1, "q", &from) == 1 && from == light && !ae_keys_has_any(&keys, light));
	/* JUMP's own G onto its second slot: it moves (no other action named) */
	CHECK(ae_keys_bind(&keys, jump, 1, "G", &from) == 1 && from == -1 && !keys.names[jump][0][0]);
	/* the wheel's up: SWITCH WEAPON's "Wheel" goes */
	CHECK(ae_keys_bind(&keys, jump, 0, "Wheel Up", &from) == 1 && from == weapon);
	CHECK(!keys.names[weapon][0][0] && !strcmp(keys.names[weapon][1], "1"));
	/* nothing there: nothing lost */
	CHECK(ae_keys_bind(&keys, light, 0, "F7", &from) == 0 && from == -1);
	/* cleared */
	CHECK(ae_keys_bind(&keys, light, 0, "", NULL) == 0 && !keys.names[light][0][0]);
	CHECK(ae_keys_bind(&keys, light, 2, "F7", &from) == 0 && ae_keys_bind(&keys, -1, 0, "F7", &from) == 0);
	ae_keys_moved_text("G", "FLASHLIGHT", 1, text, sizeof(text));
	CHECK(!strcmp(text, "G moved here from FLASHLIGHT, which has no key now."));
	ae_keys_moved_text("MOUSE 4", "MELEE", 0, text, sizeof(text));
	CHECK(!strcmp(text, "MOUSE 4 moved here from MELEE."));
}

static void dialogs(void)
{
	static const char *const labels[] = { "JUMP", "FLASHLIGHT", "MELEE", "ZOOM", "FIRE" };
	char text[400];

	ae_keys_keep_body(labels, 1, text, sizeof(text));
	CHECK(!strcmp(text, "1 action has new keys on this PC: JUMP. They are in use already."));
	ae_keys_keep_body(labels, 3, text, sizeof(text));
	CHECK(!strcmp(text, "3 actions have new keys on this PC: JUMP, FLASHLIGHT and MELEE. They are in use already."));
	ae_keys_keep_body(labels, 5, text, sizeof(text));
	CHECK(!strcmp(text, "5 actions have new keys on this PC: JUMP, FLASHLIGHT, MELEE and 2 more. They are in use "
		"already."));
	ae_keys_restore_body("MOVEMENT", 0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "Every key is already the game's."));
	ae_keys_restore_body("MOVEMENT", 1, 3, text, sizeof(text));
	CHECK(!strcmp(text, "MOVEMENT: 1 action goes back to the game's keys. Every section: 3 actions. You can still "
		"undo it as you leave."));
	ae_keys_restore_body("WEAPONS", 0, 1, text, sizeof(text));
	CHECK(!strcmp(text, "WEAPONS has the game's keys already. Every section: 1 action goes back to the game's keys. "
		"You can still undo it as you leave."));
}

int main(void)
{
	actions_and_list();
	test_actions_listed();
	parse_and_join();
	binding();
	dialogs();
	if (failures)
	{
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("ae_key_bindings_test: ok\n");
	return 0;
}
