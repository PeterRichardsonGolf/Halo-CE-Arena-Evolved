/*
AE_TRAITS_RULES.H

The host's player traits (GAME OPTIONS > SPARTAN): JUMP HEIGHT, GRAVITY and PLAYER SPEED, each a percent of the
loaded map's own value, 100% the map's own. Pure (port/linux/tests/ae_traits_rules_test.c); the engine glue is
ae_traits.c.

One table holds every trait: where its byte is stored and the percents it may have. A byte of zero is the map's
own (100%), so a gametype that never set a trait, or a build before it, plays the map as it is.
- JUMP HEIGHT: how high a player's jump rises. Its byte is game_variant_options.jump_percent, which holds the jump's
  speed (ae_jump_rules.h: Arena Evolved 0.7.3 and later read it so), so the height is the square of the speed:
  a speed of 120% rises 144%.
- GRAVITY: the pull on a player's biped in the air (ae_variant_ext's gravity_percent). Grenades, vehicles, items,
  ragdolls and projectiles keep the map's gravity. Less gravity also makes a jump rise higher and a fall land softer.
- PLAYER SPEED: how fast a player moves on foot (ae_variant_ext's speed_percent), on top of the game modes' own
  carrier and juggernaut speeds.
*/

#ifndef AE_TRAITS_RULES_H
#define AE_TRAITS_RULES_H

enum ae_trait
{
	AE_TRAIT_JUMP_HEIGHT,
	AE_TRAIT_GRAVITY,
	AE_TRAIT_SPEED,
	AE_TRAIT_COUNT
};

/* where a trait's byte is stored */
enum ae_trait_storage
{
	/* game_variant_options (game_engine.h), at the offset */
	AE_TRAIT_IN_OPTIONS,
	/* the AE block (ae_variant_ext_rules.h), at the offset */
	AE_TRAIT_IN_EXT,
};

/* what a trait's stored byte means */
enum ae_trait_kind
{
	/* the percent itself */
	AE_TRAIT_PERCENT,
	/* the percent of the jump's speed, whose square is the height's */
	AE_TRAIT_JUMP_SPEED_PERCENT,
};

struct ae_trait_rule
{
	const char *name;
	unsigned char storage;  /* enum ae_trait_storage */
	unsigned char kind;     /* enum ae_trait_kind */
	unsigned short offset;
	/* the stored byte's least and greatest (0 is always the map's own) */
	unsigned char minimum, maximum;
};

/* the trait's rule; NULL for one out of range */
const struct ae_trait_rule *ae_trait_rule(int trait);

/* the stored byte read as the trait's percent: 0 or a byte out of the rule's range is 100 (the map's own) */
int ae_trait_percent(int trait, unsigned char stored);

/* the byte that stores a trait at the percent: 0 for 100, else the nearest the rule can hold */
unsigned char ae_trait_stored(int trait, int percent);

/* the multiplier a trait's stored byte puts on the map's own value: the jump's speed for JUMP HEIGHT (the square
root of its height), gravity and speed for the others */
float ae_trait_multiplier(int trait, unsigned char stored);

/* a running player's jump rise (world units) from a biped's jump speed and the gravity on it (both per tick), with
their multipliers: speed squared over twice the gravity; 0 for none */
float ae_traits_jump_rise(float jump_velocity, float jump_multiplier, float gravity, float gravity_multiplier);

/* how far a jump reaches against the map's own (a rise's and a run's both): the rise under the multipliers over the
rise without them; 1 for none */
float ae_traits_jump_reach(float jump_multiplier, float gravity_multiplier);

/* whether another machine's build (its Delta HELLO's build name: "Arena Evolved 1.0.0-rc5", "... -dev") plays
GRAVITY and PLAYER SPEED: the same build as this machine's (local_build), or Arena Evolved 1.0.0-rc5 and later.
An earlier build zeroes their bytes as reserved and moves as the map has it, so its players get the map's own */
int ae_traits_build_has_them(const char *build, const char *local_build);

#endif
