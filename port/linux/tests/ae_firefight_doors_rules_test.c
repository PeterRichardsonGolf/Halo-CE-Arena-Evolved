#include <stdio.h>
#include "ae_firefight_doors_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the engine: a host needs DEV MODE, a client plays the host's game */
	CHECK(!ae_firefight_doors_engine_allowed(0, 0));
	CHECK(ae_firefight_doors_engine_allowed(1, 0));
	CHECK(ae_firefight_doors_engine_allowed(0, 1));
	CHECK(ae_firefight_doors_engine_allowed(1, 1));

	/* the engine index: only Firefight's is changed, and only when refused */
	CHECK(ae_firefight_doors_engine_index(7, 7, 5, 0) == 5);
	CHECK(ae_firefight_doors_engine_index(7, 7, 5, 1) == 7);
	CHECK(ae_firefight_doors_engine_index(3, 7, 5, 0) == 3);
	CHECK(ae_firefight_doors_engine_index(0, 7, 5, 0) == 0);

	/* the data build: the setting and DEV MODE */
	CHECK(!ae_firefight_doors_pack_build(1, 0));
	CHECK(!ae_firefight_doors_pack_build(0, 1));
	CHECK(!ae_firefight_doors_pack_build(0, 0));
	CHECK(ae_firefight_doors_pack_build(1, 1));

	/* the skulls' messages: only with the skulls on */
	CHECK(!ae_firefight_doors_skulls_send(0));
	CHECK(ae_firefight_doors_skulls_send(1));

	/* the skulls' data pack, written before the unlock question is answered: only with the skulls on or DEV MODE on */
	CHECK(!ae_firefight_doors_skulls_pack_early(0, 0));
	CHECK(ae_firefight_doors_skulls_pack_early(1, 0));
	CHECK(ae_firefight_doors_skulls_pack_early(0, 1));

	if (failures)
		return 1;
	printf("ae_firefight_doors_rules_test: ok\n");
	return 0;
}
