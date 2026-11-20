/*
AE_COOP_RULES.H

CAMPAIGN's split-screen co-op seats (0.9.4), the rules apart from the game: who may sit, who may leave, what starts, and
the words an error says. Pure (port/linux/tests/ae_coop_rules_test.c); the screen is ae_screen_campaign.c, the game's
calls ae_glue_campaign.c.

Player 1 is always this PC's keyboard / pad 1 (controller 1). The others are pads on controllers 2-4, up to three (0.9.4: the
engine holds MAXIMUM_NUMBER_OF_LOCAL_PLAYERS = 4 local players and the campaign's game setup (main.c create_local_players)
takes one a gamepad; the classic menus' Cooperative Play seated two). A pad that joins chooses its profile (the lobby's
card) before it sits; a lone gamepad shared with the keyboard joins from controller 1 once the keyboard has been used
(ADD PLAYER's state moves it onto controller 2).

Online co-op (0.9.4): the same engine path the classic Create Game > Map > a campaign level takes, with no game engine in the
gametype (menu_functions.c game_cooperative); up to AE_COOP_ONLINE_PLAYERS_MAX (COOPERATIVE_DEFAULT_PLAYERS). The words a
co-op game's row says in the SERVER BROWSER, and the host's and joiner's errors, are here too.
*/

#ifndef __AE_COOP_RULES_H
#define __AE_COOP_RULES_H

enum
{
	AE_COOP_CONTROLLERS = 4,
	AE_COOP_PLAYERS_MAX = 4,          /* local players: MAXIMUM_NUMBER_OF_LOCAL_PLAYERS */
	AE_COOP_ONLINE_PLAYERS_MAX = 16   /* a hosted co-op game: menu_functions.c COOPERATIVE_DEFAULT_PLAYERS */
};

struct ae_coop_seats
{
	int seated[AE_COOP_CONTROLLERS];   /* [0] is player 1's, always; [1]-[3] the other players' pads when they sit */
	int picking;                       /* profile cards open (pads choosing before they sit) */
	int adding;                        /* ADD PLAYER's state is on (a lone gamepad is on controller 2) */
};

enum { AE_COOP_JOIN_OK = 0, AE_COOP_JOIN_BAD_CONTROLLER, AE_COOP_JOIN_SEATED, AE_COOP_JOIN_FULL };
enum { AE_COOP_ERR_NONE = 0, AE_COOP_ERR_NO_PROFILE, AE_COOP_ERR_SECOND_CONTROLLER, AE_COOP_ERR_SECOND_PROFILE };

/* the screen opened: player 1 only */
void ae_coop_seats_reset(struct ae_coop_seats *seats);
/* how many players sit now */
short ae_coop_players(struct ae_coop_seats const *seats);
/* the first other player's controller (1-3), -1 when none sits */
short ae_coop_second(struct ae_coop_seats const *seats);
/* the pads that sit (not player 1's) as a mask: bit c is controller c (1-3) */
unsigned ae_coop_pad_mask(struct ae_coop_seats const *seats);
/* the controller of the player at a place (0 = player 1, 1 = the next seated pad...), -1 when there is none */
short ae_coop_controller_at(struct ae_coop_seats const *seats, short place);
/* a pad's START may open its profile card: controller 2-4, not seated, no seat left once the cards open count (four seats) */
int ae_coop_start_joins(struct ae_coop_seats const *seats, short controller);
/* the card is answered with a profile: the pad sits (AE_COOP_JOIN_OK) or is refused and why */
int ae_coop_join(struct ae_coop_seats *seats, short controller);
/* a pad's B leaves its seat: only player 2's (player 1's B goes back); 1 when it left */
int ae_coop_leave(struct ae_coop_seats *seats, short controller);
/* whether that pad's B is the seat's (its owner's): taken, never the screen's BACK */
int ae_coop_back_is_seat(struct ae_coop_seats const *seats, short controller);
/* controller 1's START is a lone gamepad's: the keyboard has been used, the pad's START is held, player 2 neither sits
nor chooses */
int ae_coop_lone_pad_joins(struct ae_coop_seats const *seats, short controller, int keyboard_used, int pad_start_held);
/* CONTINUE plays the saved game, which does not go on with two players (game_state_header_valid) */
int ae_coop_continue_allowed(struct ae_coop_seats const *seats);
/* "1 PLAYER", "2 PLAYERS" ... "4 PLAYERS" */
const char *ae_coop_players_text(short players);
/* a seated pad's controller as the game takes it: 2-4 (never player 1's, the classic menus' "Player 2 chooses their profile
with their own controller") */
int ae_coop_second_controller_ok(short controller);
/* the start's check: AE_COOP_ERR_NONE, or what is wrong with the seats (a seat's controller is out of range or player
1's; no profile for player 1; missing: the pads (the mask of ae_coop_pad_mask) that sit with no profile); title and body are
the AE dialog's words, as the classic menus' co-op said them */
int ae_coop_start_check(struct ae_coop_seats const *seats, int player1_profile, unsigned missing);
void ae_coop_error_text(int error, const char **title, const char **body);

/* ---------- online co-op (HOST CO-OP, the lobby, the SERVER BROWSER's rows) */

enum { AE_COOP_NET_LAN = 0, AE_COOP_NET_ONLINE };
/* the network a co-op game is hosted on: online (invite only until PUBLIC) when internet play is on, else the LAN */
int ae_coop_host_network(int internet_play_on);
/* the most players a hosted co-op game takes at the start: 16 (the game's own lower limit applies) */
short ae_coop_host_maximum(short engine_maximum);
/* the level (0-9) a listed game's map is: its scenario's name or path, the last part exactly a10, a30, a50, b30, b40, c10, c20, c40,
d20 or d40 (any case); -1 for anything else (a map that only holds "a10", <file>@ce, @ws<item>, NULL) */
short ae_coop_level_of_map(const char *map);
/* what a co-op game's browser row says: "CO-OP" or "CO-OP, a bullet, HEROIC" (difficulty 0-3, else none) */
void ae_coop_listing_type(short difficulty, char *text, int size);
/* a listing's gametype ("Co-op Legend", as browser.c reads it) to a difficulty 0-3, -1 when it does not say one (or is not
a co-op gametype) */
short ae_coop_difficulty_from_gametype(const char *gametype);
/* whether the gametype a game advertises is a co-op one by name: "Co-op ..." (a campaign level's game has no engine) */
int ae_coop_gametype_is_coop(const char *gametype);
/* whether a browser keeps a game: ordinary multiplayer follows its browser's rule; co-op only when PUBLIC and online
(a LAN co-op game is never listed) */
int ae_coop_listing_kept(int cooperative, int public_game, int lan);
enum
{
	AE_COOP_HOST_ERR_NONE = 0,
	AE_COOP_HOST_ERR_NO_PROFILE,
	AE_COOP_HOST_ERR_HOST,            /* the session did not open */
	AE_COOP_HOST_ERR_TIMEOUT,         /* this PC's client did not join its own lobby in time */
	AE_COOP_HOST_ERR_LEVEL,           /* the level could not be set up as co-op */
	AE_COOP_HOST_ERR_PEER,            /* a machine that does not play co-op is in the game (the original console) */
	AE_COOP_HOST_ERR_IN_GAME
};
void ae_coop_host_error_text(int error, const char **title, const char **body);
/* the AE dialog shown to a co-op client when its host is gone */
void ae_coop_host_left_text(const char **title, const char **body);

#endif
