#include <stdio.h>
#include "ae_hijack_rules.h"
#include "ae_variant_ext_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_variant_ext ext;

	/* who may throw a driver out: OFF nobody, ENEMIES an enemy, ANYONE any other player */
	CHECK(!ae_hijack_allowed(AE_EXT_HIJACK_OFF, 0, 1));
#if AE_HIJACK_SHIPPED
	CHECK(ae_hijack_allowed(AE_EXT_HIJACK_ENEMIES, 0, 1));
	CHECK(!ae_hijack_allowed(AE_EXT_HIJACK_ENEMIES, 0, 0));
	CHECK(ae_hijack_allowed(AE_EXT_HIJACK_ANYONE, 0, 0));
#else
	/* HIJACK is held from 0.8.0: every mode refuses */
	CHECK(!ae_hijack_allowed(AE_EXT_HIJACK_ENEMIES, 0, 1));
	CHECK(!ae_hijack_allowed(AE_EXT_HIJACK_ENEMIES, 0, 0));
	CHECK(!ae_hijack_allowed(AE_EXT_HIJACK_ANYONE, 0, 0));
#endif
	CHECK(!ae_hijack_allowed(AE_EXT_HIJACK_ANYONE, 1, 0));
	CHECK(!ae_hijack_allowed(9, 0, 1));

	/* only a slow vehicle: 3.5 world units a second apart at most */
	CHECK(ae_hijack_speed_ok(0.f));
	CHECK(ae_hijack_speed_ok(3.5f));
	CHECK(!ae_hijack_speed_ok(3.6f));

	/* only with MIXED-TEAM VEHICLES on (or a hijack taking the seat) an enemy riding elsewhere no longer keeps a player out of a free seat */
	CHECK(!ae_hijack_mixed_riders(AE_EXT_MIXED_VEHICLES_OFF, 0));
	CHECK(ae_hijack_mixed_riders(AE_EXT_MIXED_VEHICLES_ON, 0));
	CHECK(ae_hijack_mixed_riders(AE_EXT_MIXED_VEHICLES_OFF, 1));
	CHECK(!ae_hijack_mixed_riders(9, 0));

	/* the thrown driver: out of the seats for 1.5 s; the hijacker's first-person look lasts at most 1.2 s */
	CHECK(AE_HIJACK_LOCKOUT_TICKS == 45);

	/* the block keeps the byte; zero is OFF; out of range is OFF again */
	ae_variant_ext_default(&ext);
	CHECK(ext.hijack == AE_EXT_HIJACK_OFF);
	ext.hijack = AE_EXT_HIJACK_ANYONE;
	CHECK(!ae_variant_ext_is_default(&ext) && ae_variant_ext_sanitize(&ext) && ext.hijack == AE_EXT_HIJACK_ANYONE);
	ext.hijack = AE_EXT_HIJACK_COUNT;
	CHECK(!ae_variant_ext_sanitize(&ext) && ext.hijack == AE_EXT_HIJACK_OFF);

	/* the seat chooser's rank: a driver seat that can be hijacked ranks with the free seats (the nearer door wins), so a
	free passenger seat no longer beats it when the player is at the driver's door (owner 11-08: 'it hopped me into the
	passenger instead'); other seats keep their own state (none 0, occupied 1, available 2) */
	CHECK(ae_hijack_seat_rank(1, 1, 2) == 2);
	CHECK(ae_hijack_seat_rank(1, 0, 2) == 1);
	CHECK(ae_hijack_seat_rank(2, 0, 2) == 2);
	CHECK(ae_hijack_seat_rank(0, 0, 2) == 0);

	ext.mixed_vehicles = AE_EXT_MIXED_VEHICLES_ON;
	CHECK(!ae_variant_ext_is_default(&ext) && ae_variant_ext_sanitize(&ext) && ext.mixed_vehicles == AE_EXT_MIXED_VEHICLES_ON);
	ext.mixed_vehicles = AE_EXT_MIXED_VEHICLES_COUNT;
	CHECK(!ae_variant_ext_sanitize(&ext) && ext.mixed_vehicles == AE_EXT_MIXED_VEHICLES_OFF);

	if (failures)
		return 1;
	printf("ae_hijack_rules: ok\n");
	return 0;
}
