#include <stdio.h>
#include <string.h>

#include "ae_key_prompts_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the Xbox's default layout (input_abstraction.c): A jump, black switch grenades, X action, Y switch weapons, B
melee, white flashlight, left trigger grenade, right trigger fire, start, back, left thumb crouch, right thumb zoom */
static const unsigned char default_layout[AE_KEY_CONTROL_COUNT] = { 0, 4, 2, 3, 1, 5, 6, 7, 12, 13, 14, 15 };

static int glyph_is(const char *key, const char *glyph)
{
	const struct ae_key_cell *cell = ae_key_prompts_cell(ae_key_prompts_cell_of(key));

	return cell && !strcmp(cell->glyph, glyph);
}

static void test_action_to_setting(void)
{
	/* the actions name their control whatever the layout */
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(AE_KEY_ICON_ACTION, 2, NULL)),
		"controls.action"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(19, -1, NULL)),
		"controls.throw_grenade"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(20, -1, NULL)), "controls.fire"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(21, -1, NULL)),
		"controls.flashlight"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(22, -1, NULL)), "controls.jump"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(23, -1, NULL)), "controls.melee"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(24, -1, NULL)),
		"controls.switch_weapon"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(25, -1, NULL)),
		"controls.switch_grenade"));
	/* (CE's table: its crouch icon names the zoom control, as the pad's chip shows it) */
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(26, -1, NULL)), "controls.zoom"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(AE_KEY_ICON_ZOOM, -1, NULL)),
		"controls.crouch"));
	/* a physical button: the control the layout puts on it */
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(2, 2, default_layout)),
		"controls.action"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(0, 0, default_layout)),
		"controls.jump"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(13, 13, default_layout)),
		"controls.scoreboard"));
	CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(12, 12, default_layout)),
		"controls.pause"));
	{
		/* (another layout: X on melee) */
		unsigned char layout[AE_KEY_CONTROL_COUNT];

		memcpy(layout, default_layout, sizeof(layout));
		layout[2] = 1;
		layout[4] = 2;
		CHECK(!strcmp(ae_key_prompts_control_setting(ae_key_prompts_control_of_icon(2, 2, layout)),
			"controls.melee"));
	}
	/* no control: the sticks, a button no control is on, no layout, nonsense */
	CHECK(ae_key_prompts_control_of_icon(16, 16, default_layout) == -1);
	CHECK(ae_key_prompts_control_of_icon(AE_KEY_ICON_RIGHT_STICK, 17, default_layout) == -1);
	CHECK(ae_key_prompts_control_of_icon(8, 8, default_layout) == -1);
	CHECK(ae_key_prompts_control_of_icon(2, 2, NULL) == -1);
	CHECK(ae_key_prompts_control_of_icon(32, 2, default_layout) == -1);
	CHECK(ae_key_prompts_control_of_icon(-1, 2, default_layout) == -1);
	CHECK(ae_key_prompts_control_setting(-1) == NULL);
	CHECK(ae_key_prompts_control_setting(AE_KEY_CONTROL_COUNT) == NULL);
}

/* a stand-in for halo_input_from_name: any name but "Foo" */
static int named(const char *name)
{
	return strcmp(name, "Foo") != 0;
}

static void test_first_binding(void)
{
	char key[32];

	CHECK(ae_key_prompts_first_binding("Mouse Right, G", named, key, sizeof(key)) && !strcmp(key, "Mouse Right"));
	CHECK(ae_key_prompts_first_binding("E", named, key, sizeof(key)) && !strcmp(key, "E"));
	CHECK(ae_key_prompts_first_binding("  Left Ctrl ,C", named, key, sizeof(key)) && !strcmp(key, "Left Ctrl"));
	CHECK(ae_key_prompts_first_binding("Wheel, 1", named, key, sizeof(key)) && !strcmp(key, "Wheel"));
	/* (as xinput_sdl.c's bindings_read: empty pieces skipped, a name with no key passed over) */
	CHECK(ae_key_prompts_first_binding(", G", named, key, sizeof(key)) && !strcmp(key, "G"));
	CHECK(ae_key_prompts_first_binding(",, G", named, key, sizeof(key)) && !strcmp(key, "G"));
	CHECK(ae_key_prompts_first_binding("Foo, G", named, key, sizeof(key)) && !strcmp(key, "G"));
	/* (only two pieces are read) */
	CHECK(!ae_key_prompts_first_binding("Foo, Foo, G", named, key, sizeof(key)) && key[0] == 0);
	CHECK(!ae_key_prompts_first_binding("Foo", named, key, sizeof(key)));
	/* empty: no key (the pad's chip) */
	CHECK(!ae_key_prompts_first_binding("", named, key, sizeof(key)) && key[0] == 0);
	CHECK(!ae_key_prompts_first_binding("  ", named, key, sizeof(key)));
	CHECK(!ae_key_prompts_first_binding(" , ", named, key, sizeof(key)));
	CHECK(!ae_key_prompts_first_binding(NULL, named, key, sizeof(key)));
	/* no check: any name */
	CHECK(ae_key_prompts_first_binding("Foo, G", NULL, key, sizeof(key)) && !strcmp(key, "Foo"));
	/* cut to the buffer */
	CHECK(ae_key_prompts_first_binding("Mouse Middle", named, key, 6) && !strcmp(key, "Mouse"));
}

