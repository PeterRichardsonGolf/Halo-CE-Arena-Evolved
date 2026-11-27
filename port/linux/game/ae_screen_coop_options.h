/*
AE_SCREEN_COOP_OPTIONS.H

The CO-OP OPTIONS screen (ae_screen_coop_options.c): Content Evolved's options of a co-op game hosted online (network.coop_*):
friendly fire, ally friendly fire, player collisions, what a team wipe does, and the extra enemies, pickups and vehicles with
their growth. They act only in a game this PC hosts online; alone and in split screen the mission plays as the campaign has
it, and the screen says so. X puts every option back to its default.
*/

#ifndef __AE_SCREEN_COOP_OPTIONS_H
#define __AE_SCREEN_COOP_OPTIONS_H

/* opens CO-OP OPTIONS over the stack; crumb_head is the crumb's first words ("CAMPAIGN" or "CO-OP LOBBY") */
int ae_screen_coop_options_open(const char *crumb_head);
/* whether every option reads its default (the lobby row's STANDARD) */
int ae_screen_coop_options_standard(void);
/* (DEV MODE `menu coopoptions [changed]`) opened over the stack; changed: a few options set away from the defaults first */
int ae_screen_coop_options_open_staged(int changed);

#endif
