/*
AE_DS5_RULES.C

Arena Evolved: the DualSense's output bytes (ae_ds5_rules.h). The zone-based trigger effects are packed as in
John "Nielk1" Klein's TriggerEffectGenerator (MIT licence, 2021-2022); the body layout and the report framing follow
SDL3's PS5 driver (zlib licence). Pure: no SDL, no game.
*/

#include <stdio.h>
#include <string.h>

#include "ae_ds5_rules.h"

static int clamp(int value, int low, int high)
{
	return value < low ? low : value > high ? high : value;
}

enum ae_ds5_weapon_kind ae_ds5_weapon_kind(int burst_rifle, int needler, float rate_of_fire, float charging_time,
	float heat_per_round, int zoom_levels, int no_projectile, int plasma_rifle_type)
{
	if (no_projectile)
		return AE_DS5_WEAPON_CARRY;
	if (burst_rifle)
		return AE_DS5_WEAPON_BURST;
	if (charging_time > 0.0f)
		return AE_DS5_WEAPON_CHARGE;
	if (zoom_levels >= 2 && rate_of_fire < 3.0f)
		return AE_DS5_WEAPON_SNIPER;
	if (needler)
		return AE_DS5_WEAPON_NEEDLER;
	if (heat_per_round > 0.0f && rate_of_fire >= 4.0f)
		return plasma_rifle_type ? AE_DS5_WEAPON_PLASMA_HEAT : AE_DS5_WEAPON_FLAME;
	if (rate_of_fire >= 6.0f)
		return AE_DS5_WEAPON_AUTOMATIC;
	if (rate_of_fire > 0.0f && rate_of_fire < 1.6f)
		return AE_DS5_WEAPON_HEAVY;
	return AE_DS5_WEAPON_SEMI;
}

char const *ae_ds5_weapon_kind_name(enum ae_ds5_weapon_kind kind)
{
	static char const *const names[AE_DS5_WEAPON_KINDS] = {
		"none", "semi", "heavy", "automatic", "sniper", "charge", "burst", "needler", "plasma heat", "carry", "flame" };

	return kind >= 0 && kind < AE_DS5_WEAPON_KINDS ? names[kind] : "?";
}

void ae_ds5_effect_off(unsigned char effect[AE_DS5_EFFECT_SIZE])
{
	memset(effect, 0, AE_DS5_EFFECT_SIZE);
	effect[0] = 0x05;
}

/* zones `position` to 9 active, each with the same 3-bit level (Feedback and Vibration share it) */
static void zones(unsigned char effect[AE_DS5_EFFECT_SIZE], int position, int level)
{
	unsigned long active = 0, levels = 0;
	int zone;

	for (zone = position; zone < 10; zone++)
	{
		active |= 1UL << zone;
		levels |= (unsigned long)(level & 7) << (3 * zone);
	}
	effect[1] = (unsigned char)(active & 0xff);
	effect[2] = (unsigned char)((active >> 8) & 0xff);
	effect[3] = (unsigned char)(levels & 0xff);
	effect[4] = (unsigned char)((levels >> 8) & 0xff);
	effect[5] = (unsigned char)((levels >> 16) & 0xff);
	effect[6] = (unsigned char)((levels >> 24) & 0xff);
}

void ae_ds5_effect_feedback(unsigned char effect[AE_DS5_EFFECT_SIZE], int position, int strength)
{
	if (strength <= 0)
	{
		ae_ds5_effect_off(effect);
		return;
	}
	memset(effect, 0, AE_DS5_EFFECT_SIZE);
	effect[0] = 0x21;
	zones(effect, clamp(position, 0, 9), clamp(strength, 1, 8) - 1);
}

void ae_ds5_effect_weapon(unsigned char effect[AE_DS5_EFFECT_SIZE], int start, int end, int strength)
{
	unsigned long both;

	if (strength <= 0)
	{
		ae_ds5_effect_off(effect);
		return;
	}
	start = clamp(start, 2, 7);
	end = clamp(end, start + 1, 8);
	both = (1UL << start) | (1UL << end);
	memset(effect, 0, AE_DS5_EFFECT_SIZE);
	effect[0] = 0x25;
	effect[1] = (unsigned char)(both & 0xff);
	effect[2] = (unsigned char)((both >> 8) & 0xff);
	effect[3] = (unsigned char)(clamp(strength, 1, 8) - 1);
}

