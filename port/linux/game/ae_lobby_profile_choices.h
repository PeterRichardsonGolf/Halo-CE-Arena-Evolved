/* ae_lobby_profile_choices.h: the profile choices for a player joining AE's lobby. Pure. */

#ifndef __AE_LOBBY_PROFILE_CHOICES_H
#define __AE_LOBBY_PROFILE_CHOICES_H

/* (choice ids beside the profiles' indices) GUEST, and NEW PROFILE (the inline cards': a profile named and made
there) */
enum { AE_LOBBY_PROFILE_GUEST = -1, AE_LOBBY_PROFILE_NEW = -2 };

/* Saved profiles used by another local player are left out. GUEST is always the last choice. focus is today's
automatic choice: current when it is available and not a guest, else GUEST. */
short ae_lobby_profile_choices(int const *profiles, short profile_count, int const *used, short used_count,
	int current, int current_guest, int *choices, short maximum, short *focus);

/* Several pads choose at once (the inline cards), each card's choices rebuilt every frame from what the others hold:
the focus after a rebuild is the choice with the id it was on (GUEST is -1) while it is still there; one that another
pad took from under it leaves the focus in the same place, on the choice that moved up into it (never past the last,
GUEST). A card with no choice has focus 0. */
short ae_lobby_profile_refocus(int const *choices, short count, int focused, short previous);

#endif
