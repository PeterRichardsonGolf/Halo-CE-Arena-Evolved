#include <stdio.h>
#include <string.h>
#include "ae_vehicle_death_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define WARTHOG "vehicles\\warthog\\warthog"
#define GHOST "vehicles\\ghost\\ghost_mp"
#define BANSHEE "vehicles\\banshee\\banshee_mp"
#define SCORPION "vehicles\\scorpion\\scorpion_mp"
#define TURRET "vehicles\\c gun turret\\c gun turret_mp"

/* the shots a weapon's class takes to destroy a vehicle of a tag (the table's health, the class's hit) */
static int shots(char const *damage_tag, char const *vehicle_tag)
{
	return ae_vehicle_balance_hits_to_destroy(ae_vehicle_death_health_for(vehicle_tag),
		ae_vehicle_balance_class_for(damage_tag), ae_vehicle_balance_armor_for(vehicle_tag));
}

int main(void)
{
	/* the classes by the words of a damage effect's tag name: the stock weapons, the vehicles' guns, the packs' rifles,
	any case */
	{
		CHECK(ae_vehicle_balance_class_for("weapons\\pistol\\bullet") == _ae_vehicle_balance_class_pistol);
		CHECK(ae_vehicle_balance_class_for("weapons\\assault rifle\\bullet") == _ae_vehicle_balance_class_assault_rifle);
		CHECK(ae_vehicle_balance_class_for("weapons\\shotgun\\pellet") == _ae_vehicle_balance_class_shotgun);
		CHECK(ae_vehicle_balance_class_for("weapons\\sniper rifle\\sniper bullet") ==
			_ae_vehicle_balance_class_sniper_rifle);
		CHECK(ae_vehicle_balance_class_for("weapons\\rocket launcher\\explosion") == _ae_vehicle_balance_class_rocket);
		CHECK(ae_vehicle_balance_class_for("Weapons\\Rocket Launcher\\Explosion") == _ae_vehicle_balance_class_rocket);
		CHECK(ae_vehicle_balance_class_for("weapons\\frag grenade\\explosion") == _ae_vehicle_balance_class_frag_grenade);
		CHECK(ae_vehicle_balance_class_for("weapons\\plasma grenade\\explosion") ==
			_ae_vehicle_balance_class_plasma_grenade);
		CHECK(ae_vehicle_balance_class_for("weapons\\plasma rifle\\bolt") == _ae_vehicle_balance_class_plasma_rifle);
		CHECK(ae_vehicle_balance_class_for("weapons\\plasma rifle\\charged bolt") ==
			_ae_vehicle_balance_class_plasma_charged);
		CHECK(ae_vehicle_balance_class_for("weapons\\plasma pistol\\bolt") == _ae_vehicle_balance_class_plasma_pistol);
		CHECK(ae_vehicle_balance_class_for("weapons\\needler\\explosion") == _ae_vehicle_balance_class_needler_blast);
		CHECK(ae_vehicle_balance_class_for("weapons\\needler\\detonation damage") ==
			_ae_vehicle_balance_class_needler_shard);
		CHECK(ae_vehicle_balance_class_for("vehicles\\warthog\\bullet") == _ae_vehicle_balance_class_chaingun_warthog);
		CHECK(ae_vehicle_balance_class_for("vehicles\\scorpion\\bullet") == _ae_vehicle_balance_class_chaingun_scorpion);
		CHECK(ae_vehicle_balance_class_for("vehicles\\scorpion\\shell explosion") == _ae_vehicle_balance_class_tank_shell);
		CHECK(ae_vehicle_balance_class_for("vehicles\\ghost\\ghost bolt") == _ae_vehicle_balance_class_plasma_bolt_ghost);
		CHECK(ae_vehicle_balance_class_for("vehicles\\banshee\\banshee bolt") ==
			_ae_vehicle_balance_class_plasma_bolt_ghost);
		CHECK(ae_vehicle_balance_class_for("vehicles\\banshee\\fuel rod explosion") == _ae_vehicle_balance_class_fuel_rod);
		CHECK(ae_vehicle_balance_class_for("vehicles\\c gun turret\\bolt") == _ae_vehicle_balance_class_plasma_bolt_turret);
		CHECK(ae_vehicle_balance_class_for("vehicles\\wraith\\explosion") == _ae_vehicle_balance_class_mortar);
		/* the packs' rifles under the pack's folder */
		CHECK(ae_vehicle_balance_class_for("ae\\packs\\battle_rifle\\weapons\\evolved\\battle_rifle\\damage_effects\\"
			"battle_rifle_bullet_impact") == _ae_vehicle_balance_class_battle_rifle);
		CHECK(ae_vehicle_balance_class_for("ae\\packs\\dmr\\weapons\\evolved\\dmr\\damage_effects\\dmr_bullet_impact") ==
			_ae_vehicle_balance_class_battle_rifle);
		CHECK(ae_vehicle_balance_class_for("ae\\packs\\evo_rifle\\weapons\\evolved\\dmr\\damage_effects\\"
			"dmr_bullet_impact") == _ae_vehicle_balance_class_battle_rifle);
		CHECK(ae_vehicle_balance_class_for("ae\\packs\\carbine\\weapons\\evolved\\carbine\\damage_effects\\"
			"carbine_bolt_impact") == _ae_vehicle_balance_class_carbine);
		CHECK(ae_vehicle_balance_class_for("ae\\packs\\smg\\weapons\\evolved\\assault_rifle\\damage_effects\\"
			"assault_rifle_bullet") == _ae_vehicle_balance_class_smg);
		CHECK(ae_vehicle_balance_class_for("ae\\packs\\smg_silenced\\weapons\\evolved\\assault_rifle\\damage_effects\\"
			"assault_rifle_bullet") == _ae_vehicle_balance_class_smg);
		/* melee, fire, a sticky grenade and the shock waves do none; anything else is not in the table */
		CHECK(ae_vehicle_balance_class_for("weapons\\pistol\\melee") == _ae_vehicle_balance_class_none);
		CHECK(ae_vehicle_balance_class_for("weapons\\pistol\\melee_response") == _ae_vehicle_balance_class_none);
		CHECK(ae_vehicle_balance_class_for("weapons\\flamethrower\\explosion") == _ae_vehicle_balance_class_none);
		CHECK(ae_vehicle_balance_class_for("weapons\\plasma grenade\\attached") == _ae_vehicle_balance_class_none);
		CHECK(ae_vehicle_balance_class_for("weapons\\frag grenade\\shock wave") == _ae_vehicle_balance_class_none);
		CHECK(ae_vehicle_balance_class_for("vehicles\\scorpion\\shell shock wave") == _ae_vehicle_balance_class_none);
		CHECK(ae_vehicle_balance_class_for("globals\\vehicle_collision") == _ae_vehicle_balance_class_unknown);
		CHECK(ae_vehicle_balance_class_for("") == _ae_vehicle_balance_class_unknown);
		CHECK(ae_vehicle_balance_class_for(NULL) == _ae_vehicle_balance_class_unknown);
	}
	/* the armour: the scorpion and the wraith heavy by the tag's own name, the rest light */
	{
		CHECK(ae_vehicle_balance_armor_for(SCORPION) == _ae_vehicle_balance_armor_heavy);
		CHECK(ae_vehicle_balance_armor_for("vehicles\\wraith\\wraith") == _ae_vehicle_balance_armor_heavy);
		CHECK(ae_vehicle_balance_armor_for(WARTHOG) == _ae_vehicle_balance_armor_light);
		CHECK(ae_vehicle_balance_armor_for(GHOST) == _ae_vehicle_balance_armor_light);
		CHECK(ae_vehicle_balance_armor_for(BANSHEE) == _ae_vehicle_balance_armor_light);
		CHECK(ae_vehicle_balance_armor_for(TURRET) == _ae_vehicle_balance_armor_light);
		CHECK(ae_vehicle_balance_armor_for("vehicles\\scorpion\\pieces\\rudder") == _ae_vehicle_balance_armor_light);
		CHECK(ae_vehicle_balance_armor_for(NULL) == _ae_vehicle_balance_armor_light);
	}
	/* the rocket is the unit: its factor is 1 (its hit is its own damage), so a rocket kills as it always did */
	{
		CHECK(ae_vehicle_balance_factor(_ae_vehicle_balance_class_rocket, _ae_vehicle_balance_armor_light, 1) == 1.0f);
		CHECK(ae_vehicle_balance_factor(_ae_vehicle_balance_class_rocket, _ae_vehicle_balance_armor_heavy, 1) == 1.0f);
		CHECK(shots("weapons\\rocket launcher\\explosion", WARTHOG) == 2);
		CHECK(shots("weapons\\rocket launcher\\explosion", GHOST) == 1);
		CHECK(shots("weapons\\rocket launcher\\explosion", BANSHEE) == 1);
		CHECK(shots("weapons\\rocket launcher\\explosion", TURRET) == 1);
		CHECK(shots("weapons\\rocket launcher\\explosion", SCORPION) == 2);
	}
	/* shots to destroy, per weapon (the design's table: report and TEST RUN vehicle-balance-1 hold these) */
	{
		CHECK(shots("weapons\\pistol\\bullet", WARTHOG) == 125);
		CHECK(shots("weapons\\pistol\\bullet", GHOST) == 88);
		CHECK(shots("weapons\\assault rifle\\bullet", WARTHOG) == 210);
		CHECK(shots("ae\\packs\\battle_rifle\\x\\battle_rifle_bullet_impact", WARTHOG) == 83);
		CHECK(shots("ae\\packs\\battle_rifle\\x\\battle_rifle_bullet_impact", GHOST) == 59);
		CHECK(shots("weapons\\shotgun\\pellet", WARTHOG) == 91);
		CHECK(shots("weapons\\sniper rifle\\sniper bullet", WARTHOG) == 12);
		CHECK(shots("weapons\\sniper rifle\\sniper bullet", GHOST) == 8);
		CHECK(shots("weapons\\sniper rifle\\sniper bullet", BANSHEE) == 8);
		CHECK(shots("weapons\\frag grenade\\explosion", WARTHOG) == 2);
		CHECK(shots("weapons\\frag grenade\\explosion", GHOST) == 2);
		CHECK(shots("weapons\\plasma grenade\\explosion", WARTHOG) == 3);
		CHECK(shots("weapons\\plasma grenade\\explosion", GHOST) == 2);
		CHECK(shots("vehicles\\scorpion\\shell explosion", WARTHOG) == 2);
		CHECK(shots("vehicles\\scorpion\\shell explosion", GHOST) == 2);
		CHECK(shots("vehicles\\scorpion\\shell explosion", SCORPION) == 3);
		CHECK(shots("vehicles\\warthog\\bullet", WARTHOG) == 36);
		CHECK(shots("vehicles\\warthog\\bullet", GHOST) == 26);
		CHECK(shots("vehicles\\ghost\\ghost bolt", WARTHOG) == 18);
		CHECK(shots("vehicles\\ghost\\ghost bolt", GHOST) == 13);
		CHECK(shots("weapons\\plasma rifle\\bolt", WARTHOG) == 249);
		CHECK(shots("ae\\packs\\carbine\\x\\carbine_bolt_impact", WARTHOG) == 91);
	}
	/* the heavy armour: small arms, plasma and the sniper rifle do nothing; the tank's guns, vehicles' guns and
	explosives do (less for the small blasts) */
	{
		CHECK(shots("weapons\\pistol\\bullet", SCORPION) == 0);
		CHECK(shots("weapons\\assault rifle\\bullet", SCORPION) == 0);
		CHECK(shots("weapons\\sniper rifle\\sniper bullet", SCORPION) == 0);
		CHECK(shots("weapons\\plasma rifle\\bolt", SCORPION) == 0);
		CHECK(shots("ae\\packs\\dmr\\x\\dmr_bullet_impact", SCORPION) == 0);
		CHECK(shots("weapons\\shotgun\\pellet", SCORPION) == 0);
		CHECK(shots("vehicles\\warthog\\bullet", SCORPION) == 115);
		CHECK(shots("weapons\\frag grenade\\explosion", SCORPION) == 11);
		CHECK(shots("weapons\\plasma grenade\\explosion", SCORPION) == 14);
		CHECK(shots("weapons\\flamethrower\\explosion", WARTHOG) == 0);
		CHECK(shots("weapons\\plasma grenade\\attached", WARTHOG) == 0);
		CHECK(shots("weapons\\pistol\\melee", WARTHOG) == 0);
	}
	/* the pack rifles are one factor by their damage: the DMR's and EVO RIFLE's round (17.875) does 2.75 battle rifle
	rounds' damage (6.5) */
	{
		float factor = ae_vehicle_balance_factor(_ae_vehicle_balance_class_battle_rifle, _ae_vehicle_balance_armor_light,
			0);

		CHECK(factor > 0.76f && factor < 0.763f);
		CHECK(ae_vehicle_death_hits_to_kill(410.0f, 17.875f * factor) == 31);
	}
	/* a class the table does not know: an explosive counts whole, anything else none; the table is whole */
	{
		unsigned int row;

		CHECK(ae_vehicle_balance_factor(_ae_vehicle_balance_class_unknown, _ae_vehicle_balance_armor_light, 1) == 1.0f);
		CHECK(ae_vehicle_balance_factor(_ae_vehicle_balance_class_unknown, _ae_vehicle_balance_armor_light, 0) == 0.0f);
		CHECK(ae_vehicle_balance_factor(99, _ae_vehicle_balance_armor_light, 1) == 1.0f);
		CHECK(ae_vehicle_balance_factor(_ae_vehicle_balance_class_none, _ae_vehicle_balance_armor_light, 1) == 0.0f);
		CHECK(ae_vehicle_balance_hit(_ae_vehicle_balance_class_rocket, 7) == 0.0f);
		CHECK(ae_vehicle_balance_hit(-1, 0) == 0.0f);
		for (row = 1; row < NUMBER_OF_AE_VEHICLE_BALANCE_CLASSES; row++)
		{
			CHECK(ae_vehicle_balance_rows[row].reference > 0.0f);
			CHECK(ae_vehicle_balance_rows[row].name != NULL);
			/* the heavy armour never takes more than the light */
			CHECK(ae_vehicle_balance_rows[row].hit[_ae_vehicle_balance_armor_heavy] <=
				ae_vehicle_balance_rows[row].hit[_ae_vehicle_balance_armor_light]);
			CHECK(ae_vehicle_balance_rows[row].hit[0] >= 0.0f);
		}
		/* every word finds a class of the table */
		for (row = 0; row < sizeof(ae_vehicle_balance_words) / sizeof(ae_vehicle_balance_words[0]); row++)
		{
			CHECK(ae_vehicle_balance_words[row].weapon_class > _ae_vehicle_balance_class_unknown);
			CHECK(ae_vehicle_balance_words[row].weapon_class < NUMBER_OF_AE_VEHICLE_BALANCE_CLASSES);
		}
	}
	/* the rule: OFF never changes the damage; ON, a vehicle the rule makes mortal takes the class's factor (bullets
	now hurt it), a map's own mortal vehicle the old explosives-only rule */
	{
		int rocket = _ae_vehicle_balance_class_rocket;
		int pistol = _ae_vehicle_balance_class_pistol;
		int light = _ae_vehicle_balance_armor_light;

		CHECK(ae_vehicle_death_balance_multiplier(_ae_vehicle_death_off, 1, 0, 0.0f, pistol, light) == 0.0f);
		CHECK(ae_vehicle_death_balance_multiplier(_ae_vehicle_death_off, 1, 1, 0.7f, rocket, light) == 0.7f);
		CHECK(ae_vehicle_death_balance_multiplier(_ae_vehicle_death_on, 1, 0, 0.0f, pistol, light) > 0.0f);
		CHECK(ae_vehicle_death_balance_multiplier(_ae_vehicle_death_on, 1, 0, 0.0f, pistol, light) < 0.2f);
		CHECK(ae_vehicle_death_balance_multiplier(_ae_vehicle_death_on, 1, 1, 0.0f, rocket, light) == 1.0f);
		CHECK(ae_vehicle_death_balance_multiplier(_ae_vehicle_death_on, 1, 0, 1.0f, _ae_vehicle_balance_class_none,
			light) == 0.0f);
		CHECK(ae_vehicle_death_balance_multiplier(_ae_vehicle_death_on, 0, 0, 1.0f, pistol, light) == 0.0f);
		CHECK(ae_vehicle_death_balance_multiplier(_ae_vehicle_death_on, 0, 1, 2.0f, rocket, light) == 2.0f);
		/* (the stock bullet's metal modifier is dropped on the mortal vehicle, kept elsewhere) */
		CHECK(ae_vehicle_death_balance_modifier(_ae_vehicle_death_on, 1, 0.25f) == 1.0f);
		CHECK(ae_vehicle_death_balance_modifier(_ae_vehicle_death_on, 1, 0.0f) == 1.0f);
		CHECK(ae_vehicle_death_balance_modifier(_ae_vehicle_death_on, 0, 0.25f) == 0.25f);
		CHECK(ae_vehicle_death_balance_modifier(_ae_vehicle_death_off, 1, 0.25f) == 0.25f);
	}
	if (failures)
		return 1;
	printf("ae_vehicle_balance_rules_test: all passed\n");
	return 0;
}
