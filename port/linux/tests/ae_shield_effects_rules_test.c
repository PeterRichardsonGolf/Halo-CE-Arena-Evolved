#include <stdio.h>
#include <string.h>
#include "ae_shield_effects_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* own(network_server, accepts_remote): AE's OFFLINE lobby (a server nobody can join) draws the player's own; a LAN
	or online host and a client draw the host's; a game that is no network game is the player's own already */
	CHECK(ae_shield_effects_own(1, 0) == 1);
	CHECK(ae_shield_effects_own(1, 1) == 0);
	CHECK(ae_shield_effects_own(0, 1) == 0);
	CHECK(ae_shield_effects_own(0, 0) == 0);

	/* due(shield, charging, now, last): over 1 or charging, at once and then once a second */
	CHECK(ae_arm_shield_log_due(2.99f, 0, 5000, 0) == 1);
	CHECK(ae_arm_shield_log_due(2.99f, 0, 5999, 5000) == 0);
	CHECK(ae_arm_shield_log_due(2.95f, 0, 6000, 5000) == 1);
	CHECK(ae_arm_shield_log_due(0.80f, 1, 6000, 5000) == 1);
	CHECK(ae_arm_shield_log_due(1.00f, 0, 9000, 0) == 0);
	CHECK(ae_arm_shield_log_due(0.50f, 0, 9000, 5000) == 0);
	/* (the clock wrapping, at its own width: still once a second) */
	CHECK(ae_arm_shield_log_due(2.0f, 0, 400UL, (unsigned long)-256) == 0);
	CHECK(ae_arm_shield_log_due(2.0f, 0, 1000UL, (unsigned long)-256) == 1);

	/* names, in network_shield_effects.h's order; NONE: each machine's own */
	CHECK(!strcmp(ae_shield_effects_name(0), "stock") && !strcmp(ae_shield_effects_name(3), "overshield"));
	CHECK(!strcmp(ae_shield_effects_name(4), "glow_overshield") && !strcmp(ae_shield_effects_name(6), "players"));
	CHECK(!strcmp(ae_shield_effects_name(-1), "players' own") && !strcmp(ae_shield_effects_name(9), "?"));

	if (failures)
		return 1;
	printf("ae_shield_effects_rules_test: all passed\n");
	return 0;
}
