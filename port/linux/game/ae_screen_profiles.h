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
/* (DEV MODE's menu profiles [edit [colours|plain]]) PROFILES opened; edit 1: the first profile's EDIT page over it (a PC
with no profile: one named DEV is made first); 2: and its colours have the focus, on the one after the profile's; 3: and
it is drawn as on a PC with no Spartan model; 4: its EXPORT PROGRESS question over it; 5: its IMPORT PROGRESS files (a
message when the exports folder has none); 6: and the first file's import question */
int ae_screen_profiles_dev_open(int edit);
/* (split screen) its NEW PROFILE page alone, for player 2-4 (owned by that player): the name typed makes a profile the
player then plays with; done(player, 1) once it is made and theirs, done(player, 0) when B leaves without one */
typedef void (*ae_screen_profiles_made)(short local_player, int made, void *context);
int ae_screen_profiles_new_for(short local_player, ae_screen_profiles_made done, void *context);

#endif
