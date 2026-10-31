/*
AE_LAYOUT_NUMBERS.H

The button layouts' numbers a profile keeps (player_profile.h's _button_preset_*), moved once for 0.5.0. 0.4.x
numbered, after BUMPER JUMPER (5): the Master Chief Collection's RECON 6, HERO 7, THE DUKE 8, ANNIVERSARY 9,
RECLAIMER 10, ZOOM & SHOOT 11, BUMP & JUMP 12, GREEN FINGERS 13, CUSTOM 14. 0.5.0 takes OpenCE's FISHSTICKS 6 and
RECON 7 (which is the Master Chief Collection's Hero), and Arena Evolved's own follow from RECON 8 to GREEN FINGERS
14, CUSTOM 15. ae_glue_profiles.c moves every profile of the save folder once (ae_profiles_layouts_move). Pure C,
for the unit test (port/linux/tests/ae_layout_numbers_test.c).
*/

#ifndef AE_LAYOUT_NUMBERS_H
#define AE_LAYOUT_NUMBERS_H

/* a 0.4.x profile's layout number in 0.5.0's numbers (one past the known ones: as it is) */
int ae_layout_number_from_04x(int layout);

#endif
