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

#include "ae_lobby_countdown.h"
#include "ae_lobby_roster.h"
#include "ae_result.h"
#include "ae_ui.h"

enum ae_lobby_kind { AE_LOBBY_LOCAL, AE_LOBBY_LAN, AE_LOBBY_ONLINE };

/* hosts a game: LAN (Check B's sequence: listed on the LAN, joinable), LOCAL (the same, nobody can join, nothing
listed), ONLINE (LAN plus internet hosting, as upstream's Create Game > internet: p2p hosting allowed, INVITE ONLY in
M2 whatever network.host_public says; PUBLIC is M4's PRIVACY row) */
struct ae_result ae_lobby_host(int kind);
/* a local player for a controller (0-3; logged 1-based) */
struct ae_result ae_lobby_add_local_player(short controller);
/* (0.3.9) split screen: whether a controller (0-3) has a player of this machine in the game; how many do; ADD PLAYER
on or off (a lone gamepad moves off the keyboard's controller to its own, for its START to join); a controller (1-3)
joining with its profile (a guest's, "Guest n", when it has none or another player here has it); its player leaving
(never the machine's last: that leaves the lobby) */
int ae_lobby_controller_plays(short controller);
int ae_lobby_local_players(void);
void ae_lobby_adding_player(int adding);
struct ae_result ae_lobby_join_controller(short controller);
struct ae_result ae_lobby_leave_controller(short controller);
/* asks the host to put this controller's own player on the other team, through the classic pregame player-settings
request; false outside a team lobby or when the request could not be sent */
int ae_lobby_change_team(short controller);
/* the host puts a LAN bot (by its roster machine and controller) on the other team and holds it there (the lobby's team
arrows, ae_lan_bots_flip_team); false off the host, outside a team lobby, or for none */
int ae_lobby_change_bot_team(short machine, short controller);
/* the roster now, in AE's order (ae_lobby_roster_order) */
struct ae_result ae_lobby_roster(struct ae_lobby_roster *roster);
/* the host's map, by its file name: "bloodgulch" -> levels\test\bloodgulch\bloodgulch */
struct ae_result ae_lobby_set_map(const char *map_file);
/* the host's gametype, by its name ("slayer", "team_slayer"...: game_engine_get_variant_by_name) */
struct ae_result ae_lobby_set_gametype(const char *stored_name);
struct ae_result ae_lobby_start(void);
/* (0.4.3) the host's START GAME with the lobby's countdown (mockups-042/countdown SPEC): the server's own pregame
countdown from milliseconds (5999: 5, 4, 3, 2, 1 a second each, then 0 until the start), sent to every machine as the
stock lobby's ("ae lobby: starting (countdown N ms)"); milliseconds <= 0 starts at once (ae_lobby_start: test launches,
debug.ae_lobby_countdown_ms 0). Refused when the lobby can't start (the server's own rule) */
struct ae_result ae_lobby_start_countdown(long milliseconds);
/* the host cancels it: the server's countdown cleared and held (as the take-over holds it after a game: other machines'
stock "faster" can't start it again) and one NONE countdown message sent, so every machine stops showing the number (no
new message: network 38 as it is); refused once the server sent the start */
struct ae_result ae_lobby_cancel_start(void);
/* the seconds to the start this machine's client has (every machine's, the host's own included): >= 0,
AE_COUNTDOWN_SECONDS_NONE, or AE_COUNTDOWN_SECONDS_STARTED once the client is past its pregame (ae_lobby_countdown.h) */
short ae_lobby_seconds_to_start(void);
/* the game's gametype has PRE-GAME COUNTDOWN (the variant every machine has from the host): the lobby's count beeps
softly then (ae_countdown_beep) */
int ae_lobby_pregame_countdown(void);
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
/* (AE's LAN GAMES) this machine searching the LAN, joining nothing yet: the games it hears read with ae_lobby_lan_games,
one joined with ae_lobby_lan_join (then ae_lobby_join_state, as any join); ae_lobby_leave ends the search */
struct ae_lan_game
{
	short index;                /* the client's own (ae_lobby_lan_join's) */
	unsigned char nonce[8];     /* the host's stable identity while slots are reused */
	char name[48], map[128];    /* UTF-8; map: the scenario's name or path, as advertised */
	short engine, players, maximum_players;
	int teams, in_progress;
	int join_state;             /* HALO_PORT_JOIN_* (halo_port_limits.h): 0 joinable */
	unsigned int version;
	unsigned char kind;         /* the game's kind (OpenCE's network 37: game_modes.h's game_modes_kind) */
};
enum { AE_LAN_GAMES = 9 };
struct ae_result ae_lobby_lan_search(void);
/* whether the LAN search goes on (none of a join under way) */
int ae_lobby_lan_searching(void);
/* the games heard now (open ones of this platform), at most maximum; their count */
short ae_lobby_lan_games(struct ae_lan_game *games, short maximum);
struct ae_result ae_lobby_lan_join(short index, const unsigned char nonce[8]);
/* the game an invite link or code leads to (pasted text, trimmed), as upstream's Server Browser joins one
(browser_screen.c: p2p_join_invite, then network_game_client_join_invite_host each frame): its own host's game, never
another. Reasons: internet play is off, not an invite, can't be joined from this version, the host did not answer
(15 s). (Not a bare IP address: upstream joins by invite only) */
struct ae_result ae_lobby_join_address(const char *address);
/* (0.7.0) a host named by its address (an IPv4 address or a name the system looks up, on a thread of its own; the port
the game's own when the player typed none): this machine's client connects to it directly, as it does to a game it is
hosting, and ae_lobby_join_state follows it as the others. Reasons: nothing there answered in 10 s (also an unknown
name), the host's lobby is full or closed to this machine, the game is running. ok means reaching it */
struct ae_result ae_lobby_join_host(const char *host, unsigned short port);
/* the name is looked up first, on a thread of its own: true while that goes on (ae_lobby_leave drops it) */
int ae_lobby_join_looking_up(void);
/* the port a game listens on (the one a join by address uses when none is typed) */
unsigned short ae_lobby_host_port(void);
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
