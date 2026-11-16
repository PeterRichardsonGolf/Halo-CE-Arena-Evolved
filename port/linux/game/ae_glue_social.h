/*
AE_GLUE_SOCIAL.H

The offline social career's core, glued to the game (0.9.3): the playlists of social.txt, which maps this PC has, a profile's EXP,
rank and hidden match rating in career.txt (ae_glue_career.h), a roster drawn for a playlist, a game's result applied once, a
pending game, and the DEV commands "social ..." (ae_dev_cmd_rules.h). The rules are the pure modules ae_social_rules.h,
ae_social_match_rules.h, ae_social_playlists.h and ae_social_lobby_rules.h; the screens and the game's start are a later step.
Menus' thread only.

Log prefix "ae social:". Events (DEV MODE): social_roster, social_result.
*/

#ifndef __AE_GLUE_SOCIAL_H
#define __AE_GLUE_SOCIAL_H

#include <stddef.h>

#include "ae_social_match_rules.h"
#include "ae_social_playlists.h"

struct ae_dev_cmd;

/* ---- playlists and maps ---- */

/* social.txt beside the game (else a small built-in set), read once; the count and the playlist at an index */
int ae_social_playlist_count(void);
struct ae_social_playlist const *ae_social_playlist_at(int index);
/* the playlist with this id (any case), NULL for none */
struct ae_social_playlist const *ae_social_playlist_by_id(char const *id);
/* whether this PC has the map (the game's own check) unless a DEV "social maps hide" hides it: the callback of
   ae_social_playlist_counts / _pick (the context is unused) */
int ae_social_map_here(void *context, char const *map);
/* hides the pack's maps ("smp"), one map (its name or file) or nothing ("none"), for this run */
void ae_social_maps_hide(char const *what);

/* ---- profiles ---- */

/* the career's key of local player 0 .. 3's profile (the lower-case name); 0 for a guest or nobody (no EXP is kept) */
int ae_social_profile_key(short local_player, char *key, size_t size);

/* the name of the profile of local player 0 .. 3 (UTF-8); 0 for a guest */
int ae_social_profile_name(short local_player, char *name, size_t size);

/* ---- EXP, rank, match rating, mode ---- */

/* the profile's EXP in a playlist (0 for none); rank 1 .. 50 from it; the EXP of every playlist added up, and its overall rank */
long ae_social_profile_exp(char const *profile, char const *playlist);
int ae_social_profile_rank(char const *profile, char const *playlist);
long ae_social_profile_exp_total(char const *profile);
int ae_social_profile_overall_rank(char const *profile);
/* the hidden match rating (points; AE_SOCIAL_MATCH_START for a playlist never played) and the games counted in it */
long ae_social_profile_match_points(char const *profile, char const *playlist);
long ae_social_profile_match_games(char const *profile, char const *playlist);
/* the bots' mode of a playlist: 0 AUTO, 1 .. 7 pinned at that skill (this run; the setting that keeps it is the screens' step) */
int ae_social_mode(char const *playlist);
void ae_social_mode_set(char const *playlist, int mode);
/* the modes written to config.toml (social.skills): the matchmaking screen calls it when the player changes one; DEV MODE's `social mode` does not (this run only). Read with the playlists */
void ae_social_modes_save(void);

/* ---- a game ---- */

/* a roster for a playlist and a profile: the hidden rating and the mode read, humans local players on one team, seeded. 1, or 0
   (the playlist unknown or its shape does not fit) */
int ae_social_roster_for(char const *playlist, char const *profile, int humans, unsigned long seed, struct ae_social_roster *roster);

struct ae_social_result
{
	int outcome;                /* AE_SOCIAL_WIN, _TIE, _LOSS or _QUIT */
	long exp_before, exp_after;
	int rank_before, rank_after;
	long match_before, match_after;
	int saved;                  /* career.txt was written; 0: the progress is in memory only (the post-game must not claim it) */
};

/* a finished (or left) game counted once: the EXP of the outcome, the game, the pending line cleared, the hidden rating moved
   against the opponents of the roster (seats on another team in a team game; every other seat in a free for all). seat_place, for
   a free for all, gives every seat's zero-based place (NULL: a team game's outcome decides every opponent: a win beats them all, a
   loss loses to them all, a tie moves nothing, a quit loses). self is the human's seat. career.txt is written; the answer is
   logged ("ae social: result ...") and sent as the social_result event. 1 when counted */
int ae_social_result_apply(char const *profile, char const *playlist, int outcome, struct ae_social_roster const *roster, int self,
	int const *seat_place, struct ae_social_result *result);

/* a game started: the profile's pending line written (it is cleared by the result, found and forgiven at the next start if the game
   never ended); 1 when written */
int ae_social_pending_begin(char const *profile, char const *playlist);
int ae_social_pending_end(char const *profile);

/* ---- DEV MODE ---- */

/* "social ..." (ae_dev_cmd_rules.h): runs it, the answer into text; 1 when it ran */
int ae_social_dev(struct ae_dev_cmd const *cmd, char *text, int size);

#endif
