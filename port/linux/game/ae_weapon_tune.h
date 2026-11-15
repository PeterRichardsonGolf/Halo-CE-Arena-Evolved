/*
AE_WEAPON_TUNE.H

DEV MODE's tune command: the numbers of the weapon in the player's hands, changed live in the loaded tags and written as
a tune file, which tools/weapon_pack/apply_tune.py puts into the weapon's recipe (the keys, their field paths and
their limits are ae_weapon_tune_rules.h). Numbers are those the tag editors show: a rate in rounds a second, a spread
and an aim assist in degrees, a speed in world units a second, a time in seconds.

The tags are the loaded map's own memory: a change is this machine's, so it is made only in a game with no other
player (the dev command refuses it otherwise), and it goes with the map (a new game forgets what was changed, as the
tags are loaded again). The first change of a number keeps the number it had, for tune reset.

The tune file, <config folder>/tune/<weapon key>.json, holds only the numbers changed since the map loaded:

	{"format": 1, "weapon_key": "<key>", "weapon_tag": "<tag path>", "edits": [
	 {"tag_kind": "weapon|projectile|impact_effect|melee_effect", "tag": "<tag path>", "set": ["<field path>", <value or [a, b]>]}]}
*/

#ifndef __AE_WEAPON_TUNE_H
#define __AE_WEAPON_TUNE_H

#include "cseries.h"

/* the weapon in hand's key (a pack weapon's name, else its tag's last name), its tag, and every key with its present
number, one line each, "*" before the keys changed; 1, or 0 with no weapon in hand */
int ae_weapon_tune_list(long player_index, char *out, int size);

/* changes one number of the weapon in hand ("trigger0.rate", "9" or "6..9"); the answer says what changed or why not;
1 when it changed */
int ae_weapon_tune_set(long player_index, char const *key_text, char const *value_text, char *answer, int size);

/* writes the changed numbers of the weapon in hand as <config folder>/tune/<weapon key>.json: 1 and its path, or 0
(nothing changed, or the file could not be written, the reason in path_out) */
int ae_weapon_tune_save(long player_index, char *path_out, int size);

/* the weapon in hand's changed numbers back as the map loaded them; how many */
int ae_weapon_tune_reset(long player_index);

/* how many numbers are changed now */
int ae_weapon_tune_count(void);

/* every changed number back as the map loaded it (a network session started, or a remote player joined); how many */
int ae_weapon_tune_reset_all(void);

/* a new game: the tags are loaded again, nothing is kept */
void ae_weapon_tune_new_game(void);

#endif
