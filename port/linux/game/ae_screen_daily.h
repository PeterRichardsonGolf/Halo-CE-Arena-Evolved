/*
AE_SCREEN_DAILY.H

The DAILY CHALLENGE screen and its PAST DAYS list (ae_screen_daily.c). A mission a day with the randomizer and skulls set, the same
for everyone: the day's rows on the left, the day's board on the right. The day is this PC's (Content Evolved's offline mode:
the challenge rolled here from the day, the board this PC's profiles', nothing sent) unless COMMUNITY SERVICES and ONLINE DAY AND
BOARDS are on, then the community server's. The main menu's row shows with SETTINGS > CAMPAIGN > DAILY CHALLENGE on.
*/

#ifndef __AE_SCREEN_DAILY_H
#define __AE_SCREEN_DAILY_H

/* opens DAILY CHALLENGE over the stack, on today; 0 when the stack is full */
int ae_screen_daily_open(void);
/* once, as the main menu opens: a challenge's level was just won here, so the screen comes back on its day (a random run's: today) */
void ae_screen_daily_after_win(void);
/* whether the main menu shows its DAILY CHALLENGE row (game.daily_challenge) */
int ae_screen_daily_row_shown(void);

/* (DEV MODE `menu daily [today|past|online|unreached|unreached-custom|loading|empty|closed]`) a made-up day, with nothing asked of any server: today's, the
past days' list, an online day, a server not reached, a request waiting, a day nobody has played, a closed day */
int ae_screen_daily_open_staged(const char *scene);

#endif
