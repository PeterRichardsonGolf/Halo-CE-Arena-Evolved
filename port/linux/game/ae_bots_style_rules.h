/*
AE_BOTS_STYLE_RULES.H

Bot play styles (0.9.3, the offline social career; owner 2026-11-15: play styles, and per-bot attributes as other
games' bots have them, ideas only), as pure rules. A style only bends what a bot chooses, mildly: its strength (aim,
reaction, strafing) stays its shooting and movement levels (ae_bots_seat.h), so the hidden match rating still reads
its tier. No engine includes: port/linux/tests/ae_bots_style_rules_test.c compiles these alone.

- AGGRESSION (-2..+2): the fight's confidence + AE_STYLE_CONFIDENCE a step (retreats later, charges more; or the other
  way).
- JUMPINESS (-2..+2): the fight's jump delay x (1 - AE_STYLE_JUMP a step); a level that never jumps still never jumps.
- PATIENCE (-2..+2): the fight's halts x (1 + AE_STYLE_HALT a step): holds still longer, or keeps moving.
- FAVOURITE WEAPON (AE_STYLE_WEAPON_*): its value + AE_STYLE_FAVOURITE when a bot weighs a loose weapon against its own.
- PRESETS: STEADY 0/0/0, PUSHER +2/+1/-1, ANCHOR -2/-1/+2, HOPPER +1/+2/-1, CAREFUL -1/0/+1. A name's hash
  (ae_social_name_hash) picks one, weighted 3/2/2/1/2 of 10, and a favourite weapon (none 1 in 3), so a name always
  plays the same way.
*/

#ifndef __AE_BOTS_STYLE_RULES_H
#define __AE_BOTS_STYLE_RULES_H

#define AE_STYLE_STEP_MOST 2
#define AE_STYLE_CONFIDENCE 0.06f
#define AE_STYLE_JUMP 0.15f
#define AE_STYLE_HALT 0.25f
#define AE_STYLE_FAVOURITE 2

enum
{
	AE_STYLE_STEADY = 0,
	AE_STYLE_PUSHER,
	AE_STYLE_ANCHOR,
	AE_STYLE_HOPPER,
	AE_STYLE_CAREFUL,
	AE_STYLE_PRESETS
};

enum
{
	AE_STYLE_WEAPON_NONE = 0,
	AE_STYLE_WEAPON_ASSAULT_RIFLE,
	AE_STYLE_WEAPON_BATTLE_RIFLE,
	AE_STYLE_WEAPON_SHOTGUN,
	AE_STYLE_WEAPON_SNIPER,
	AE_STYLE_WEAPON_PISTOL,
	AE_STYLE_WEAPON_ROCKETS,
	AE_STYLE_WEAPON_PLASMA_RIFLE,
	AE_STYLE_WEAPON_NEEDLER,
	AE_STYLE_WEAPONS
};

struct ae_bots_style
{
	int preset;	/* AE_STYLE_* (the name it is logged by) */
	int aggression, jumpiness, patience;	/* -2..+2 each */
	int weapon;	/* AE_STYLE_WEAPON_* */
};

/* a preset's traits with a favourite weapon: 1, or 0 for a preset or a weapon out of range */
int ae_bots_style_preset(int preset, int weapon, struct ae_bots_style *style);
/* a name's style from its hash */
void ae_bots_style_from_hash(unsigned long hash, struct ae_bots_style *style);
/* names: a preset's ("PUSHER"), a weapon's ("shotgun"); a preset or a weapon from its name (any case; -1 when none) */
char const *ae_bots_style_preset_name(int preset);
char const *ae_bots_style_weapon_name(int weapon);
int ae_bots_style_preset_of_name(char const *name);
int ae_bots_style_weapon_of_name(char const *name);
/* the weapon class of a weapon tag's path ("weapons\shotgun\shotgun": AE_STYLE_WEAPON_SHOTGUN), NONE (0) for another */
int ae_bots_style_weapon_of_tag(char const *tag_path);

/* the knobs: the fight's confidence; a jump delay in ticks (0 stays 0); a halt in ticks; a weapon's value */
float ae_bots_style_confidence(struct ae_bots_style const *style, float confidence);
long ae_bots_style_jump_delay(struct ae_bots_style const *style, long ticks);
long ae_bots_style_halt(struct ae_bots_style const *style, long ticks);
int ae_bots_style_weapon_value(struct ae_bots_style const *style, char const *tag_path, int value);

#endif
