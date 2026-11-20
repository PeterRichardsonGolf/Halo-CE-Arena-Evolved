/*
AE_SETTINGS_TEST.C

The Settings list's pictures by entry (port/linux/game/ae_settings.c): each
entry its own picture or none, wherever it is in the list.
*/

#include <stdio.h>

#include "../game/ae_settings.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	static const char *const no_picture[] =
	{
		"controller_setup_profile_item", "mouse_settings_profile_item", "audio_settings_profile_item",
		"video_settings_profile_item", "hud_profile_item", "network_settings_profile_item", "mods_profile_item",
		"map_files_profile_item", "about_item", "some_later_profile_item",
	};
	unsigned index;
	int preset;

	CHECK(ae_settings_picture_frame("name_profile_item", 0) == AE_SETTINGS_FRAME_NAME);
	CHECK(ae_settings_picture_frame("name_profile_item", 7) == 0);
	CHECK(ae_settings_picture_frame("color_profile_item", 0) == AE_SETTINGS_FRAME_COLOR);
	CHECK(ae_settings_picture_frame("color_profile_item", 3) == 7);
	/* (GAMEPADS: the Xbox's picture of the first five button settings, else its own) */
	for (preset = 0; preset < 5; preset++)
		CHECK(ae_settings_picture_frame("gamepads_profile_item", preset) == 9 + preset);
	CHECK(ae_settings_picture_frame("gamepads_profile_item", 5) == AE_SETTINGS_FRAME_GAMEPADS);
	CHECK(ae_settings_picture_frame("gamepads_profile_item", 13) == 2);
	CHECK(ae_settings_picture_frame("gamepads_profile_item", -1) == 2);
	/* (the entries with no picture of their own, and one a later version adds) */
	for (index = 0; index < sizeof(no_picture) / sizeof(no_picture[0]); index++)
	{
		for (preset = -1; preset < 14; preset++)
			CHECK(ae_settings_picture_frame(no_picture[index], preset) == AE_SETTINGS_NO_PICTURE);
	}
	/* (the buttons, and no name: nothing changes) */
	CHECK(ae_settings_picture_frame("profile_edit_button_bar", 0) == AE_SETTINGS_NOT_AN_ENTRY);
	CHECK(ae_settings_picture_frame("profile_edit_button_ok", 0) == AE_SETTINGS_NOT_AN_ENTRY);
	CHECK(ae_settings_picture_frame("", 0) == AE_SETTINGS_NOT_AN_ENTRY);
	CHECK(ae_settings_picture_frame("item", 0) == AE_SETTINGS_NOT_AN_ENTRY);
	CHECK(ae_settings_picture_frame(NULL, 0) == AE_SETTINGS_NOT_AN_ENTRY);
	if (failures)
	{
		printf("ae_settings: %d failed\n", failures);
		return 1;
	}
	printf("ae_settings: all passed\n");
	return 0;
}
