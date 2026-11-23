#include <stdio.h>
#include <string.h>

#include "ae_record_step_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void add(struct ae_service_record *record, char const *key, unsigned long seen)
{
	struct ae_sr_player *player = &record->player[record->player_count++];

	snprintf(player->key, sizeof(player->key), "%s", key);
	snprintf(player->name, sizeof(player->name), "%s", key);
	player->seen = seen;
}

int main(void)
{
	static struct ae_service_record record;
	int order[AE_SR_PLAYERS], t;

	ae_sr_init(&record);
	/* nothing kept: nothing to step to */
	CHECK(ae_rstep_order(&record, "", order) == 0);
	CHECK(ae_rstep_target(&record, "", "goat", 1) == -1);
	/* one player: no step from it, a step from a name not kept reaches it */
	add(&record, "goat", 5);
	CHECK(ae_rstep_target(&record, "goat", "goat", 1) == -1 && ae_rstep_target(&record, "goat", "goat", -1) == -1);
	CHECK(ae_rstep_target(&record, "goat", "nobody", 1) == 0);
	/* the viewer first, then the most recently seen; a tie keeps the record's order */
	add(&record, "tiger", 9);
	add(&record, "wolf", 2);
	add(&record, "ox", 9);
	CHECK(ae_rstep_order(&record, "wolf", order) == 4);
	CHECK(order[0] == 2 && order[1] == 1 && order[2] == 3 && order[3] == 0);
	CHECK(ae_rstep_order(&record, "", order) == 4 && order[0] == 1 && order[1] == 3 && order[2] == 0 && order[3] == 2);
	/* next and previous wrap at the ends */
	t = ae_rstep_target(&record, "wolf", "wolf", 1);
	CHECK(t == 1);
	CHECK(ae_rstep_target(&record, "wolf", "goat", 1) == 2);
	CHECK(ae_rstep_target(&record, "wolf", "wolf", -1) == 0);
	CHECK(ae_rstep_target(&record, "wolf", "tiger", -1) == 2);
	/* a name not kept: next goes to the first, previous to the last */
	CHECK(ae_rstep_target(&record, "wolf", "nobody", 1) == 2);
	CHECK(ae_rstep_target(&record, "wolf", "nobody", -1) == 0);
	/* null names behave as empty */
	CHECK(ae_rstep_target(&record, NULL, NULL, 1) == 1);
	if (failures)
		return 1;
	printf("ae_record_step_rules_test: ok\n");
	return 0;
}
