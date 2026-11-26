#include <stdio.h>
#include <string.h>
#include "ae_firefight_doors_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the engine: a host needs the FIREFIGHT lobby or DEV MODE, a client plays the host's game */
	CHECK(!ae_firefight_doors_engine_allowed(0, 0, 0));
	CHECK(ae_firefight_doors_engine_allowed(1, 0, 0));
	CHECK(ae_firefight_doors_engine_allowed(0, 1, 0));
	CHECK(ae_firefight_doors_engine_allowed(1, 1, 0));
	CHECK(ae_firefight_doors_engine_allowed(0, 0, 1));

	/* the engine index: only Firefight's is changed, and only when refused */
	CHECK(ae_firefight_doors_engine_index(7, 7, 5, 0) == 5);
	CHECK(ae_firefight_doors_engine_index(7, 7, 5, 1) == 7);
	CHECK(ae_firefight_doors_engine_index(3, 7, 5, 0) == 3);
	CHECK(ae_firefight_doors_engine_index(0, 7, 5, 0) == 0);

	/* the data build: the FIREFIGHT lobby, or the setting and DEV MODE */
	CHECK(!ae_firefight_doors_pack_build(1, 0, 0));
	CHECK(!ae_firefight_doors_pack_build(0, 1, 0));
	CHECK(!ae_firefight_doors_pack_build(0, 0, 0));
	CHECK(ae_firefight_doors_pack_build(1, 1, 0));
	CHECK(ae_firefight_doors_pack_build(0, 0, 1));

	/* the data's state: written, being written, no campaign map on this PC, not written */
	CHECK(ae_firefight_doors_data_state(4, 1, 0, 0) == AE_FFD_DATA_READY);
	CHECK(ae_firefight_doors_data_state(4, 0, 1, 0) == AE_FFD_DATA_PREPARING);
	CHECK(ae_firefight_doors_data_state(0, 0, 0, 1) == AE_FFD_DATA_NEEDS_MAPS);
	CHECK(ae_firefight_doors_data_state(0, 0, 0, 0) == AE_FFD_DATA_NEEDS_MAPS);
	CHECK(ae_firefight_doors_data_state(3, 0, 0, 1) == AE_FFD_DATA_FAILED);
	CHECK(ae_firefight_doors_data_state(3, 0, 0, 0) == AE_FFD_DATA_PREPARING);
	CHECK(ae_firefight_doors_data_state(2, 1, 1, 0) == AE_FFD_DATA_PREPARING);
	CHECK(ae_firefight_doors_data_startable(AE_FFD_DATA_READY) && !ae_firefight_doors_data_startable(AE_FFD_DATA_PREPARING) &&
		!ae_firefight_doors_data_startable(AE_FFD_DATA_NEEDS_MAPS) && !ae_firefight_doors_data_startable(AE_FFD_DATA_FAILED));
	CHECK(!strcmp(ae_firefight_doors_data_reason(AE_FFD_DATA_NEEDS_MAPS), "NEEDS THE XBOX MAPS"));
	CHECK(!ae_firefight_doors_data_reason(AE_FFD_DATA_READY)[0] && ae_firefight_doors_data_help(AE_FFD_DATA_READY)[0]);
	CHECK(ae_firefight_doors_data_help(AE_FFD_DATA_NEEDS_MAPS)[0] && ae_firefight_doors_data_help(AE_FFD_DATA_FAILED)[0]);

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
