/* ae_jump_rules.h: the gametype's JUMP byte and a map layout's jump line */

#include <stdio.h>

#include "ae_jump_rules.h"

static int failures;

#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the stored byte: 0 (old gametypes) and anything out of range play Halo CE's */
	CHECK(ae_jump_percent_of(AE_JUMP_STOCK) == 100);
	CHECK(ae_jump_percent_of(49) == 100 && ae_jump_percent_of(201) == 100 && ae_jump_percent_of(255) == 100);
	CHECK(ae_jump_percent_of(50) == 50 && ae_jump_percent_of(120) == 120 && ae_jump_percent_of(200) == 200);
	/* a map layout's jump line replaces the gametype's, whoever the player */
	CHECK(ae_jump_player_percent(115, 120, 1) == 115 && ae_jump_player_percent(115, 120, 0) == 115);
	CHECK(ae_jump_player_percent(115, AE_JUMP_STOCK, 1) == 115);
	/* else the gametype's, only for a machine that has it */
	CHECK(ae_jump_player_percent(0, 120, 1) == 120 && ae_jump_player_percent(0, 120, 0) == 100);
	CHECK(ae_jump_player_percent(0, AE_JUMP_STOCK, 1) == 100);
	/* another machine's build: Arena Evolved 0.7.3 and later have it; earlier AE, other games, no name: not */
	CHECK(ae_jump_build_has_it("Arena Evolved 0.7.3-beta") && ae_jump_build_has_it("Arena Evolved 0.7.3-beta-dev"));
	CHECK(ae_jump_build_has_it("Arena Evolved 0.7.10") && ae_jump_build_has_it("Arena Evolved 0.8.0-beta") &&
		ae_jump_build_has_it("Arena Evolved 1.0.0"));
	CHECK(!ae_jump_build_has_it("Arena Evolved 0.7.2-beta") && !ae_jump_build_has_it("Arena Evolved 0.6.0-beta") &&
		!ae_jump_build_has_it("Arena Evolved 0.7.2-beta-dev"));
	CHECK(!ae_jump_build_has_it("ChupathingyCE 0.8.1b") && !ae_jump_build_has_it("") && !ae_jump_build_has_it(NULL) &&
		!ae_jump_build_has_it("Arena Evolved ") && !ae_jump_build_has_it("Arena Evolved beta"));
	if (failures)
		return 1;
	printf("ae_jump_rules_test: all passed\n");
	return 0;
}
