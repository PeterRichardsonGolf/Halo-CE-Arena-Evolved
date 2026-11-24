#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ae_fall_stun_rules.h"
#include "ae_variant_ext_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_variant_ext ext, wire;

	/* the rule: ON (Halo CE's lock) by default, OFF drops it; a stray byte keeps the lock */
	CHECK(!ae_fall_stun_off(AE_EXT_FALL_STUN_ON));
	CHECK(ae_fall_stun_off(AE_EXT_FALL_STUN_OFF));
	CHECK(!ae_fall_stun_off(7));

	/* the byte: zero is ON, so a gametype that never set it keeps today's play and needs no block */
	CHECK(offsetof(struct ae_variant_ext, fall_stun) == 39 && sizeof(struct ae_variant_ext) == AE_VARIANT_EXT_SIZE);
	ae_variant_ext_default(&ext);
	CHECK(ext.fall_stun == AE_EXT_FALL_STUN_ON && ae_variant_ext_is_default(&ext));

	/* OFF travels in the block as it is copied host to guest, and survives the guest's sanitising */
	ext.fall_stun = AE_EXT_FALL_STUN_OFF;
	CHECK(!ae_variant_ext_is_default(&ext));
	memcpy(&wire, &ext, sizeof(wire));
	CHECK(ae_variant_ext_sanitize(&wire) && wire.fall_stun == AE_EXT_FALL_STUN_OFF);
	CHECK(ae_fall_stun_off(wire.fall_stun));

	/* out of range falls back to ON */
	wire.fall_stun = 9;
	CHECK(!ae_variant_ext_sanitize(&wire) && wire.fall_stun == AE_EXT_FALL_STUN_ON);

	if (failures)
		return 1;
	printf("ae_fall_stun_rules_test: ok\n");
	return 0;
}
