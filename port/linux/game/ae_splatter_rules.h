/*
AE_SPLATTER_RULES.H

FRIENDLY SPLATTER as a gametype rule (owner 2026-10-26), pure (port/linux/tests/ae_splatter_rules_test.c): how much of
Halo CE's vehicle collision damage a teammate of the vehicle's driver takes. HALO CE: all of it from 0.067 world units
a tick (2 a second: a warthog nudging a teammate kills him). FAST ONLY: none below AE_SPLATTER_MINIMUM (the vehicle's
own speed), growing to all of it at AE_SPLATTER_FULL (the owner's tuning: a hard ram is 0.20-0.23). Enemies and
driverless vehicles keep Halo CE's rule; FRIENDLY FIRE OFF already spares teammates (the damage is the driver's).

The gametype keeps it in bit 3 of the universal variant's pad5 (bits 0-1 COMBAT FEEDBACK, 2 JUGGERNAUT, 4-7
SILENT): set = FAST ONLY, so every gametype from before plays HALO CE. Only the host's matters (the damage is the
host's); the byte travels with the variant, the file's and the network's sizes unchanged.
*/

#ifndef __AE_SPLATTER_RULES_H
#define __AE_SPLATTER_RULES_H

#define AE_SPLATTER_BYTE_BIT 0x08u
#define AE_SPLATTER_MINIMUM 0.10f
#define AE_SPLATTER_FULL 0.20f

static inline int ae_splatter_fast_only(unsigned char byte)
{
	return (byte & AE_SPLATTER_BYTE_BIT) != 0;
}

static inline unsigned char ae_splatter_byte_set(unsigned char byte, int fast_only)
{
	return (unsigned char)(fast_only ? (byte | AE_SPLATTER_BYTE_BIT) : (byte & ~AE_SPLATTER_BYTE_BIT));
}

/* the share of the damage a biped takes: 1 for an enemy or under HALO CE; under FAST ONLY a teammate's grows from 0
at the minimum speed to 1 at the full one (speed: the vehicle's own, world units a tick) */
static inline float ae_splatter_scale(int fast_only, int teammate, float speed)
{
	float scale;

	if (!fast_only || !teammate)
		return 1.0f;
	scale = (speed - AE_SPLATTER_MINIMUM) / (AE_SPLATTER_FULL - AE_SPLATTER_MINIMUM);
	return scale < 0.0f ? 0.0f : scale > 1.0f ? 1.0f : scale;
}

static inline char const *ae_splatter_rule_name(int fast_only)
{
	return fast_only ? "FAST ONLY" : "HALO CE";
}

#endif
