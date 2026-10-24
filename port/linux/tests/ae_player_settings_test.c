/* ae_player_settings_test.c: a player's own settings file (ae_player_settings.h): read, written, seeded, upgraded. */

#include <stdio.h>
#include <string.h>

#include "ae_player_settings.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int is_string(const char *name)
{
	return !strcmp(name, "display.hud_style") || !strcmp(name, "input.zoom_mode") ||
		!strcmp(name, "display.scoreboard_background_color") || !strcmp(name, "game.callout_voice");
}

/* config.toml as a test has it: the file chose these (the rest at their defaults: not given) */
static int seed(const char *name, char *text, size_t size)
{
	if (!strcmp(name, "display.fov"))
		return snprintf(text, size, "100") > 0;
	if (!strcmp(name, "input.zoom_mode"))
		return snprintf(text, size, "both") > 0;
	return 0;
}

int main(void)
{
	static struct ae_player_settings settings, again;
	static char text[8192];
	static const char *const names[] = { "display.fov", "input.zoom_mode", "display.hud_style", "display.zoom_blur" };

	/* read: sections, comments, quotes with escapes, a value with a comment after it, nonsense skipped */
	CHECK(ae_player_settings_parse("# a comment\n[ae]\nversion = 1\n\n[display]\nfov = 95.5\nhud_style = \"classic\"\n"
		"zoom_blur = false # off\n[input]\nzoom_mode=\"ho\\\"ld\"\nbroken\n= nothing\nempty =\n[game]\n"
		"callout_voice = \"un", &settings) == 4);
	CHECK(settings.version == 1);
	CHECK(!strcmp(ae_player_settings_get(&settings, "display.fov"), "95.5"));
	CHECK(!strcmp(ae_player_settings_get(&settings, "display.hud_style"), "classic"));
	CHECK(!strcmp(ae_player_settings_get(&settings, "display.zoom_blur"), "false"));
	CHECK(!strcmp(ae_player_settings_get(&settings, "input.zoom_mode"), "ho\"ld"));
	CHECK(!ae_player_settings_get(&settings, "game.callout_voice"));
	CHECK(!ae_player_settings_get(&settings, "input.broken"));
	/* Windows line ends */
	CHECK(ae_player_settings_parse("[display]\r\nfov = 90\r\n", &settings) == 1);
	CHECK(!strcmp(ae_player_settings_get(&settings, "display.fov"), "90") && settings.version == 0);
	/* nothing: an empty or missing file */
	CHECK(ae_player_settings_parse("", &settings) == 0 && settings.count == 0);
	CHECK(ae_player_settings_parse(NULL, &settings) == 0);

	/* put: a new name, the same again, a name with no section, too long */
	ae_player_settings_clear(&settings);
	CHECK(ae_player_settings_put(&settings, "display.fov", "90"));
	CHECK(ae_player_settings_put(&settings, "input.zoom_mode", "hold"));
	CHECK(ae_player_settings_put(&settings, "display.hud_style", "a \"quoted\" \\ one"));
	CHECK(ae_player_settings_put(&settings, "display.fov", "110"));
	CHECK(settings.count == 3 && !strcmp(ae_player_settings_get(&settings, "display.fov"), "110"));
	CHECK(!ae_player_settings_put(&settings, "nosection", "1"));
	{
		char long_value[200];

		memset(long_value, 'x', sizeof(long_value) - 1);
		long_value[sizeof(long_value) - 1] = 0;
		CHECK(!ae_player_settings_put(&settings, "display.fov", long_value));
	}

	/* written and read back: the same settings, sections joined, strings quoted and escaped */
	settings.version = AE_PLAYER_SETTINGS_VERSION;
	CHECK(ae_player_settings_write(&settings, is_string, text, sizeof(text)));
	CHECK(strstr(text, "[ae]\nversion = 1\n") != NULL);
	CHECK(strstr(text, "[display]\nfov = 110\nhud_style = \"a \\\"quoted\\\" \\\\ one\"\n") != NULL);
	CHECK(strstr(text, "[input]\nzoom_mode = \"hold\"\n") != NULL);
	CHECK(ae_player_settings_parse(text, &again) == 3 && again.version == AE_PLAYER_SETTINGS_VERSION);
	CHECK(!strcmp(ae_player_settings_get(&again, "display.hud_style"), "a \"quoted\" \\ one"));
	CHECK(!strcmp(ae_player_settings_get(&again, "input.zoom_mode"), "hold"));
	/* too small a buffer: 0, never a cut file */
	CHECK(!ae_player_settings_write(&settings, is_string, text, 40));

	/* the first copy: a profile with no file is seeded with config.toml's chosen values only */
	CHECK(ae_player_settings_needs_seed(0, 0));
	ae_player_settings_clear(&settings);
	CHECK(ae_player_settings_seed(&settings, names, 4, seed) == 2);
	CHECK(settings.version == AE_PLAYER_SETTINGS_VERSION);
	CHECK(!strcmp(ae_player_settings_get(&settings, "display.fov"), "100"));
	CHECK(!strcmp(ae_player_settings_get(&settings, "input.zoom_mode"), "both"));
	CHECK(!ae_player_settings_get(&settings, "display.hud_style"));
	/* a file of this version: no copy again */
	CHECK(!ae_player_settings_needs_seed(1, AE_PLAYER_SETTINGS_VERSION));

	/* an upgrade: a file of an older version keeps its own values and gets only the missing ones */
	CHECK(ae_player_settings_parse("[display]\nfov = 80\n", &settings) == 1);
	CHECK(ae_player_settings_needs_seed(1, settings.version));
	CHECK(ae_player_settings_seed(&settings, names, 4, seed) == 1);
	CHECK(!strcmp(ae_player_settings_get(&settings, "display.fov"), "80"));
	CHECK(!strcmp(ae_player_settings_get(&settings, "input.zoom_mode"), "both"));
	CHECK(!ae_player_settings_needs_seed(1, settings.version));

	/* a profile's index (its top bit, valid, set: negative as the game's 32-bit long, 0x804C0000 on the owner's PC) is
	one, whose file is written; none (the game's NONE) and not looked at yet are not */
	CHECK(ae_player_settings_profile_has_file((long)(int)0x804C0000u));
	CHECK(ae_player_settings_profile_has_file((long)(int)0xC0000001u));
	CHECK(ae_player_settings_profile_has_file(0x00010001L));
	CHECK(!ae_player_settings_profile_has_file(AE_PLAYER_SETTINGS_NO_PROFILE));
	CHECK(!ae_player_settings_profile_has_file(AE_PLAYER_SETTINGS_UNREAD));

	if (failures)
	{
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("ae_player_settings: ok\n");
	return 0;
}
