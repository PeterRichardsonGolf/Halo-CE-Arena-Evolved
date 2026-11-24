/*
AE_CROUCH_AIR_RULES.H

A crouched jump's clips (owner 2026-11-11: "they should have there weapons at ready like the rest of weapons when they
crouch jump"), as a pure rule (no engine includes: port/linux/tests/ae_crouch_air_rules_test.c compiles it alone).
The stock player graph gives a crouched pistol's AIRBORNE the unarmed body's clip ("stand unarmed airborne": the arms
drop, the gun hangs) and its LAND SOFT / LAND HARD the rocket launcher's. A crouch seat's airborne or land clip whose name
is not its own seat's and weapon class's ("crouch <class> ...") is wrong; the stand seat's clip of the same class takes
its place (ae_crouch_air.c), so the weapon stays at the ready. A class whose crouch clips are its own plays as before.
*/

#ifndef __AE_CROUCH_AIR_RULES_H
#define __AE_CROUCH_AIR_RULES_H

/* whether a crouch seat clip's name belongs to its weapon class: "crouch <class_label> ..." (case ignored) */
int ae_crouch_air_name_is_own(const char *class_label, const char *animation_name);
/* whether the seat (by label) is the crouch seat whose airborne / land clips this rule checks */
int ae_crouch_air_seat(const char *seat_label);

#endif
