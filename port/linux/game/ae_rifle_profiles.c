/*
AE_RIFLE_PROFILES.C

The AE rifles' numbers (ae_rifle_profiles.h). No engine includes.

Where they come from (numbers only; no game data or code is copied):
- BATTLE RIFLE, the casual rifle (10-30: AE's 3x; approved in
  play, test burst-a): the burst (3 rounds 2 ticks apart, 6.5 damage a round, shield x1.06 so the 4th burst's
  last round can be the killing head shot) with a recovery (0.3 s: in AE's ticks 8 make its 1.55 s perfect
  kill, the bursts 14 ticks apart, 1.53 s), no spread, 36 loaded, 144 at first, 216 in reserve, aim assist 2.2 degrees
  to 18 wu and magnetism 5.75 degrees to 30 wu. The bullet: 400 wu a second to 40 wu.
- EVO RIFLE, the competitive rifle (10-30): one round a pull, 4 rounds
  empty CE's shield (a quarter of the 11 burst rounds that do: 17.875) and the 5th to the head kills; a pull every 11
  ticks (the Evo's 2.715 a second, 0.37 s): a perfect kill in 44 ticks, 1.47 s; 15 loaded, 60 at first, 75 in
  reserve, a tracer every round; aim assist 2.0 degrees to 18 wu, magnetism 5.0 degrees to 25 wu, no spread; 2x as
  seen (the owner; the Evo's own is 1.6x).
- DMR, the zero-bloom marksman (the owner's "stock DMR", 10-30, "good to ship"): one round a pull, 3 a second (a pull
  every 10 ticks), the EVO RIFLE's damage (a perfect kill in 40 ticks, 1.33 s), 15 loaded, 45 at first, 60 in reserve,
  autoaim 2.25 degrees to 20 wu, magnetism 7.0 degrees to 25 wu (the owner in play: stickier than the read 5.0 to 20),
  a constant 0.15 degree spread (no bloom), a tracer every round, 3x as seen, a bullet of 3000 wu a second to 250 wu.
Its numbers are in CE's world units.
The aim reticles' growth a shot (the owner 10-30, a test: his picks' firing examples): the EVO RIFLE's whole reticle
(a ring and a ringed dot) grows 35 % and is back in 0.3 s, before the next pull; the BATTLE RIFLE's ring keeps its
size and the four ticks spread 35 % once a burst (its rounds don't start it again: the owner, 10-30, "a little less
busy"), back in 0.3 s, before the next burst; the zero-bloom DMR's middle (the dot and the inner ring)
stays on the target while its outer ring kicks out 35 % and is back in 0.3 s, before the next shot (the owner 10-30: "the
same treatment ... an animation pass"; the drawing only, its spread stays fixed).
*/

#include <stddef.h>
#include <stdio.h>

#include "ae_rifle_profiles.h"

/* (ae_pack_format.h's AE_PACK_BEHAVIOUR_AE_*: the pack format's values, kept apart from its engine includes) */
#define BEHAVIOUR_AE_BATTLE_RIFLE 1
#define BEHAVIOUR_AE_EVO_RIFLE 2
#define BEHAVIOUR_AE_DMR 3

/* 6.5 a round; the single shots' a quarter of the 11 burst rounds that empty the shield */
#define BURST_ROUND_DAMAGE 6.5f
#define SINGLE_ROUND_DAMAGE (BURST_ROUND_DAMAGE * 11.0f / 4.0f)

static struct ae_rifle_profile const profiles[AE_RIFLE_KINDS] =
{
	/* name, rounds a pull, keeps a pull, recovery ticks, damage, loaded, at first, reserve, between tracers,
	autoaim degrees / wu, magnetism degrees / wu, spread lower / upper, zoom seen, bullet speed / range,
	reticle growth / ms / fixed items, scope readouts */
	{ "BATTLE RIFLE", 3, FALSE, 8, BURST_ROUND_DAMAGE, 36, 144, 216, 2,
		2.2f, 18.0f, 5.75f, 30.0f, 0.0f, 0.0f, 3.0f, 400.0f, 40.0f, 0.35f, 300, 0x01, FALSE },
	{ "EVO RIFLE", 1, TRUE, 9, SINGLE_ROUND_DAMAGE, 15, 60, 75, 0,
		2.0f, 18.0f, 5.0f, 25.0f, 0.0f, 0.0f, 2.0f, 400.0f, 40.0f, 0.35f, 300, 0x00, TRUE },
	{ "DMR", 1, TRUE, 8, SINGLE_ROUND_DAMAGE, 15, 45, 60, 0,
		2.25f, 20.0f, 7.0f, 25.0f, 0.15f, 0.15f, 3.0f, 3000.0f, 250.0f, 0.35f, 300, 0x01, TRUE },
};

enum ae_rifle_kind ae_rifle_kind_of_behaviour(unsigned int behaviour)
{
	switch (behaviour)
	{
	case BEHAVIOUR_AE_BATTLE_RIFLE: return AE_RIFLE_BATTLE_RIFLE;
	case BEHAVIOUR_AE_EVO_RIFLE: return AE_RIFLE_EVO_RIFLE;
	case BEHAVIOUR_AE_DMR: return AE_RIFLE_DMR;
	default: return AE_RIFLE_NONE;
	}
}

struct ae_rifle_profile const *ae_rifle_profile(enum ae_rifle_kind kind)
{
	return kind >= 0 && kind < AE_RIFLE_KINDS ? &profiles[kind] : NULL;
}

/* a pull's rounds go out 2 ticks apart; the next pull comes the tick after the recovery, which starts the tick after
the pull's last round: pulls (2 x rounds + recovery) ticks apart. A burst rifle's perfect kill is 4 bursts (the 12th
round the head shot), a single shot's 5 pulls */
long ae_rifle_perfect_kill_ticks(enum ae_rifle_kind kind)
{
	struct ae_rifle_profile const *profile = ae_rifle_profile(kind);
	long spacing, pulls;

	if (!profile)
		return 0;
	spacing = 2L * profile->rounds_a_pull + profile->recovery_ticks;
	pulls = profile->rounds_a_pull > 1 ? 4 : 5;
	return (pulls - 1) * spacing + 2L * (profile->rounds_a_pull - 1);
}

unsigned long ae_rifle_pull_ms(struct ae_rifle_profile const *profile)
{
	return ((unsigned long)(profile->rounds_a_pull - 1) * 2UL + 1UL) * 1000UL / 30UL;
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
