/*
AE_GLUE_MOUSE.H

What the AE hook lines of the MCC mouse style call in the game's sources
(ae_hooks.txt lists those lines; notes/ae-hooks.md says where each goes), and
in the platform layer (port/linux/src/ae_platform.h declares those).
Plain types only.
*/

#ifndef __AE_GLUE_MOUSE_H
#define __AE_GLUE_MOUSE_H

/* (player_control.c, after the zoom's division) the mouse look in radians scaled by the zoomed and vehicle
sensitivity scales: MCC style only; classic leaves it as it is */
void ae_mouse_scale_look(float *yaw, float *pitch, int zoomed, int in_vehicle);
/* (game_engine.c game_engine_log_rules, a match's start) logs "mouse: style ..., sensitivity ..., zoom scale ...,
vehicle scale ..." */
void ae_mouse_log_settings(void);
/* (menu_functions.c setting_text) the MCC sensitivity row shows the seeded value, not 0, while it is unset */
void ae_mouse_menu_text(char const *name, char *text, unsigned int size, int default_value);

#endif
