#include <stdio.h>
#include <string.h>
#include "ae_vehicle_pack_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the kinds by a word of the tag's own name: the stock multiplayer vehicles (Xbox and Halo PC names), a map's
	copies under other folders; the scorpion and the turret have no look (the core's basic death), nor a map's wreck
	pieces filed under a vehicle's folder */
	{
		static struct
		{
			char const *name;
			int kind;
		} const samples[] =
		{
			{ "vehicles\\warthog\\warthog", _ae_vehicle_pack_warthog },
			{ "vehicles\\warthog\\mp_warthog", _ae_vehicle_pack_warthog },
			{ "vehicles\\rwarthog\\rwarthog", _ae_vehicle_pack_warthog },
			{ "levels\\test\\x\\G_Warthog", _ae_vehicle_pack_warthog },
			{ "vehicles\\ghost\\ghost_mp", _ae_vehicle_pack_ghost },
			{ "vehicles/h2_ghost/h2_ghost", _ae_vehicle_pack_ghost },
			{ "vehicles\\banshee\\banshee_mp", _ae_vehicle_pack_banshee },
			{ "vehicles\\scorpion\\scorpion_mp", _ae_vehicle_pack_none },
			{ "vehicles\\c gun turret\\c gun turret_mp", _ae_vehicle_pack_none },
			{ "vehicles\\h2_ghost\\pieces\\dead_hull", _ae_vehicle_pack_none },
			{ "vehicles\\h2_banshee\\pieces\\single_rudder", _ae_vehicle_pack_none },
			{ "", _ae_vehicle_pack_none },
		};
		unsigned int index;

		for (index = 0; index < sizeof(samples) / sizeof(samples[0]); index++)
			CHECK(ae_vehicle_pack_kind_for(samples[index].name) == samples[index].kind);
		CHECK(ae_vehicle_pack_kind_for(NULL) == _ae_vehicle_pack_none);
	}

	/* every kind's look: a damage effect, a loop and an explosion; the warthog and the banshee leave a wreck, the
	ghost its burning body; TSC:E's thresholds */
	{
		int kind;

		for (kind = _ae_vehicle_pack_none + 1; kind < NUMBER_OF_AE_VEHICLE_PACK_KINDS; kind++)
		{
			struct ae_vehicle_pack_look const *look = &ae_vehicle_pack_looks[kind];

			CHECK(look->word && look->damaged && look->damaged_loop && look->death);
			CHECK(look->damaged_fraction > 0.3f && look->damaged_fraction <= 0.5f);
		}
		CHECK(ae_vehicle_pack_looks[_ae_vehicle_pack_warthog].wreck != NULL);
		CHECK(ae_vehicle_pack_looks[_ae_vehicle_pack_banshee].wreck != NULL);
		CHECK(ae_vehicle_pack_looks[_ae_vehicle_pack_ghost].wreck == NULL);
		CHECK(ae_vehicle_pack_looks[_ae_vehicle_pack_none].death == NULL);
	}

	/* the pack's names: its prefix and CMT's name without "cmt\" (as the pack's builder names its own tags) */
	{
		char name[256];
		char tiny[24];

		CHECK(ae_vehicle_pack_name(ae_vehicle_pack_looks[_ae_vehicle_pack_warthog].wreck, name, sizeof(name)));
		CHECK(!strcmp(name, "ae\\packs\\vehicle_damage\\vehicles\\_shared\\warthog\\scenery\\"
			"warthog_chaingun_damaged_smoking"));
		CHECK(!ae_vehicle_pack_name(ae_vehicle_pack_looks[_ae_vehicle_pack_warthog].wreck, tiny, sizeof(tiny)));
		CHECK(tiny[0] == 0);
		CHECK(!ae_vehicle_pack_name(NULL, name, sizeof(name)) && name[0] == 0);
		CHECK(!ae_vehicle_pack_name("x", NULL, 0));
	}

	/* damaged: health given (a rule-mortal vehicle gets its maximum on its first hit; a client never knows it),
	alive, at its kind's fraction or below. A warthog after one rocket (410 - 325 = 85 of 410) is damaged, a fresh
	one is not; a held one at 0 with riders is; a dead one, and one of no kind, never */
	{
		float rocket = AE_VEHICLE_DEATH_ROCKET_DAMAGE;
		float warthog = ae_vehicle_death_health_for("vehicles\\warthog\\warthog");

		CHECK(!ae_vehicle_pack_damaged(_ae_vehicle_pack_warthog, warthog, 1.0f, 0));
		CHECK(ae_vehicle_pack_damaged(_ae_vehicle_pack_warthog, warthog, (warthog - rocket) / warthog, 0));
		CHECK(!ae_vehicle_pack_damaged(_ae_vehicle_pack_warthog, warthog, 0.5f, 0));
		CHECK(ae_vehicle_pack_damaged(_ae_vehicle_pack_warthog, warthog, 0.0f, 0));
		CHECK(!ae_vehicle_pack_damaged(_ae_vehicle_pack_warthog, warthog, 0.0f, 1));
		CHECK(!ae_vehicle_pack_damaged(_ae_vehicle_pack_warthog, 0.0f, 0.0f, 0));
		CHECK(ae_vehicle_pack_damaged(_ae_vehicle_pack_ghost, 290.0f, 0.5f, 0));
		CHECK(!ae_vehicle_pack_damaged(_ae_vehicle_pack_ghost, 290.0f, 0.51f, 0));
		CHECK(!ae_vehicle_pack_damaged(_ae_vehicle_pack_none, 290.0f, 0.0f, 0));
		CHECK(!ae_vehicle_pack_damaged(NUMBER_OF_AE_VEHICLE_PACK_KINDS, 290.0f, 0.0f, 0));
	}

	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_vehicle_pack_rules_test: ok\n");
	return 0;
}
