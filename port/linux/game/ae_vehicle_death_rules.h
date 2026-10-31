/*
AE_VEHICLE_DEATH_RULES.H

VEHICLES DESTRUCTIBLE, a gametype rule (GAME OPTIONS > VEHICLES; seed revision 23: ON in every AE gametype with
vehicles, the NHE set and the stock gametypes OFF), pure (port/linux/tests/ae_vehicle_death_rules_test.c):
ae_vehicle_death.c reads it for the AE hooks in damage.c and game_engine.c.

- OFF (0, every gametype from before, stock and custom ones, and no game at all): Halo CE's own. The stock
  multiplayer vehicles' tags give them no health, so nothing kills them; a map's own mortal vehicle dies as its
  tags say.
- ON (1) (2026-10-22):
  * EXPLOSIVES ONLY: only damage that detonates explosives (rockets, grenades, the tank's shell, a vehicle's death)
    hurts a vehicle's body; bullets and plasma bolts do not chip it.
  * HEALTH by the vehicle's tag (the table below), for a vehicle whose tag gives it none (the stock multiplayer
    ones), so that Halo CE's rocket kills it: a warthog in 2 rockets, a ghost or a banshee in 1,
    a scorpion in 2 to 3, a gun turret in 1. Its tag's 0 body multipliers count as 1, and the stock banshee's "only
    hurt while occupied" no longer keeps an empty one alive. A map's own mortal vehicle keeps its tag's health.
  * NO DEATH WHILE OCCUPIED ("cannot die with riders"): a vehicle whose health runs out while a living
    rider sits in it holds at 0 instead. Its last rider dying in it destroys it then (credited to the last damage
    it took); its last rider getting out alive gives it back at least a quarter of its health.
  * THE WRECK burns for 10 seconds, then the host clears it; the gametype's vehicle respawn puts a new one on its
    pad, after 30 seconds where the gametype's respawn is "never" (only for a destroyed one: a vehicle left intact
    away from its pad stays, as "never" says).

Where it lives: bit 11 of the universal variant's flags (game_engine.h: the Xbox game's bits are 0-5, SENSOR
RANGE's 6-7, CONTROLLER CROUCH's 8, MELEE UNZOOMS' 9, MOUSE AIM ASSIST's 10, the port's own from 16). The flags
travel with the variant; the host decides (only the host deals damage), and a client takes its word through the
object's dead bit, so older builds, OpenCE and ChupathingyCE clients see the vehicle die (without the explosion).
*/

#ifndef __AE_VEHICLE_DEATH_RULES_H
#define __AE_VEHICLE_DEATH_RULES_H

#define AE_VEHICLE_DEATH_BIT 11
#define AE_VEHICLE_DEATH_FLAG (1UL << AE_VEHICLE_DEATH_BIT)

enum ae_vehicle_death
{
	/* Halo CE's own: the stock multiplayer vehicles never die (every gametype from before) */
	_ae_vehicle_death_off = 0,
	_ae_vehicle_death_on,
	NUMBER_OF_AE_VEHICLE_DEATHS
};

static inline int ae_vehicle_death_get(unsigned long flags)
{
	return (flags & AE_VEHICLE_DEATH_FLAG) ? _ae_vehicle_death_on : _ae_vehicle_death_off;
}

/* the flags with the rule set (the other bits kept; a rule out of the list is OFF) */
static inline unsigned long ae_vehicle_death_flags_set(unsigned long flags, int rule)
{
	return rule == _ae_vehicle_death_on ? flags | AE_VEHICLE_DEATH_FLAG : flags & ~AE_VEHICLE_DEATH_FLAG;
}

/* the rule's name in the log ("AE vehicles destructible: ...") */
static inline char const *ae_vehicle_death_name(int rule)
{
	return rule == _ae_vehicle_death_on ? "on" : "off";
}

enum
{
	/* a wreck burns this long, then the host deletes it (ticks: 30 a second) */
	AE_VEHICLE_DEATH_WRECK_TICKS = 10 * 30,
	/* the respawn of a destroyed vehicle where the gametype's is "never" */
	AE_VEHICLE_DEATH_FALLBACK_RESPAWN_TICKS = 30 * 30,
};

/* Halo CE's rocket explosion at its centre on a vehicle, measured (the spike's box runs: 0.78-0.80 of a 410 pool,
area damage on a warthog): what the table's healths are counted in */
#define AE_VEHICLE_DEATH_ROCKET_DAMAGE 325.0f
/* the health a held vehicle gets back when its last rider gets out alive (at least a quarter) */
#define AE_VEHICLE_DEATH_RIDERS_OUT_VITALITY 0.25f

/* the health the rule gives a vehicle whose tag has none, by its tag. The model vitality (warthog 250, ghost
and banshee 175, scorpion 400, turrets 175) against its rocket (200 at its core) is Halo CE's against its own
(~325): x1.65. The stock multiplayer tags by name first, then a word of the tag's own name, its last part (a map's
copy of a stock vehicle under another folder: g_warthog, h2_ghost; never a folder's word, so a map's wreck pieces,
vehicles\h2_ghost\pieces\dead_hull, stay as their tags make them). Rockets to kill at the centre: warthog 2, ghost 1, banshee 1, scorpion 3 (2 when both land
near its centre), turret 1. */
struct ae_vehicle_death_health
{
	char const *name;
	float health;
};

