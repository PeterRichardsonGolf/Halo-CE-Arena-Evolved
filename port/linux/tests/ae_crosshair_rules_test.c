#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_crosshair_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int same(const struct ae_crosshair *a, const struct ae_crosshair *b)
{
	struct ae_crosshair x = *a, y = *b;

	memset(x.name, 0, sizeof(x.name));
	memset(y.name, 0, sizeof(y.name));
	return !memcmp(&x, &y, sizeof(x));
}

/* the fuzz entry the sanitizer build and libFuzzer share */
int ae_xh_fuzz_one(const unsigned char *data, size_t size)
{
	char text[AE_XH_CODE_INPUT_MAX + 40];
	struct ae_crosshair xh, again;
	struct ae_xh_weapons table;
	int result, unknown;
	char out[AE_XH_CODE_TEXT], line[512];

	if (size >= sizeof(text))
		size = sizeof(text) - 1;
	memcpy(text, data, size);
	text[size] = 0;
	memset(&xh, 0, sizeof(xh));
	result = ae_xh_code_decode(text, &xh, &unknown);
	if (result == AE_XH_CODE_OK)
	{
		/* (whatever decodes is in range and encodes to a code that decodes to the same) */
		ae_xh_code_encode(&xh, out, sizeof(out));
		memset(&again, 0, sizeof(again));
		if (ae_xh_code_decode(out, &again, NULL) != AE_XH_CODE_OK || !same(&xh, &again))
			return 1;
		if (xh.shape > ae_xh_shape_count() || xh.opacity > 100 || xh.frame_size < AE_XH_FRAME_SIZE_MIN ||
			xh.frame_size > AE_XH_FRAME_SIZE_MAX || xh.inner.length > AE_XH_LENGTH_MAX ||
			xh.centre_size < 1 || xh.centre_size > AE_XH_CENTRE_SIZE_MAX || xh.centre > AE_XH_CENTRE_PLUS)
			return 1;
		{
			struct ae_xh_piece pieces[AE_XH_PIECES];
			struct ae_xh_motion motion = { 4.0f, 2.0f, 9.0f };

			if (ae_xh_build(&xh, &motion, 0xFFFFFFul, 1.0f, pieces, AE_XH_PIECES) < 0)
				return 1;
		}
	}
	memset(&xh, 0, sizeof(xh));
	ae_xh_slot_parse(text, &xh);
	ae_xh_weapons_parse(text, &table);
	ae_xh_weapons_format(&table, line, sizeof(line));
	/* (0.7.7: a typed CUSTOM COLOR, and a tag name made a weapon key: anything that reads must format back the same) */
	{
		unsigned long rgb;
		char key[AE_XH_WEAPON_KEY + 8];

		if (ae_xh_hex_parse(text, &rgb))
		{
			unsigned long back;

			ae_xh_hex_format(rgb, line, sizeof(line));
			if (rgb > 0xFFFFFFul || !ae_xh_hex_parse(line, &back) || back != rgb)
				return 1;
		}
		ae_xh_weapon_key_of_tag(text, key, sizeof(key));
		if (strlen(key) >= AE_XH_WEAPON_KEY)
			return 1;
	}
	return 0;
}

#ifdef AE_XH_FUZZER
int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
	return ae_xh_fuzz_one(data, size) ? __builtin_trap(), 0 : 0;
}
#else

static unsigned long long state = 88172645463325252ull;
static unsigned long long rnd(void)
{
	state ^= state << 13;
	state ^= state >> 7;
	state ^= state << 17;
	return state;
}

