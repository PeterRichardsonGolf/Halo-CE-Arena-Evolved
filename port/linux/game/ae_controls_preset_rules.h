/*
AE_CONTROLS_PRESET_RULES.H

The Xbox menus' pause CONTROLS screen (controls_screen.c) and the game's button presets: the profile names one of the
game's presets (player_profile.h, _button_preset_standard 0 .. _button_preset_custom AE_CONTROLS_GAME_CUSTOM), while the
screen's layouts (controller_layout.c) are the Xbox's five, BUMPER JUMPER and CUSTOM. These pure functions map the two
(port/linux/tests/ae_controls_preset_rules_test.c; tools/test_controller_layout.py checks the numbers and names against
player_profile.h and the profiles screen).

- The game's presets 0 .. 5 are controller_layout.c's 0 .. 5; the game's custom is CONTROLLER_PRESET_CUSTOM, both ways.
- A game preset controller_layout.c has no table for (FISHSTICKS .. GREEN FINGERS) is no layout: the screen keeps it
  as the profile has it and shows its name.
- BUTTON LAYOUT's left and right step over the known layouts only, in the game's order, wrapping: from an unknown one
  to the next known one either way. Never a number outside the known ones.
*/

#ifndef AE_CONTROLS_PRESET_RULES_H
#define AE_CONTROLS_PRESET_RULES_H

/* the game's presets (player_profile.h): their count, and the custom one's number (the last) */
#define AE_CONTROLS_GAME_PRESETS 16
#define AE_CONTROLS_GAME_CUSTOM 15

/* controller_layout.c's layout of a game preset, -1 for one it has no table for */
int ae_controls_layout_of(int game_preset);

/* the game's preset of a controller_layout.c layout, -1 for none */
int ae_controls_game_preset_of(int layout);

/* the known game preset after (direction > 0) or before game_preset, wrapping */
int ae_controls_step(int game_preset, int direction);

/* a game preset's name, as the profiles screen names it ("DEFAULT" for a number out of range) */
char const *ae_controls_preset_title(int game_preset);

#endif /* AE_CONTROLS_PRESET_RULES_H */