void ae_ds5_effect_vibration(unsigned char effect[AE_DS5_EFFECT_SIZE], int position, int amplitude, int frequency)
{
	if (amplitude <= 0 || frequency <= 0)
	{
		ae_ds5_effect_off(effect);
		return;
	}
	memset(effect, 0, AE_DS5_EFFECT_SIZE);
	effect[0] = 0x26;
	zones(effect, clamp(position, 0, 9), clamp(amplitude, 1, 8) - 1);
	effect[9] = (unsigned char)clamp(frequency, 1, 255);
}

/* a shape: OFF, FEEDBACK (a position, b strength), WEAPON (a start, b strength, c end), VIBRATION (a position,
b amplitude, c frequency; c 0: the gun's rate of fire times the profile's per_round) */
enum { OFF, FEEDBACK, WEAPON, VIBRATION, SAME };
struct shape
{
	unsigned char mode, a, b, c;
};

/* ONE profile a weapon kind: THE TUNING TABLE (the owner's feel tests set it; TRIGGER STRENGTH scales every b):
- held: the trigger as it rests and is pulled;
- firing: while a round went in the last ~170 ms (SAME: the held one), e.g. a vibration at the rate of fire, one
  pulse a round, timed by the pad itself (no report a round);
- kick: a short shape sent at each round, then back to held/firing after kick_ms (two reports a round);
- ramp: 1 heat, 2 charge: the held and firing shapes' b (strength / amplitude) goes from its table value at 0 to
  ramp_to at 1; locked: the shape while overheated;
- the weapon's state comes first, for every kind (ae_ds5_right_held): readying, overheated without a locked shape,
  reloading and empty let the trigger go. */
#define NO { OFF, 0, 0, 0 }
#define SM { SAME, 0, 0, 0 }
#define F(position, strength) { FEEDBACK, position, strength, 0 }
#define W(start, end, strength) { WEAPON, start, strength, end }
#define V(position, amplitude, frequency) { VIBRATION, position, amplitude, frequency }
static struct profile
{
	struct shape held, firing, kick, locked;
	int kick_ms;
	int per_round;
	int ramp, ramp_to;
	char const *feel;
	/* the kick of a fully charged shot (charged_kick_ms 0: the ordinary kick) */
	struct shape charged_kick;
	int charged_kick_ms;
} const profiles[AE_DS5_WEAPON_KINDS] = {
	/* none */ { NO, SM, NO, NO, 0, 0, 0, 0, "off", NO, 0 },
	/* semi (pistol): its click, a sharp snap back each shot (3.5 shots/s: 35 ms of 285) */
	{ W(4, 7, 4), SM, F(0, 8), NO, 35, 0, 0, 0, "click, snap a shot", NO, 0 },
	/* heavy (shotgun, rockets): a heavy click, a big kick each shot (a 30 Hz full jolt, 130 ms of ~700) */
	{ W(3, 7, 6), SM, V(0, 8, 30), NO, 130, 0, 0, 0, "heavy click, big kick", NO, 0 },
	/* automatic (assault rifle): light hold; firing, round 1's buzz at 1x the rate of fire = a pulse a round */
	{ F(3, 2), V(3, 5, 0), NO, NO, 0, 1, 0, 0, "light hold, a pulse a round", NO, 0 },
	/* sniper: firm, a heavy thump each shot (2 shots/s: 90 ms of 500) */
	{ F(2, 6), SM, F(0, 8), NO, 90, 0, 0, 0, "firm, heavy thump", NO, 0 },
	/* charge (plasma pistol): resistance from zone 2 (round 1's zone 4 was not felt) building to full with the
	charge; each ordinary shot a powerful pulse (a full 40 Hz jolt, 70 ms), the overcharged shot's release a big heavy
	kick, longer and lower than the shotgun's (a full 15 Hz jolt, 240 ms) */
	{ F(2, 4), SM, V(0, 8, 40), NO, 70, 0, 2, 8, "builds with charge, a pulse a shot, a blast charged",
		V(0, 8, 15), 240 },
	/* burst (battle rifle): its click, three quick pulses a burst (15 rounds/s inside it: 30 ms of 67). Zones 4-7 (the
	pistol's, felt in play) at strength 6 (the shotgun's, felt): round 1's zones 3-6 were not felt at all */
	{ W(4, 7, 6), SM, F(0, 7), NO, 30, 0, 0, 0, "click, three pulses a burst", NO, 0 },
	/* needler: barely there; firing, a fast light buzz (2x the rate of fire, amplitude 3) */
	{ F(3, 1), V(3, 3, 0), NO, NO, 0, 2, 0, 0, "fast light buzz", NO, 0 },
	/* plasma heat (plasma rifle): resistance and buzz rising with heat, locked when overheated */
	{ F(2, 1), V(2, 3, 0), NO, F(0, 8), 0, 1, 1, 7, "rises with heat, locked overheated", NO, 0 },
	/* carry (ball, flag: no projectile, R2 swings it): free, nothing to shoot (ruled off) */
	{ NO, SM, NO, NO, 0, 0, 0, 0, "free (melee carry)", NO, 0 },
	/* flame (flamethrower: heat a round, not the plasma rifle's weapon type): light hold; firing, a low 18 Hz roar
	rising with heat */
	{ F(3, 2), V(3, 3, 18), NO, NO, 0, 0, 1, 6, "light hold, a roar while it burns", NO, 0 },
};
#undef NO
#undef SM
#undef F
#undef W
#undef V

