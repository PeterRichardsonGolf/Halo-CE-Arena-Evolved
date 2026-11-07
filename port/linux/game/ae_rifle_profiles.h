/*
AE_RIFLE_PROFILES.H

The AE rifles' numbers (ae_burst_rifle.c plays them): one PROFILE a rifle, picked by its pack weapon entry's
behaviour (ae_pack_format.h): the casual BATTLE RIFLE (a burst a pull), the competitive EVO RIFLE (one round a
pull), the zero-bloom DMR (one round a pull) and the burst PDW (a burst a pull, its rounds a tick apart, its damage
falling off with range: it is no rifle, so the rifles' own rules, its kill feed name and medal role and the shield's
finishing hit, leave it out) and the RAVAGER (no rifle either: three of its own plasma bolts a tap, one after
another; its bolts, heat, battery, zoom and charged shot stay its tags'). The pure rules, no engine includes
(port/linux/tests/ae_rifle_profiles_test.c).

The numbers are constants (rules R14: no setting or environment tunes them, or a client could tune its own aim
assist and zoom). Every machine of a game plays the same.
*/

#ifndef __AE_RIFLE_PROFILES_H
#define __AE_RIFLE_PROFILES_H

#include "ae_pure_types.h"

enum ae_rifle_kind
{
	AE_RIFLE_NONE = -1,
	AE_RIFLE_BATTLE_RIFLE = 0,
	AE_RIFLE_EVO_RIFLE,
	AE_RIFLE_DMR,
	AE_RIFLE_PDW,
	AE_RIFLE_RAVAGER,
	AE_RIFLE_KINDS
};

struct ae_rifle_profile
{
	char const *name;
	/* one of the AE rifles (the BATTLE RIFLE, the EVO RIFLE, the DMR: their kill feed names, medal roles, shield
	finishing hit, bots' rifle play and HUD hint); FALSE for another weapon the burst driver plays (the PDW) */
	boolean rifle;
	/* rounds a pull: 3 (a burst) or 1 */
	short rounds_a_pull;
	/* the ticks between a pull's rounds: 2 (15 a second) or 1 (30 a second) */
	short round_ticks;
	/* a pull during the recovery is kept and fires as the recovery ends (single shots), else lost (the burst) */
	boolean keeps_pull;
	/* the recovery after a pull's last round, in ticks (30 a second) */
	short recovery_ticks;
	/* damage a round (before the shield and body modifiers): at the bullet's speed, and once it has slowed to its
	final speed (equal: no falloff) */
	real round_damage;
	real round_damage_slowed;
	/* the damage's modifiers on a cyborg's shield and body */
	real shield_modifier;
	real body_modifier;
	short rounds_loaded;
	short rounds_total_initial;
	short rounds_total_maximum;
	/* a tracer every this many rounds plus one */
	short rounds_between_tracers;
	real autoaim_degrees;
	real autoaim_distance;
	real magnetism_degrees;
	real magnetism_distance;
	/* the spread cone, degrees (equal: no bloom) */
	real error_lower_degrees;
	real error_upper_degrees;
	/* the one zoom level, as SEEN (ae_burst_rifle.c turns it into CE's magnification); 0 none */
	real zoom_seen;
	/* the bullet, world units a second: its speed and the speed it slows to (equal: none; the slowing's span is the
	tag's air damage range), and its range */
	real bullet_speed;
	real bullet_final_speed;
	real bullet_range;
	/* the aim reticle's growth a shot (the HUD only, ae_reticle_growth.c): its peak, a part of its size; how long it
	takes to come back, ms; and its fixed items (a bit an aim crosshair item: a frame that keeps its size while the
	ticks inside it spread) */
	real reticle_growth;
	short reticle_growth_ms;
	unsigned char reticle_fixed_items;
	/* the zoom overlay's readouts, drawn live (ae_scope_readout.c): the zoom as seen and the rounds loaded, in the
	frames the pack's overlay draws at its window's sides */
	boolean scope_readouts;
	/* (0.7.6, the RAVAGER) its rounds are its tags' own: the trigger's projectile (a plasma bolt with gravity), its
	damage, heat and battery, its zoom, aim assist, spread and kick, and its charged shot (a second trigger) are left as
	the tags say; the driver only paces the tap fire. The first round goes out as the tags fire it (a charging trigger's
	tap fires on its release, a tick after the press at the quickest), the rest round_ticks apart, then the recovery.
	The numbers above it does not use are 0 */
	boolean tags_rounds;
};

