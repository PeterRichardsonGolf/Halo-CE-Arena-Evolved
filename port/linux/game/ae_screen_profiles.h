/*
AE_SCREEN_PROFILES.H

AE's PROFILES screen (ae_screen_profiles.c; the 0.3.7 menus): the profiles on this PC, which one player 1 plays
with, a profile's colour and controls edited (saved at once), a new profile made, one deleted. Opened from AE's main
menu.
*/

#ifndef __AE_SCREEN_PROFILES_H
#define __AE_SCREEN_PROFILES_H

/* opens PROFILES over the stack (the profiles read now); 0 when the stack is full */
int ae_screen_profiles_open(void);

#endif
