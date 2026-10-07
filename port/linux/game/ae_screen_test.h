/*
AE_SCREEN_TEST.H

The AE menus' test screen (ae_screen_test.c): a 40-row list in the menus'
look, for the headless tests and contact sheets (debug.ae_test_screen).
*/

#ifndef __AE_SCREEN_TEST_H
#define __AE_SCREEN_TEST_H

/* opens it over the game's menus, drawn into 1, 2 or 4 views (2 and 4: split-screen views, synthetic, to see the
layout at their sizes; the first takes the pointer); 0 if the stack is full */
int ae_screen_test_open(int views);

#endif
