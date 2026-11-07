#ifndef AE_SERVER_INGAME_H
#define AE_SERVER_INGAME_H

/*
AE_SERVER_INGAME.H

Running a dedicated server from inside the game (0.7.7, plan Task 12). The rules: ae_server_ingame_rules.h.

The server's side (halo --server): Delta Peer's moderation, which the server offers each machine that agrees to it
(delta_moderation.h), mapped onto the server's roles: a player's moderator key listed in admins.txt ("key:" rows), the
owner's key HOST A SERVER hands the server it starts (HALO_SERVER_OWNER_KEY), or the address a player joins from
("cidr:" rows). Each signed action a player takes is run as a server command of their role (the same checks, limits,
immunity and audit as the web page's and the console's). While a game is played, each signed-in player with a role is
sent the admin message (distributed message 113): their role, what it may do here, the server's name, its players and
the games to come.

The client's side: on a server that offers moderation the game signs in by itself (the key never leaves this PC: the
server sees its public half, as it sees the hardware id every join sends); the admin message turns on the pause menu's
RUN THE SERVER row and the player card's server rows (ae_screen_pause.c). Others see nothing new.
*/

#include "ae_server_ingame_rules.h"

/* ---------- the server's (each frame, from ae_server_mod_update; nothing outside a dedicated server) */
void ae_server_ingame_host_update(void);
/* (ae_server_mod.c's immunity for a command run by a player: the actor "key:<64 hex>") the immunity of that key: 1 and
it in *immunity, 0 if it has no row */
int ae_server_ingame_key_immunity(char const *key, int *immunity);
/* the role (ae_ingame role: 0 none .. 3 owner) the key this machine signed in with holds, and its immunity; 0 when the
machine has no key */
int ae_server_ingame_machine_role(int machine_index, int *immunity);
/* (server_commands.c's sv_team) moves a player to a team: "sv_team <player> <red|blue>" */
unsigned char ae_server_ingame_command_team(char const *line, long count, char **words);

/* ---------- the client's */

/* each frame, from AE's tick (ae_hooks.c): the sign-in, the message's age, the actions' results */
void ae_server_ingame_client_tick(unsigned long now);
/* the role this machine has on the server it joined (0: none, or not on a server that says it) */
int ae_server_ingame_role(void);
/* a rotation's or a vote's map (its own name, a pack's or Workshop's identity) as the scoreboard names it: "BLOOD
GULCH", a pack map by its pack's name for it, a Workshop map by its title; else its file name in capitals */
void ae_server_ingame_map_caption(char const *map, char *text, int size);
/* the server's last admin message, NULL without one */
struct ae_ingame_wire const *ae_server_ingame_info(void);
/* an action (AE_INGAME_*) on the player at machine (NONE for the server's: END, NEXT, SAY, RESTART); value: MOVE's team
(0 red, 1 blue), BAN's minutes (0: for ever), NEXT's entry, RESTART's seconds; text: the player's name (MOVE, MUTE: the
player of that machine), SAY's line. 1 if it was sent; what came of it: ae_server_ingame_result */
int ae_server_ingame_act(int action, short machine, int value, char const *text);
/* the server's answer to the last action, once: 1 and its text and whether it was done, else 0 */
int ae_server_ingame_result(char *text, int size, int *done);

/* the netcode's (network_distributed.c's hook): whether a message is the admin message, and its handling */
int ae_server_ingame_handles_message(unsigned short const *message, unsigned short size);
void ae_server_ingame_handle_message(long machine_index, unsigned short const *message, unsigned short size);
/* a game left (network_distributed.c's reset): nothing heard any more */
void ae_server_ingame_reset(void);

#endif
