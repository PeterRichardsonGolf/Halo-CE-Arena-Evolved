/*
AE_SHIELD_POP_RULES.H

When COMBAT FEEDBACK's early shield pop shows: "a pop when one headshot will finish", reckoned for the weapon in use.
No engine includes: the unit test in port/linux/tests/ae_shield_pop_rules_test.c compiles these alone.

The rule (the owner's TEST RUN of 2026-11-09): the pop shows as the shield left falls to or under the least of the
finishing points (25, the magnum's) and one hit of the weapon in use against the shield, with a small margin (the
magnum's exact 25 left pops). A weapon whose rounds carry nothing past a breaking shield (the EVO RIFLE's no_spill)
never pops early: its pop is the shield's own break. Damage that takes no head off (the AR, the SMGs, plasma, melee)
and the burst rifles (three rounds a pull, finished by ae_shield_fx_rifle_finish's raise) keep the finishing points.
*/

#ifndef __AE_SHIELD_POP_RULES_H
#define __AE_SHIELD_POP_RULES_H

/* what kind of hit the pop is reckoned for */
enum ae_shield_pop_hit
{
	/* damage that cannot kill with a headshot (the AR, the SMGs, plasma, melee, grenades): the finishing points */
	_ae_shield_pop_hit_plain = 0,
	/* damage that kills with a headshot (the magnum, the sidekick, the DMR): one more of its own hits must finish */
	_ae_shield_pop_hit_headshot,
	/* a burst weapon's round (three a pull: the BATTLE RIFLE): the finishing points, as before */
	_ae_shield_pop_hit_burst,
	/* a round that carries nothing past a breaking shield (the EVO RIFLE): never early, the pop is the break */
	_ae_shield_pop_hit_no_spill,
};

/* the kind of a hit: headshots, its damage can kill with a headshot; profile, it is a round of a weapon the burst
driver plays (an AE rifle profile), with that profile's no_spill and rounds_a_pull (unused without a profile) */
enum ae_shield_pop_hit ae_shield_pop_hit_kind(int headshots, int profile, int no_spill, int rounds_a_pull);

/* the shield points left at or under which the pop shows for a hit of that kind whose own damage against the shield
is hit_shield_damage: the finishing points, the least of them and the hit for a headshot weapon, 0 for no_spill (the
break only) */
float ae_shield_pop_points(enum ae_shield_pop_hit kind, float finisher_points, float hit_shield_damage);

/* whether the pop shows with shield_left points left: at or under the points, with margin for the float rounding of
the shield (the host's own, or the guest's as it was sent) */
int ae_shield_pop_shows(float shield_left, float points, float margin);

/* (a guest's view) whether the pop fires now: the unit has just come down for the view (was_down not, down so) AND its
shield fell since the last frame. The point a guest's pop shows at follows the weapon its own player holds, so a switch
of weapons (a death, a respawn, a pickup) can bring a damaged unit 'down' with no hit: that is no pop, the view only
takes the new state */
int ae_shield_pop_fires(int was_down, int down, float shield_before, float shield_now);

#endif
