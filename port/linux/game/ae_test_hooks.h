/*
AE_TEST_HOOKS.H

AE's test hooks in the game's code (ae_test_hooks.c), each OFF by default: the
calls do nothing while their debug.* setting is empty or false. The upstream
files call them from single lines marked AE hook (ae_hooks.txt).

- debug.capture_on_event: a screenshot shortly after a game event (capture.h's
  capture_test_event: the kinds, the delays, the log line).
- debug.network_test_all_weapons: an automated test host's player gets the
  map's weapons (cheat_all_weapons) a second after each spawn, and the log says
  what the map lists and what was made.
- debug.scoreboard_shown: every view's scoreboard stays shown, as with BACK (Tab)
  held, for screenshots of it.
*/

#ifndef AE_TEST_HOOKS_H
#define AE_TEST_HOOKS_H

/* a game event of this kind ("start", "pickup", "postgame") with no text: a capture delay_ms later */
void ae_test_capture(const char *kind, int delay_ms);
/* a HUD message (hud_print_message): kind "hud", its text (as ASCII) to match a "hud:<text>" entry */
void ae_test_capture_hud(const wchar_t *text);
/* a player killed (game_engine_player_killed): "death" when the dead player is a local one, "kill" when a local
player killed them */
void ae_test_capture_player_killed(long dead_player_index, long killing_player_index);
/* (network_test_update, each frame of a test host's game) debug.network_test_all_weapons */
void ae_test_all_weapons_update(void);
/* (game_engine_post_rasterize_in_game, each view's frame) debug.scoreboard_shown: TRUE keeps the scoreboard shown */
int ae_test_scoreboard_shown(void);

#endif