static int ramped(int base, int to, float amount)
{
	if (amount <= 0.0f)
		return base;
	if (amount >= 1.0f)
		return to;
	return base + (int)((float)(to - base) * amount + 0.5f);
}

/* the shape's bytes; text: its words */
/* a strength or amplitude (1-8) scaled by TRIGGER STRENGTH's percent, still 1-8 */
static int scaled(int value, int percent)
{
	if (value <= 0)
		return value;
	return clamp((value * percent + 50) / 100, 1, 8);
}

static void shape_effect(unsigned char effect[AE_DS5_EFFECT_SIZE], struct shape shape, float rate_of_fire, int per_round,
	int strength, char *text, size_t text_size)
{
	int frequency;

	shape.b = (unsigned char)scaled(shape.b, strength);

	switch (shape.mode)
	{
	case FEEDBACK:
		ae_ds5_effect_feedback(effect, shape.a, shape.b);
		snprintf(text, text_size, "feedback from zone %d strength %d", shape.a, shape.b);
		break;
	case WEAPON:
		ae_ds5_effect_weapon(effect, shape.a, shape.c, shape.b);
		snprintf(text, text_size, "click zones %d-%d strength %d", shape.a, shape.c, shape.b);
		break;
	case VIBRATION:
		frequency = shape.c ? shape.c : clamp((int)(rate_of_fire * (float)(per_round > 0 ? per_round : 1) + 0.5f), 6, 60);
		ae_ds5_effect_vibration(effect, shape.a, shape.b, frequency);
		snprintf(text, text_size, "vibration from zone %d amplitude %d at %d Hz", shape.a, shape.b, frequency);
		break;
	default:
		ae_ds5_effect_off(effect);
		snprintf(text, text_size, "off");
		break;
	}
}

void ae_ds5_right_held(unsigned char effect[AE_DS5_EFFECT_SIZE], struct ae_ds5_weapon_state const *state, int strength,
	char *text, size_t text_size)
{
	struct profile const *profile = &profiles[state->kind >= 0 && state->kind < AE_DS5_WEAPON_KINDS ? state->kind : 0];
	struct shape shape = profile->held;
	char words[80];
	char const *why = "held";

	char const *free_why = NULL;

	if (shape.mode == OFF)
		free_why = state->kind == AE_DS5_WEAPON_CARRY ? "free (carrying)" : "off";
	else if (state->readying)
		free_why = "free (readying)";
	else if (state->overheated && profile->locked.mode == OFF)
		free_why = "free (overheated, venting)";
	else if (state->reloading && !state->overheated)
		free_why = "free (reloading)";
	else if (state->empty && !state->overheated)
		free_why = "free (empty)";
	if (free_why)
	{
		ae_ds5_effect_off(effect);
		snprintf(text, text_size, "%s", free_why);
		return;
	}
	if (state->overheated)
	{
		shape = profile->locked;
		why = "locked (overheated)";
	}
	else
	{
		if (state->firing && profile->firing.mode != SAME)
		{
			shape = profile->firing;
			why = "firing";
		}
		if (profile->ramp == 1)
			shape.b = (unsigned char)ramped(shape.b, profile->ramp_to, state->heat);
		else if (profile->ramp == 2)
			shape.b = (unsigned char)ramped(shape.b, profile->ramp_to, state->charge);
	}
	shape_effect(effect, shape, state->rate_of_fire, profile->per_round, strength, words, sizeof(words));
	snprintf(text, text_size, "%s: %s", why, words);
}

