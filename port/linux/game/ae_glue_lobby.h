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
/* whether AE's glue hosted (or, Task 13, joined) the session there is now */
int ae_lobby_owns_session(void);
/* the screen AE shows for its session's pregame (M2: the lobby test screen; M4: the Custom Games lobby) */
void ae_lobby_set_pregame_screen(struct ae_screen_class const *screen_class, void *data);

#endif
