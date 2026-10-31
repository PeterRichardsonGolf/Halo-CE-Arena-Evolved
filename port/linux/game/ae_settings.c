/*
AE_SETTINGS.C

The Settings list's pictures by entry (ae_settings.h).
*/

#include <string.h>

#include "ae_settings.h"

int ae_settings_picture_frame(char const *entry_name, int button_preset)
{
	size_t length;

	if (!entry_name)
		return AE_SETTINGS_NOT_AN_ENTRY;
	if (!strcmp(entry_name, "name_profile_item"))
		return AE_SETTINGS_FRAME_NAME;
	if (!strcmp(entry_name, "gamepads_profile_item"))
	{
		return button_preset >= 0 && button_preset < AE_SETTINGS_LAYOUT_FRAMES ?
			AE_SETTINGS_FRAME_FIRST_LAYOUT + button_preset : AE_SETTINGS_FRAME_GAMEPADS;
	}
	if (!strcmp(entry_name, "color_profile_item"))
		return AE_SETTINGS_FRAME_COLOR;
	/* (every other entry, one a later version adds too: the list's entries are named ..._item, its buttons' bar is
	not) */
	length = strlen(entry_name);
	if (length >= 5 && !strcmp(entry_name + length - 5, "_item"))
		return AE_SETTINGS_NO_PICTURE;
	return AE_SETTINGS_NOT_AN_ENTRY;
}
