/*
AE_TEST_RUN_SETUP.H

TEST RUN (DEV MODE): puts a test's scenario up in the running game, with no restart. A map test is its map,
gametype, `set` config values and local players, started as debug.network_test does (network_test_restart makes it
read its settings again); a screen test is an AE menu screen (debug.ae_test_screen, opened again). A map test names its gametype for network_test twice: in debug.network_test
("local:<map>:<gametype>", built-in names) and in debug.network_test_gametype (a saved or seeded one, "AE SLAYER").

Undo puts back the exact value every key had before the last apply changed it (the test's `set` keys and the
debug.network_test, debug.network_test_local_players and debug.ae_test_screen keys the setup itself uses).
*/

#ifndef AE_TEST_RUN_SETUP_H
#define AE_TEST_RUN_SETUP_H

#include "ae_test_run_list.h"

/* (network_test.c) forgets the settings read and the state; the next network_test_update reads config again and
starts again */
void network_test_restart(void);

/* undoes the last apply, then sets the test up. 1 = setup started (poll ae_test_run_setup_ready); 0 = nothing changed
and ae_test_run_setup_error says why (an unknown `set` key, a screen test with AE's menus off). The test is const, so
the error is not written into test->error */
int ae_test_run_setup_apply(struct ae_test_run_test const *test);
/* the last apply's error text, "" when it started */
const char *ae_test_run_setup_error(void);
/* the scenario is playable: a map test's map is loaded and its game running past the pre-game countdown with its local
players spawned, a screen test's screen is open; for a short while (the fade) after that it still says 0. 0 with no
apply pending. Call it once a frame; it logs "test run setup ready <test id>" once */
int ae_test_run_setup_ready(void);
/* puts back every config value the last apply changed (the game is left where it is) */
void ae_test_run_setup_undo(void);

#endif
