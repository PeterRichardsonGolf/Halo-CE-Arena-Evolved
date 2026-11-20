/*
AE_CONFIG_PLAYERS.H

(Arena Evolved, 0.3.7 SETTINGS) Per-player settings: the AE SETTINGS' CONTROLS, VIEW and HUD rows (the profile's own
ten aside) are each local player's own, kept in their profile's ae.toml (ae_glue_player_settings.c reads and writes
the files; ae_player_settings.c is their text). The game's memory holds each player's values here. A setting read
while the game works for a player (between ae_config_player_enter and ae_config_player_leave: that player's view,
input or HUD) is that player's value; read for no one, player 1's; a player with no value of their own has
config.toml's. A setting given by the environment wins over all of them (the tests' HALO_* variables).

port_config.c asks ae_config_player_value in its config_value (one AE hook) and tells ae_config_player_written of
each config_write (the old menus' rows: player 1's, as they always were).
*/

#ifndef __AE_CONFIG_PLAYERS_H
#define __AE_CONFIG_PLAYERS_H

#include <stddef.h>

enum { AE_CONFIG_PLAYERS = 4 };

/* (port_config.c's hook) the value of a setting for the player the game works for now, when it is a per-player
setting that player has a value of (and the environment gives none): 1 with the fields set (type: port_config.c's
config_type, 0 boolean 1 integer 2 real 3 string; the string stays valid while the program runs) */
int ae_config_player_value(const char *name, const char *environment, int type, int *boolean, long *integer,
	double *real, char **string);
/* (port_config.c's hook) config_write of a setting: a per-player one goes to the writer (set below), as the player's
the game works for now */
void ae_config_player_written(const char *name, const char *value);

/* (port_config.c's hook in config_changes) how many times a player's value changed: config_changes counts them too,
so a reader of a setting reads it again when a player's own value changes */
unsigned long ae_config_player_value_changes(void);

/* whether name is a per-player setting; the list of them */
int ae_config_player_setting(const char *name);
int ae_config_player_setting_count(void);
/* whether a per-player setting is a string (written quoted in ae.toml) */
int ae_config_player_is_string(const char *name);
const char *ae_config_player_setting_name(int index);
/* a player's (0-3) own value, from its text; NULL: none of their own (config.toml's then). Memory only: 1 when set */
int ae_config_player_set(int player, const char *name, const char *value);
void ae_config_player_clear(int player);
/* whether the player has a value of their own; the value that player has now (their own, else config.toml's), as
text (config_text's) */
int ae_config_player_has(int player, const char *name);
int ae_config_player_text(int player, const char *name, char *text, size_t size);
/* a setting as that player has it (their own, else config.toml's; the environment's wins) */
int ae_config_player_boolean(int player, const char *name);
double ae_config_player_real(int player, const char *name);
/* config.toml's value of a setting when it is not the default (and the environment gives none): a profile's first
copy (ae.toml seeded); 0 when it has the default */
int ae_config_player_seed_text(const char *name, char *text, size_t size);
/* the game works for this player (0-3; -1: no one) on this thread from now on: returns the one before, for leave */
int ae_config_player_enter(int player);
void ae_config_player_leave(int previous);
/* the player the game works for now on this thread (no one: 0, player 1's values) */
int ae_config_player_now(void);
/* config_changes() for a reader of per-player settings that keeps one copy: it moves when any setting or player
value changes, and when the player the game works for changes, so the copy is read again for each in turn */
unsigned long ae_config_player_changes(void);
/* told of each config_write of a per-player setting, with the player the game works for (the old menus' rows: no
one, player 1's) */
void ae_config_player_set_writer(void (*writer)(int player, const char *name, const char *value));

#endif
