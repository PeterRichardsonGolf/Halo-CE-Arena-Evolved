/* bug 139: what a gametype's mode needs from a map's flags and spawns (port/linux/game/ae_mode_guard_rules.c) */
#include <stdio.h>
#include <string.h>

#include "ae_mode_guard_rules.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_missing(void)
{
	struct ae_mode_guard_map none = { { 0, 0 }, 0, 0, 0 };
	struct ae_mode_guard_map red_only = { { 1, 0 }, 0, 0, 0 };
	struct ae_mode_guard_map full = { { 1, 1 }, 3, 1, 8 };

	/* (Terminal: no flags at all) */
	AE_CHECK(strcmp(ae_mode_guard_missing(AE_MODE_GUARD_CTF, &none), "red CTF flag") == 0);
	AE_CHECK(strcmp(ae_mode_guard_missing(AE_MODE_GUARD_CTF, &red_only), "blue CTF flag") == 0);
	AE_CHECK(ae_mode_guard_missing(AE_MODE_GUARD_ODDBALL, &none) != NULL);
	AE_CHECK(ae_mode_guard_missing(AE_MODE_GUARD_KING, &none) != NULL);
	AE_CHECK(ae_mode_guard_missing(AE_MODE_GUARD_RACE, &none) != NULL);
	/* (Slayer and the stub engines need no flag) */
	AE_CHECK(ae_mode_guard_missing(AE_MODE_GUARD_SLAYER, &none) == NULL);
	AE_CHECK(ae_mode_guard_missing(0, &none) == NULL);
	AE_CHECK(ae_mode_guard_missing(7, &none) == NULL);
	AE_CHECK(ae_mode_guard_missing(AE_MODE_GUARD_CTF, &full) == NULL);
	AE_CHECK(ae_mode_guard_missing(AE_MODE_GUARD_ODDBALL, &full) == NULL);
	AE_CHECK(ae_mode_guard_missing(AE_MODE_GUARD_KING, &full) == NULL);
	AE_CHECK(ae_mode_guard_missing(AE_MODE_GUARD_RACE, &full) == NULL);
	/* (Bomb: its goals are both teams' CTF flags) */
	AE_CHECK(strcmp(ae_mode_guard_missing(AE_MODE_GUARD_BOMB, &none), "red CTF flag") == 0);
	AE_CHECK(strcmp(ae_mode_guard_missing(AE_MODE_GUARD_BOMB, &red_only), "blue CTF flag") == 0);
	AE_CHECK(ae_mode_guard_missing(AE_MODE_GUARD_BOMB, &full) == NULL);
}

static void test_names_and_spawns(void)
{
	AE_CHECK(strcmp(ae_mode_guard_mode_name(AE_MODE_GUARD_CTF), "CTF") == 0);
	AE_CHECK(strcmp(ae_mode_guard_mode_name(AE_MODE_GUARD_KING), "KING OF THE HILL") == 0);
	AE_CHECK(strcmp(ae_mode_guard_mode_name(AE_MODE_GUARD_BOMB), "BOMB") == 0);
	AE_CHECK(strcmp(ae_mode_guard_mode_name(99), "") == 0);
	/* (Gemini in CTF: 16 spawns, none CTF's) */
	AE_CHECK(ae_mode_guard_spawn_anywhere(0, 16));
	AE_CHECK(!ae_mode_guard_spawn_anywhere(4, 16));
	/* (a map with no spawns at all: nothing to fall back to) */
	AE_CHECK(!ae_mode_guard_spawn_anywhere(0, 0));
	/* (the fallback Slayer's score: never the mode's own, KING's 5 minutes taken for 5 kills) */
	AE_CHECK(ae_mode_guard_slayer_score(1) == 50);
	AE_CHECK(ae_mode_guard_slayer_score(0) == 25);
}

int main(void)
{
	test_missing();
	test_names_and_spawns();
	if (failures)
		return 1;
	printf("ae_mode_guard_rules: all passed\n");
	return 0;
}
