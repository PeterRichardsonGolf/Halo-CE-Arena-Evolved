#include <stdio.h>
#include "ae_ghost_tune_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the stock multiplayer ghost only, as maps name it */
	CHECK(ae_ghost_tune_is_ghost("vehicles\\ghost\\ghost_mp"));
	CHECK(ae_ghost_tune_is_ghost("Vehicles\\Ghost\\Ghost_MP"));
	CHECK(ae_ghost_tune_is_ghost("vehicles/ghost/ghost_mp"));
	CHECK(!ae_ghost_tune_is_ghost("vehicles\\ghost\\ghost"));
	CHECK(!ae_ghost_tune_is_ghost("vehicles\\ghost\\ghost_mp_x"));
	CHECK(!ae_ghost_tune_is_ghost("vehicles\\ghost\\ghost_m"));
	CHECK(!ae_ghost_tune_is_ghost("ae\\packs\\vehicle_damage\\vehicles\\evolved\\ghost\\_ghost_mp\\ghost_mp"));
	CHECK(!ae_ghost_tune_is_ghost(""));
	CHECK(!ae_ghost_tune_is_ghost(NULL));

	/* the medium numbers (owner, 2026-10-31) on the stock tag: 0.225 -> 0.27, 0.005 -> 0.006, 5-6 -> 8 a second,
	0.5-3 degrees -> 0.25-1.5 */
	CHECK(0.225f * AE_GHOST_TUNE_SPEED_SCALE > 0.2699f && 0.225f * AE_GHOST_TUNE_SPEED_SCALE < 0.2701f);
	CHECK(0.005f * AE_GHOST_TUNE_SPEED_SCALE > 0.00599f && 0.005f * AE_GHOST_TUNE_SPEED_SCALE < 0.00601f);
	CHECK(ae_ghost_tune_rate(6.0f) == 8.0f);
	CHECK(ae_ghost_tune_rate(5.0f) == 8.0f);
	CHECK(ae_ghost_tune_rate(12.0f) == 12.0f);
	CHECK(3.0f * AE_GHOST_TUNE_SPREAD_SCALE == 1.5f && 0.5f * AE_GHOST_TUNE_SPREAD_SCALE == 0.25f);

	if (failures)
		return 1;
	printf("ae_ghost_tune_rules_test: ok\n");
	return 0;
}
