/*
AE_VEHICLE_BALANCE_RULES.H

What hurts a vehicle under VEHICLES DESTRUCTIBLE ON, and how much: AE's vehicle balance, pure
(port/linux/tests/ae_vehicle_balance_rules_test.c). ae_vehicle_death.c reads it for the AE hooks in damage.c;
ae_vehicle_death_rules.h keeps the rule, the health table and the respawn.

The design (one constants table, here): a vehicle is shot to pieces by the kind of weapon that hits it, not by a
flag. Every damage effect the game knows belongs to a WEAPON CLASS (found by words of its tag's name, so a map's
copy, a pack's rifle or a vehicle's gun all find theirs), and each class does a fixed HIT of damage to a vehicle's
pool, against LIGHT armour (the warthog, ghost, banshee, gun turret) or HEAVY armour (the scorpion, the wraith).
The pool is the vehicle's health in ae_vehicle_death_rules.h (warthog 410, ghost and banshee 290, scorpion 660, gun
turret 290).

THE SCALE. The hits are counted in one unit with the health: the rocket's explosion at its centre is 330 of the
pool, the rocket's own damage (Halo CE, 300-330) taken whole, so a rocket needs the shots it always did (a warthog
2, a ghost, banshee or gun turret 1, a scorpion 2-3). Every other class is set against the rocket: its hit is its
share of a rocket in its time to destroy a vehicle, so the common weapons take: pistol 125 shots on a warthog,
battle rifle 83 rounds (28 bursts), sniper rifle 12 on a warthog (8 on a ghost), shotgun 7 shells on a warthog, a
frag grenade 2, a plasma grenade 3, the tank's shell 2 (3 on a scorpion). Small arms do nothing to the heavy
armour; the sniper rifle too; plasma rifles chip a light one (hundreds of bolts); the vehicles' own guns are what
kills a vehicle besides explosives.

A class's REFERENCE is the damage its Halo CE damage effect does at its source (a pistol round 25, a rifle round
10): the factor handed to the damage is hit / reference, so a hit with a falloff (an explosion's far edge, a
bullet's range) is a part of the hit the same way it is a part of the weapon's own damage. The material modifiers
of the damage effect (0.25 for bullets on metal, 0 for the sniper rifle) are not used on a vehicle the rule makes
mortal: the hit is the whole of it.

Not in the table: a class the table does not know does the old rule's: an explosive counts as the pool damage it is,
anything else none. Melee, fire (the flamethrower), a sticky plasma grenade and a vehicle ramming do none.
*/

#ifndef __AE_VEHICLE_BALANCE_RULES_H
#define __AE_VEHICLE_BALANCE_RULES_H

enum ae_vehicle_balance_armor
{
	_ae_vehicle_balance_armor_light = 0,
	_ae_vehicle_balance_armor_heavy,
	NUMBER_OF_AE_VEHICLE_BALANCE_ARMORS
};

enum ae_vehicle_balance_class
{
	/* not in the table */
	_ae_vehicle_balance_class_unknown = 0,
	_ae_vehicle_balance_class_none,
	_ae_vehicle_balance_class_pistol,
	_ae_vehicle_balance_class_assault_rifle,
	_ae_vehicle_balance_class_smg,
	_ae_vehicle_balance_class_battle_rifle,
	_ae_vehicle_balance_class_carbine,
	_ae_vehicle_balance_class_shotgun,
	_ae_vehicle_balance_class_sniper_rifle,
	_ae_vehicle_balance_class_rocket,
	_ae_vehicle_balance_class_tank_shell,
	_ae_vehicle_balance_class_frag_grenade,
	_ae_vehicle_balance_class_plasma_grenade,
	_ae_vehicle_balance_class_plasma_rifle,
	_ae_vehicle_balance_class_plasma_charged,
	_ae_vehicle_balance_class_plasma_pistol,
	_ae_vehicle_balance_class_needler_shard,
	_ae_vehicle_balance_class_needler_blast,
	_ae_vehicle_balance_class_chaingun_warthog,
	_ae_vehicle_balance_class_chaingun_scorpion,
	_ae_vehicle_balance_class_plasma_bolt_ghost,
	_ae_vehicle_balance_class_plasma_bolt_turret,
	_ae_vehicle_balance_class_fuel_rod,
	_ae_vehicle_balance_class_mortar,
	NUMBER_OF_AE_VEHICLE_BALANCE_CLASSES
};

