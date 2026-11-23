/*
AE_SHIELD_EFFECTS_RULES.H

The rules of whose shield effects a machine draws and of the arm shield's log, with nothing of the engine's
(port/linux/tests/ae_shield_effects_rules_test.c).

- A network game draws the host's network.shield_effects (network_shield_effects.c), so no player sees what another
  does not. AE's OFFLINE lobby is hosted as a network server that takes no remote connections (ae_glue_lobby.c): no
  other machine is there to keep in step, so it draws each player's own settings (display.overshield_effect,
  display.shield_glow), as an offline game always did. Before 1.0.0-rc1 a host's "stock" turned the overshield effect
  off in its own offline games, whatever display.overshield_effect said (the rc1 round's overshield-arms test).
- The arm shield's log (ae_arm_shield_log.c): once a second for each local player while their shield is over 1 or
  charging past it.
*/

#ifndef __AE_SHIELD_EFFECTS_RULES_H
#define __AE_SHIELD_EFFECTS_RULES_H

/* whether this machine draws each player's own shield effects, not the host's choice: network_server, this machine
hosts a network game; accepts_remote, other machines may join it */
int ae_shield_effects_own(
	int network_server,
	int accepts_remote);

/* whether the arm shield's line is due: shield, the unit's shield vitality; charging, its overshield charges;
now_ms and last_ms (0: never), milliseconds */
int ae_arm_shield_log_due(
	float shield,
	int charging,
	unsigned long now_ms,
	unsigned long last_ms);

/* the name of a network.shield_effects value (network_shield_effects.h's order; a negative value: "players' own") */
char const *ae_shield_effects_name(
	int effects);

#endif /* __AE_SHIELD_EFFECTS_RULES_H */
