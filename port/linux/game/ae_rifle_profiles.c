/*
AE_RIFLE_PROFILES.C

The AE rifles' numbers (ae_rifle_profiles.h). No engine includes.

Where they come from (numbers only; no game data or code is copied):
- BATTLE RIFLE, the casual rifle (10-30: AE's 3x; approved in
  play, test burst-a): the burst (3 rounds 2 ticks apart, 6.5 damage a round, shield x1.06 so the 4th burst's
  last round can be the killing head shot) with a recovery (0.3 s: in AE's ticks 8 make its 1.55 s perfect
  kill, the bursts 14 ticks apart, 1.53 s), no spread, 36 loaded, 144 at first, 216 in reserve, aim assist 2.2 degrees
  to 18 wu and magnetism 5.75 degrees to 30 wu. The bullet: 400 wu a second to 40 wu.
- EVO RIFLE, the competitive rifle (10-30): one round a pull, 17.875 a round (x1.06 on the shield: 4 rounds break
  CE's 75 with 0.79 over, the pop shows on the 4th); its breaking round carries nothing past the shield (no spill,
  ae_shield_bleed.c: CE kills a head hit that has damage left after the shield, so the 4th to the head does not), the
  5th to the head kills, and the body alone takes 7 (after the break 17.875 x1.5 = 26.8 a round, 3 rounds kill 75); a
  pull every 11 ticks (the Evo's 2.715 a second, 0.37 s): a perfect kill in 44 ticks,
  1.47 s; 15 loaded, 60 at first, 75 in reserve, a tracer every round; aim assist 2.0 degrees to 18 wu, magnetism 5.0 degrees to 25 wu, no spread; 2x as
  seen (the owner; the Evo's own is 1.6x).
- DMR, the zero-bloom marksman (the owner's "stock DMR", 10-30, "good to ship"): one round a pull, 3 a second (a pull
  every 10 ticks), a quarter of the 11 burst rounds that empty the shield (17.875: the 4th round breaks it with 0.79
  over, the pop shows and the 4th to the head kills, a perfect kill in 30 ticks, 1.0 s; its damage spills), 15 loaded,
  45 at first, 60 in reserve,
  autoaim 2.25 degrees to 20 wu, magnetism 7.0 degrees to 25 wu (the owner in play: stickier than the read 5.0 to 20),
  a constant 0.15 degree spread (no bloom), a tracer every round, 3x as seen, a bullet of 3000 wu a second to 250 wu.
Its numbers are in CE's world units.
The aim reticles' growth a shot (the owner 10-30, a test: his picks' firing examples): the EVO RIFLE's whole reticle
(a ring and a ringed dot) grows 35 % and is back in 0.3 s, before the next pull; the BATTLE RIFLE's ring keeps its
size and the four ticks spread 35 % once a burst (its rounds don't start it again: the owner, 10-30, "a little less
busy"), back in 0.3 s, before the next burst; the zero-bloom DMR's middle (the dot and the inner ring)
stays on the target while its outer ring kicks out 35 % and is back in 0.3 s, before the next shot (the owner 10-30: "the
same treatment ... an animation pass"; the drawing only, its spread stays fixed).
- BURST PDW, the burst personal defence weapon (0.7.6, the owner: "3 bursts close, 4 at mid" on a full shield, no delay
  on the pull): 3 rounds a pull a tick apart (30 a second), a recovery of 12 ticks so pulls come 16 ticks apart (0.53 s:
  the owner, its perfect kill at mid range "just under the BR", strictly slower: 4 bursts with the 11th round the
  killing one 49 ticks after the first, 1.63 s, where the BATTLE RIFLE's takes 46; close, the 8th round, 33 ticks,
  1.10 s); 20 a round at the bullet's 400 wu a second, 14 once it has slowed to 240 (over its tag's air
  damage range, the SMG round's: full inside about 5 wu, slowed past 6.5), x1.0 on CE's shield and body (75 and 75,
  the shield's excess carried on): close the 8th round kills (the 3rd burst's 2nd, one to spare; 2 bursts 120), at mid
  range the 11th (the 4th burst's 2nd, one to spare; 3 bursts 126); 36 loaded, 144 at first, 144 in reserve, a tracer
  each burst, spread 0.4 to 1.2 degrees (a burst's rounds climb it, a third a tick), aim assist 4 degrees to 14 wu,
  magnetism 8 degrees to 20 wu, no zoom, no reticle growth (its reticle is CMT's SMG one, one sprite).
- RAVAGER, the plasma burst launcher (0.7.6, the latest-gen setup: a 3-bolt burst at 7.5 a second, 0.5 s between
  bursts, a burst every about 0.77 s, no bloom): 3 bolts a tap 4 ticks apart (7.5 a second), the recovery 12 ticks, so
  the next tap's bolt, which leaves on the trigger's release a tick after its press, comes 15 ticks (0.5 s) after a
  burst's last bolt and taps are 23 ticks apart (0.77 s); the 7th bolt, the 3rd tap's first, kills a full player 46
  ticks (1.53 s) after the first. Its bolts, their damage, the heat and battery a bolt (a third of a tap's), the zoom,
  aim assist and kick are its tags' (tags_rounds); its charged shot (the second trigger) too, followed by the recovery.
- HJ MAGNUM, the scoped pistol (0.8.0, the owner 11-09: a 5-shot kill, 4 to take the shield down and the 5th to the
  head, about 3.57 shots a second, no recoil, no spread, its zoom kept, a fastest kill near 1.12 s): no rifle; one
  round a pull, the EVO RIFLE's round (17.875, x1.06 on the shield, x1.5 on the body, no spill: 4 rounds break CE's 75 with
  0.79 over and carry nothing, the 5th to the head kills, the body alone takes 7), a pull every 8 ticks (3.75 a second,
  0.267 s: of AE's 30 ticks a second, 4 spacings of 8 make 32 ticks, 1.07 s, nearest 1.12 s; 9 would make 36, 1.2 s),
  no spread and no bloom, the view's kick off. Its pack plays its tags (behaviour 0): its magazine (12, 48 at first, 48
  in reserve), its 2x zoom, aim assist, magnetism, tracers, bullet and rumble stay its tags' (tags_handling), and it is
  found by its weapon key, not a behaviour.
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ae_rifle_profiles.h"

/* (ae_pack_format.h's AE_PACK_BEHAVIOUR_AE_*: the pack format's values, kept apart from its engine includes) */
#define BEHAVIOUR_AE_BATTLE_RIFLE 1
#define BEHAVIOUR_AE_EVO_RIFLE 2
#define BEHAVIOUR_AE_DMR 3
#define BEHAVIOUR_AE_PDW 4
#define BEHAVIOUR_AE_RAVAGER 5
/* (the tags' own behaviour, AE_PACK_BEHAVIOUR_TAGS; the weapon key of the HJ MAGNUM, whose pack has it: the registry's
magnum_scoped, the weapon cache's id 10) */
#define BEHAVIOUR_TAGS 0
#define HJ_MAGNUM_KEY "magnum_scoped"