/* a class: the damage of Halo CE's effect at its source (what the factor divides), and the hit it does to the pool
of a light and a heavy armour (0: nothing) */
struct ae_vehicle_balance_row
{
	char const *name;
	float reference;
	float hit[NUMBER_OF_AE_VEHICLE_BALANCE_ARMORS];
};

/* THE CONSTANTS TABLE (indexed by the class) */
static struct ae_vehicle_balance_row const ae_vehicle_balance_rows[NUMBER_OF_AE_VEHICLE_BALANCE_CLASSES] =
{
	{ "unknown", 0.0f, { 0.0f, 0.0f } },
	/* melee, the flamethrower, a sticky plasma grenade: none */
	{ "none", 1.0f, { 0.0f, 0.0f } },
	{ "pistol", 25.0f, { 3.30f, 0.0f } },
	{ "assault rifle", 10.0f, { 1.96f, 0.0f } },
	{ "smg", 12.0f, { 1.96f, 0.0f } },
	/* (the battle rifle's round of 6.5; the DMR and EVO rifles' rounds are bigger by their damage, one factor) */
	{ "battle rifle", 6.5f, { 4.95f, 0.0f } },
	{ "carbine", 15.2f, { 4.54f, 0.0f } },
	/* (a pellet: 15 to a shell) */
	{ "shotgun", 25.0f, { 4.54f, 0.0f } },
	{ "sniper rifle", 101.0f, { 37.13f, 0.0f } },
	{ "rocket", 330.0f, { 330.0f, 330.0f } },
	{ "tank shell", 351.0f, { 247.5f, 247.5f } },
	{ "frag grenade", 120.0f, { 247.5f, 61.88f } },
	{ "plasma grenade", 120.0f, { 198.0f, 49.5f } },
	{ "plasma rifle", 13.0f, { 1.65f, 0.0f } },
	{ "plasma charged", 70.0f, { 1.65f, 0.0f } },
	{ "plasma pistol", 18.0f, { 1.16f, 0.0f } },
	{ "needler shard", 10.0f, { 0.33f, 0.0f } },
	{ "needler blast", 60.0f, { 82.5f, 20.63f } },
	{ "warthog chaingun", 20.0f, { 11.55f, 5.78f } },
	{ "scorpion chaingun", 10.0f, { 11.55f, 5.78f } },
	{ "ghost and banshee bolt", 12.0f, { 23.1f, 11.55f } },
	{ "gun turret bolt", 11.0f, { 13.2f, 6.6f } },
	{ "fuel rod", 75.0f, { 165.0f, 165.0f } },
	{ "mortar", 100.0f, { 330.0f, 330.0f } },
};

/* the words of a damage effect's tag name that find its class (ASCII case aside, any part of the name; the first
match wins, so the specific before the general) */
struct ae_vehicle_balance_word
{
	char const *word;
	int weapon_class;
};

static struct ae_vehicle_balance_word const ae_vehicle_balance_words[] =
{
	{ "melee", _ae_vehicle_balance_class_none },
	{ "flamethrower", _ae_vehicle_balance_class_none },
	{ "attached", _ae_vehicle_balance_class_none },
	{ "shock wave", _ae_vehicle_balance_class_none },
	{ "rocket launcher", _ae_vehicle_balance_class_rocket },
	{ "shell explosion", _ae_vehicle_balance_class_tank_shell },
	{ "fuel rod", _ae_vehicle_balance_class_fuel_rod },
	{ "wraith\\explosion", _ae_vehicle_balance_class_mortar },
	{ "frag grenade", _ae_vehicle_balance_class_frag_grenade },
	{ "plasma grenade", _ae_vehicle_balance_class_plasma_grenade },
	{ "needler\\explosion", _ae_vehicle_balance_class_needler_blast },
	{ "needler", _ae_vehicle_balance_class_needler_shard },
	{ "charged bolt", _ae_vehicle_balance_class_plasma_charged },
	{ "plasma pistol", _ae_vehicle_balance_class_plasma_pistol },
	{ "plasma rifle", _ae_vehicle_balance_class_plasma_rifle },
	{ "sniper", _ae_vehicle_balance_class_sniper_rifle },
	{ "shotgun", _ae_vehicle_balance_class_shotgun },
	{ "pistol", _ae_vehicle_balance_class_pistol },
	{ "warthog\\bullet", _ae_vehicle_balance_class_chaingun_warthog },
	{ "scorpion\\bullet", _ae_vehicle_balance_class_chaingun_scorpion },
	{ "ghost bolt", _ae_vehicle_balance_class_plasma_bolt_ghost },
	{ "banshee bolt", _ae_vehicle_balance_class_plasma_bolt_ghost },
	{ "gun turret\\bolt", _ae_vehicle_balance_class_plasma_bolt_turret },
	/* (the packs' rifles keep their own names under the pack's folder) */
	{ "battle_rifle", _ae_vehicle_balance_class_battle_rifle },
	{ "dmr", _ae_vehicle_balance_class_battle_rifle },
	{ "carbine", _ae_vehicle_balance_class_carbine },
	{ "smg", _ae_vehicle_balance_class_smg },
	{ "assault rifle", _ae_vehicle_balance_class_assault_rifle },
	{ "assault_rifle", _ae_vehicle_balance_class_smg },
};

