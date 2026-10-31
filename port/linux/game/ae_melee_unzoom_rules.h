/*
AE_MELEE_UNZOOM_RULES.H

MELEE UNZOOMS, a gametype rule (GAME OPTIONS; seed revision 22: ON in every AE gametype, the NHE set keeps it OFF),
pure (port/linux/tests/ae_melee_unzoom_rules_test.c): ae_melee_unzoom.c reads it for the AE hooks in bipeds.c and
player_control.c.

- OFF (0, every gametype from before, stock and custom ones, and no game at all): Halo CE's own. A player whose weapon
  is zoomed does not melee (bipeds.c's melee gate asks for no zoom) until the zoom is taken off.
- ON (1): a melee while zoomed (any weapon, any zoom level) takes the zoom off and strikes.
  The machine whose player it is drops its zoom while melee is held (player_control.c: the action it sends asks for
  none), and the melee's start unzooms the unit on every machine (bipeds.c), so all play the same blow.

Where it lives: bit 9 of the universal variant's flags (game_engine.h: the Xbox game's bits are 0-5, SENSOR RANGE's
6-7, CONTROLLER CROUCH's 8, the port's own from 16). The flags travel with the variant, so every machine in a game
plays the host's rule, split screen players too; older builds, OpenCE and ChupathingyCE ignore the bit and play OFF.
*/

#ifndef __AE_MELEE_UNZOOM_RULES_H
#define __AE_MELEE_UNZOOM_RULES_H

#define AE_MELEE_UNZOOM_BIT 9
#define AE_MELEE_UNZOOM_FLAG (1UL << AE_MELEE_UNZOOM_BIT)

enum ae_melee_unzoom
{
	/* Halo CE's own: no melee while zoomed (every gametype from before) */
	_ae_melee_unzoom_off = 0,
	_ae_melee_unzoom_on,
	NUMBER_OF_AE_MELEE_UNZOOMS
};

static inline int ae_melee_unzoom_get(unsigned long flags)
{
	return (flags & AE_MELEE_UNZOOM_FLAG) ? _ae_melee_unzoom_on : _ae_melee_unzoom_off;
}

/* the flags with the rule set (the other bits kept; a rule out of the list is OFF) */
static inline unsigned long ae_melee_unzoom_flags_set(unsigned long flags, int rule)
{
	return rule == _ae_melee_unzoom_on ? flags | AE_MELEE_UNZOOM_FLAG : flags & ~AE_MELEE_UNZOOM_FLAG;
}

/* the rule's name in the log ("AE melee unzooms: ...") */
static inline char const *ae_melee_unzoom_name(int rule)
{
	return rule == _ae_melee_unzoom_on ? "on" : "off";
}

/* player_control.c: the zoom level the local player asks for (-1 none), melee held or not: none while melee is held
under ON, else the level it had */
static inline short ae_melee_unzoom_local_level(int rule, int melee_held, short zoom_level)
{
	return rule == _ae_melee_unzoom_on && melee_held ? (short)-1 : zoom_level;
}

/* bipeds.c's melee gate, its zoom term: whether a player may start a melee, zoomed or not (zoomed under ON: yes,
and the zoom comes off) */
static inline int ae_melee_unzoom_allows(int rule, int zoomed)
{
	return !zoomed || rule == _ae_melee_unzoom_on;
}

#endif
