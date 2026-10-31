/*
AE_PLAYER_SETTINGS.H

A player's own settings file (the AE SETTINGS' CONTROLS, VIEW and HUD rows that are not the profile's ten): ae.toml
in the folder of the player's profile save (ae_glue_player_settings.c reads and writes it). Its text, read and
written (a TOML subset: sections, "key = value", strings quoted), its version, and its first copy: a profile with no
file yet (or one of an older version) is seeded with config.toml's chosen values, so nobody's settings change as
they move into their profile. Pure: no engine includes (unit test: port/linux/tests/ae_player_settings_test.c).
*/

#ifndef __AE_PLAYER_SETTINGS_H
#define __AE_PLAYER_SETTINGS_H

#include <stddef.h>

enum
{
	/* 1: the first (0.3.7) */
	AE_PLAYER_SETTINGS_VERSION = 1,
	AE_PLAYER_SETTINGS_MAXIMUM = 96,
	AE_PLAYER_SETTINGS_NAME = 64,
	AE_PLAYER_SETTINGS_VALUE = 64,
	/* a player's profile index when they have none (the game's NONE), and before any was looked at */
	AE_PLAYER_SETTINGS_NO_PROFILE = -1,
	AE_PLAYER_SETTINGS_UNREAD = -2
};

#define AE_PLAYER_SETTINGS_FILE "ae.toml"

struct ae_player_setting { char name[AE_PLAYER_SETTINGS_NAME]; char value[AE_PLAYER_SETTINGS_VALUE]; };
struct ae_player_settings
{
	int version;     /* 0: none read (no file, or a file with no version) */
	int count;
	struct ae_player_setting settings[AE_PLAYER_SETTINGS_MAXIMUM];
};

void ae_player_settings_clear(struct ae_player_settings *settings);
/* a file's text: its settings ("section.key" names, values as text: strings unquoted); lines it can't read are
skipped. Returns the settings read */
int ae_player_settings_parse(const char *text, struct ae_player_settings *settings);
/* the value of name, or NULL */
const char *ae_player_settings_get(struct ae_player_settings const *settings, const char *name);
/* sets (or adds) name's value; 0 when full or a name or value too long */
int ae_player_settings_put(struct ae_player_settings *settings, const char *name, const char *value);
/* the file's text, the settings by section in the order first put (is_string(name): written quoted); 0 when it
does not fit */
int ae_player_settings_write(struct ae_player_settings const *settings, int (*is_string)(const char *name),
	char *text, size_t size);
/* whether a player's profile index names a profile, whose file is read and written: any but the two above. (A saved
game file's index has its top bit set (_saved_game_file_index_valid_bit): as the game's 32-bit long every real one is
negative; 0.3.7-0.3.9 took a negative index for none and never wrote a change) */
int ae_player_settings_profile_has_file(long profile);
/* whether a profile's file needs its first copy: none read, or an older version's */
int ae_player_settings_needs_seed(int found, int version);
/* the first copy: each of the names the file has no value of, given config.toml's chosen value (seed returns 1 with
it), is put; the version set to this one's. Returns the values put */
int ae_player_settings_seed(struct ae_player_settings *settings, const char *const *names, int count,
	int (*seed)(const char *name, char *text, size_t size));

#endif
