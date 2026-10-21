/*
AE_CROUCH_RULES.H

CONTROLLER CROUCH, a gametype rule (ARENA OPTIONS; seed revision 21: STAYS DOWN in every AE gametype, the NHE set
keeps HALO CE), pure (port/linux/tests/ae_crouch_rules_test.c): ae_crouch.c reads it for player_control.c's AE hook.

- HALO CE (0, every gametype from before, stock and custom ones, and no game at all): Halo's own crouch. A grounded
  player whose stick is pushed 0.98 or more of the way stands up, even with crouch held.
- STAYS DOWN (1): a controller's held crouch keeps the player crouched with the stick pushed all the way, as the
  keyboard's crouch key always does (input_abstraction_port_crouch; the keyboard is not this rule's).

Where it lives: bit 8 of the universal variant's flags (game_engine.h: the Xbox game's bits are 0-5, SENSOR RANGE's
6-7, the port's own from 16; nothing else reads 8-15). The flags travel with the variant, so every machine in a game
plays the host's rule, split screen players too; older builds, OpenCE and ChupathingyCE ignore the bit and play
HALO CE.
*/

#ifndef __AE_CROUCH_RULES_H
#define __AE_CROUCH_RULES_H

#define AE_CONTROLLER_CROUCH_BIT 8
#define AE_CONTROLLER_CROUCH_FLAG (1UL << AE_CONTROLLER_CROUCH_BIT)

enum ae_controller_crouch
{
	/* Halo's own: full stick stands you up (every gametype from before) */
	_ae_controller_crouch_halo_ce = 0,
	_ae_controller_crouch_stays_down,
	NUMBER_OF_AE_CONTROLLER_CROUCHES
};

static inline int ae_controller_crouch_get(unsigned long flags)
{
	return (flags & AE_CONTROLLER_CROUCH_FLAG) ? _ae_controller_crouch_stays_down : _ae_controller_crouch_halo_ce;
}

/* the flags with the rule set (the other bits kept; a rule out of the list is HALO CE) */
static inline unsigned long ae_controller_crouch_flags_set(unsigned long flags, int rule)
{
	return rule == _ae_controller_crouch_stays_down ? flags | AE_CONTROLLER_CROUCH_FLAG :
		flags & ~AE_CONTROLLER_CROUCH_FLAG;
}

/* the rule's name in the log ("AE controller crouch: ...") */
static inline char const *ae_controller_crouch_name(int rule)
{
	return rule == _ae_controller_crouch_stays_down ? "stays down" : "halo ce";
}

/* player_control.c's crouch gate: TRUE lets the crouch button set the crouch whatever the stick (the gate's other
terms are Halo's and the keyboard's); only a held crouch under STAYS DOWN */
static inline int ae_controller_crouch_holds(int rule, int crouch_held)
{
	return rule == _ae_controller_crouch_stays_down && crouch_held;
}

#endif
