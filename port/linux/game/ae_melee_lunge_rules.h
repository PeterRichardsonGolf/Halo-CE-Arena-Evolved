/*
AE_MELEE_LUNGE_RULES.H

MELEE LUNGE, a gametype rule (GAME OPTIONS, CONTROLS; the gametype's byte is ae_variant_ext's melee_lunge,
AE_EXT_MELEE_LUNGE_*): every weapon's melee, with an enemy just past the melee's own reach, takes the swinger a step
toward him and swings at once. Pure (port/linux/tests/ae_melee_lunge_rules_test.c); ae_sword_lunge.c plays it.
- OFF: Halo CE's melee (a blow 0.8 units from the head, no lunge).
- AE: reach 1.1 units centre to centre, 1.5 units a second, a pull of 1.5 degrees a tick on the yaw only, once a second.
- STRONG: reach 1.05, 4 units a second, a pull of 4 degrees, once a second.
In both the enemy is inside a 15 degree cone (plus the edge of his body), the body is eased toward him at most 0.375 of
the gap a tick, the blow is the weapon's own melee damage, and within 0.8 units of him the melee is Halo CE's.
*/

#ifndef AE_MELEE_LUNGE_RULES_H
#define AE_MELEE_LUNGE_RULES_H

enum
{
	AE_MELEE_LUNGE_TICKS_PER_SECOND = 30,
};

struct ae_melee_lunge_params
{
	float reach;            /* the farthest an enemy is lunged at, centre to centre */
	float minimum;          /* at or inside this the melee is Halo CE's */
	float cone_degrees;     /* the cone off the aim, before his body's edge is added */
	float speed;            /* units a second */
	float turn_degrees;     /* the yaw pull a tick */
	long cooldown_ticks;    /* between two lunges of one player */
	float ease;             /* the most of the gap left one tick closes */
};

enum ae_melee_lunge_verdict
{
	_ae_melee_lunge_plain = 0,  /* at or inside the minimum: Halo CE's melee */
	_ae_melee_lunge_too_soon,   /* in the window, but the last lunge was too lately: the plain swing */
	_ae_melee_lunge_go,         /* the lunge */
};

/* the rule's numbers (rule: AE_EXT_MELEE_LUNGE_*); 0 for OFF and for anything unknown, which is Halo CE's melee */
int ae_melee_lunge_params_get(int rule, struct ae_melee_lunge_params *params);
/* the rule's name for the logs ("off", "ae", "strong") */
char const *ae_melee_lunge_name(int rule);
/* an enemy distance centre-to-centre away, found inside the reach and the cone: what the swing does (now and
next_time are game ticks; a lunge is allowed from next_time on) */
enum ae_melee_lunge_verdict ae_melee_lunge_verdict(struct ae_melee_lunge_params const *params, float distance, long now,
	long next_time);
/* a tick's step (units) toward him: the gap to the arrival distance, at most the ease's share of the gap and at most the
speed's share of a tick; never negative */
float ae_melee_lunge_step(struct ae_melee_lunge_params const *params, float gap, float arrival);
#endif
