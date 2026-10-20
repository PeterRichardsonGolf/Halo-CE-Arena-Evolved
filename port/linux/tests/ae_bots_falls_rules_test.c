#include <stdio.h>
#include <string.h>
#include "ae_bots_falls_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the cause of a death: nothing known is other */
	CHECK(ae_bots_death_cause(0, "weapons\\frag grenade\\explosion", 1, 0, 0, 0) == AE_BOTS_DEATH_OTHER);
	/* another player's damage, whatever it was */
	CHECK(ae_bots_death_cause(1, "weapons\\pistol\\bullet", 0, 0, 0, 0) == AE_BOTS_DEATH_PLAYER);
	CHECK(ae_bots_death_cause(1, "weapons\\frag grenade\\explosion", 0, 0, 0, 0) == AE_BOTS_DEATH_PLAYER);
	/* its own grenades, frag and plasma (stuck: "attached"), its own rockets and fuel rods */
	CHECK(ae_bots_death_cause(1, "weapons\\frag grenade\\explosion", 1, 0, 0, 0) == AE_BOTS_DEATH_OWN_GRENADE);
	CHECK(ae_bots_death_cause(1, "weapons\\plasma grenade\\attached", 1, 0, 0, 0) == AE_BOTS_DEATH_OWN_GRENADE);
	CHECK(ae_bots_death_cause(1, "weapons\\Plasma Grenade\\explosion", 1, 0, 0, 0) == AE_BOTS_DEATH_OWN_GRENADE);
	CHECK(ae_bots_death_cause(1, "weapons\\rocket launcher\\explosion", 1, 0, 0, 0) == AE_BOTS_DEATH_OWN_EXPLOSIVE);
	CHECK(ae_bots_death_cause(1, "vehicles\\ghost\\ghost bolt", 1, 0, 0, 0) == AE_BOTS_DEATH_SELF);
	/* the globals' damages: a fall past the deadliest distance, a landing, the map's kill (at once, no owner) */
	CHECK(ae_bots_death_cause(1, "globals\\distance", 0, 1, 0, 0) == AE_BOTS_DEATH_PIT);
	CHECK(ae_bots_death_cause(1, "globals\\falling", 0, 1, 0, 0) == AE_BOTS_DEATH_FALL);
	CHECK(ae_bots_death_cause(1, "globals\\falling", 0, 1, 1, 0) == AE_BOTS_DEATH_BARRIER);
	/* (a rider dies with its vehicle: not the map's kill) */
	CHECK(ae_bots_death_cause(1, "globals\\falling", 0, 1, 1, 1) == AE_BOTS_DEATH_OTHER);
	/* (only the tag's last part is read: a folder named "distance" is not a pit) */
	CHECK(ae_bots_death_cause(1, "distance\\collision", 0, 1, 0, 0) == AE_BOTS_DEATH_OTHER);
	CHECK(ae_bots_death_cause(1, NULL, 0, 1, 0, 0) == AE_BOTS_DEATH_OTHER);

	/* the words of the log */
	CHECK(!strcmp(ae_bots_death_cause_name(AE_BOTS_DEATH_PIT), "pit"));
	CHECK(!strcmp(ae_bots_death_cause_name(AE_BOTS_DEATH_OWN_GRENADE), "own_grenade"));
	CHECK(!strcmp(ae_bots_death_cause_name(-1), "other"));
	CHECK(!strcmp(ae_bots_death_cause_name(AE_BOTS_DEATH_CAUSES), "other"));

	if (failures)
		printf("%d failed\n", failures);
	else
		printf("ae_bots_falls_rules: all passed\n");
	return failures != 0;
}
