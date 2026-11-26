/*
AE_TEST_START.H

A campaign level's start point for TEST RUN tests: the game state of the level at the first moment the player has
control after the intro, kept in the player's profile folder as ae-test-start-<level>-<variant>.bin (a saved game's file,
beside the player's own savegame.bin, never over it) with ae-test-start-<level>-<variant>.bin.stamp, the program file's
size and time it was taken by. The level's next test load reads it through the engine's saved game load (main_new_map:
game_state_try_and_load_from_persistent_storage), so the level opens at first control, not after the ride. A program
that is not the one the file was taken by does not read it: the level plays its intro and takes the file again.

The file name is the level's saved game file only while armed (ae_saved_game_file.c asks), from a test's load to its
setup being ready: the pause menu's QUIT in a test keeps writing savegame.bin.
*/

#ifndef __AE_TEST_START_H
#define __AE_TEST_START_H

/* arms the start point of a level (the test's map name) for the settings that shape the level as it loads (a number the
caller made of them: the swapped weapons, the randomizer): 1 when a file made by this program is there to be read, 0 when
the level's game will make one */
int ae_test_start_arm(const char *level, unsigned long variant);

/* the start file's name while armed, else NULL */
const char *ae_test_start_file(void);

/* the file may be read: it is there, of a saved game's size, made by this program */
int ae_test_start_readable(void);

/* a saved game was taken by the engine (ae_campaign_score_saved_game_taken) */
void ae_test_start_note_loaded(void);

/* how many saved games the engine has taken in this run */
unsigned long ae_test_start_loads(void);

/* writes the game state now as the armed level's start point and the stamp beside it: 1 done */
int ae_test_start_capture(void);

/* the file's name is the level's saved game file no more */
void ae_test_start_disarm(void);

#endif
