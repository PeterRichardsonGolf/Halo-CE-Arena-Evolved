/*
AE_GLUE_PLAYER_SETTINGS.H

(0.3.7 AE SETTINGS) Each local player's own settings (ae_config_players.h): their profile's ae.toml read as the
player starts playing with that profile (a profile with none yet is seeded from config.toml's chosen values, and the
file written: ae_player_settings.h), their values put in the game's memory for the game to read as theirs, and each
change saved to their file at once. A guest (no profile) keeps theirs for the session only. Logs "ae player settings:
player N <profile> read (R values) / seeded (S values) / set <name> <value>". Include after cseries.h.
*/

#ifndef __AE_GLUE_PLAYER_SETTINGS_H
#define __AE_GLUE_PLAYER_SETTINGS_H

/* once a frame (ae_ui_process, AE's menus on or off): a local player whose profile changed has that profile's own
settings from now on */
void ae_player_settings_update(void);
/* a setting as the player has it now (their own, else config.toml's), as text; 0 when there is no such setting */
int ae_player_setting_get(short local_player, const char *name, char *text, int size);
/* the player's own value (in use at once; saved to their profile's file, a guest's kept for the session); NULL: back
to none of their own. 0 when it is not a per-player setting */
int ae_player_setting_put(short local_player, const char *name, const char *value);
/* a setting's default, as text */
int ae_player_setting_default(const char *name, char *text, int size);
/* whether the setting is a player's own (the AE SETTINGS' CONTROLS, VIEW and HUD rows, the profile's aside) */
int ae_player_setting_is_own(const char *name);

#endif
