/*
AE_MOUSE_AIM_RULES.H

MOUSE AIM ASSIST, a gametype rule (GAME OPTIONS; seed revision 22: OFF in every AE gametype, the NHE set ALLOWED),
pure (port/linux/tests/ae_mouse_aim_rules_test.c): ae_mouse_aim.c reads it for network_aim_assist.c's AE hooks.

- ALLOWED (0, every gametype from before, stock and custom ones, and no game at all): as before it, each player's own
  input.mouse_aim_assist decides, unless the host's network.block_mouse_aim_assist blocks it.
- OFF (1): the mouse's aim assist (the view's magnetism while the mouse aims) is blocked for every player of the game.
  Controllers keep their aim assist; bullet autoaim is unchanged.

Where it lives: bit 10 of the universal variant's flags (game_engine.h: the Xbox game's bits are 0-5, SENSOR RANGE's
6-7, CONTROLLER CROUCH's 8, MELEE UNZOOMS' 9, the port's own from 16). The host's policy, which OpenCE's message 90
(network_aim_assist.c) carries to every client unchanged on the wire, is its own setting OR the running gametype's
rule; so OpenCE and ChupathingyCE clients of an AE host obey it too.
*/

#ifndef __AE_MOUSE_AIM_RULES_H
#define __AE_MOUSE_AIM_RULES_H

#define AE_MOUSE_AIM_ASSIST_BIT 10
#define AE_MOUSE_AIM_ASSIST_FLAG (1UL << AE_MOUSE_AIM_ASSIST_BIT)

enum ae_mouse_aim_assist
{
	/* each player's own choice (every gametype from before) */
	_ae_mouse_aim_assist_allowed = 0,
	_ae_mouse_aim_assist_off,
	NUMBER_OF_AE_MOUSE_AIM_ASSISTS
};

static inline int ae_mouse_aim_assist_get(unsigned long flags)
{
	return (flags & AE_MOUSE_AIM_ASSIST_FLAG) ? _ae_mouse_aim_assist_off : _ae_mouse_aim_assist_allowed;
}

/* the flags with the rule set (the other bits kept; a rule out of the list is ALLOWED) */
static inline unsigned long ae_mouse_aim_assist_flags_set(unsigned long flags, int rule)
{
	return rule == _ae_mouse_aim_assist_off ? flags | AE_MOUSE_AIM_ASSIST_FLAG : flags & ~AE_MOUSE_AIM_ASSIST_FLAG;
}

/* the rule's name in the log ("AE mouse aim assist: ...") */
static inline char const *ae_mouse_aim_assist_name(int rule)
{
	return rule == _ae_mouse_aim_assist_off ? "off" : "allowed";
}

/* the host's policy (message 90's block_mouse): its own setting (network.block_mouse_aim_assist, 0 or 1) or the
running gametype's OFF; 1 blocks */
static inline int ae_mouse_aim_assist_policy(int host_setting, int rule)
{
	return host_setting != 0 || rule == _ae_mouse_aim_assist_off;
}

#endif
