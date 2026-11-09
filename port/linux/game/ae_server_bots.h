/*
AE_SERVER_BOTS.H

(0.7.7) Bots on a dedicated server (the rules: ae_server_bots_rules.h). The server's machine has no person, so
ae_lan_bots.c asks this for the bots the game wants (humans only count) and takes them on virtual machines; this also
holds the dropped humans' slots (rejoin) and writes the server's status and start-up lines. No menu or HUD: settings
(server.bots_fill, server.bots_skill, server.bots_alone, server.bots_delay), the log and /api/status only.
*/

#ifndef AE_SERVER_BOTS_H
#define AE_SERVER_BOTS_H

struct network_game;
struct network_player;
struct server_json;

/* whether this run is a dedicated server whose current entry takes bots (the resolved server.bots_fill is not off) */
boolean ae_server_bots_enabled(void);
/* (a dedicated server) the bots the current entry wants: humans are the people in the game (lobby or game in
progress), bots_now the bots seated now, lobby whether the game is not started yet (the start delay applies there when
no bot is seated), now seconds on a clock. Includes the dropped humans' slots a bot holds */
long ae_server_bots_target(long humans, long max_players, boolean played, long bots_now, boolean lobby, double now);
/* (server_director.c's server_apply_entry) the lobby's entry is set: the gametype name decides what "auto" means */
void ae_server_bots_entry_applied(char const *gametype);
/* (ae_lan_bots.c's ae_lan_bots_ingame, every frame of a dedicated server's game) the humans who dropped are held, the
ones who came back get their statistics back */
void ae_server_bots_update(struct network_game *game);
/* (network_server_manager.c's network_game_server_add_player_to_game, an AE hook) the team a joining human gets: the
one it left when it comes back to the game in progress, else the one the game chose */
int ae_server_bots_rejoin_team(struct network_player const *player, int team);
/* whether bot-only games run (server.bots_alone) and the server has bots at all */
boolean ae_server_bots_alone(void);
/* the people in a game's players (the bots are not): what the director's counts mean */
long ae_server_bots_humans(struct network_game const *game);
long ae_server_bots_count(struct network_game const *game);
/* the skill name for the bots (server.bots_skill when valid, else normal) */
char const *ae_server_bots_skill_name(void);
/* "off", "minimum" or "empty_slots": the fill the current entry uses */
char const *ae_server_bots_fill_name(void);
/* /api/status: "bots":{"count","skill","fill","alone","held"} */
void ae_server_bots_status(struct server_json *json, struct network_game const *game);
/* the start-up lines */
void ae_server_bots_summary(void);

#endif
