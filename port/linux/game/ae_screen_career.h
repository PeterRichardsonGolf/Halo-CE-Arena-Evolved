/*
AE_SCREEN_CAREER.H

The CAREER screen (0.9.3, ae_screen_career.c): the offline AE TOURNAMENT career of a local player's profile. Left: the TOURNAMENT PLAYLISTS
with their EXP and rank, and the RANKED group (not in this version). Right: the OVERALL RANK (every playlist's EXP added up) and
the focused playlist's rank, EXP, games, wins, ties, losses, left early, win rate and BOT SKILL. The hidden match rating is never
shown. LB / RB the next local player. Opened by AE TOURNAMENT's Y and by DEV MODE's `menu career`.
*/

#ifndef __AE_SCREEN_CAREER_H
#define __AE_SCREEN_CAREER_H

/* local player 0 .. 3's career, the playlist with this id focused (NULL: the first). 1 when open */
int ae_screen_career_open(char const *crumb, short local_player, char const *playlist);
void ae_screen_career_close(void);

#endif
