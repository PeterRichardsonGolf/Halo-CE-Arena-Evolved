/*
AE_HEAD_CHECK.H

HEAD CHECK (network.head_check, ae_head_check_rules.h): the host's test of a client's reported head hit. Called from
network_damage.c's network_damage_handle_reports (one AE hook).
*/

#ifndef AE_HEAD_CHECK_H
#define AE_HEAD_CHECK_H

/* the material a reported shot deals its damage with: the report's, or the target's body material when the report names
a head material and its point (hit) is off the head, the head node being where the host had the target when the shooter
saw it (seen_position). The report's own material when network.head_check is off. A swap is counted and logged. */
short ae_head_check_material(short player_index, long object_index, short node_index, short material_index,
	real_point3d const *hit, real_point3d const *seen_position);

/* the head hits made body hits since the game started (for TEST RUN checks and the log) */
long ae_head_check_swaps(void);

#endif
