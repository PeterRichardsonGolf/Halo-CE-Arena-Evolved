/*
AE_WEAPON_TUNE_RULES.H

DEV MODE's tune command: the weapon numbers it may change, as pure rules (no engine includes:
port/linux/tests/ae_weapon_tune_rules_test.c compiles these alone). Each key is a short text
("trigger0.rate"), as the tag editors of the community's content tools spell their keys (credit: Halo Content
Evolved, Watanoge/halo-content-evolved, AGPL-3.0-or-later: the key scheme only), and each names one field of one of
the tags a weapon leads to. The recipe tools (tools/weapon_pack/apply_tune.py) write the keys back into a recipe as
`set` edits on the field path.

The field names are those of the pinned Invader 0.44.0's `invader-edit -l`, read on a weapon, a projectile and a
damage effect tag:
  weapon      triggers[n].maximum_rate_of_fire (bounds), triggers[n].error_angle (bounds, degrees),
              triggers[n].projectiles_per_shot, triggers[n].rounds_per_shot, magazines[n].rounds_loaded_maximum,
              magazines[n].rounds_reserved_maximum, magazines[n].reload_time, zoom_magnification_range (bounds),
              autoaim_angle (degrees)
  projectile  initial_velocity and final_velocity (both, one speed), impact_damage (the damage effect tag)
  damage      damage_upper_bound (bounds: the damage's lower and upper end), in the impact effect and in the melee
              effect (the weapon's player_melee_damage)
A bounds field takes one value for both ends, or "a..b".
*/

#ifndef __AE_WEAPON_TUNE_RULES_H
#define __AE_WEAPON_TUNE_RULES_H

enum ae_tune_field
{
	AE_TUNE_TRIGGER_RATE,
	AE_TUNE_TRIGGER_SPREAD,
	AE_TUNE_TRIGGER_PROJECTILES,
	AE_TUNE_TRIGGER_ROUNDS,
	AE_TUNE_MAGAZINE_SIZE,
	AE_TUNE_MAGAZINE_TOTAL,
	AE_TUNE_MAGAZINE_RELOAD,
	AE_TUNE_PROJECTILE_SPEED,
	AE_TUNE_IMPACT_DAMAGE,
	AE_TUNE_MELEE_DAMAGE,
	AE_TUNE_ZOOM,
	AE_TUNE_AUTOAIM,
	AE_TUNE_FIELD_COUNT
};

/* which tag a key's field is in */
enum ae_tune_tag_kind
{
	AE_TUNE_IN_WEAPON,
	AE_TUNE_IN_PROJECTILE,
	AE_TUNE_IN_IMPACT_EFFECT,
	AE_TUNE_IN_MELEE_EFFECT
};

struct ae_tune_key
{
	enum ae_tune_field field;
	/* the trigger, magazine or projectile (a projectile is its trigger's) number; 0 where the key has none */
	short index;
};

/* (a value of a key: one number, or for a bounds key the two ends) */
struct ae_tune_value
{
	double low;
	double high;
};

/* the key a text spells: 1, or 0 for a text that is no key */
int ae_tune_key_parse(char const *text, struct ae_tune_key *out);
/* the key's text */
void ae_tune_key_name(struct ae_tune_key key, char *out, int size);

/* whether the key's field is a pair of bounds (a value of it may be "a..b") */
int ae_tune_key_is_bounds(struct ae_tune_key key);
/* whether the key's number is a whole number (a count) */
int ae_tune_key_is_count(struct ae_tune_key key);
enum ae_tune_tag_kind ae_tune_key_tag_kind(struct ae_tune_key key);

/* the Invader field path of the key in its tag, for a recipe's `set` edit (the damage keys: the damage effect's);
1 or 0 for a key with none */
int ae_tune_invader_path(struct ae_tune_key key, char *out, int size);
/* a second field the same value is set in (the projectile's final velocity goes with its initial): 1, or 0 for a key
with one field */
int ae_tune_invader_path_second(struct ae_tune_key key, char *out, int size);

/* the value a text spells for the key ("9", "6..9"; a pair only for a bounds key): 1, or 0 */
int ae_tune_value_parse(struct ae_tune_key key, char const *text, struct ae_tune_value *out);
/* whether the engine can hold the number: refuses NaN and infinity, a number a 32-bit float cannot hold, a count
below 0 or above its field's type, and a rate or speed below 0; nothing about balance */
int ae_tune_value_allowed(struct ae_tune_key key, double value);
/* the value as a text the recipe holds it as: "9", or "6 9" for the two ends of a bounds key */
void ae_tune_value_text(struct ae_tune_key key, struct ae_tune_value value, char *out, int size);

/* a name safe to be a file's: lower case, letters, digits, "_" and "-" kept, everything else "_", at most size-1 characters
(AE_TUNE_NAME_MOST at most); "weapon" for an empty result. A weapon's key comes from a map, which is not trusted */
#define AE_TUNE_NAME_MOST 64
void ae_tune_safe_name(char const *text, char *out, int size);

/* text as the inside of a JSON string: quote, backslash and control characters escaped, bytes of 0x80 and over as
\u00XX (always valid JSON); 1, or 0 when it did not fit (out then holds what did, whole escapes only) */
int ae_tune_json_escape(char const *text, char *out, int size);

/* where each changed number came from: the memory a key's number lives in (a location, any unique number: tags can
share a projectile or damage effect between weapons) and the two numbers it held before the first change of it. One
canonical original for a location, whoever changed it first */
#define AE_TUNE_ORIGINS_MOST 128
struct ae_tune_origin
{
	unsigned long where;
	double before[2];
};
struct ae_tune_origins
{
	int count;
	struct ae_tune_origin origin[AE_TUNE_ORIGINS_MOST];
};
/* the location's original: kept from the first call (with the numbers given then), returned on every call; 1, or 0
when the table is full */
int ae_tune_origin_keep(struct ae_tune_origins *table, unsigned long where, double a, double b, double *a_out,
	double *b_out);
/* forgets a location (its original goes back to the game) */
void ae_tune_origin_drop(struct ae_tune_origins *table, unsigned long where);

#endif