int ae_ds5_right_kick(unsigned char effect[AE_DS5_EFFECT_SIZE], enum ae_ds5_weapon_kind kind, int charged, int strength)
{
	struct profile const *profile = &profiles[kind >= 0 && kind < AE_DS5_WEAPON_KINDS ? kind : 0];
	char words[80];

	if (charged && profile->charged_kick.mode != OFF && profile->charged_kick_ms > 0)
	{
		shape_effect(effect, profile->charged_kick, 0.0f, 1, strength, words, sizeof(words));
		return profile->charged_kick_ms;
	}
	if (profile->kick.mode == OFF || profile->kick_ms <= 0)
	{
		ae_ds5_effect_off(effect);
		return 0;
	}
	shape_effect(effect, profile->kick, 0.0f, 1, strength, words, sizeof(words));
	return profile->kick_ms;
}

char const *ae_ds5_profile_feel(enum ae_ds5_weapon_kind kind)
{
	return profiles[kind >= 0 && kind < AE_DS5_WEAPON_KINDS ? kind : 0].feel;
}

char const *ae_ds5_left_effect(unsigned char effect[AE_DS5_EFFECT_SIZE], int zoomed, int strength)
{
	if (zoomed)
	{
		ae_ds5_effect_feedback(effect, 2, scaled(2, strength));
		return "light resistance (from zone 2)";
	}
	ae_ds5_effect_off(effect);
	return "off";
}

#define JUGGERNAUT_FLASH_TICKS 20
#define JUGGERNAUT_BUZZ_TICKS 12
#define CHARGE_PULSE_TICKS 15
#define HELD_BREATH_TICKS 60

/* a triangle wave 0..1..0 over `period` ticks, in `steps` + 1 levels */
static float wave(long ticks, long period, int steps)
{
	long phase = ticks % period;
	long half = period / 2;
	float value = phase < half ? (float)phase / (float)half : (float)(period - phase) / (float)(period - half);

	return (float)(int)(value * (float)steps + 0.5f) / (float)steps;
}

void ae_ds5_lightbar(unsigned char rgb[3], unsigned char const base[3], struct ae_ds5_powerups const *powerups,
	char *text, size_t text_size)
{
	unsigned char colour[3];
	float scale = 1.0f, white = 0.0f;
	char const *what = "colour";
	int i;

	memcpy(colour, base, 3);
	if (colour[0] < 24 && colour[1] < 24 && colour[2] < 24)
	{
		colour[0] = colour[1] = colour[2] = 48;
		what = "dim white (black armour)";
	}
	if (powerups->juggernaut_ticks >= 0 && powerups->juggernaut_ticks < JUGGERNAUT_FLASH_TICKS)
	{
		white = 1.0f - (float)(int)((float)powerups->juggernaut_ticks / (float)JUGGERNAUT_FLASH_TICKS * 5.0f) / 5.0f;
		what = "juggernaut flash";
	}
	else if (powerups->shield > 1.0f && powerups->shield_rising)
	{
		scale = 0.35f + 0.65f * wave(powerups->ticks, CHARGE_PULSE_TICKS, 5);
		what = "overshield charging (2 Hz pulse)";
	}
	else if (powerups->shield > 1.0f)
	{
		scale = 0.7f + 0.3f * wave(powerups->ticks, HELD_BREATH_TICKS, 3);
		what = "overshield (slow breath)";
	}
	if (powerups->camo > 0.05f)
	{
		float camo = (float)(int)(powerups->camo * 10.0f + 0.5f) / 10.0f;

		scale *= 1.0f - 0.85f * (camo > 1.0f ? 1.0f : camo);
		what = "active camouflage (dimmed)";
	}
	for (i = 0; i < 3; i++)
	{
		float value = ((float)colour[i] + (255.0f - (float)colour[i]) * white) * scale;

		rgb[i] = (unsigned char)(value >= 255.0f ? 255 : value <= 0.0f ? 0 : (int)(value + 0.5f));
	}
	snprintf(text, text_size, "%s", what);
}

