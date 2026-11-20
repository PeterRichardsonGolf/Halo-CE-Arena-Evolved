/*
AE_SPLIT_QUIT.H

A split-screen player who quits leaves the screen (ae_split_quit_rules.h): the game's side.
*/

#ifndef AE_SPLIT_QUIT_H
#define AE_SPLIT_QUIT_H

/* game.c's remove_quitting_players_from_game, once a player's quit took effect (their unit killed): the player (a player
datum's index) comes off this machine's local list when others stay on it */
void ae_split_quit_player_quit(long player_index);

#endif