/* 6.5 a round; the single shots' a quarter of the 11 burst rounds that empty the shield: 4 rounds break it with 0.79
over. The DMR's 4th to the head kills (the 0.79 spills); the EVO RIFLE's breaking round carries nothing (no_spill), so
its 4th only pops and the 5th to the head kills */
#define BURST_ROUND_DAMAGE 6.5f
#define SINGLE_ROUND_DAMAGE (BURST_ROUND_DAMAGE * 11.0f / 4.0f)
/* the rifles' damage modifiers: on CE's cyborg shield x1.06 (the owner's "Test B": 11 burst rounds empty CE's 75 shield,
so the 12th can be the killing head shot; 4 single shots break it); on the body x1.5 (the prototype's, as CE's 75
health against a taller body's) */
#define RIFLE_SHIELD_MODIFIER 1.06f
#define RIFLE_BODY_MODIFIER 1.5f

static struct ae_rifle_profile const profiles[AE_RIFLE_KINDS] =
{
	/* name, a rifle, rounds a pull, ticks between its rounds, keeps a pull, recovery ticks, damage / slowed, shield /
	body modifier, loaded, at first, reserve, between tracers, autoaim degrees / wu, magnetism degrees / wu, spread
	lower / upper, zoom seen, bullet speed / slowed / range, reticle growth / ms / fixed items, scope readouts, the
	tags' rounds, no spill, the tags' handling */
	{ "BATTLE RIFLE", TRUE, 3, 2, FALSE, 8, BURST_ROUND_DAMAGE, BURST_ROUND_DAMAGE, RIFLE_SHIELD_MODIFIER,
		RIFLE_BODY_MODIFIER, 36, 144, 216, 2,
		2.2f, 18.0f, 5.75f, 30.0f, 0.0f, 0.0f, 3.0f, 400.0f, 400.0f, 40.0f, 0.35f, 300, 0x01, FALSE, FALSE, FALSE,
		FALSE },
	{ "EVO RIFLE", TRUE, 1, 2, TRUE, 9, SINGLE_ROUND_DAMAGE, SINGLE_ROUND_DAMAGE, RIFLE_SHIELD_MODIFIER,
		RIFLE_BODY_MODIFIER, 15, 60, 75, 0,
		2.0f, 18.0f, 5.0f, 25.0f, 0.0f, 0.0f, 2.0f, 400.0f, 400.0f, 40.0f, 0.35f, 300, 0x00, TRUE, FALSE, TRUE, FALSE },
	{ "DMR", TRUE, 1, 2, TRUE, 8, SINGLE_ROUND_DAMAGE, SINGLE_ROUND_DAMAGE, RIFLE_SHIELD_MODIFIER,
		RIFLE_BODY_MODIFIER, 15, 45, 60, 0,
		2.25f, 20.0f, 7.0f, 25.0f, 0.15f, 0.15f, 3.0f, 3000.0f, 3000.0f, 250.0f, 0.35f, 300, 0x01, TRUE, FALSE, FALSE,
		FALSE },
	{ "BURST PDW", FALSE, 3, 1, FALSE, 12, 20.0f, 14.0f, 1.0f, 1.0f, 36, 144, 144, 2,
		4.0f, 14.0f, 8.0f, 20.0f, 0.4f, 1.2f, 0.0f, 400.0f, 240.0f, 40.0f, 0.0f, 0, 0x00, FALSE, FALSE, FALSE, FALSE },
	/* (the tags' own bolts: every number but the pacing is the tags') */
	{ "RAVAGER", FALSE, 3, 4, FALSE, 12, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0x00, FALSE, TRUE, FALSE, FALSE },
	/* (a pistol: the EVO RIFLE's round and no spill, a pull every 8 ticks, no spread; its magazine, zoom, aim assist,
	tracers and bullet the tags') */
	{ "HJ MAGNUM", FALSE, 1, 2, TRUE, 6, SINGLE_ROUND_DAMAGE, SINGLE_ROUND_DAMAGE, RIFLE_SHIELD_MODIFIER,
		RIFLE_BODY_MODIFIER, 0, 0, 0, 0,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0x00, FALSE, FALSE, TRUE, TRUE },
};