int main(void)
{
	struct ae_crosshair xh, back, made;
	struct ae_xh_piece pieces[AE_XH_PIECES];
	char code[AE_XH_CODE_TEXT], text[160];
	int shape, unknown, index, n;

	CHECK(ae_xh_shape_count() == 37);
	CHECK(!strcmp(ae_xh_shape_name(4), "TWIN CROSS"));
	CHECK(ae_xh_shape_of_name("OCTAGON") == 31 && ae_xh_shape_of_name("NOPE") == 0);
	CHECK(ae_xh_shape_family(1) == 0 && ae_xh_shape_family(11) == 1 && ae_xh_shape_family(16) == 2 && ae_xh_shape_family(29) == 3);

	/* every shape: applies, round-trips through its code, builds pieces inside the 128 box at 100 % */
	for (shape = 1; shape <= 37; shape++)
	{
		ae_xh_default(&xh);
		ae_xh_apply_shape(&xh, shape);
		ae_xh_code_encode(&xh, code, sizeof(code));
		CHECK(strlen(code) == 4 + 37 + 9);
		memset(&back, 0, sizeof(back));
		CHECK(ae_xh_code_decode(code, &back, &unknown) == AE_XH_CODE_OK && !unknown && same(&xh, &back));
		n = ae_xh_build(&xh, NULL, 0xFFFFFFul, 1.0f, pieces, AE_XH_PIECES);
		CHECK(n > 0 && n < AE_XH_PIECES);
		CHECK(ae_xh_extent(&xh) <= 64.0f);
		CHECK(ae_xh_extent(&xh) > 1.0f);
	}
	/* the documented example form */
	ae_xh_default(&xh);
	ae_xh_code_encode(&xh, code, sizeof(code));
	CHECK(!strncmp(code, "AE1-", 4) && code[8] == '-' && code[13] == '-');

	/* tolerance: lower case, no dashes, spaces, O for 0, I and L for 1 */
	{
		char loose[AE_XH_CODE_TEXT * 2];
		size_t at = 0, i;

		for (i = 0; code[i]; i++)
		{
			char c = code[i];

			if (c == '-')
				continue;
			if (c == '0')
				c = 'o';
			else if (c == '1' && i > 2)
				c = 'l';
			else if (c >= 'A' && c <= 'Z')
				c = (char)(c - 'A' + 'a');
			loose[at++] = c;
			if (at % 5 == 0)
				loose[at++] = ' ';
		}
		loose[at] = 0;
		memset(&back, 0, sizeof(back));
		CHECK(ae_xh_code_decode(loose, &back, NULL) == AE_XH_CODE_OK && same(&xh, &back));
	}

	/* refusals: one character changed (the CRC), a short and a long code, a version above the build's, no prefix */
	for (index = 4; index < (int)strlen(code); index++)
	{
		char bad[AE_XH_CODE_TEXT];

		if (code[index] == '-')
			continue;
		snprintf(bad, sizeof(bad), "%s", code);
		bad[index] = bad[index] == '7' ? '8' : '7';
		CHECK(ae_xh_code_decode(bad, &back, NULL) == AE_XH_CODE_DAMAGED);
	}
	snprintf(text, sizeof(text), "%.40s", code);
	CHECK(ae_xh_code_decode(text, &back, NULL) == AE_XH_CODE_DAMAGED);
	snprintf(text, sizeof(text), "%sQ", code);
	CHECK(ae_xh_code_decode(text, &back, NULL) == AE_XH_CODE_DAMAGED);
	text[0] = 'A'; text[1] = 'E'; text[2] = '2';
	snprintf(text, sizeof(text), "AE2-%s", code + 4);
	CHECK(ae_xh_code_decode(text, &back, NULL) == AE_XH_CODE_NEWER);
	CHECK(ae_xh_code_decode("hello world", &back, NULL) == AE_XH_CODE_NOT_A_CODE);
	CHECK(ae_xh_code_decode("", &back, NULL) == AE_XH_CODE_NOT_A_CODE);
	CHECK(ae_xh_code_decode(NULL, &back, NULL) == AE_XH_CODE_NOT_A_CODE);
	CHECK(ae_xh_code_decode("AE1-UUUU", &back, NULL) == AE_XH_CODE_NOT_A_CODE);
	CHECK(ae_xh_code_decode("AE1-%%%%", &back, NULL) == AE_XH_CODE_NOT_A_CODE);
	{
		char huge[400];

		memset(huge, '0', sizeof(huge));
		memcpy(huge, "AE1-", 4);
		huge[399] = 0;
		CHECK(ae_xh_code_decode(huge, &back, NULL) == AE_XH_CODE_NOT_A_CODE);
	}
	/* a refused code leaves the crosshair alone */
	made = xh;
	made.opacity = 33;
	CHECK(ae_xh_code_decode("AE1-0000-0000", &made, NULL) != AE_XH_CODE_OK && made.opacity == 33);

	/* an unknown shape id (a newer library) falls back to none with a note: build the bytes by hand */
	{
		struct ae_crosshair odd = xh;
		char again[AE_XH_CODE_TEXT];

		odd.shape = 37;
		ae_xh_code_encode(&odd, again, sizeof(again));
		CHECK(ae_xh_code_decode(again, &back, &unknown) == AE_XH_CODE_OK && back.shape == 37 && !unknown);
	}

	/* clamps */
	memset(&made, 255, sizeof(made));
	ae_xh_clamp(&made);
	CHECK(made.shape == 0 && made.opacity == 100 && made.frame_size == 200 && made.inner.length == 80 &&
		made.outline_thickness == 8 && made.centre == 0 && made.centre_size == 16 && made.inner.on == 1);
	memset(&made, 0, sizeof(made));
	ae_xh_clamp(&made);
	CHECK(made.opacity == 10 && made.frame_size == 50 && made.inner.thickness == 1 && made.centre_size == 1 &&
		made.outline_thickness == 1);

	/* names: cleaned, raised, 12 at most, trimmed */
	CHECK(ae_xh_name_clean("  my cross!! v2 long long name", text, sizeof(text)) == 11 && !strcmp(text, "MY CROSS V2"));
	ae_xh_name_clean("  ab%cd ", text, sizeof(text));
	CHECK(!strcmp(text, "ABCD"));
	CHECK(ae_xh_name_clean("%%%", text, sizeof(text)) == 0 && !text[0]);

	/* the profile's strings */
	ae_xh_default(&xh);
	snprintf(xh.name, sizeof(xh.name), "SNAP");
	ae_xh_slot_format(&xh, text, sizeof(text));
	memset(&back, 0, sizeof(back));
	CHECK(ae_xh_slot_parse(text, &back) && !strcmp(back.name, "SNAP") && same(&xh, &back));
	CHECK(!ae_xh_slot_parse("", &back) && !ae_xh_slot_parse("NAME", &back) && !ae_xh_slot_parse("NAME|AE1-0", &back));

	/* the per-weapon table keeps unknown keys, drops MAIN, refuses bad keys and a full table */
	{
		struct ae_xh_weapons table;
		int own, custom;

		ae_xh_weapons_parse("battle_rifle=own;needler=2;junk;bad key=1;pistol=9;x=0", &table);
		CHECK(table.count == 2);
		CHECK(ae_xh_weapons_get(&table, "battle_rifle") == AE_XH_CHOICE_OWN);
		CHECK(ae_xh_weapons_get(&table, "needler") == 1);
		CHECK(ae_xh_weapons_get(&table, "pistol") == AE_XH_CHOICE_MAIN);
		CHECK(ae_xh_weapons_set(&table, "carbine", 7) && ae_xh_weapons_set(&table, "needler", AE_XH_CHOICE_MAIN));
		ae_xh_weapons_summary(&table, &own, &custom);
		CHECK(own == 1 && custom == 1);
		ae_xh_weapons_format(&table, text, sizeof(text));
		CHECK(!strcmp(text, "battle_rifle=own;carbine=8"));
		CHECK(!ae_xh_weapons_set(&table, "Bad Key", 1));
		for (index = 0; index < 40; index++)
		{
			char key[16];

			snprintf(key, sizeof(key), "w%d", index);
			ae_xh_weapons_set(&table, key, 0);
		}
		CHECK(table.count == AE_XH_WEAPON_ENTRIES);
		CHECK(ae_xh_weapons_set(&table, "w0", AE_XH_CHOICE_MAIN));
	}
	ae_xh_weapon_key_of_tag("weapons\\assault rifle\\assault rifle", text, sizeof(text));
	CHECK(!strcmp(text, "assault_rifle"));
	/* (0.7.7) a multiplayer copy is the same gun; the stationary gun's two tags one key (the CE/MCC maps' own tag names) */
	ae_xh_weapon_key_of_tag("vehicles\\ghost\\mp_ghost gun", text, sizeof(text));
	CHECK(!strcmp(text, "ghost_gun"));
	ae_xh_weapon_key_of_tag("vehicles\\ghost\\ghost gun", text, sizeof(text));
	CHECK(!strcmp(text, "ghost_gun"));
	ae_xh_weapon_key_of_tag("vehicles\\banshee\\mp_banshee gun", text, sizeof(text));
	CHECK(!strcmp(text, "banshee_gun"));
	ae_xh_weapon_key_of_tag("vehicles\\c gun turret\\mp gun turret gun", text, sizeof(text));
	CHECK(!strcmp(text, "gun_turret_gun"));
	ae_xh_weapon_key_of_tag("vehicles\\c gun turret\\c gun turret gun", text, sizeof(text));
	CHECK(!strcmp(text, "gun_turret_gun"));
	ae_xh_weapon_key_of_tag("vehicles\\rwarthog\\rwarthog_gun", text, sizeof(text));
	CHECK(!strcmp(text, "rwarthog_gun"));
	ae_xh_weapon_key_of_tag("mp_", text, sizeof(text));
	CHECK(!strcmp(text, "mp"));

	/* (0.7.7) the starters: PRECISE, SNAP, OCTA, each about 22 pixels across (19 to 26), round-tripping; "starter:N" reads
	as one, nothing else does */
	CHECK(ae_xh_starter_count() == 3 && !ae_xh_starter(3, &xh) && !ae_xh_starter(-1, &xh));
	for (index = 0; index < ae_xh_starter_count(); index++)
	{
		char slot_text[160];
		float across;

		CHECK(ae_xh_starter(index, &xh));
		across = 2.0f * ae_xh_extent(&xh);
		CHECK(across >= 19.0f && across <= 26.0f);
		snprintf(slot_text, sizeof(slot_text), "starter:%d", index + 1);
		CHECK(ae_xh_slot_parse(slot_text, &back));
		CHECK(!memcmp(&back, &xh, sizeof(xh)));
		ae_xh_slot_format(&xh, slot_text, sizeof(slot_text));
		CHECK(ae_xh_slot_parse(slot_text, &back) && !strcmp(back.name, xh.name));
	}
	CHECK(!strcmp((ae_xh_starter(0, &xh), xh.name), "PRECISE") && !strcmp((ae_xh_starter(1, &xh), xh.name), "SNAP") &&
		!strcmp((ae_xh_starter(2, &xh), xh.name), "OCTA"));
	CHECK(!ae_xh_slot_parse("starter:4", NULL) && !ae_xh_slot_parse("starter:0", NULL) && !ae_xh_slot_parse("starter:12", NULL) &&
		!ae_xh_slot_parse("starter:", NULL));

	/* (0.7.7) CUSTOM COLOR: exactly six hex digits, an optional #, spaces round it; nothing else */
	{
		unsigned long rgb = 0;

		CHECK(ae_xh_hex_parse("#a1B2c3", &rgb) && rgb == 0xA1B2C3ul);
		CHECK(ae_xh_hex_parse(" 00ff7f ", &rgb) && rgb == 0x00FF7Ful);
		CHECK(!ae_xh_hex_parse("#12345", &rgb) && !ae_xh_hex_parse("1234567", &rgb) && !ae_xh_hex_parse("##123456", &rgb) &&
			!ae_xh_hex_parse("12345g", &rgb) && !ae_xh_hex_parse("", &rgb) && !ae_xh_hex_parse(NULL, &rgb) &&
			!ae_xh_hex_parse("#123456x", &rgb) && !ae_xh_hex_parse("0x123456", &rgb));
		ae_xh_hex_format(0xA1B2C3ul, text, sizeof(text));
		CHECK(!strcmp(text, "#A1B2C3"));
		ae_xh_hex_format(0x00000Ful, text, 4);
		CHECK(strlen(text) == 3);
	}

	/* (0.7.7) FOV BASE and RETICLE MOTION */
	CHECK(ae_xh_fov_base_of("70") == AE_XH_FOV_BASE_70 && ae_xh_fov_base_of("78") == AE_XH_FOV_BASE_78 &&
		ae_xh_fov_base_of("x") == AE_XH_FOV_BASE_70 && ae_xh_fov_base_of(NULL) == AE_XH_FOV_BASE_70);
	CHECK(ae_xh_fov_base_factor(AE_XH_FOV_BASE_70) == 1.0f);
	CHECK(ae_xh_fov_base_factor(AE_XH_FOV_BASE_78) > 1.15f && ae_xh_fov_base_factor(AE_XH_FOV_BASE_78) < 1.16f);
	CHECK(ae_xh_stock_motion_of("all") == AE_XH_STOCK_MOTION_ALL && ae_xh_stock_motion_of("off") == AE_XH_STOCK_MOTION_OFF &&
		ae_xh_stock_motion_of("rifles") == AE_XH_STOCK_MOTION_RIFLES && ae_xh_stock_motion_of(NULL) == AE_XH_STOCK_MOTION_RIFLES);
	{
		struct ae_xh_motion_class assault, pistol, shotgun, sniper, needler, unknown;

		/* (the stock tags' numbers: shots a second, error cone in degrees) */
		ae_xh_motion_class_of(15.0f, 2.0f, 6.5f, &assault);
		ae_xh_motion_class_of(3.5f, 0.2f, 2.0f, &pistol);
		ae_xh_motion_class_of(1.0f, 10.0f, 10.0f, &shotgun);
		ae_xh_motion_class_of(2.0f, 0.5f, 0.5f, &sniper);
		ae_xh_motion_class_of(10.0f, 4.0f, 4.0f, &needler);
		ae_xh_motion_class_of(0.0f, 0.0f, 0.0f, &unknown);
		CHECK(assault.has_spread && assault.gain > 0.349f && assault.gain < 0.351f && pistol.has_spread && pistol.gain > 0.2f && pistol.gain < 0.3f);
		CHECK(!shotgun.has_spread && shotgun.growth == 0.22f && shotgun.ms == 500);
		CHECK(!sniper.has_spread && sniper.growth == 0.22f && sniper.ms == 350);
		CHECK(!needler.has_spread && needler.growth == 0.15f && needler.ms == 200);
		CHECK(!unknown.has_spread && unknown.growth == 0.15f && unknown.ms == 300);
		/* (no motion at rest; never past the AE rifles' 35 %; a spread weapon never grows past its real cone) */
		CHECK(ae_xh_stock_motion_scale(&assault, 0.0f, 0.0f, 0.0f) == 1.0f && ae_xh_stock_motion_scale(&shotgun, 0.0f, 0.0f, 0.0f) == 1.0f);
		CHECK(ae_xh_stock_motion_scale(&assault, 0.0f, 1.0f, 100.0f) > 1.349f && ae_xh_stock_motion_scale(&assault, 0.0f, 1.0f, 100.0f) < 1.351f);
		CHECK(ae_xh_stock_motion_scale(&assault, 0.0f, 1.0f, 2.4f) > 1.09f && ae_xh_stock_motion_scale(&assault, 0.0f, 1.0f, 2.4f) < 1.11f);
		CHECK(ae_xh_stock_motion_scale(&assault, 0.0f, 0.5f, 100.0f) > 1.17f && ae_xh_stock_motion_scale(&assault, 0.0f, 0.5f, 100.0f) < 1.18f);
		CHECK(ae_xh_stock_motion_scale(&sniper, 1.0f, 0.0f, 0.0f) > 1.21f && ae_xh_stock_motion_scale(&sniper, 1.0f, 0.0f, 0.0f) < 1.23f);
		CHECK(ae_xh_stock_motion_scale(&sniper, 100.0f, 0.0f, 0.0f) < 1.23f && ae_xh_stock_motion_scale(&sniper, -1.0f, -5.0f, -5.0f) == 1.0f);
		CHECK(ae_xh_stock_motion_scale(NULL, 1.0f, 1.0f, 1.0f) == 1.0f);
	}
	CHECK(ae_xh_main_of("3") == 2 && ae_xh_main_of("own") == AE_XH_CHOICE_OWN && ae_xh_main_of("9") == AE_XH_CHOICE_OWN);

	/* the palette: no green, no red; set and read back; a custom colour is -1 */
	CHECK(ae_xh_palette_count() == 10 && !strcmp(ae_xh_palette_name(0), "WHITE") && ae_xh_palette_name(99)[0] == 0);
	ae_xh_default(&xh);
	for (index = 0; index < ae_xh_palette_count(); index++)
	{
		unsigned long rgb = ae_xh_palette_rgb(index);

		ae_xh_color_set(&xh, rgb);
		CHECK(ae_xh_palette_of(&xh) == index);
		CHECK(!((rgb >> 8 & 0xFF) > 0xC0 && (rgb >> 16 & 0xFF) < 0x80 && (rgb & 0xFF) < 0x80)); /* (not green) */
		CHECK(!((rgb >> 16 & 0xFF) > 0xC0 && (rgb >> 8 & 0xFF) < 0x60 && (rgb & 0xFF) < 0x60)); /* (not red) */
	}
	ae_xh_color_set(&xh, 0x123456ul);
	CHECK(ae_xh_palette_of(&xh) == -1);

	/* every weapon that can show a crosshair has a row of its own in HUD > RETICLE > WEAPONS: see the settings rows' test */

	/* the stock rows */
	CHECK(ae_xh_reticle_size_clamp(10) == 50 && ae_xh_reticle_size_clamp(999) == 200 && ae_xh_reticle_size_clamp(120) == 120);
	CHECK(ae_xh_reticle_scaling_of("fixed") == AE_XH_SCALING_FIXED && ae_xh_reticle_scaling_of("x") == AE_XH_SCALING_FOV);
	CHECK(ae_xh_tint_of("cyan") == AE_XH_TINT_CYAN && ae_xh_tint_of("zzz") == AE_XH_TINT_ORIGINAL);
	CHECK(ae_xh_tint_rgb(AE_XH_TINT_ORIGINAL, 0x123456ul) == 0x123456ul && ae_xh_tint_rgb(AE_XH_TINT_ICE, 0) == 0x96D2FFul);
	CHECK(ae_xh_state_rgb(0x96D2FFul, 1, 0) == 0xFF3030ul && ae_xh_state_rgb(0x96D2FFul, 0, 1) == 0x00FF00ul &&
		ae_xh_state_rgb(0x96D2FFul, 0, 0) == 0x96D2FFul);

	/* the host rule keeps the other bits of the byte (BALL MELEE 0-1, FALL DAMAGE 3, SPAWNS 4-5, slots 6, marks 7) */
	for (index = 0; index < 256; index++)
	{
		unsigned char off = ae_xh_allowed_set((unsigned char)index, 0), on = ae_xh_allowed_set((unsigned char)index, 1);

		CHECK(!ae_xh_allowed_of(off) && ae_xh_allowed_of(on));
		CHECK((off & ~0x04) == (index & ~0x04) && (on & ~0x04) == (index & ~0x04));
	}
	CHECK(ae_xh_allowed_of(0));

	/* motion: lines move out by the spread and the kick, the centre does not; FIRING SPREAD off ignores both */
	ae_xh_default(&xh);
	{
		struct ae_xh_motion motion = { 6.0f, 2.0f, 0.0f };
		struct ae_xh_piece rest[AE_XH_PIECES], moved[AE_XH_PIECES];
		int a = ae_xh_build(&xh, NULL, 0xFFFFFFul, 1.0f, rest, AE_XH_PIECES);
		int b = ae_xh_build(&xh, &motion, 0xFFFFFFul, 1.0f, moved, AE_XH_PIECES);
		int c;

		CHECK(a == b);
		CHECK(memcmp(rest, moved, sizeof(rest[0]) * (size_t)a) != 0);
		xh.firing = 0;
		c = ae_xh_build(&xh, &motion, 0xFFFFFFul, 1.0f, moved, AE_XH_PIECES);
		CHECK(c == a && !memcmp(rest, moved, sizeof(rest[0]) * (size_t)a));
		xh.movement = 1;
		motion.move = 9.0f; /* (capped at 3) */
		motion.spread = motion.kick = 0.0f;
		ae_xh_build(&xh, &motion, 0xFFFFFFul, 1.0f, moved, AE_XH_PIECES);
		motion.move = 3.0f;
		ae_xh_build(&xh, &motion, 0xFFFFFFul, 1.0f, rest, AE_XH_PIECES);
		CHECK(!memcmp(rest, moved, sizeof(rest[0]) * (size_t)a));
	}
	/* a pixel grid: a 1 pixel stroke covers a whole pixel; the pieces fit the array */
	ae_xh_default(&xh);
	xh.inner.thickness = 2;
	xh.outline_on = 0;
	n = ae_xh_build(&xh, NULL, 0xFFFFFFul, 1.0f, pieces, AE_XH_PIECES);
	for (index = 0; index < n; index++)
		if (pieces[index].kind == AE_XH_PIECE_RECT)
			CHECK(pieces[index].x0 == (float)(int)pieces[index].x0 && pieces[index].y1 == (float)(int)pieces[index].y1);
	CHECK(ae_xh_build(&xh, NULL, 0xFFFFFFul, 1.0f, pieces, 2) == 2);
	/* the outline comes first and is black */
	xh.outline_on = 1;
	n = ae_xh_build(&xh, NULL, 0xA0B0C0ul, 1.0f, pieces, AE_XH_PIECES);
	CHECK((pieces[0].rgba >> 8) == 0x000000u && (pieces[n - 1].rgba >> 8) == 0xA0B0C0u);

	/* the spread in pixels: wider fov, smaller; no error, none; nonsense, none */
	CHECK(ae_xh_spread_pixels(0.0f, 1.2f) == 0.0f && ae_xh_spread_pixels(-1.0f, 1.2f) == 0.0f);
	CHECK(ae_xh_spread_pixels(0.02f, 1.2f) > ae_xh_spread_pixels(0.02f, 1.6f) && ae_xh_spread_pixels(0.02f, 1.2f) > 5.0f);
	CHECK(ae_xh_spread_pixels(0.02f, 0.0f) == 0.0f && ae_xh_spread_pixels(0.02f, 9.0f) == 0.0f);

	/* random crosshairs (valid CRC, every field anywhere): clamped, they round trip, build and stay inside the array */
	for (index = 0; index < 100000; index++)
	{
		unsigned char *raw = (unsigned char *)&made;
		size_t at;
		struct ae_xh_motion motion = { (float)(rnd() % 40), (float)(rnd() % 20), (float)(rnd() % 10) };

		for (at = 0; at < sizeof(made); at++)
			raw[at] = (unsigned char)rnd();
		ae_xh_clamp(&made);
		made.shape = (unsigned char)(rnd() % 38);
		ae_xh_code_encode(&made, code, sizeof(code));
		memset(&back, 0, sizeof(back));
		CHECK(ae_xh_code_decode(code, &back, &unknown) == AE_XH_CODE_OK && same(&made, &back));
		n = ae_xh_build(&back, &motion, 0x112233ul, 0.5f + (float)(rnd() % 8) / 2.0f, pieces, AE_XH_PIECES);
		CHECK(n >= 0 && n <= AE_XH_PIECES);
		if (failures)
			break;
	}

	/* the fuzz: a million inputs, random bytes, and good codes with a few characters changed or cut */
	ae_xh_default(&xh);
	ae_xh_code_encode(&xh, code, sizeof(code));
	for (index = 0; index < 1000000; index++)
	{
		unsigned char data[160];
		size_t size = (size_t)(rnd() % 120), at;

		if (index % 3 == 0)
		{
			for (at = 0; at < size; at++)
				data[at] = (unsigned char)rnd();
		}
		else
		{
			size = strlen(code);
			memcpy(data, code, size);
			for (at = 0; at < 1 + rnd() % 4; at++)
				data[rnd() % size] = (unsigned char)(index % 3 == 1 ? (unsigned long long)(unsigned char)"0123456789ABCDEFGHJKMNPQRSTVWXYZ-ioOlL "[rnd() % 38] : rnd());
			if (rnd() % 4 == 0)
				size = rnd() % size;
		}
		if (ae_xh_fuzz_one(data, size))
		{
			printf("FAIL fuzz input %d\n", index);
			failures++;
			break;
		}
	}

	if (failures)
		return 1;
	printf("ae_crosshair_rules_test: all passed\n");
	return 0;
}
#endif
