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
/* (test launches: network_test.c's host:/local: with the AE menus on) the copy from a gametype's rules as they are (a
variant and its PC options), of no file: never saved over; 0 when either is missing */
struct game_variant;
struct game_variant_options;
int ae_setup_edit_begin_rules(struct game_variant const *variant, struct game_variant_options const *options);
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
/* the WEAPON PACKS screen's rows only (the packs, PACK WEAPONS ON MAP, PRIMARY, SECONDARY) back to the base's */
void ae_setup_edit_defaults_packs(void);
/* the copy's stored name (11 characters), as ASCII */
void ae_setup_edit_name(char *name, int size);
/* the copy renamed (printable ASCII, 11 characters at most; nothing typed keeps the original name) */
void ae_setup_edit_set_name(char const *name);
/* why a row can't be changed now (shown in place of its value), or NULL: a weapon pack's row for a pack that is not
loaded here and not on in the gametype ("NOT INSTALLED") */
char const *ae_setup_edit_reason(int option);
/* a new gametype file of the copy, named name (11 characters at most), which is written into: the copy's name with
" 2", " 3" ... until it is free. 1 when written; the copy then starts from it (no changes) */
int ae_setup_edit_save_as_new(char *name, int size);
/* the name ae_setup_edit_save_as_new would write under now, written into name; 0 (name empty) when none is free */
int ae_setup_edit_free_name(char *name, int size);
/* (after ae_setup_edit_save_as_new) the new file's longer display name (31 characters at most), as the lists show
it in place of the stored name; 1 when written */
int ae_setup_edit_set_display_name(char const *name);
/* whether a gametype the game lists already has this name (letters' case aside): its stored name, the table's display
name or a player's own display name; a stored name's length: its save directory too */
int ae_setup_name_taken(char const *name);
/* whether the edit may be saved over its own file: the player's own saved gametype, never a built-in or one AE seeds
(AE's, NHE's): those are only saved as a copy (owner 2026-10-22) */
int ae_setup_edit_can_save(void);
/* the copy written over its own file (when ae_setup_edit_can_save); 1 when written: the copy then has no changes */
int ae_setup_edit_save(void);
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
