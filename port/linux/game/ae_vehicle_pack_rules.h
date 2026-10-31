/*
AE_VEHICLE_PACK_RULES.H

The vehicle damage pack's look under VEHICLES DESTRUCTIBLE (ae_vehicle_death_rules.h), pure
(port/linux/tests/ae_vehicle_pack_rules_test.c): ae_vehicle_pack.c reads it.

The pack (packs\ae_vehicle_damage.aepack, tools/weapon_pack/vehicle_damage/: CMT's TSC:E 1.4.1c damage effects
and wrecks, credited in the pack) is a pack of tags only: the game finds its tags by name, each the pack's prefix
and CMT's name without its leading "cmt\". It is added to a multiplayer map with vehicles when the game about to
be played has the rule ON (ae_weapon_pack.c). Each machine shows what it has: a machine without the pack (or a
stock client) sees the core's basic death (the map's explosion, the dead vehicle, cleared after 10 seconds).

The look of a vehicle the table knows (by a word of its tag's own name, as the core's health table: warthog,
ghost, banshee; the scorpion and the gun turret keep the basic death):
- DAMAGED, once its health is at its kind's fraction or below (TSC:E's own thresholds: the warthog's 100 of 225,
  the ghost's 50 of 100, the banshee's 35 of 75): a damage effect once (sparks, smoke, the warthog's glass
  breaking) and a loop (smoke and fire) while it lasts. Health is the host's (and a local game's): a client never
  knows it, and shows no damaged look.
- DEATH: the pack's explosion beside the map's (the map's deals the damage, with the pack or without: the pack's
  explosions deal none), parts coming off (the warthog's hood, the ghost's wings), and the WRECK: the warthog's and
  the banshee's burning wreck model stands in for the dead vehicle (which is hidden, the wreck its child: it goes
  with it, cleared with it), the ghost keeps its body, burning (the damaged loop) until it is cleared.
*/

#ifndef __AE_VEHICLE_PACK_RULES_H
#define __AE_VEHICLE_PACK_RULES_H

#include "ae_vehicle_death_rules.h"

#define AE_VEHICLE_PACK_ID "ae_vehicle_damage"
#define AE_VEHICLE_PACK_PREFIX "ae\\packs\\vehicle_damage\\"

enum ae_vehicle_pack_kind
{
	_ae_vehicle_pack_none = 0,
	_ae_vehicle_pack_warthog,
	_ae_vehicle_pack_ghost,
	_ae_vehicle_pack_banshee,
	NUMBER_OF_AE_VEHICLE_PACK_KINDS
};

enum
{
	AE_VEHICLE_PACK_DEATH_EXTRAS = 2,
};

/* a kind's look: the pack's tags by their names without AE_VEHICLE_PACK_PREFIX (NULL: none) */
struct ae_vehicle_pack_look
{
	char const *word;
	/* the health fraction at or below which it is damaged */
	float damaged_fraction;
	char const *damaged;
	char const *damaged_extra;
	char const *damaged_loop;
	char const *death;
	char const *death_extras[AE_VEHICLE_PACK_DEATH_EXTRAS];
	/* a scenery: the wreck that stands in for the dead vehicle (NULL: the dead vehicle stays, burning) */
	char const *wreck;
};

static struct ae_vehicle_pack_look const ae_vehicle_pack_looks[NUMBER_OF_AE_VEHICLE_PACK_KINDS] =
{
	{ NULL, 0.0f, NULL, NULL, NULL, NULL, { NULL, NULL }, NULL },
	{
		"warthog", 100.0f / 225.0f,
		"vehicles\\_shared\\warthog\\effects\\warthog_body_damaged",
		"vehicles\\_shared\\warthog\\effects\\warthog_glass_destroyed",
		"vehicles\\_shared\\warthog\\effects\\warthog_damaged_loop",
		"vehicles\\_shared\\warthog\\_warthog_chaingun\\effects\\warthog_chaingun_body_depleted",
		{ "vehicles\\_shared\\warthog\\_warthog_chaingun\\effects\\warthog_chaingun_hood_destroyed", NULL },
		"vehicles\\_shared\\warthog\\scenery\\warthog_chaingun_damaged_smoking",
	},
	{
		"ghost", 50.0f / 100.0f,
		"vehicles\\_shared\\ghost\\effects\\ghost_body_damaged",
		NULL,
		"vehicles\\_shared\\ghost\\effects\\ghost_damaged_loop",
		"vehicles\\evolved\\ghost\\effects\\ghost_body_depleted",
		{ "vehicles\\_shared\\ghost\\effects\\ghost_left_wing_destroyed",
			"vehicles\\_shared\\ghost\\effects\\ghost_right_wing_destroyed" },
		NULL,
	},
	{
		"banshee", 35.0f / 75.0f,
		"vehicles\\_shared\\banshee\\effects\\banshee_body_damaged",
		NULL,
		"vehicles\\_shared\\banshee\\effects\\banshee_damaged_loop",
		"vehicles\\evolved\\banshee\\effects\\banshee_body_depleted",
		{ NULL, NULL },
		"vehicles\\_shared\\banshee\\scenery\\banshee_damaged_smoking",
	},
};

/* the kind of a vehicle by its tag's name (ASCII case aside): a word of its own name, its last part, as the core's
health table reads it (a map's wreck pieces filed under a vehicle's folder are none); the rocket warthog is a
warthog */
static inline int ae_vehicle_pack_kind_for(char const *tag_name)
{
	char const *own = tag_name;
	char const *c;
	int kind;

	if (!tag_name)
		return _ae_vehicle_pack_none;
	for (c = tag_name; *c; c++)
	{
		if (*c == '\\' || *c == '/')
			own = c + 1;
	}
	for (kind = _ae_vehicle_pack_none + 1; kind < NUMBER_OF_AE_VEHICLE_PACK_KINDS; kind++)
	{
		if (ae_vehicle_death_contains(own, ae_vehicle_pack_looks[kind].word))
			return kind;
	}
	return _ae_vehicle_pack_none;
}

/* the pack's name of a tag of a look (the prefix and the look's name) written to name (size bytes): nonzero if it
fits, zero (name empty) for none or one too long */
static inline int ae_vehicle_pack_name(char const *look_name, char *name, unsigned int size)
{
	char const *parts[2];
	unsigned int length = 0;
	unsigned int part;

	if (!name || !size)
		return 0;
	name[0] = 0;
	if (!look_name)
		return 0;
	parts[0] = AE_VEHICLE_PACK_PREFIX;
	parts[1] = look_name;
	for (part = 0; part < 2; part++)
	{
		char const *c;

		for (c = parts[part]; *c; c++)
		{
			if (length + 1 >= size)
			{
				name[0] = 0;
				return 0;
			}
			name[length++] = *c;
		}
	}
	name[length] = 0;
	return 1;
}

/* whether a vehicle of a kind looks damaged: it has health (a maximum the rule or its tag gave it), is alive, and
its health fraction is at its kind's fraction or below */
static inline int ae_vehicle_pack_damaged(int kind, float maximum, float fraction, int dead)
{
	if (kind <= _ae_vehicle_pack_none || kind >= NUMBER_OF_AE_VEHICLE_PACK_KINDS || dead || maximum <= 0.0f)
		return 0;
	return fraction <= ae_vehicle_pack_looks[kind].damaged_fraction;
}

#endif
