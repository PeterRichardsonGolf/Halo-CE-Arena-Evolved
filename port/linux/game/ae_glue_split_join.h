/*
AE_GLUE_SPLIT_JOIN.H

Joining and leaving a match in progress as a split screen player (rules: ae_split_join_rules.h). A pad of this PC with no
player asks the game for one: the same request a player added in the lobby makes (network_game_client_add_player), which
in a running game goes to the host as an in-game addition; the host adds the player (a bot gives its place up in a full
game: ae_lan_bots_make_room), every machine is told, and this machine gives the player their own view and HUD. A player
leaves with the request a quit makes (network_game_client_local_player_quit): their view goes, the others' fill the
screen. The host's switch is checked here by the asking machine and again by the host (ae_split_join_host_allows).

Its log: "split join: ..." lines.
*/

#ifndef __AE_GLUE_SPLIT_JOIN_H
#define __AE_GLUE_SPLIT_JOIN_H

/* the reason (ae_split_join_rules.h) the controller (0-3) may not join the game now; 0 when it may. pad_staged: DEV MODE's
staging, the controller counted as a pad whether this PC has one */
int ae_split_join_reason(short controller, int pad_staged);
/* DEV MODE: counts the controller (1-3) as a pad for ADD PLAYER's row (a test with no second pad) */
void ae_split_join_stage_pad(short controller);
/* a game begins: the pads DEV MODE counted and the joins waiting or asked are forgotten */
void ae_split_join_new_game(void);
/* the first free controller with a pad (1-3); -1 when none */
short ae_split_join_free_controller(void);
/* joins the controller (the reason when it may not: 0 when asked; the host's answer comes with the game's own update) */
int ae_split_join_request(short controller, int pad_staged);
/* once a frame: a controller waiting for a bot to leave its place asks when the place is free */
void ae_split_join_tick(void);
/* whether a joining controller waits for the host's answer, and its seconds so far */
int ae_split_join_waiting(short controller);
/* leaves the controller's player (the reason for not: 0 left, -1 not allowed) */
int ae_split_leave_request(short controller);
/* the players on this PC */
int ae_split_join_local_players(void);
/* the host's side of the switch (network_server_message_handler.c): 1 when the machine's player request is allowed */
int ae_split_join_host_allows_machine(short machine_index);
/* the reason's words for a menu */
const char *ae_split_join_words(int reason);

#endif
