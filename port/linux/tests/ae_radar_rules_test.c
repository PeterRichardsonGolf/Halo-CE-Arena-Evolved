#include <stdio.h>
#include "ae_radar_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* ENEMIES in a team game hides allies only: enemies and empty vehicles show */
static void test_team_game(void)
{
	CHECK(ae_radar_rules_shows(1, 1, 1, 0, AE_RADAR_CONTACT_ENEMY));
	CHECK(!ae_radar_rules_shows(1, 1, 1, 0, AE_RADAR_CONTACT_ALLY));
	CHECK(ae_radar_rules_shows(1, 1, 1, 0, AE_RADAR_CONTACT_EMPTY_VEHICLE));
}

/* every other value, a free for all game, the shared scan (no viewer) and the names over heads: as before */
static void test_unchanged(void)
{
	int contact;

	for (contact = AE_RADAR_CONTACT_ENEMY; contact <= AE_RADAR_CONTACT_EMPTY_VEHICLE; contact++)
	{
		CHECK(ae_radar_rules_shows(0, 1, 1, 0, (enum ae_radar_contact)contact));
		CHECK(ae_radar_rules_shows(1, 0, 1, 0, (enum ae_radar_contact)contact));
		CHECK(ae_radar_rules_shows(1, 1, 0, 0, (enum ae_radar_contact)contact));
		CHECK(ae_radar_rules_shows(1, 1, 1, 1, (enum ae_radar_contact)contact));
	}
}

int main(void)
{
	test_team_game();
	test_unchanged();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
