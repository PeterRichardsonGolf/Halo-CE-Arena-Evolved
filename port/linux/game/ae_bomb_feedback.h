/*
AE_BOMB_FEEDBACK.H

SEARCH AND DESTROY's pickup and drop feedback (2026-10-28; the approved mockup,
plan notes/plans/2026-10-28-ae-snd-bomb.md behaviour 6): the attackers hear the bomb pack's keypad
chirp-chirp and read "You picked up the bomb" / "<name> has the bomb" / "Bomb dropped by <name>"; the defenders hear and read nothing;
a dropped bomb lands with its thud where it lies. Every machine sees the carrier change from the host's state and says
it to its own local players. A drop that is a plant or a round's end says nothing. The carried tile's animation is
ae_bomb_hud.c's. Included after cseries.h.
*/

#ifndef __AE_BOMB_FEEDBACK_H
#define __AE_BOMB_FEEDBACK_H

/* each tick, every machine (ae_vehicle_death_update, beside ae_bomb_pack_update) */
void ae_bomb_feedback_update(void);

#endif