static void test_glyphs(void)
{
	const struct ae_key_cell *cell;

	/* letters and digits: the square keycaps, the outline's code point (the approved design's) */
	cell = ae_key_prompts_cell(ae_key_prompts_cell_of("E"));
	CHECK(cell && !strcmp(cell->glyph, "keyboard_e") && cell->code == 0xE05B && !cell->mouse);
	CHECK(ae_key_prompts_cell_of("e") == ae_key_prompts_cell_of("E"));
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("F"))->code == 0xE07F);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("G"))->code == 0xE083);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Q"))->code == 0xE0B4);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("R"))->code == 0xE0BA);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Z"))->code == 0xE0E8);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("1"))->code == 0xE004);
	/* the wide keys, both sides one glyph */
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Space"))->code == 0xE0CE);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Left Shift"))->code == 0xE0C6);
	CHECK(ae_key_prompts_cell_of("Right Shift") == ae_key_prompts_cell_of("Left Shift"));
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("F12"))->code == 0xE06D);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Left Ctrl"))->code == 0xE055);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Right Alt"))->code == 0xE018);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Tab"))->code == 0xE0D6);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Return"))->code == 0xE05F);
	CHECK(glyph_is("Escape", "keyboard_escape") && glyph_is("Backspace", "keyboard_backspace"));
	CHECK(glyph_is("Up", "keyboard_arrow_up") && glyph_is("F1", "keyboard_f1") && glyph_is("Comma", "keyboard_comma"));
	/* the mouse */
	cell = ae_key_prompts_cell(ae_key_prompts_cell_of("Mouse Left"));
	CHECK(cell && cell->code == 0xE0EC && cell->mouse);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Mouse Right"))->code == 0xE0F0);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Mouse Middle"))->code == 0xE0F4);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Wheel"))->code == 0xE0F4);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Wheel Up"))->code == 0xE0F6);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Wheel Down"))->code == 0xE0F3);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Mouse 4"))->code == 0xE0FB);
	CHECK(ae_key_prompts_cell(ae_key_prompts_cell_of("Mouse 5"))->code == 0xE0FD);
	/* no glyph: the plain keycap */
	CHECK(ae_key_prompts_cell_of("Keypad 7") == -1);
	CHECK(ae_key_prompts_cell_of("Menu") == -1);
	CHECK(ae_key_prompts_cell_of("") == -1);
	CHECK(ae_key_prompts_cell_of(NULL) == -1);
	CHECK(ae_key_prompts_cell(-1) == NULL);
	cell = ae_key_prompts_cell(ae_key_prompts_frame_cell());
	CHECK(cell && !strcmp(cell->glyph, "frame") && cell->code == 0);
}

static void test_fallback_text(void)
{
	char text[16];

	ae_key_prompts_fallback_text("Keypad 7", text, sizeof(text));
	CHECK(!strcmp(text, "Num 7"));
	ae_key_prompts_fallback_text("Menu", text, sizeof(text));
	CHECK(!strcmp(text, "Menu"));
	ae_key_prompts_fallback_text("AudioMute Long Name", text, sizeof(text));
	CHECK(!strcmp(text, "AudioMute L"));
	ae_key_prompts_fallback_text("Keypad Backspace", text, sizeof(text));
	CHECK(!strcmp(text, "Num Backspa"));
	ae_key_prompts_fallback_text("Keypad 7", text, 4);
	CHECK(text[0] == 0);
	ae_key_prompts_fallback_text(NULL, text, sizeof(text));
	CHECK(text[0] == 0);
}

