/*
AE_CAREER_VIEW_RULES.H

The words and numbers the career surfaces (CAREER, CHALLENGES, the post-game rows, the lobby row) show, as pure rules
(port/linux/tests/ae_career_xp_test.c runs them): a number is grouped ("10,350"), a percent has no space ("53%"), a count of games
never counts a quit twice, and each caption says what the bar is waiting for ("710 XP TO GO", "NEXT: COMMAND SERGEANT MAJOR").
Everything is UTF-8 into text (size bytes, always ended).
*/

#ifndef __AE_CAREER_VIEW_RULES_H
#define __AE_CAREER_VIEW_RULES_H

/* "—" (U+2014): an unplayed playlist's value, a rate with no games */
#define AE_CV_DASH "\xE2\x80\x94"

/* a whole number with thousands separators: "10,350" */
void ae_cv_number(long value, char *out, int size);
/* the losses of a playlist: games less wins, ties and the games left early (a quit is counted once, as left early); never below 0 */
long ae_cv_losses(long games, long wins, long ties, long quits);
/* wins over games as a whole percent without a space, "53%"; "—" with no games */
void ae_cv_win_rate(long games, long wins, char *out, int size);
/* the career rank's name size for a room: 40 u when its width at 40 fits, else 34, else 30 (a name is never cut short); widths are
   the name's at 40, 34 and 30 u */
int ae_cv_fit_size(float width40, float width34, float width30, float room);
/* the meta line's three parts: "RANK 14 OF 30", "TOUR 1 OF 11", "10,350 XP" (any part may be NULL) */
void ae_cv_meta(int step, long xp, char *rank, int rank_size, char *tour, int tour_size, char *xp_text, int xp_size);
/* the bar's two captions for a career step and the XP to its next: left "710 XP TO GO" (the last step: "THE LAST RANK"), right
   "NEXT: COMMAND SERGEANT MAJOR" (rank 30 of a tour below the last: "NEXT: TOUR 2"; the last step: "XP KEEPS COUNTING") */
void ae_cv_career_captions(int step, long to_next, char *left, int left_size, char *right, int right_size);
/* the skill bar's captions: left "3 WINS OR TIES TO GO" (1: "1 WIN OR TIE TO GO"; the top rank: "THE TOP RANK"), right "NEXT: RANK 11" */
void ae_cv_skill_captions(int rank, long wins_to_go, char *left, int left_size, char *right, int right_size);
/* the OVERALL ladder's, "NEXT: OVERALL RANK 9" on the right */
void ae_cv_overall_captions(int rank, long wins_to_go, char *left, int left_size, char *right, int right_size);
/* a challenge's "N GAMES TO GO" from the metric (ae_career_challenge_rules.h) and what is left; singular at 1 */
void ae_cv_challenge_to_go(int metric, long left, char *out, int size);

#endif
