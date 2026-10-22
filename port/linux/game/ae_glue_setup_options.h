/*
AE_GLUE_SETUP_OPTIONS.H

The game setup spike's GAME OPTIONS glue (ae_glue_setup_options.c): one edit COPY of a gametype's rules (its variant
and PC options) and the copy it started from, read and set through the model's rows (ae_setup_options.h) by value
index. Each row's field and raw values are menu_functions.c's gametype_options[] (copied: that table is static in an
upstream file), plus AE's rules the PC table has no row for: COMBAT FEEDBACK and SILENT (pad5), GRENADES ON MAP and
INFINITE AMMO (pack_loadout bits 7 and 5), SENSOR RANGE (flags bits 6-7). Nothing is written to a file except by
ae_setup_edit_save_as_new. Menus' thread only. Include after cseries.h.

Log: "ae game options: ..." (the begin, a set, defaults, a save, a table that disagrees with the model).
*/

#ifndef __AE_GLUE_SETUP_OPTIONS_H
#define __AE_GLUE_SETUP_OPTIONS_H

#include "ae_setup_options.h"

/* the copy from a gametype file (a saved game file index); 0 when it can't be read */
int ae_setup_edit_begin(long profile);
/* the copy from the gametype whose stored name is name (letters' case aside), or the last used one (NULL or not
found); 0 when there is none */
int ae_setup_edit_begin_named(char const *name);
/* whether an edit is open */
int ae_setup_edit_open(void);
/* the model's access over the copy (get, changed; the engine; AE's rules) */
void ae_setup_edit_access(struct ae_setup_access *access);
/* a row set to a value index (out of range: nothing) */
void ae_setup_edit_set(int option, int value_index);
/* the base's value index of a row (the "was" value), -1 for none */
int ae_setup_edit_base(int option);
/* every rule back to the gametype the edit started from */
void ae_setup_edit_defaults(void);
/* the copy's stored name (11 characters), as ASCII */
void ae_setup_edit_name(char *name, int size);
/* a new gametype file of the copy, named name (11 characters at most), which is written into: the copy's name with
" 2", " 3" ... until it is free. 1 when written; the copy then starts from it (no changes) */
int ae_setup_edit_save_as_new(char *name, int size);
/* (CREATE GAME, the LOBBY) the copy as the game's gametype: player_ui's variant and options (so the server takes the
options with it), the server's when there is one (PREGAME: network_game_server_change_game_variant), the stage's
for the next round. 1 when applied */
int ae_setup_edit_apply(void);
/* the saved gametypes the game lists (ui_widget_port_gametypes): each one's stored name (16 bytes) and file, at most
maximum; the edit's own file's place in current (-1 none). Returns the count */
short ae_setup_gametypes(char (*names)[16], long *profiles, short maximum, short *current);

/* (CHANGE GAMETYPE's panel) a saved gametype's rules as the model's value indexes (the nearest listed value; -1 for an
option not in this build), count of them from the first; 0 when its file can't be read */
int ae_setup_profile_values(long profile, int *values, int count);

/* the table against the model: every row's value count; logs each that differs; 1 when all match */
int ae_setup_edit_check(void);

#endif