static void test_wanted(void)
{
	/* alone: keys while the keyboard and mouse are the last used */
	CHECK(ae_key_prompts_wanted(1, 0, 0, 0, 0));
	CHECK(!ae_key_prompts_wanted(0, 0, 0, 0, 0));
	/* sharing the machine: the keyboard's player always, a pad player never, whatever was last used */
	CHECK(ae_key_prompts_wanted(0, 1, 1, 1, 0));
	CHECK(ae_key_prompts_wanted(1, 1, 1, 1, 0));
	CHECK(!ae_key_prompts_wanted(1, 0, 1, 1, 0));
	CHECK(!ae_key_prompts_wanted(0, 0, 1, 1, 0));
	/* another player's prompt, a forced pad, no keyboard player */
	CHECK(!ae_key_prompts_wanted(1, 1, 0, 0, 0));
	CHECK(!ae_key_prompts_wanted(1, 0, 0, 0, 1));
	CHECK(!ae_key_prompts_wanted(1, 0, 0, 1, 1));
	CHECK(!ae_key_prompts_wanted(1, -1, -1, 1, 0));
	CHECK(!ae_key_prompts_wanted(1, 0, -1, 1, 0));
}

static void test_player_of_controller(void)
{
	const short none[4] = { -1, -1, -1, -1 };
	const short campaign[4] = { 2, -1, -1, -1 };
	const short split[4] = { 1, 0, -1, -1 };

	/* a network game (split screen included): the controller's own index */
	CHECK(ae_key_prompts_player_of_controller(2, 1, campaign, 4) == 2);
	CHECK(ae_key_prompts_player_of_controller(0, 1, split, 4) == 0);
	/* else the single player bindings: controller 2 plays player 1 (index 0); controller 0 plays nobody */
	CHECK(ae_key_prompts_player_of_controller(2, 0, campaign, 4) == 0);
	CHECK(ae_key_prompts_player_of_controller(0, 0, campaign, 4) == -1);
	CHECK(ae_key_prompts_player_of_controller(0, 0, split, 4) == 1);
	CHECK(ae_key_prompts_player_of_controller(1, 0, split, 4) == 0);
	/* none bound: the controller's own index; no keyboard controller: nobody */
	CHECK(ae_key_prompts_player_of_controller(1, 0, none, 4) == 1);
	CHECK(ae_key_prompts_player_of_controller(-1, 0, none, 4) == -1);
	CHECK(ae_key_prompts_player_of_controller(-1, 1, none, 4) == -1);
	CHECK(ae_key_prompts_player_of_controller(3, 0, NULL, 0) == 3);
}

static void test_sizes(void)
{
	float w, h, m;

	/* the AE keycap 26 px at 1080p, 2 px of outline round it: 30 x 30 */
	ae_key_prompts_picture_size(ae_key_prompts_cell_of("E"), AE_KEY_STYLE_AE, 26.0f, &w, &h, &m);
	CHECK(h > 29.9f && h < 30.1f && w > 29.0f && w < 31.0f && m > 1.99f && m < 2.01f);
	/* scaled with the HUD */
	ae_key_prompts_picture_size(ae_key_prompts_cell_of("E"), AE_KEY_STYLE_AE, 52.0f, &w, &h, &m);
	CHECK(h > 59.8f && h < 60.2f && m > 3.99f && m < 4.01f);
	/* a mouse 30 px for a 26 px keycap, a space wider than a letter */
	ae_key_prompts_picture_size(ae_key_prompts_cell_of("Mouse Right"), AE_KEY_STYLE_AE, 26.0f, &w, &h, &m);
	CHECK(h > 33.9f && h < 34.1f && w < h);
	ae_key_prompts_picture_size(ae_key_prompts_cell_of("Space"), AE_KEY_STYLE_AE, 26.0f, &w, &h, &m);
	CHECK(w > 40.0f + 4.0f);
	/* the CE keycap 30 px, 3 px of shadow round it */
	ae_key_prompts_picture_size(ae_key_prompts_cell_of("E"), AE_KEY_STYLE_CE, 30.0f, &w, &h, &m);
	CHECK(h > 35.9f && h < 36.1f && m > 2.99f && m < 3.01f);
	ae_key_prompts_picture_size(-1, AE_KEY_STYLE_AE, 26.0f, &w, &h, &m);
	CHECK(w == 0.0f && h == 0.0f && m == 0.0f);
	/* the plain keycap: the name + 0.62 H, at least H */
	CHECK(ae_key_prompts_frame_width(40.0f, 26.0f) > 56.0f && ae_key_prompts_frame_width(40.0f, 26.0f) < 56.2f);
	CHECK(ae_key_prompts_frame_width(5.0f, 26.0f) == 26.0f);
	{
		float sw, sh;

		ae_key_prompts_sheet_size(AE_KEY_STYLE_AE, &sw, &sh);
		CHECK(sw == 2048.0f && sh > 0.0f);
	}
}

int main(void)
{
	test_action_to_setting();
	test_first_binding();
	test_glyphs();
	test_fallback_text();
	test_wanted();
	test_player_of_controller();
	test_sizes();
	if (failures)
		return 1;
	printf("ae_key_prompts_rules_test: ok\n");
	return 0;
}
