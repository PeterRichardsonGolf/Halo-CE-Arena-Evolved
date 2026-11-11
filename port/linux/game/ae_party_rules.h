/*
AE_PARTY_RULES.H

Arena Evolved's PARTY manager (menus phase 1, spec §8): B is two gestures. A tap (released within 250 ms) is Back. A
hold on a guest's card arms after 250 ms and removes the guest at 1050 ms (0.8 s of the REMOVE chip filling); let go
before that and nothing happens (no Back either). Player 1's card (and any item that is no guest's) cannot be removed: a
hold there arms nothing, and its release is Back whatever its length (a slow tap stays a tap). Pure
(port/linux/tests/ae_party_rules_test.c); the screen is ae_screen_party.c.
*/

#ifndef __AE_PARTY_RULES_H
#define __AE_PARTY_RULES_H

enum ae_b_gesture { AE_B_NONE, AE_B_BACK, AE_B_HOLDING, AE_B_REMOVE };

#define AE_B_TAP_MS 250ul
#define AE_B_HOLD_MS 800ul

/* B held down_ms so far (still_down: not let go yet); can_remove: the focused card is a guest's */
int ae_b_gesture_step(unsigned long down_ms, int still_down, int can_remove);
/* the REMOVE chip's fill: 0 until 250 ms, then linear to 1 at 1050 ms */
float ae_b_hold_fill(unsigned long down_ms);

#endif
