/*
AE_SWORD_LUNGE_RULES.H

The energy sword lunge's pure carry/strike choices. No engine includes: the unit
test in port/linux/tests/ae_sword_lunge_rules_test.c compiles these alone.
*/

#ifndef __AE_SWORD_LUNGE_RULES_H
#define __AE_SWORD_LUNGE_RULES_H

enum ae_sword_lunge_style
{
	_ae_sword_lunge_style_carry = 0,
	_ae_sword_lunge_style_strike,
};

enum ae_sword_lunge_end
{
	_ae_sword_lunge_end_arrived = 0,
	_ae_sword_lunge_end_timeout,
	_ae_sword_lunge_end_stalled,
	/* (0.7.5: the target got away, past the reach and a half unit, or the lunge used up its distance) */
	_ae_sword_lunge_end_lost,
};

/* one tick of a moving lunge, judged: distance (centre to centre now), strike (the arrival distance), contact (a
lunge held up within this still lands), reach (the lunge's trigger range: past reach + 0.5 he got away), covered
(units moved since the start), budget (the most it may move: the starting gap + 0.3), ticks_left (after this
tick's), stalled (held-up ticks in a row) and stall_ticks (how many make a hold-up). -1: it goes on */
struct ae_sword_lunge_tick
{
	float distance, strike, contact, reach, covered, budget;
	short ticks_left, stalled, stall_ticks;
};

enum ae_sword_lunge_end_action
{
	_ae_sword_lunge_end_action_cancel = 0,
	_ae_sword_lunge_end_action_damage,
	_ae_sword_lunge_end_action_strike,
};

enum ae_sword_lunge_style ae_sword_lunge_style_from_text(char const *text);
short ae_sword_lunge_duration_ticks(enum ae_sword_lunge_style style, float distance, float speed);
float ae_sword_lunge_turn_angle(float current, float target, float limit);
int ae_sword_lunge_cooldown_blocks_melee(enum ae_sword_lunge_style style);
/* the widest angle (radians) off the aim at which an enemy distance away is in the lunge's cone: the cone's angle plus
the angle his body's half width (body_radius) takes up there */
float ae_sword_lunge_cone(float angle, float distance, float body_radius);
enum ae_sword_lunge_end_action ae_sword_lunge_end_action(enum ae_sword_lunge_style style,
	enum ae_sword_lunge_end end);
int ae_sword_lunge_judge(struct ae_sword_lunge_tick const *tick);

#endif