static inline int ae_vehicle_balance_lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static inline int ae_vehicle_balance_contains(char const *text, char const *word)
{
	for (; *text; text++)
	{
		char const *t = text;
		char const *w = word;

		while (*t && *w && ae_vehicle_balance_lower((unsigned char)*t) == ae_vehicle_balance_lower((unsigned char)*w))
		{
			t++;
			w++;
		}
		if (*w == 0)
			return 1;
	}
	return 0;
}

/* the class of a damage effect by its tag's name (_unknown: none of the words) */
static inline int ae_vehicle_balance_class_for(char const *tag_name)
{
	unsigned int index;

	if (!tag_name)
		return _ae_vehicle_balance_class_unknown;
	for (index = 0; index < sizeof(ae_vehicle_balance_words) / sizeof(ae_vehicle_balance_words[0]); index++)
	{
		if (ae_vehicle_balance_contains(tag_name, ae_vehicle_balance_words[index].word))
			return ae_vehicle_balance_words[index].weapon_class;
	}
	return _ae_vehicle_balance_class_unknown;
}

/* the armour of a vehicle by its tag's name (its last part): the scorpion and the wraith heavy, any other light */
static inline int ae_vehicle_balance_armor_for(char const *tag_name)
{
	char const *own = tag_name;
	char const *c;

	if (!tag_name)
		return _ae_vehicle_balance_armor_light;
	for (c = tag_name; *c; c++)
	{
		if (*c == '\\' || *c == '/')
			own = c + 1;
	}
	return ae_vehicle_balance_contains(own, "scorpion") || ae_vehicle_balance_contains(own, "wraith") ?
		_ae_vehicle_balance_armor_heavy : _ae_vehicle_balance_armor_light;
}

/* the hit of a class on an armour, in the pool's unit (0 for a class the table does not know) */
static inline float ae_vehicle_balance_hit(int weapon_class, int armor)
{
	if (weapon_class <= _ae_vehicle_balance_class_unknown || weapon_class >= NUMBER_OF_AE_VEHICLE_BALANCE_CLASSES ||
		armor < 0 || armor >= NUMBER_OF_AE_VEHICLE_BALANCE_ARMORS)
	{
		return 0.0f;
	}
	return ae_vehicle_balance_rows[weapon_class].hit[armor];
}

/* the factor the damage of a class is multiplied by on a vehicle the rule makes mortal: its hit over its reference.
A class the table does not know: an explosive counts whole (1), anything else none (0) */
static inline float ae_vehicle_balance_factor(int weapon_class, int armor, int explosive)
{
	if (weapon_class <= _ae_vehicle_balance_class_unknown || weapon_class >= NUMBER_OF_AE_VEHICLE_BALANCE_CLASSES)
		return explosive ? 1.0f : 0.0f;
	return ae_vehicle_balance_hit(weapon_class, armor) / ae_vehicle_balance_rows[weapon_class].reference;
}

/* whole hits of a class that destroy a health (0: it never does; a hit at its source) */
static inline int ae_vehicle_balance_hits_to_destroy(float health, int weapon_class, int armor)
{
	float hit = ae_vehicle_balance_hit(weapon_class, armor);
	int hits = 0;

	if (hit <= 0.0f)
		return 0;
	while (health > 0.0f && hits < 100000)
	{
		health -= hit;
		hits++;
	}
	return hits;
}

#endif