enum ae_rifle_kind ae_rifle_kind_of_behaviour(unsigned int behaviour)
{
	switch (behaviour)
	{
	case BEHAVIOUR_AE_BATTLE_RIFLE: return AE_RIFLE_BATTLE_RIFLE;
	case BEHAVIOUR_AE_EVO_RIFLE: return AE_RIFLE_EVO_RIFLE;
	case BEHAVIOUR_AE_DMR: return AE_RIFLE_DMR;
	case BEHAVIOUR_AE_PDW: return AE_RIFLE_PDW;
	case BEHAVIOUR_AE_RAVAGER: return AE_RIFLE_RAVAGER;
	default: return AE_RIFLE_NONE;
	}
}

enum ae_rifle_kind ae_rifle_kind_of_weapon(unsigned int behaviour, char const *key)
{
	if (behaviour != BEHAVIOUR_TAGS)
		return ae_rifle_kind_of_behaviour(behaviour);
	return key && !strcmp(key, HJ_MAGNUM_KEY) ? AE_RIFLE_HJ_MAGNUM : AE_RIFLE_NONE;
}

struct ae_rifle_profile const *ae_rifle_profile(enum ae_rifle_kind kind)
{
	return kind >= 0 && kind < AE_RIFLE_KINDS ? &profiles[kind] : NULL;
}

