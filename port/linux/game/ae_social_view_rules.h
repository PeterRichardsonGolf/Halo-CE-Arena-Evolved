/*
AE_SOCIAL_VIEW_RULES.H

The words the offline social career's screens show (0.9.3): the rank badge, the "wins to the next rank" line, the maps line of
a playlist's card, the shape line, the post-game's EXP sub line, the quit dialog's text. Pure code (no engine headers):
port/linux/tests/ae_social_view_rules_test.c runs it. Each function writes UTF-8 into text (size bytes, always ended) and
returns the length.
*/

#ifndef __AE_SOCIAL_VIEW_RULES_H
#define __AE_SOCIAL_VIEW_RULES_H

#include "ae_social_playlists.h"

/* "RANK 14" */
int ae_social_view_rank(int rank, char *text, int size);
/* "37 EXP" */
int ae_social_view_exp(long exp, char *text, int size);
/* "3 WINS TO RANK 15", "1 WIN TO RANK 15", "TOP RANK" at rank 50 */
int ae_social_view_wins_to(long exp, char *text, int size);
/* the same for the overall rank: "38 EXP TO OVERALL RANK 23", "TOP OVERALL RANK" */
int ae_social_view_overall_to(long total, char *text, int size);
/* the card's shape line: "4 V 4 · 19 MAPS" (teams), "8 PLAYERS · 19 MAPS" (a free for all); maps = the distinct maps on offer */
int ae_social_view_shape(struct ae_social_playlist const *playlist, int maps, char *text, int size);
/* the maps line: "7 of 19 SOCIAL MAP PACK + 7 STOCK" while the stock ones play, "19 of 19 SOCIAL MAP PACK" when they do not, "7 STOCK
   MAPS" for a playlist with no pack map */
int ae_social_view_maps(struct ae_social_counts const *counts, char *text, int size);
/* whether the card shows the "get the rest" line: pack maps are missing */
int ae_social_view_maps_missing(struct ae_social_counts const *counts);
/* the card's "get the rest" lines (two, each short enough for the card) when maps of the playlist are missing, 1; else 0 and both
   empty. A playlist with pack maps: the Social Map Pack's Workshop and converter, or joining an AE server (maps download from the
   host). CLASSIC COMP (the maps' own weapons, no pack): joining AE #5 once downloads its classic maps. Either line may be NULL */
int ae_social_view_get_rest(struct ae_social_playlist const *playlist, struct ae_social_counts const *counts, char *first,
	int first_size, char *second, int second_size);
/* the bot skill line under the BOT SKILL chips: "Matched to how you play" (mode 0) or "Every bot HEROIC" */
int ae_social_view_bot_line(int mode, char *text, int size);
/* the DEV line: "DEV · MATCH 17.4 · TIER 7 NORMAL / NORMAL · RANGE 11-24" (the match as points / 100 shown from 1) */
int ae_social_view_dev_line(long points, char *text, int size);
/* the post-game sub line: "TEAM SLAYER · RANK 15 · 10 TO RANK 16"; a free for all that earns nothing: "FREE FOR ALL · 4TH OR BETTER
   EARNS EXP · RANK 16" (earns 0); a rank 50 one: "... · RANK 50" */
int ae_social_view_postgame_sub(char const *label, int ffa, int earns, int players, long exp_after, char *text, int size);
/* the quit dialog's body: "This is a tournament TEAM SLAYER game. Leaving before the end costs 1 EXP (37 to 36) and counts as left
   early. Leaving at the post-game costs nothing." (at 0 EXP: "... costs nothing here: you have no EXP to lose, but it counts as left early.") */
int ae_social_view_quit_body(char const *label, long exp, char *text, int size);
/* the quit's help line in the pause: "Leave the game: -1 EXP in TEAM SLAYER." */
int ae_social_view_quit_help(char const *label, char *text, int size);
/* the ordinal of a zero-based place: "1ST", "2ND", "3RD", "4TH" ... */
int ae_social_view_ordinal(int place, char *text, int size);

#endif