/* the aim reticle's grow-in: ms to the peak (eased out); then back to its own size by the profile's ms (eased) */
#define AE_RIFLE_RETICLE_RISE_MS 40

/* the weapons the burst driver plays on one map (ae_burst_rifle.c), each by its weapon tag: every pack on the map adds
its own (DEV MODE and the TEST RUN lists add every installed pack), so one map may hold a weapon of every profile at
once, and some room over */
#define AE_RIFLE_WEAPONS_A_MAP 8
typedef char ae_rifle_weapons_hold_every_kind[AE_RIFLE_WEAPONS_A_MAP >= AE_RIFLE_KINDS ? 1 : -1];

struct ae_rifle_weapons
{
	short count;
	/* each weapon's tag, its bullet's tag (-1: none kept, a weapon whose rounds are its tags') and its profile */
	long definitions[AE_RIFLE_WEAPONS_A_MAP];
	long projectiles[AE_RIFLE_WEAPONS_A_MAP];
	signed char kinds[AE_RIFLE_WEAPONS_A_MAP];
};

/* a weapon added (its tag, its bullet's or -1, its kind): its place, or -1 when the table is full or the tag (-1) or
kind is none */
short ae_rifle_weapons_add(struct ae_rifle_weapons *weapons, long definition, long projectile, enum ae_rifle_kind kind);

/* the profile of a weapon tag the table holds; NULL for any other (and for -1) */
struct ae_rifle_profile const *ae_rifle_weapons_profile(struct ae_rifle_weapons const *weapons, long definition);

/* the kind a pack weapon entry's behaviour names (AE_PACK_BEHAVIOUR_AE_*); AE_RIFLE_NONE for the tags' own behaviour */
enum ae_rifle_kind ae_rifle_kind_of_behaviour(unsigned int behaviour);

/* a kind's profile; NULL for none */
struct ae_rifle_profile const *ae_rifle_profile(enum ae_rifle_kind kind);

/* a pull's span, ms: its rounds (round_ticks apart) and a tick more; the aim reticle's growth starts once a pull, rounds
coming within it carry nothing new */
unsigned long ae_rifle_pull_ms(struct ae_rifle_profile const *profile);

/* the zoom as the scope readout shows it: "2x", "1.6x" (one decimal when not whole, as the stock scopes' "2x" /
"10x"); text of a size */
void ae_rifle_zoom_text(real zoom_seen, char *text, int size);

/* the aim reticle's size (1 its own) since_ms after a shot: from `from` (its size when the shot came, so the rounds of
a burst carry it on) up to 1 + growth by AE_RIFLE_RETICLE_RISE_MS, then back to 1 by growth_ms */
real ae_rifle_reticle_scale(real from, real growth, short growth_ms, unsigned long since_ms);

/* the ticks from a pull to the next at the quickest: the pull's last round (round_ticks apart), the tick after it,
then the recovery's ticks and the tick the next pull fires on (and, for tags' rounds, the tick a tap's release takes) */
long ae_rifle_pull_spacing_ticks(struct ae_rifle_profile const *profile);

/* the perfect kill's ticks: the pulls a 4-shot shield pop plus the killing head shot take, first round to last */
long ae_rifle_perfect_kill_ticks(enum ae_rifle_kind kind);

#endif