static struct ae_vehicle_death_health const ae_vehicle_death_tags[] =
{
	/* (the Xbox maps' warthog; Halo PC's is mp_warthog) */
	{ "vehicles\\warthog\\warthog", 410.0f },
	{ "vehicles\\warthog\\mp_warthog", 410.0f },
	{ "vehicles\\rwarthog\\rwarthog", 410.0f },
	{ "vehicles\\ghost\\ghost_mp", 290.0f },
	{ "vehicles\\banshee\\banshee_mp", 290.0f },
	{ "vehicles\\scorpion\\scorpion_mp", 660.0f },
	{ "vehicles\\c gun turret\\c gun turret_mp", 290.0f },
};

static struct ae_vehicle_death_health const ae_vehicle_death_words[] =
{
	/* (the rocket warthog's "rwarthog" too) */
	{ "warthog", 410.0f },
	{ "ghost", 290.0f },
	{ "banshee", 290.0f },
	{ "scorpion", 660.0f },
	{ "turret", 290.0f },
};

static inline int ae_vehicle_death_lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static inline int ae_vehicle_death_same(char const *a, char const *b)
{
	while (*a && *b && ae_vehicle_death_lower((unsigned char)*a) == ae_vehicle_death_lower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return *a == 0 && *b == 0;
}

static inline int ae_vehicle_death_contains(char const *text, char const *word)
{
	for (; *text; text++)
	{
		char const *t = text;
		char const *w = word;

		while (*t && *w && ae_vehicle_death_lower((unsigned char)*t) == ae_vehicle_death_lower((unsigned char)*w))
		{
			t++;
			w++;
		}
		if (*w == 0)
			return 1;
	}
	return 0;
}

/* the table's health for a vehicle tag (ASCII case aside), 0 for none: the vehicle stays as its tag makes it */
static inline float ae_vehicle_death_health_for(char const *tag_name)
{
	unsigned int index;

	if (!tag_name)
		return 0.0f;
	for (index = 0; index < sizeof(ae_vehicle_death_tags) / sizeof(ae_vehicle_death_tags[0]); index++)
	{
		if (ae_vehicle_death_same(tag_name, ae_vehicle_death_tags[index].name))
			return ae_vehicle_death_tags[index].health;
	}
	{
		char const *own = tag_name;
		char const *c;

		for (c = tag_name; *c; c++)
		{
			if (*c == '\\' || *c == '/')
				own = c + 1;
		}
		for (index = 0; index < sizeof(ae_vehicle_death_words) / sizeof(ae_vehicle_death_words[0]); index++)
		{
			if (ae_vehicle_death_contains(own, ae_vehicle_death_words[index].name))
				return ae_vehicle_death_words[index].health;
		}
	}
	return 0.0f;
}

/* how many hits of damage each kill a health (the ratios the tests hold) */
static inline int ae_vehicle_death_hits_to_kill(float health, float damage)
{
	int hits = 0;

	if (damage <= 0.0f)
		return 0;
	while (health > 0.0f && hits < 1000)
	{
		health -= damage;
		hits++;
	}
	return hits;
}

/* a vehicle's body damage multiplier under the rule. mortal_by_rule: its tag has no health (the rule gives it the
table's); explosive: the damage detonates explosives. OFF: the material's own. ON: no damage but explosives', and a
rule-mortal vehicle's 0 multipliers count as 1 */
static inline float ae_vehicle_death_multiplier(int rule, int mortal_by_rule, int explosive, float multiplier)
{
	if (rule != _ae_vehicle_death_on)
		return multiplier;
	if (!explosive)
		return 0.0f;
	if (mortal_by_rule && multiplier <= 0.0f)
		return 1.0f;
	return multiplier;
}

/* whether the tag's "only hurt while occupied" (the stock banshee's) still keeps an empty vehicle unhurt: not for a
vehicle the rule makes mortal */
static inline int ae_vehicle_death_only_while_occupied(int rule, int mortal_by_rule)
{
	return !(rule == _ae_vehicle_death_on && mortal_by_rule);
}

/* object_damage_body, a vehicle whose health just ran out: whether it holds at 0 instead of dying (ON, a living
rider in it) */
static inline int ae_vehicle_death_holds(int rule, int living_riders)
{
	return rule == _ae_vehicle_death_on && living_riders > 0;
}

/* a held vehicle whose riders are all gone: it dies when one of them died in it (the last to go), else it gets
back at least a quarter of its health */
enum ae_vehicle_death_riders_gone
{
	_ae_vehicle_death_riders_gone_dies = 0,
	_ae_vehicle_death_riders_gone_restored,
};

static inline int ae_vehicle_death_after_riders(int a_rider_died)
{
	return a_rider_died ? _ae_vehicle_death_riders_gone_dies : _ae_vehicle_death_riders_gone_restored;
}

static inline float ae_vehicle_death_restored_vitality(float vitality)
{
	return vitality < AE_VEHICLE_DEATH_RIDERS_OUT_VITALITY ? AE_VEHICLE_DEATH_RIDERS_OUT_VITALITY : vitality;
}

/* the vehicle respawn's ticks: the gametype's; where it is "never" (0) and the rule is ON, the fallback (for a
destroyed vehicle only: ae_vehicle_death_intact_returns_rule) */
static inline long ae_vehicle_death_respawn(int rule, long respawn_ticks)
{
	return respawn_ticks <= 0 && rule == _ae_vehicle_death_on ? (long)AE_VEHICLE_DEATH_FALLBACK_RESPAWN_TICKS :
		respawn_ticks;
}

/* whether an intact vehicle left away from its pad goes back by the respawn: always but under the fallback (the
gametype says "never") */
static inline int ae_vehicle_death_intact_returns_rule(int rule, long gametype_respawn_ticks)
{
	return !(gametype_respawn_ticks <= 0 && rule == _ae_vehicle_death_on);
}

#endif
