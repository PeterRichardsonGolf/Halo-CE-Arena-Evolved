/*
AE_SCREEN_SKULL_RULES.H

The SKULL RULES screen (ae_screen_skull_rules.c): how the skulls are had (game.skulls_unlock_mode and its keys), whether dying
loses them, how many a level's enemies drop and how likely, the RANDOM roll's two limits, and RESET SKULL PROGRESS (with
its own dialog). Opened from SKULLS' first row.
*/

#ifndef __AE_SCREEN_SKULL_RULES_H
#define __AE_SCREEN_SKULL_RULES_H

int ae_screen_skull_rules_open(void);
/* (DEV MODE `menu skullrules [reset]`) opened over the stack; reset: the forget dialog open at once */
int ae_screen_skull_rules_open_staged(int reset);

#endif
