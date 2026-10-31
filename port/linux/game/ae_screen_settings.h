/*
AE_SCREEN_SETTINGS.H

The 0.3.7 menus' SETTINGS (ae_screen_settings.c; the approved design, the model ae_settings_rows.h): the top level (the nine groups, whose each is, the focused group's panel) and a group's
screen (its sections' rail, one list, the help, the prompts). Each change is in use at once; B with changes asks KEEP /
UNDO; Y RESTORE DEFAULTS (asks, CANCEL first); X switches player while 2+ players are joined. CONTROLS, VIEW and HUD
are each player's own (ae_glue_player_settings.h; the profile's ten through ae_glue_profiles.h); the other groups are
listed and arrive with phase B. Debug screens: debug.ae_test_screen 34 (the top level), 35 (a group:
$HALO_AE_TEST_SETTINGS_GROUP, 0-8).

Its log: "ae settings: open player N", "group <NAME> player N rows R", "focus <LABEL>", "section <NAME>", "set <LABEL>
<value>", "player N", "keep", "undo (N rows)", "defaults (N rows)", "closed <NAME> (N changed)".
*/

#ifndef __AE_SCREEN_SETTINGS_H
#define __AE_SCREEN_SETTINGS_H

/* SETTINGS' top level over the stack, for this local player (0-3; the one whose pad or keyboard opened it); 0 when
the stack is full */
int ae_screen_settings_open(short local_player);
/* a group's screen over the stack (ae_settings_rows.h's AE_SETTINGS_*), for this local player; 0 when it is not
built yet or the stack is full */
int ae_screen_settings_open_group(short group, short local_player);

/* (debug.ae_test_screen 34 / 35) player 1 (and players 2 to `players`) on a test profile of their own (made when
missing), then the top level, or the group's screen; closes upstream's menus */
int ae_screen_settings_test_open(int group_screen, int group_number, int players);

#endif
