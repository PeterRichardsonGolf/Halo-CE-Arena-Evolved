/*
AE_GLUE_SETTINGS.C

The Settings list's picture by entry (ae_glue_settings.h; the rule is
ae_settings.c's). The list's description widget has two children, its text
and its picture (ui_widget_game_data_input_functions.c's
player_profile_edit_select_menu_update_extended_description); the profile's
name is the screen's own child, level with the text's last line ("Profile:").
With no picture the text and the name go up by the picture's height, to the
top of the panel.
*/

#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "interface/ui_widget_instance.h"
#include "saved games/player_profile.h"

#include "ae_glue_settings.h"
#include "ae_settings.h"

/* how far the description goes up with no picture over it: from its own top (290) to the picture's (85), a little
under the panel's edge (port/assets/menus/ce/...player_profile_edit.xml) */
#define AE_SETTINGS_TEXT_RISE 200

static struct
{
	/* the list last seen, and when it was made: its widgets' own offsets are read once for each */
	struct widget_instance const *list;
	int creation_time;
	short text_offset;
	short label_offset;
	/* the picture and the frame chosen for it (ae_settings_profile_picture) */
	struct widget_instance const *picture;
	int frame;
} settings = { NULL, 0, 0, 0, NULL, AE_SETTINGS_NOT_AN_ENTRY };

static struct widget_instance *child_named(struct widget_instance *widget, char const *name)
{
	struct widget_instance *child;

	for (child = widget ? widget->child : NULL; child; child = child->next)
	{
		if (child->name && !strcmp(child->name, name))
			return child;
	}
	return NULL;
}

void ae_settings_profile_list(struct widget_instance *list)
{
	struct widget_instance *description = list ? list->parameters.list.extended_description : NULL;
	struct widget_instance *text = description ? description->child : NULL;
	struct widget_instance *picture = text ? text->next : NULL;
	struct widget_instance *label = list ? child_named(list->parent, "profile_label") : NULL;
	struct player_profile *profile;
	int frame;

	if (!list || !list->focused_child || !text || !picture)
		return;
	profile = player_ui_get_edit_player_profile();
	frame = ae_settings_picture_frame(list->focused_child->name,
		profile ? (int)profile->controller_settings.button_preset : 0);
	/* (the buttons: the last entry's stays, as its description does) */
	if (frame == AE_SETTINGS_NOT_AN_ENTRY)
		return;
	if (settings.list != list || settings.creation_time != list->creation_time)
	{
		settings.list = list;
		settings.creation_time = list->creation_time;
		settings.text_offset = text->vertical_offset;
		settings.label_offset = label ? label->vertical_offset : 0;
	}
	settings.picture = picture;
	settings.frame = frame;
	picture->visible = frame != AE_SETTINGS_NO_PICTURE;
	if (frame != AE_SETTINGS_NO_PICTURE)
		picture->animation.current_frame_index = (short)frame;
	text->vertical_offset = (short)(settings.text_offset - (frame == AE_SETTINGS_NO_PICTURE ? AE_SETTINGS_TEXT_RISE : 0));
	if (label)
		label->vertical_offset = (short)(settings.label_offset - (frame == AE_SETTINGS_NO_PICTURE ? AE_SETTINGS_TEXT_RISE : 0));
}

void ae_settings_profile_picture(struct widget_instance *picture)
{
	if (picture && picture == settings.picture && settings.frame >= 0)
		picture->animation.current_frame_index = (short)settings.frame;
}
