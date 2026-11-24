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
#include "ae_social_vote_rules.h"

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
	/* (rc5) the party's bots take the first bot seats, on the leader's team; a bot's own name where the seed's is not wanted (a party
	bot, a rematch's fill-in): bot_name[i] is bot i's (seat humans + i), "" for the seed's */
	int party_bots;
	char party_seat[AE_SOCIAL_SEATS];  /* 1 for the seat of a bot in the party (the first bot seats when the plan is made) */
	int staged;                 /* DEV MODE's staged scene: its game is not remembered (recent maps, the party store) */
	int rematch_no;             /* the game of a rematch lobby: how many games it has played in a row (0: not a rematch's) */
	char bot_name[AE_SOCIAL_SEATS][24];
};

/* the bots' names of a plan: the seed's, the plan's own where it has one; out holds roster.count - roster.humans names of 24 letters */
int ae_social_plan_names(struct ae_social_plan const *plan, char out[][24]);

/* the career's key for a local player as the screens read it: the profile's, else (DEV MODE on) "dev-player<n>"; 0 for a guest */
int ae_social_screen_key(short local, char *key, size_t size);
/* the local players in the party (the main menu's signed in pads), 1 .. 4 */
int ae_social_party_size(void);
/* DEV MODE: that many local players in the party until set again (1 .. 4; 0: the signed in pads) */
void ae_social_dev_party(int players);

/* a plan for a playlist: the map by the playlist's weighted pick (never the last map played while others are on offer) unless one
   is named, the gametype the entry's unless one is named, the roster for the profile of local player 0 (the party at his level).
   seed 0: from the clock. 1, or 0 (no playlist, no map this PC has, a shape that does not fit) with the plan's ok 0 */
int ae_social_plan_make(char const *playlist, unsigned long seed, char const *map, char const *gametype, struct ae_social_plan *plan);
/* the maps the lobby's vote offers for this plan's playlist (ae_social_vote_rules.h: up to max, never the map last played while
   others are on offer, seeded by the plan's seed). 0 or 1: nothing to vote on */
int ae_social_plan_vote_choices(struct ae_social_plan const *plan, struct ae_social_vote_choice *out, int max);
/* the plan's game becomes a playlist entry's (the vote's winner): its map and game type, the start loadout by the plan's seed.
   The roster stays. 1, or 0 for an entry that is not the playlist's */
int ae_social_plan_choose(struct ae_social_plan *plan, int entry);
/* the next plan's start loadout, one time: AE_SOCIAL_START_BR .. PROTOMAG, 0 the game type's own, -1 (the default) by the plan's seed.
   DEV MODE's `social start <id> variant <br|evo|magnum|protomag|none>` */
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
/* (rc5) the finished game as the post-game lobby's LAST GAME card shows it; 0 when no social game was played */
struct ae_social_last
{
	int valid;
	char title[48], score[24], map[48];
	int exp_delta, rank_after;
	int kills[AE_SOCIAL_LOCAL], deaths[AE_SOCIAL_LOCAL];
};
int ae_social_game_last(struct ae_social_last *last);
/* the plan of the next game of a rematch: the finished game's roster kept, each bot that is neither a party bot nor a local player
leaving with a chance (ae_rematch_rules.h) and a fill-in of its tier taking its seat. game_no counts the rematches (0 the first); fresh[seat]
is 1 for a fill-in's seat, left[] holds the names of the leavers (up to 2, left_count of them). 0 when no game was played */
int ae_social_game_rematch(struct ae_social_plan *plan, int *game_no, int *fresh, char left[][24], int *left_count);
/* the post-game's CONTINUE: the game is left and MATCHMAKING comes back on the playlist (1 when it applies) */
int ae_social_game_continue(void);
/* 1 while the post-game's CONTINUE waits for the menus to open MATCHMAKING again (AE's main menu waits for it) */
int ae_social_game_reopen_pending(void);

/* DEV MODE: a made-up post-game for the screens' staging. kind "win", "tie", "loss", "place" (place_index of place_players) or
   "rankup"; exp_before < 0: the profile's own EXP. Each call is one local player's (0 .. 3): the view of every player named so far
   is shown (a second player's call after the first's makes it split screen). 1 when it opened */
int ae_social_game_dev_postgame(char const *kind, char const *playlist, long exp_before, int place_index, int place_players,
	int player, char *text, size_t size);
/* DEV MODE: the social lobby of a playlist (the screen: ae_screen_social_lobby.h) with a plan made from these words */
int ae_social_game_dev_lobby(char const *playlist, char const *map, unsigned long seed, int now, char *text, size_t size);
/* DEV MODE (rc5): the post-game lobby (the REMATCH) of a playlist on a made-up last game: bots of the roster are in the party,
online: the fake online states, still: the "Still there?" question up; the vote's seconds left and the bots' votes as for the vote */
int ae_social_game_dev_lobby_post(char const *playlist, unsigned long seed, int left_seconds, int const *votes, int has_votes,
	int online, int still, int bots, char *text, size_t size);
/* DEV MODE: the same lobby opened at its map vote: left_seconds 0 .. 10 of the vote left (-1: all of it), the bots' votes
   votes[0 .. 2] if has_votes (else they vote by themselves as time passes) */
int ae_social_game_dev_lobby_vote(char const *playlist, unsigned long seed, int left_seconds, int const *votes, int has_votes,
	char *text, size_t size);

#endif
