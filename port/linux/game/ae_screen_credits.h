/*
AE_SCREEN_CREDITS.H

Arena Evolved's CREDITS page (0.9.4, the TAB menu's): who made what, the upstream projects and their licences, the
libraries and the rule that the game needs the player's own Halo files, as a scrolling text of headed sections. The text
is credits.txt beside the game (port/assets/credits.txt; ae_credits_rules.h has its format), a built-in short one when the
file is missing. Up / Down or the wheel scroll, the page keys (LT / RT, Page Up / Page Down) a page; B / Esc / right
click go back to the TAB menu. Its log: "ae credits: open (<source>, <n> blocks)", "ae credits: closed".
*/

#ifndef __AE_SCREEN_CREDITS_H
#define __AE_SCREEN_CREDITS_H

/* over the main menu, for that local player (the TAB menu opens again on B); 0 when the stack is full */
int ae_screen_credits_open(short local_player);

#endif