short ae_rifle_weapons_add(struct ae_rifle_weapons *weapons, long definition, long projectile, enum ae_rifle_kind kind)
{
	short index;

	if (!weapons || definition == -1 || !ae_rifle_profile(kind) || weapons->count < 0 ||
		weapons->count >= AE_RIFLE_WEAPONS_A_MAP)
	{
		return -1;
	}
	index = weapons->count++;
	weapons->definitions[index] = definition;
	weapons->projectiles[index] = projectile;
	weapons->kinds[index] = (signed char)kind;
	return index;
}

struct ae_rifle_profile const *ae_rifle_weapons_profile(struct ae_rifle_weapons const *weapons, long definition)
{
	short index;

	if (!weapons || definition == -1)
		return NULL;
	for (index = 0; index < weapons->count && index < AE_RIFLE_WEAPONS_A_MAP; index++)
	{
		if (weapons->definitions[index] == definition)
			return ae_rifle_profile((enum ae_rifle_kind)weapons->kinds[index]);
	}
	return NULL;
}

/* a pull's rounds go out round_ticks apart; the recovery starts the tick after the pull's last round and the next pull
fires the tick after it: pulls ((rounds - 1) x round_ticks + recovery + 2) ticks apart (the rifles' 2 x rounds +
recovery); the tags' rounds a tick more, the tap's first round leaving on its release */
long ae_rifle_pull_spacing_ticks(struct ae_rifle_profile const *profile)
{
	if (!profile)
		return 0;
	return (long)(profile->rounds_a_pull - 1) * profile->round_ticks + profile->recovery_ticks + 2L +
		(profile->tags_rounds ? 1L : 0L);
}

/* a burst's perfect kill is 4 bursts (the 12th round the head shot), the EVO RIFLE's and the HJ MAGNUM's 5 pulls (4 pop
the shield, the 5th kills), the DMR's 4 (the 4th pops it and kills) */
long ae_rifle_perfect_pulls(enum ae_rifle_kind kind)
{
	struct ae_rifle_profile const *profile = ae_rifle_profile(kind);

	if (!profile)
		return 0;
	return profile->rounds_a_pull == 1 && profile->no_spill ? 5 : 4;
}

long ae_rifle_perfect_kill_ticks(enum ae_rifle_kind kind)
{
	struct ae_rifle_profile const *profile = ae_rifle_profile(kind);
	long pulls = ae_rifle_perfect_pulls(kind);

	if (!profile)
		return 0;
	return (pulls - 1) * ae_rifle_pull_spacing_ticks(profile) + (long)profile->round_ticks * (profile->rounds_a_pull - 1);
}

unsigned long ae_rifle_pull_ms(struct ae_rifle_profile const *profile)
{
	return ((unsigned long)(profile->rounds_a_pull - 1) * (unsigned long)profile->round_ticks + 1UL) * 1000UL / 30UL;
}

void ae_rifle_zoom_text(real zoom_seen, char *text, int size)
{
	long tenths = (long)(zoom_seen * 10.0f + 0.5f);

	if (tenths % 10 == 0)
		snprintf(text, (size_t)size, "%ldx", tenths / 10);
	else
		snprintf(text, (size_t)size, "%ld.%ldx", tenths / 10, tenths % 10);
}

real ae_rifle_reticle_scale(real from, real growth, short growth_ms, unsigned long since_ms)
{
	real peak = 1.0f + growth;
	real t;

	if (growth <= 0.0f || growth_ms <= AE_RIFLE_RETICLE_RISE_MS || since_ms >= (unsigned long)growth_ms)
		return 1.0f;
	if (since_ms < AE_RIFLE_RETICLE_RISE_MS)
	{
		t = 1.0f - (real)since_ms / AE_RIFLE_RETICLE_RISE_MS;
		return from + (peak - from) * (1.0f - t * t * t);
	}
	t = (real)(since_ms - AE_RIFLE_RETICLE_RISE_MS) / (real)(growth_ms - AE_RIFLE_RETICLE_RISE_MS);
	return peak - growth * (1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t));
}