int ae_ds5_left_powerup(unsigned char effect[AE_DS5_EFFECT_SIZE], struct ae_ds5_powerups const *powerups, int strength,
	char *text, size_t text_size)
{
	if (powerups->juggernaut_ticks >= 0 && powerups->juggernaut_ticks < JUGGERNAUT_BUZZ_TICKS)
	{
		ae_ds5_effect_vibration(effect, 0, scaled(7, strength), 25);
		snprintf(text, text_size, "juggernaut buzz (vibration amplitude %d at 25 Hz)", scaled(7, strength));
		return 1;
	}
	if (powerups->shield > 1.0f && powerups->shield_rising)
	{
		float amount = (powerups->shield - 1.0f) / 2.0f;
		int amplitude = scaled(1 + (int)((amount > 1.0f ? 1.0f : amount) * 5.0f + 0.5f), strength);

		ae_ds5_effect_vibration(effect, 0, amplitude, 30);
		snprintf(text, text_size, "overshield charging (vibration amplitude %d at 30 Hz)", amplitude);
		return 1;
	}
	return 0;
}

#define SHIELD_POP_FLASH_TICKS 6
#define SHIELD_LOW_BLINK_TICKS 8
#define SHIELD_STEPS 16
/* (the HUD's blue, for a meter with no colours; the HUD's red for an empty meter with none) */
#define HUD_BLUE 0x2896ffUL
#define HUD_RED 0xff2020UL

static void colour_scaled(unsigned char rgb[3], unsigned long colour, float scale)
{
	int i;

	for (i = 0; i < 3; i++)
	{
		float value = (float)((colour >> (16 - 8 * i)) & 0xff) * scale;

		rgb[i] = (unsigned char)(value >= 255.0f ? 255 : value <= 0.0f ? 0 : (int)(value + 0.5f));
	}
}

static unsigned long colour_mix(unsigned long from, unsigned long to, float amount)
{
	unsigned long result = 0;
	int i;

	for (i = 0; i < 3; i++)
	{
		int shift = 16 - 8 * i;
		float a = (float)((from >> shift) & 0xff), b = (float)((to >> shift) & 0xff);

		result |= (unsigned long)(int)(a + (b - a) * amount + 0.5f) << shift;
	}
	return result;
}

static float stepped(float value)
{
	if (value <= 0.0f)
		return 0.0f;
	if (value >= 1.0f)
		return 1.0f;
	return (float)(int)(value * (float)SHIELD_STEPS + 0.5f) / (float)SHIELD_STEPS;
}

