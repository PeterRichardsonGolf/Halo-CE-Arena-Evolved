/*
AE_HIT_COVER_RULES.H

HIT COVER (network.hit_cover, the host's): a client's reported explosion or melee hit counts only when the level leaves
a line to the target, as its reported shots already need one (network_damage.c, distributed_report_line_of_sight).
These pure functions give the lines and the rule (port/linux/tests/ae_hit_cover_rules_test.c); ae_hit_cover.c tests
them against the level's structure.

- An explosion: from its epicenter, moved a little toward the target (a rocket or a grenade goes off at a wall's face,
  and a line from the face itself can start behind it), to the target's centre where the host had it, or to a point
  above that centre (a target half behind a low wall). Refused only when both lines are blocked.
- A melee blow: from the striker's head (the report's origin) or body (its epicenter) to the target's centre. Refused
  only when both are blocked.
*/

#ifndef AE_HIT_COVER_RULES_H
#define AE_HIT_COVER_RULES_H

struct ae_hit_cover_point
{
	float x, y, z;
};

enum
{
	/* how far an explosion's line starts toward the target, world units x100 (never past half the way) */
	AE_HIT_COVER_NUDGE_100 = 10,
	/* the second point of an explosion's target, above its centre, world units x100 */
	AE_HIT_COVER_RAISE_100 = 50,
	/* the most lines a report is tested by */
	AE_HIT_COVER_MAXIMUM_LINES = 2,
};

struct ae_hit_cover_line
{
	struct ae_hit_cover_point from, to;
};

/* the lines a report is tested by: an explosion's from its epicenter (nudged) to the centre and the point above it, a
melee blow's from the head and the body to the centre; returns their count */
int ae_hit_cover_lines(int melee, struct ae_hit_cover_point const *origin, struct ae_hit_cover_point const *epicenter,
	struct ae_hit_cover_point const *centre, struct ae_hit_cover_line lines[AE_HIT_COVER_MAXIMUM_LINES]);

/* whether the report is refused: the setting on and every one of its lines blocked (no line: never) */
int ae_hit_cover_refused(int enabled, int count, int const *blocked);

#endif
