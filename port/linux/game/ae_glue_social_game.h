/*
AE_GLUE_SOCIAL_GAME.H

A social career game from the lobby's GO to the way out (0.9.3; ae_glue_social.h has the core's glue). A PLAN is what the lobby
shows and the game plays: the playlist, the map and gametype the playlist's pick chose, the humans (the party), and the roster
(tiers, teams). STARTING a plan keeps the player's own bot settings (BOTS, BOT SKILL, ... are saved and set for this run only
with config_set), gives every bot its own SHOOTING and MOVEMENT skill (ae_bots_seat.h), and opens the offline game
(ae_screen_create_game_social_open). Each frame (ae_social_game_tick, from ae_screen_postgame_tick): the first frame of the game
writes the pending line of each local profile; the post-game's opening counts the result once (EXP and the hidden rating,
ae_social_result_apply); once the game is over the settings come back and the bots' pairs are cleared. QUIT from the pause counts
a quit once, before the game is left (ae_social_game_quit). A guest pad (no profile) earns nothing; with DEV MODE on a pad with
no profile plays as "dev-player<n>" so a staged scene keeps its numbers.

Log prefix "ae social:". Events (DEV MODE): social_lobby (screen), social_start, social_result, social_quit.
*/

#ifndef __AE_GLUE_SOCIAL_GAME_H
#define __AE_GLUE_SOCIAL_GAME_H

#include <stddef.h>

#include "ae_social_match_rules.h"
#include "ae_social_playlists.h"

enum { AE_SOCIAL_LOCAL = 4 };

struct ae_social_plan
{
	int ok;
	char playlist[AE_SOCIAL_ID];
	char label[AE_SOCIAL_LABEL];
	char map[AE_SOCIAL_MAP];
	char gametype[AE_SOCIAL_GAMETYPE];
	int entry;                  /* the playlist's entry the pick chose */
	int start;                  /* the start loadout the game mixed in: AE_SOCIAL_START_* (0: the game type's own) */
	int ae_weapons;             /* 1 the game plays AE's weapons (the weapon cache is, or can be, loaded), 2 the maps' own, 0 stock */
	unsigned long seed;
	int humans;                 /* local players in the party (1 .. 4) */
	int mode;                   /* 0 AUTO, else the pinned skill step */
	struct ae_social_roster roster;
};

/* the career's key for a local player as the screens read it: the profile's, else (DEV MODE on) "dev-player<n>"; 0 for a guest */
int ae_social_screen_key(short local, char *key, size_t size);
/* the local players in the party (the main menu's signed in pads), 1 .. 4 */
int ae_social_party_size(void);

/* a plan for a playlist: the map by the playlist's weighted pick (never the last map played while others are on offer) unless one
   is named, the gametype the entry's unless one is named, the roster for the profile of local player 0 (the party at his level).
   seed 0: from the clock. 1, or 0 (no playlist, no map this PC has, a shape that does not fit) with the plan's ok 0 */
int ae_social_plan_make(char const *playlist, unsigned long seed, char const *map, char const *gametype, struct ae_social_plan *plan);
/* the next plan's start loadout, one time: AE_SOCIAL_START_BR .. MAGNUM, 0 the game type's own, -1 (the default) by the plan's seed.
   DEV MODE's `social start <id> variant <br|evo|magnum|none>` */
void ae_social_plan_force_start(int start);
/* the plan's game: settings kept and set, the bots' pairs given, the offline game opened. 1 when it opened */
int ae_social_plan_start(struct ae_social_plan const *plan);

/* whether a social game is on (from its start until the settings are back) */
int ae_social_game_active(void);
/* whether that game lays AE's weapons on the map (AE PICKUPS, ae_pickup_rules.h): a tournament game of AE's weapons */
int ae_social_game_pickups(void);
/* the start loadout of the game being played ("BATTLE RIFLE"), "" for the game type's own or when no tournament game is on */
char const *ae_social_game_start_label(void);
/* the label of the playlist being played ("TEAM SLAYER"), "" when none */
char const *ae_social_game_label(void);
/* once a frame */
void ae_social_game_tick(void);
/* local player's QUIT from the pause menu, before the game is left: counted once (-1 EXP). 1 when it counted, 0 when it did not
   apply (no social game, a guest, the game already counted) */
int ae_social_game_quit(short local);
/* whether player's quit would cost EXP now (a social game, a profile, not counted yet), and the dialog's texts for it: its body, and
   the pause row's help line */
int ae_social_game_quit_costs(short local);
void ae_social_game_quit_text(short local, char *body, size_t body_size);
void ae_social_game_quit_help(char *help, size_t size);
/* a game over and its result counted for a local player: what the post-game's EXP row shows */
struct ae_social_pg
{
	int shown;                  /* 1: the post-game has an EXP row for this player */
	int guest;                  /* no profile: "Sign in a profile to keep EXP", 0 */
	int delta;                  /* the EXP this game moved: +1, 0 */
	int ffa;                    /* a free for all */
	int players;
	long exp_before, exp_after;
	int rank_before, rank_after;
	char sub[120];
};
int ae_social_game_postgame_seat(short local, struct ae_social_pg *out);
/* the post-game's CONTINUE: the game is left and MATCHMAKING comes back on the playlist (1 when it applies) */
int ae_social_game_continue(void);

/* DEV MODE: a made-up post-game for the screens' staging. kind "win", "tie", "loss", "place" (place_index of place_players) or
   "rankup"; exp_before < 0: the profile's own EXP. Each call is one local player's (0 .. 3): the view of every player named so far
   is shown (a second player's call after the first's makes it split screen). 1 when it opened */
int ae_social_game_dev_postgame(char const *kind, char const *playlist, long exp_before, int place_index, int place_players,
	int player, char *text, size_t size);
/* DEV MODE: the social lobby of a playlist (the screen: ae_screen_social_lobby.h) with a plan made from these words */
int ae_social_game_dev_lobby(char const *playlist, char const *map, unsigned long seed, int now, char *text, size_t size);

#endif
