/*
AE_GLUE_SETTINGS.H

What the AE hook lines of the PC menus' Settings list call (ae_hooks.txt lists
those lines; notes/ae-hooks.md says where each goes): the picture beside the
list goes by the focused entry's name (ae_settings.h), and an entry with no
picture of its own shows none, its description at the top of the panel.
*/

#ifndef __AE_GLUE_SETTINGS_H
#define __AE_GLUE_SETTINGS_H

struct widget_instance;

/* (menu_functions.c's game data inputs) "ae profile picture", the Settings list's own, after the game's update of
its description: the picture's frame and whether it shows, and where the description and the profile's name are */
void ae_settings_profile_list(struct widget_instance *list);
/* (menu_functions.c's profile_gamepad_layout, the picture's own input, at its end) the frame the list chose, again:
the game's function sets one by the row's number */
void ae_settings_profile_picture(struct widget_instance *picture);

#endif
