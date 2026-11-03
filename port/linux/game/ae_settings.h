/*
AE_SETTINGS.H

The PC menus' Settings list (EDIT PROFILE SETTINGS): which picture each of its
entries shows in the panel beside the list. The game shows the picture whose
number is the focused row's place in the list, so a row added to the list
moved every picture after it; here each entry has its picture by its name.
Pure (no game types): unit tested in port/linux/tests/ae_settings_test.c.
*/

#ifndef __AE_SETTINGS_H
#define __AE_SETTINGS_H

enum
{
	/* the entry has no picture of its own: the picture's box is not drawn */
	AE_SETTINGS_NO_PICTURE = -1,
	/* the name is not one of the list's entries (its buttons): nothing changes */
	AE_SETTINGS_NOT_AN_ENTRY = -2,
	/* the frames of the picture's bitmap (profile_options: the PC version's nine, then the Xbox's Controller Setup's
	five button settings, tools/port_settings.py's BITMAP_FRAMES) */
	AE_SETTINGS_FRAME_NAME = 0,
	AE_SETTINGS_FRAME_GAMEPADS = 2,
	AE_SETTINGS_FRAME_COLOR = 7,
	AE_SETTINGS_FRAME_FIRST_LAYOUT = 9,
	AE_SETTINGS_LAYOUT_FRAMES = 5
};

/* the frame of the entry with this widget name (its definition's last part, "color_profile_item"), with the
edited profile's button preset (GAMEPADS shows the Xbox's picture of the first five; the rest, and none, its own):
a frame, AE_SETTINGS_NO_PICTURE, or AE_SETTINGS_NOT_AN_ENTRY */
int ae_settings_picture_frame(char const *entry_name, int button_preset);

#endif
