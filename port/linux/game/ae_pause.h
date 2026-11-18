/*
AE_PAUSE.H

What the AE pause menu's hook lines in upstream files call (ae_glue_pause.c; the screen: ae_screen_pause.c). Included
after cseries.h (boolean).

- ui_widget.c ui_check_for_pause_game: a player's START in game opens AE's pause instead of the classic pause widget
  while AE's menus are on (display.arena_menus); with them off the classic pause opens as before;
- player_control.c handle_one_player_input: a player whose AE pause is open has no input (as a classic pause menu's
  player_control_port_menu_has_controller);
- game_engine.c: the pause's SCOREBOARD shows that player's in-game scoreboard as BACK held does.
*/

#ifndef __AE_PAUSE_H
#define __AE_PAUSE_H

/* TRUE: AE's pause took the START the controller pressed (network_game: as ui_check_for_pause_game found it) */
boolean ae_pause_start_pressed(short controller_index, boolean network_game);
/* TRUE while the local player's AE pause is open (its panel, SCOREBOARD, or a screen it opened) */
boolean ae_pause_player_open(short local_player_index);
/* TRUE while the local player's pause shows the scoreboard (SCOREBOARD) */
boolean ae_pause_scoreboard_shown(short local_player_index);

#endif
