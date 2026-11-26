/*
AE_FALL_STUN_RULES.H

FALL STUN, a gametype rule (the gametype's byte is ae_variant_ext's fall_stun, AE_EXT_FALL_STUN_*). Pure
(port/linux/tests/ae_fall_stun_rules_test.c).
- ON: Halo CE's hard landing: the player cannot move or jump until the landing's recovery time has run out.
- OFF: a hard landing is played as a soft one: the same landing animation and sound, no lock on movement or jump.
Fall damage is its own rule (FALL DAMAGE) and is not touched.
*/

#ifndef AE_FALL_STUN_RULES_H
#define AE_FALL_STUN_RULES_H

/* whether a hard landing's lock on movement and jump is dropped under the rule byte */
int ae_fall_stun_off(int rule);

#endif
