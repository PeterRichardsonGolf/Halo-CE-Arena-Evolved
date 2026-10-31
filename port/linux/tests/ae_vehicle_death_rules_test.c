#include <stdio.h>
#include <string.h>
#include "ae_crouch_rules.h"
#include "ae_melee_unzoom_rules.h"
#include "ae_mouse_aim_rules.h"
#include "ae_vehicle_death_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* game_engine.h's bits beside it: SENSOR RANGE's 6-7, CONTROLLER CROUCH's 8, MELEE UNZOOMS' 9, MOUSE AIM ASSIST's 10,
the port's from 16 */
#define SENSOR_RANGE_MASK 0xC0UL

/* Halo CE's rocket explosion at its centre (weapons\rocket launcher\explosion: 300-330), its frag and plasma
grenades' (120), the tank's shell (351) */
#define ROCKET AE_VEHICLE_DEATH_ROCKET_DAMAGE
#define GRENADE 120.0f
#define SHELL 351.0f

int main(void)
{
	/* VEHICLES DESTRUCTIBLE (seed revision 23): bit 11 of the universal variant's flags; the other bits kept and
	never read as it */
	{
		unsigned long const samples[] = { 0x0UL, 0x3FUL, 0xC0UL, 0x700UL, 0xFFFFFFFFUL, 0x80000001UL, 0x0001FE3FUL,
			0x7FFF0000UL, 0x800UL };
		unsigned int sample;
		int rule;

		for (sample = 0; sample < sizeof(samples) / sizeof(samples[0]); sample++)
		{
			for (rule = 0; rule < NUMBER_OF_AE_VEHICLE_DEATHS; rule++)
			{
				unsigned long flags = ae_vehicle_death_flags_set(samples[sample], rule);

				CHECK(ae_vehicle_death_get(flags) == rule);
				CHECK((flags & ~AE_VEHICLE_DEATH_FLAG) == (samples[sample] & ~AE_VEHICLE_DEATH_FLAG));
				CHECK(ae_controller_crouch_get(flags) == ae_controller_crouch_get(samples[sample]));
				CHECK(ae_melee_unzoom_get(flags) == ae_melee_unzoom_get(samples[sample]));
				CHECK(ae_mouse_aim_assist_get(flags) == ae_mouse_aim_assist_get(samples[sample]));
			}
			CHECK(ae_vehicle_death_get(ae_vehicle_death_flags_set(samples[sample], 2)) == _ae_vehicle_death_off);
			CHECK(ae_vehicle_death_get(ae_vehicle_death_flags_set(samples[sample], -1)) == _ae_vehicle_death_off);
		}
		CHECK(ae_vehicle_death_get(0x0UL) == _ae_vehicle_death_off);
		CHECK(ae_vehicle_death_get(0x3FUL | SENSOR_RANGE_MASK | AE_CONTROLLER_CROUCH_FLAG | AE_MELEE_UNZOOM_FLAG |
			AE_MOUSE_AIM_ASSIST_FLAG) == _ae_vehicle_death_off);
		CHECK(ae_vehicle_death_get(0xFFFFF7FFUL) == _ae_vehicle_death_off);
		CHECK(ae_vehicle_death_get(0x800UL) == _ae_vehicle_death_on);
		CHECK((AE_VEHICLE_DEATH_FLAG & (SENSOR_RANGE_MASK | AE_CONTROLLER_CROUCH_FLAG | AE_MELEE_UNZOOM_FLAG |
			AE_MOUSE_AIM_ASSIST_FLAG)) == 0);
		CHECK(AE_VEHICLE_DEATH_FLAG < 0x10000UL);
		CHECK(!strcmp(ae_vehicle_death_name(_ae_vehicle_death_on), "on"));
		CHECK(!strcmp(ae_vehicle_death_name(_ae_vehicle_death_off), "off"));
	}
	/* the table by tag: the stock multiplayer tags (Xbox and Halo PC names, any case), a map's copy by a word of its
	name, nothing else */
	{
		CHECK(ae_vehicle_death_health_for("vehicles\\warthog\\warthog") == 410.0f);
		CHECK(ae_vehicle_death_health_for("vehicles\\warthog\\mp_warthog") == 410.0f);
		CHECK(ae_vehicle_death_health_for("Vehicles\\RWarthog\\rwarthog") == 410.0f);
		CHECK(ae_vehicle_death_health_for("vehicles\\ghost\\ghost_mp") == 290.0f);
		CHECK(ae_vehicle_death_health_for("vehicles\\banshee\\banshee_mp") == 290.0f);
		CHECK(ae_vehicle_death_health_for("vehicles\\scorpion\\scorpion_mp") == 660.0f);
		CHECK(ae_vehicle_death_health_for("vehicles\\c gun turret\\c gun turret_mp") == 290.0f);
		CHECK(ae_vehicle_death_health_for("levels\\test\\snowhog\\snow_warthog") == 410.0f);
		CHECK(ae_vehicle_death_health_for("vehicles\\pelican\\pelican") == 0.0f);
		CHECK(ae_vehicle_death_health_for("vehicles\\g_warthog\\g_warthog") == 410.0f);
		CHECK(ae_vehicle_death_health_for("vehicles\\h2_ghost\\h2_ghost") == 290.0f);
		/* (a map's wreck pieces and parts: the folder's word is not the tag's) */
		CHECK(ae_vehicle_death_health_for("vehicles\\h2_ghost\\pieces\\dead_hull") == 0.0f);
		CHECK(ae_vehicle_death_health_for("vehicles\\h2_banshee\\pieces\\single_rudder") == 0.0f);
		CHECK(ae_vehicle_death_health_for("vehicles\\hog_tire\\hog_tire") == 0.0f);
		CHECK(ae_vehicle_death_health_for("vehicles/warthog/") == 0.0f);
		CHECK(ae_vehicle_death_health_for("") == 0.0f);
		CHECK(ae_vehicle_death_health_for(NULL) == 0.0f);
	}
	/* the ratios ( 2026-10-22) in Halo CE's rocket at its centre: warthog 2, ghost 1, banshee 1, scorpion
	2-3, turret 1; the rocket at its far end of 330 too; grenades and the shell as they come */
	{
		float rocket;

		for (rocket = 300.0f; rocket <= 330.0f; rocket += 5.0f)
		{
			int scorpion = ae_vehicle_death_hits_to_kill(ae_vehicle_death_health_for("vehicles\\scorpion\\scorpion_mp"),
				rocket);

			CHECK(ae_vehicle_death_hits_to_kill(ae_vehicle_death_health_for("vehicles\\warthog\\warthog"), rocket) == 2);
			CHECK(ae_vehicle_death_hits_to_kill(ae_vehicle_death_health_for("vehicles\\ghost\\ghost_mp"), rocket) == 1);
			CHECK(ae_vehicle_death_hits_to_kill(ae_vehicle_death_health_for("vehicles\\banshee\\banshee_mp"), rocket) ==
				1);
			CHECK(scorpion >= 2 && scorpion <= 3);
			CHECK(ae_vehicle_death_hits_to_kill(ae_vehicle_death_health_for("vehicles\\c gun turret\\c gun turret_mp"),
				rocket) == 1);
		}
		CHECK(ae_vehicle_death_hits_to_kill(410.0f, ROCKET) == 2);
		CHECK(ae_vehicle_death_hits_to_kill(410.0f, GRENADE) == 4);
		CHECK(ae_vehicle_death_hits_to_kill(290.0f, GRENADE) == 3);
		CHECK(ae_vehicle_death_hits_to_kill(290.0f, SHELL) == 1);
		CHECK(ae_vehicle_death_hits_to_kill(410.0f, SHELL) == 2);
		CHECK(ae_vehicle_death_hits_to_kill(410.0f, 0.0f) == 0);
	}
	/* EXPLOSIVES ONLY: OFF keeps the material's multiplier; ON zeroes all but explosives', a rule-mortal vehicle's 0
	as 1, a map's own multipliers kept */
	{
		CHECK(ae_vehicle_death_multiplier(_ae_vehicle_death_off, 1, 0, 0.0f) == 0.0f);
		CHECK(ae_vehicle_death_multiplier(_ae_vehicle_death_off, 0, 0, 0.5f) == 0.5f);
		CHECK(ae_vehicle_death_multiplier(_ae_vehicle_death_on, 1, 0, 0.0f) == 0.0f);
		CHECK(ae_vehicle_death_multiplier(_ae_vehicle_death_on, 1, 0, 1.0f) == 0.0f);
		CHECK(ae_vehicle_death_multiplier(_ae_vehicle_death_on, 0, 0, 1.0f) == 0.0f);
		CHECK(ae_vehicle_death_multiplier(_ae_vehicle_death_on, 1, 1, 0.0f) == 1.0f);
		CHECK(ae_vehicle_death_multiplier(_ae_vehicle_death_on, 1, 1, 0.5f) == 0.5f);
		CHECK(ae_vehicle_death_multiplier(_ae_vehicle_death_on, 0, 1, 0.0f) == 0.0f);
		CHECK(ae_vehicle_death_multiplier(_ae_vehicle_death_on, 0, 1, 2.0f) == 2.0f);
		/* the banshee's "only hurt while occupied" stays but for a vehicle the rule makes mortal */
		CHECK(ae_vehicle_death_only_while_occupied(_ae_vehicle_death_off, 1));
		CHECK(ae_vehicle_death_only_while_occupied(_ae_vehicle_death_on, 0));
		CHECK(!ae_vehicle_death_only_while_occupied(_ae_vehicle_death_on, 1));
	}
	/* NO DEATH WHILE OCCUPIED: holds under ON with a living rider; riders gone, it dies when one died in it, else a
	quarter of its health back (more kept) */
	{
		CHECK(!ae_vehicle_death_holds(_ae_vehicle_death_off, 1));
		CHECK(!ae_vehicle_death_holds(_ae_vehicle_death_on, 0));
		CHECK(ae_vehicle_death_holds(_ae_vehicle_death_on, 1));
		CHECK(ae_vehicle_death_holds(_ae_vehicle_death_on, 4));
		CHECK(ae_vehicle_death_after_riders(1) == _ae_vehicle_death_riders_gone_dies);
		CHECK(ae_vehicle_death_after_riders(0) == _ae_vehicle_death_riders_gone_restored);
		CHECK(ae_vehicle_death_restored_vitality(0.0f) == 0.25f);
		CHECK(ae_vehicle_death_restored_vitality(-0.5f) == 0.25f);
		CHECK(ae_vehicle_death_restored_vitality(0.6f) == 0.6f);
	}
	/* the wreck and the respawn: 10 s; the gametype's respawn, 30 s for a destroyed one where it is never (ON only),
	an intact one never back then */
	{
		CHECK(AE_VEHICLE_DEATH_WRECK_TICKS == 300);
		CHECK(ae_vehicle_death_respawn(_ae_vehicle_death_off, 0) == 0);
		CHECK(ae_vehicle_death_respawn(_ae_vehicle_death_on, 0) == 900);
		CHECK(ae_vehicle_death_respawn(_ae_vehicle_death_on, 1800) == 1800);
		CHECK(ae_vehicle_death_respawn(_ae_vehicle_death_off, 1800) == 1800);
		CHECK(ae_vehicle_death_intact_returns_rule(_ae_vehicle_death_off, 0));
		CHECK(!ae_vehicle_death_intact_returns_rule(_ae_vehicle_death_on, 0));
		CHECK(ae_vehicle_death_intact_returns_rule(_ae_vehicle_death_on, 900));
	}
	if (failures)
		return 1;
	printf("ae_vehicle_death_rules_test: all passed\n");
	return 0;
}
