/* ae_forge_doors_rules.c: the pack's rows, a map's title, FORGE KEYS' key and button numbers and the pad speeds' steps */
#include <stdio.h>
#include <string.h>

#include "ae_forge_doors_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char text[64];
	int action;

	/* the pack: ready wins; writing or queued is building; failed or no maps wanted needs the maps */
	CHECK(ae_forge_doors_pack_state(1, 1, 1, 0) == AE_FORGE_PACK_READY);
	CHECK(ae_forge_doors_pack_state(0, 1, 0, 17) == AE_FORGE_PACK_BUILDING);
	CHECK(ae_forge_doors_pack_state(0, 0, 0, 17) == AE_FORGE_PACK_BUILDING);
	CHECK(ae_forge_doors_pack_state(0, 0, 1, 17) == AE_FORGE_PACK_NEEDS_MAPS);
	CHECK(ae_forge_doors_pack_state(0, 0, 0, 0) == AE_FORGE_PACK_NEEDS_MAPS);
	CHECK(ae_forge_doors_percent(0, 17) == 0 && ae_forge_doors_percent(17, 17) == 100 && ae_forge_doors_percent(9, 17) == 52);
	CHECK(ae_forge_doors_percent(5, 0) == 0 && ae_forge_doors_percent(30, 17) == 100 && ae_forge_doors_percent(-1, 17) == 0);
	ae_forge_doors_pack_reason(AE_FORGE_PACK_BUILDING, 40, text, sizeof(text));
	CHECK(!strcmp(text, "GETTING READY 40 %"));
	ae_forge_doors_pack_reason(AE_FORGE_PACK_BUILDING, 140, text, sizeof(text));
	CHECK(!strcmp(text, "GETTING READY 100 %"));
	ae_forge_doors_pack_reason(AE_FORGE_PACK_NEEDS_MAPS, 0, text, sizeof(text));
	CHECK(!strcmp(text, "NEEDS THE GAME'S MAPS"));
	ae_forge_doors_pack_reason(AE_FORGE_PACK_READY, 100, text, sizeof(text));
	CHECK(text[0] == 0);
	ae_forge_doors_pack_progress(6, 17, text, sizeof(text));
	CHECK(!strcmp(text, "6 OF 17 MAPS READ"));
	ae_forge_doors_pack_progress(6, 0, text, sizeof(text));
	CHECK(text[0] == 0);
	ae_forge_doors_pack_reason(AE_FORGE_PACK_BUILDING, 5, text, 8);
	CHECK(strlen(text) == 7);

	/* the hub rows that wait on the pack */
	CHECK(ae_forge_doors_row_needs_pack(AE_FORGE_HUB_CONTINUE) && ae_forge_doors_row_needs_pack(AE_FORGE_HUB_NEW_MAP));
	CHECK(!ae_forge_doors_row_needs_pack(AE_FORGE_HUB_MY_MAPS) && !ae_forge_doors_row_needs_pack(AE_FORGE_HUB_ITEM_SETS) &&
		!ae_forge_doors_row_needs_pack(AE_FORGE_HUB_CONTROLS));

	/* a map's title */
	ae_forge_doors_map_title("levels\\test\\bloodgulch\\bloodgulch", text, sizeof(text));
	CHECK(!strcmp(text, "BLOODGULCH"));
	ae_forge_doors_map_title("gulch_remix@ce", text, sizeof(text));
	CHECK(!strcmp(text, "GULCH_REMIX"));
	ae_forge_doors_map_title("", text, sizeof(text));
	CHECK(text[0] == 0);
	ae_forge_doors_map_title(NULL, text, sizeof(text));
	CHECK(text[0] == 0);
	ae_forge_doors_map_title("levels\\test\\bloodgulch\\bloodgulch", text, 5);
	CHECK(!strcmp(text, "BLOO"));

	/* the twelve actions have a label and a help each, in Content Evolved's order */
	for (action = 0; action < AE_FORGE_KEYS_ACTIONS; action++)
		CHECK(ae_forge_keys_label(action) && ae_forge_keys_help(action) && ae_forge_keys_help(action)[0]);
	CHECK(!strcmp(ae_forge_keys_label(0), "FORGE / SPARTAN") && !strcmp(ae_forge_keys_label(11), "OBJECT SETTINGS"));
	CHECK(ae_forge_keys_label(-1) == NULL && ae_forge_keys_label(AE_FORGE_KEYS_ACTIONS) == NULL);
	CHECK(ae_forge_keys_help(AE_FORGE_KEYS_ACTIONS) == NULL);

	/* SDL scancodes as virtual keys: letters, digits, F keys, modifiers, arrows; Escape, Backspace and Delete are the
	screen's own (cancel, clear) */
	CHECK(ae_forge_keys_vk_of_scancode(4) == 'A' && ae_forge_keys_vk_of_scancode(29) == 'Z');
	CHECK(ae_forge_keys_vk_of_scancode(30) == '1' && ae_forge_keys_vk_of_scancode(38) == '9' && ae_forge_keys_vk_of_scancode(39) == '0');
	CHECK(ae_forge_keys_vk_of_scancode(58) == 0x70 && ae_forge_keys_vk_of_scancode(69) == 0x7B);
	CHECK(ae_forge_keys_vk_of_scancode(226) == 0xA4 && ae_forge_keys_vk_of_scancode(225) == 0xA0);
	CHECK(ae_forge_keys_vk_of_scancode(80) == 0x25 && ae_forge_keys_vk_of_scancode(79) == 0x27);
	CHECK(ae_forge_keys_vk_of_scancode(44) == 0x20 && ae_forge_keys_vk_of_scancode(43) == 0x09);
	CHECK(ae_forge_keys_vk_of_scancode(41) == 0 && ae_forge_keys_vk_of_scancode(42) == 0 && ae_forge_keys_vk_of_scancode(76) == 0);
	CHECK(ae_forge_keys_vk_of_scancode(-1) == 0 && ae_forge_keys_vk_of_scancode(500) == 0 && ae_forge_keys_vk_of_scancode(0) == 0);
	CHECK(ae_forge_keys_vk_of_mouse(1) == 0x01 && ae_forge_keys_vk_of_mouse(3) == 0x02 && ae_forge_keys_vk_of_mouse(2) == 0x04);
	CHECK(ae_forge_keys_vk_of_mouse(4) == 0 && ae_forge_keys_vk_of_mouse(0) == 0);

	/* the controller's buttons as Forge's (A is 1, the right stick 16) */
	CHECK(ae_forge_keys_gp_of_button(0) == 1 && ae_forge_keys_gp_of_button(3) == 4 && ae_forge_keys_gp_of_button(15) == 16);
	CHECK(ae_forge_keys_gp_of_button(-1) == 0 && ae_forge_keys_gp_of_button(16) == 0);

	/* the pad speeds: 25 % steps from 25 to 250 */
	CHECK(ae_forge_keys_pad_percent(ae_forge_keys_pad_step(1.0f, 1)) == 125);
	CHECK(ae_forge_keys_pad_percent(ae_forge_keys_pad_step(1.0f, -1)) == 75);
	CHECK(ae_forge_keys_pad_percent(ae_forge_keys_pad_step(0.25f, -1)) == 25);
	CHECK(ae_forge_keys_pad_percent(ae_forge_keys_pad_step(2.5f, 1)) == 250);
	CHECK(ae_forge_keys_pad_percent(ae_forge_keys_pad_step(0.2f, 1)) == 50);
	CHECK(ae_forge_keys_pad_percent(ae_forge_keys_pad_step(4.0f, 0)) == 250);
	CHECK(ae_forge_keys_pad_percent(ae_forge_keys_pad_step(1.1f, 0)) == 100);

	ae_forge_keys_taken_text("F2", "OBJECT PALETTE", text, sizeof(text));
	CHECK(!strcmp(text, "F2 moved here from OBJECT PALETTE, which has none now."));

	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_forge_doors_rules_test: all passed\n");
	return 0;
}
