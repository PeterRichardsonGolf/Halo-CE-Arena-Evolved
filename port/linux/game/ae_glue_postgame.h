/*
AE_GLUE_POSTGAME.H

The AE post-game's contact with the game (ae_glue_postgame.c), for its screen (ae_screen_postgame.c): the facts of the
game that just ended, from the engine's own statistics (populate_statistic_buffer's ranking, game_engine_did_player_win,
the players' statistics, game_stats.c's best spree), and the two ways out, the engine calls the classic carnage report's
buttons make (game_engine.c game_engine_update_non_deterministic, ae_postgame.c):

- CONTINUE (the host): network_game_server_reset_to_pregame, the stock A ("pick game"); the lobby comes back
  (ae_lobby_take_pregame_ui). A guest's machine follows its host there, as before;
- LEAVE: network_game_abort, the stock B (ae_postgame.c asked first; the screen asks with its own dialog).
*/

#ifndef __AE_GLUE_POSTGAME_H
#define __AE_GLUE_POSTGAME_H

enum { AE_POSTGAME_SEATS = 4 };
/* a local player's result */
enum { AE_POSTGAME_WIN, AE_POSTGAME_LOSS, AE_POSTGAME_TIE, AE_POSTGAME_PLACE };

/* one local player's GAME RESULT (UTF-8) */
struct ae_postgame_seat
{
	int present;
	short slot;                 /* 1..4: the emblem's number */
	unsigned int chip;          /* the emblem's colour: the team's, in FFA the player's */
	char name[48];
	char team[16];              /* "RED TEAM", "BLUE TEAM"; "" in FFA */
	char title[48];             /* the big line: "RED TEAM WINS", "TIE GAME"; FFA "SECOND PLACE", "TIED 2ND PLACE" */
	int result;                 /* AE_POSTGAME_* */
	char result_text[16];       /* "WIN" "LOSS" "TIE" "2ND" */
	char result_sub[64];        /* "Red Team 50, Blue Team 38" (the player's team first); FFA "of 8 players" */
	unsigned int stripe;        /* the RESULT row's stripe: the player's team colour, the title's in FFA */
	/* the title's colour: the winning team's; FFA the player's own armour (the title is their place; owner 10-25),
	lightened on the dark screen */
	unsigned int title_color;
	long kills, deaths, assists;
	long spree;                 /* the best spree, -1 when not known (no game browser build) */
};

/* the game that ended */
struct ae_postgame_facts
{
	int teams;
	int host;                   /* this machine hosts: its CONTINUE takes everyone back to the lobby */
	int can_continue;           /* the post-game's scores are up (the host's continue is taken now) */
	unsigned int title_color;   /* the winning team's colour; FFA the winner's armour (lightened): the banner's */
	char gametype[48], map[48];
	char played[16];            /* the time played, "12:07" */
	/* CARNAGE's banner: "RED TEAM WINS" / "<NAME> WINS" / "TIE GAME", its colour and its score ("50 - 38", FFA the
	winner's score), and the colour of its words (white, or navy on a light armour colour) */
	char banner[48], banner_score[24];
	unsigned int banner_color, banner_text_color;
	/* a light armour colour (white is the focus's alone) or a black one: the banner the dark panel with a stripe of it
	(0: none) */
	unsigned int banner_stripe;
	short players;              /* in the ranking */
	struct ae_postgame_seat seats[AE_POSTGAME_SEATS];
};

/* the facts now (cheap enough for every frame: one ranking) */
void ae_glue_postgame_facts(struct ae_postgame_facts *facts);
/* a local player's view as the game lays the views out in play (the post-game itself draws one): its index among them
(left to right, top to bottom), their count, and whether it spans two columns (three views: the first); 0 when the
player has none */
int ae_glue_postgame_view(short local_player, short *view, short *views, short *spans_two_columns);
struct ae_pause_card;
/* the FINAL SCORE card's rows (the pause menu's game card's: team scores, FFA the leaders), the teams named in full */
void ae_glue_postgame_card(short local_player, struct ae_pause_card *card);
/* CARNAGE's board (the AE scoreboard, ae_scoreboard_draw_with) in AE's layer over the whole layout (layout_width x
layout_height units), its lists from top: its foot, and its left and width (the banner's above it) */
void ae_glue_postgame_board(float layout_width, float layout_height, float top, float *bottom, float *x, float *width);
/* whether the AE post-game is due: AE's menus on and drawn, a multiplayer game over and its post-game up
(game_engine_force_single_screen's states: the scores' delay and the scores) */
int ae_glue_postgame_due(void);
/* each frame: the game's time while it is played, for TIME PLAYED */
void ae_glue_postgame_track(void);
/* CONTINUE (the host, once can_continue): 1 when the server went back to the pregame */
int ae_glue_postgame_continue(void);
/* LEAVE THE GAME */
void ae_glue_postgame_leave(void);

#endif
