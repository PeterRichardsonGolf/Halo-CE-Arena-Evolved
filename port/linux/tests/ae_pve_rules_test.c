/* ae_pve_rules.c: the campaign score, medals, service rank and Firefight stats do nothing in a multiplayer game */
#include <stdio.h>

#include "ae_pve_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int consented;

	/* a multiplayer game (no campaign level, no Firefight): no record, medal, score or rank is written */
	CHECK(ae_pve_rules_records(0, 0) == 0);
	/* and nothing is sent, with the opt-in or without it */
	for (consented = 0; consented < 2; consented++)
		CHECK(ae_pve_rules_stats_upload(0, 0, consented) == 0);
	/* a campaign level or a Firefight game writes its records */
	CHECK(ae_pve_rules_records(1, 0) == 1);
	CHECK(ae_pve_rules_records(0, 1) == 1);
	/* the stats leave only with the opt-in, in those games */
	CHECK(ae_pve_rules_stats_upload(1, 0, 0) == 0);
	CHECK(ae_pve_rules_stats_upload(0, 1, 0) == 0);
	CHECK(ae_pve_rules_stats_upload(1, 0, 1) == 1);
	CHECK(ae_pve_rules_stats_upload(0, 1, 1) == 1);
	/* the loopback socket listens only with the opt-in */
	CHECK(ae_pve_rules_stats_socket(0) == 0);
	CHECK(ae_pve_rules_stats_socket(1) == 1);
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_pve_rules_test: all passed\n");

	return 0;
}