void ae_ds5_lightbar_shield(unsigned char rgb[3], struct ae_ds5_shield const *shield, char *text, size_t text_size)
{
	/* (hud_unit.c's overcharge layers: none, then red, green, yellow, violet) */
	static unsigned long const layers[5] = { 0, 0xff0000UL, 0x00ff00UL, 0xffff00UL, 0x7f00ffUL };
	unsigned long min_color = shield->min_color & 0xffffffUL, max_color = shield->max_color & 0xffffffUL;
	unsigned long flash = shield->flash_color & 0xffffffUL, empty = shield->empty_color & 0xffffffUL;
	float fill;

	if (!min_color && !max_color)
		min_color = max_color = HUD_BLUE;
	else if (!min_color)
		min_color = max_color;
	else if (!max_color)
		max_color = min_color;
	if (shield->dead)
	{
		colour_scaled(rgb, max_color, 0.06f);
		snprintf(text, text_size, "shield: dark (dead)");
		return;
	}
	if (shield->pop_flash && shield->popped_ticks >= 0 && shield->popped_ticks < SHIELD_POP_FLASH_TICKS)
	{
		colour_scaled(rgb, 0xffffffUL, 1.0f);
		snprintf(text, text_size, "shield: popped (white flash)");
		return;
	}
	if (shield->vitality > 1.0f)
	{
		int layer = (int)shield->vitality;
		float part = shield->vitality - (float)layer;

		/* (exactly full layers: the one below, full) */
		if (part <= 0.0f)
		{
			layer--;
			part = 1.0f;
		}
		layer = layer < 1 ? 1 : layer > 4 ? 4 : layer;
		colour_scaled(rgb, layers[layer], 0.4f + 0.6f * stepped(part));
		snprintf(text, text_size, "shield: overshield layer %d", layer);
		return;
	}
	if (shield->vitality <= 0.0f)
	{
		colour_scaled(rgb, empty ? empty : HUD_RED, 0.6f);
		snprintf(text, text_size, "shield: empty");
		return;
	}
	fill = stepped(shield->vitality);
	if (shield->vitality < 0.25f && (shield->ticks / (SHIELD_LOW_BLINK_TICKS / 2)) % 2)
	{
		colour_scaled(rgb, flash ? flash : HUD_RED, 0.25f + 0.75f * fill);
		snprintf(text, text_size, "shield: low (blink)");
		return;
	}
	colour_scaled(rgb, colour_mix(min_color, max_color, fill), 0.25f + 0.75f * fill);
	snprintf(text, text_size, "%s", shield->vitality < 0.25f ? "shield: low (blink)" : "shield: meter");
}

int ae_ds5_strength_percent(char const *name)
{
	if (!strcmp(name, "light"))
		return 60;
	if (!strcmp(name, "strong"))
		return 140;
	return 100;
}

/* (SDL's PS5 driver's pattern for each player, zlib licence: 1 the middle light, 2 the two beside it, 3 the middle
and the two outer ones, 4 all but the middle; 0x20 changes them at once instead of fading) */
static unsigned char const player_lights[4] = { 0x04, 0x0a, 0x15, 0x1b };

unsigned char ae_ds5_player_lights(int player_number)
{
	if (player_number < 1 || player_number > 4)
		return 0;
	return (unsigned char)(player_lights[player_number - 1] | 0x20);
}

int ae_ds5_player_index_of_lights(unsigned char lights)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		if ((lights & 0x1f) == player_lights[i])
			return i;
	}
	return -1;
}

unsigned long ae_ds5_crc32(unsigned long crc, unsigned char const *data, size_t size)
{
	size_t i;
	int bit;

	crc = ~crc & 0xffffffffUL;
	for (i = 0; i < size; i++)
	{
		crc ^= data[i];
		for (bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
	}
	return ~crc & 0xffffffffUL;
}

size_t ae_ds5_output_report(unsigned char *report, size_t size, int bluetooth, int sequence,
	unsigned char const body[AE_DS5_BODY_SIZE])
{
	if (!bluetooth)
	{
		if (size < AE_DS5_USB_REPORT_SIZE)
			return 0;
		memset(report, 0, AE_DS5_USB_REPORT_SIZE);
		report[0] = 0x02;
		memcpy(report + 1, body, AE_DS5_BODY_SIZE);
		return AE_DS5_USB_REPORT_SIZE;
	}
	else
	{
		static unsigned char const hidp_header = 0xA2; /* the Bluetooth HID "data, output" header, part of the CRC */
		unsigned long crc;

		if (size < AE_DS5_BLUETOOTH_REPORT_SIZE)
			return 0;
		memset(report, 0, AE_DS5_BLUETOOTH_REPORT_SIZE);
		report[0] = 0x31;
		report[1] = (unsigned char)((sequence & 0x0f) << 4);
		report[2] = 0x10;
		memcpy(report + 3, body, AE_DS5_BODY_SIZE);
		crc = ae_ds5_crc32(0, &hidp_header, 1);
		crc = ae_ds5_crc32(crc, report, AE_DS5_BLUETOOTH_REPORT_SIZE - 4);
		report[74] = (unsigned char)(crc & 0xff);
		report[75] = (unsigned char)((crc >> 8) & 0xff);
		report[76] = (unsigned char)((crc >> 16) & 0xff);
		report[77] = (unsigned char)((crc >> 24) & 0xff);
		return AE_DS5_BLUETOOTH_REPORT_SIZE;
	}
}
