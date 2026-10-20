/*
AE_BOTS_FALLS_RULES.H

Arena Evolved: the bots' deaths by their own hand, as pure rules (port/linux/tests/ae_bots_falls_rules_test.c runs
them). The owner, 2026-10-20: "can you have a helper work on tuning bots for suicide and fall deaths. i find that is
what makes our bots still look like bots."

- THE CAUSE OF A DEATH (the fight log's ev=death, ae_bots_events.inc), from the last damage the bot's unit took: its
  damage effect's tag name, its owner and whether it killed at once. AE's gametypes have NO FALLING DAMAGE, so a
  landing never hurts there; a fall past the globals' maximum falling distance still kills (bipeds.c, the "distance"
  damage while falling faster than its speed), and so does the map's own kill (unit_kill, an act of god: a script's
  kill volume, a telefrag), dealt as the "falling" damage with kill instantly and no owner.
*/

#ifndef __AE_BOTS_FALLS_RULES_H
#define __AE_BOTS_FALLS_RULES_H

enum
{
	/* no damage known, or none of the below (a vehicle's end, a corpse's) */
	AE_BOTS_DEATH_OTHER = 0,
	/* another player's damage */
	AE_BOTS_DEATH_PLAYER,
	/* a landing's damage (gametypes with falling damage) */
	AE_BOTS_DEATH_FALL,
	/* a fall past the maximum falling distance: off a roof, into a pit, out of the map */
	AE_BOTS_DEATH_PIT,
	/* the map's kill: a kill volume or death barrier, a telefrag */
	AE_BOTS_DEATH_BARRIER,
	/* its own grenade */
	AE_BOTS_DEATH_OWN_GRENADE,
	/* its own rocket, fuel rod or other explosion */
	AE_BOTS_DEATH_OWN_EXPLOSIVE,
	/* its own damage of another kind */
	AE_BOTS_DEATH_SELF,
	AE_BOTS_DEATH_CAUSES
};

/* the cause's word in the fight log */
char const *ae_bots_death_cause_name(int cause);

/* the cause of a death from its last damage: known (any damage seen since the unit spawned), the damage effect's tag
name (NULL for none), whether the damage owner is the bot's own player, whether it is no player, whether the damage
killed at once (an act of god), and whether the unit rode a vehicle */
int ae_bots_death_cause(int known, char const *damage_name, int owner_self, int owner_none, int kill_instantly,
	int riding);

#endif
