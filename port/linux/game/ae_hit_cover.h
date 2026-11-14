/*
AE_HIT_COVER.H

HIT COVER (network.hit_cover, ae_hit_cover_rules.h): the host's wall check of a client's reported explosion or melee
hit. Called from network_damage.c's distributed_report_valid (one AE hook).
*/

#ifndef AE_HIT_COVER_H
#define AE_HIT_COVER_H

/* whether a reported explosion (melee FALSE) or melee blow (TRUE) reaches the target past the level's structure: the
report's origin and epicenter, the target object, and where the host had the target's origin when the shooter saw it.
TRUE when network.hit_cover is off. A refusal is counted and logged. */
boolean ae_hit_cover_clear(boolean melee, short player_index, real_point3d const *origin,
	real_point3d const *epicenter, long object_index, real_point3d const *seen_position);

/* the reports refused since the game started (for TEST RUN checks and the log) */
long ae_hit_cover_refusals(void);

#endif
