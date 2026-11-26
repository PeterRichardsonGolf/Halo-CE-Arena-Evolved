/*
AE_SCREEN_SKULLS.H

The SKULLS screen (ae_screen_skulls.c): Content Evolved's 40 skulls as a grid of tiles, turned on and off before a campaign
game (game.skulls, game.skulls_active), with SKULL RULES (ae_screen_skull_rules.h) as the row above. A skull is on at once
when A is pressed; every toggle is written as it is made. The first time it opens, Content Evolved's two questions (how the
skulls are had, and whether dying loses them) are asked as dialogs before the grid is shown.

Its log: "ae skulls: open (<profile>)", "ae skulls: <n> on", "ae skulls: rolled <n>".
*/

#ifndef __AE_SCREEN_SKULLS_H
#define __AE_SCREEN_SKULLS_H

/* opens SKULLS over the stack (the campaign's profile in its crumb); 0 when the stack is full */
int ae_screen_skulls_open(void);
/* Firefight's STARTING SKULLS: the grid on the Firefight copy being edited (ae_glue_setup_options.h), no SKULL RULES row, no questions, no
random roll; the skulls the variant carries show on and stay on. 0 when no Firefight copy is open or the stack is full */
int ae_screen_skulls_open_firefight(void);
/* (DEV MODE `menu skulls [questions]`) opened over the stack; questions: as the first time, with the answers not yet given.
An open one is replaced */
int ae_screen_skulls_open_staged(int questions);

#endif
