/*
AE_GLUE_LOBBY.H

The lobby's glue (ae_glue_lobby.c): hosting a game, the local players, the
roster, the map and gametype, starting, ending and going back to the lobby,
from C with no widget (M1 Task 6's Check B: notes/ae-early-checks.md). Every
call returns an ae_result: ok, or the reason (ae_strings.c), also logged
"ae lobby: <call>: <reason>". The roster's types are plain
(ae_lobby_roster.h). The post-game take-over's hook (ae_lobby_take_pregame_ui)
is declared in ae_hooks.h (preflight P19). Include after cseries.h.
*/

#ifndef __AE_GLUE_LOBBY_H
#define __AE_GLUE_LOBBY_H

#include "ae_lobby_roster.h"
#include "ae_result.h"
#include "ae_ui.h"

enum ae_lobby_kind { AE_LOBBY_LOCAL, AE_LOBBY_LAN, AE_LOBBY_ONLINE };

/* hosts a game: LAN (Check B's sequence: listed on the LAN, joinable), LOCAL (the same, nobody can join, nothing
listed), ONLINE (LAN plus internet hosting, as upstream's Create Game > internet: p2p hosting allowed, listed public
as network.host_public says; network.list_hosted_games honoured by p2p) */
struct ae_result ae_lobby_host(int kind);
/* a local player for a controller (0-3; logged 1-based) */
struct ae_result ae_lobby_add_local_player(short controller);
/* the roster now, in AE's order (ae_lobby_roster_order) */
struct ae_result ae_lobby_roster(struct ae_lobby_roster *roster);
/* the host's map, by its file name: "bloodgulch" -> levels\test\bloodgulch\bloodgulch */
struct ae_result ae_lobby_set_map(const char *map_file);
/* the host's gametype, by its name ("slayer", "team_slayer"...: game_engine_get_variant_by_name) */
struct ae_result ae_lobby_set_gametype(const char *stored_name);
struct ae_result ae_lobby_start(void);
/* the host ends the game now (game_engine_end_game: the scores show) */
struct ae_result ae_lobby_end_game(void);
/* the host, after the post-game: everyone back to the lobby */
struct ae_result ae_lobby_back_to_pregame(void);
/* whether AE's glue hosted or joined the session there is now (checked each frame: a session disposed by anything
else is not AE's) */
int ae_lobby_owns_session(void);
/* (Task 13) joining: this machine searches the LAN and joins the first game it can (network_test.c's join path
without its widget); ok means searching. ae_lobby_update goes on with it each frame; ae_lobby_join_state says how it
went: 1 joined ("ae lobby: joined"), 0 still going, -1 failed (the reason: no game found after 10 s, a host on
another network version with both numbers (3 s when only such hosts are heard), the lobby full or closed to this
machine), -2 no join (none tried, or left) */
struct ae_result ae_lobby_join_first_available(void);
/* the game an invite link or code leads to (pasted text, trimmed), as upstream's Server Browser joins one
(browser_screen.c: p2p_join_invite, then network_game_client_join_invite_host each frame): its own host's game, never
another. Reasons: internet play is off, not an invite, can't be joined from this version, the host did not answer
(15 s). (Not a bare IP address: upstream joins by invite only) */
struct ae_result ae_lobby_join_address(const char *address);
int ae_lobby_join_state(struct ae_result *failure);
/* leaves (client) or closes (host) the session: client and server disposed, connection local, AE's session over;
upstream's menus are not reopened (the caller shows the next AE screen); logs "ae lobby: left" / "closed" */
struct ae_result ae_lobby_leave(void);
/* once a frame (ae_hooks, in the menus and in the game): the join going on, the session's owner checked */
void ae_lobby_update(unsigned long now_ms);
/* (the advertisement hook, network_client_manager.c: declared in ae_hooks.h) */
/* the screen AE shows for its session's pregame (M2: the lobby test screen; M4: the Custom Games lobby) */
void ae_lobby_set_pregame_screen(struct ae_screen_class const *screen_class, void *data);

#endif
