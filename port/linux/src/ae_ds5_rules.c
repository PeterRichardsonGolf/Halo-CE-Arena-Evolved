/*
AE_DS5_RULES.C

Arena Evolved: the DualSense's output bytes (ae_ds5_rules.h). The zone-based trigger effects are packed as in
John "Nielk1" Klein's TriggerEffectGenerator (MIT licence, 2021-2022); the body layout and the report framing follow
SDL3's PS5 driver (zlib licence). Pure: no SDL, no game.
*/

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "ae_ds5_rules.h"

static int clamp(int value, int low, int high)
{
	return value < low ? low : value > high ? high : value;
}

enum ae_ds5_weapon_kind ae_ds5_weapon_kind(int burst_rifle, int pack, int needler, float rate_of_fire,
	float charging_time, float heat_per_round, int zoom_levels, int no_projectile, int plasma_rifle_type)
{
	if (no_projectile)
		return AE_DS5_WEAPON_CARRY;
	if (burst_rifle || pack == AE_DS5_PACK_BATTLE_RIFLE)
		return AE_DS5_WEAPON_BURST;
	/* (the packs' weapons by the pack, not their numbers: the SMGs' 15 rounds a second read as the assault rifle's,
	the carbine's 5 as the pistol's) */
	if (pack == AE_DS5_PACK_SMG)
		return AE_DS5_WEAPON_SMG;
	if (pack == AE_DS5_PACK_SMG_SILENCED)
		return AE_DS5_WEAPON_SMG_SILENCED;
	if (pack == AE_DS5_PACK_CARBINE)
		return AE_DS5_WEAPON_CARBINE;
	if (pack == AE_DS5_PACK_AR_SILENCED)
		return AE_DS5_WEAPON_AR_SILENCED;
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
		"none", "semi", "heavy", "automatic", "sniper", "charge", "burst", "needler", "plasma heat", "carry", "flame",
		"smg", "silenced smg", "carbine", "silenced ar" };

	return kind >= 0 && kind < AE_DS5_WEAPON_KINDS ? names[kind] : "?";
}

void ae_ds5_effect_off(unsigned char effect[AE_DS5_EFFECT_SIZE])
{
	memset(effect, 0, AE_DS5_EFFECT_SIZE);
	effect[0] = 0x05;
}

/* zones `position` to `last` active, each with the same 3-bit level (Feedback and Vibration share it) */
static void zones(unsigned char effect[AE_DS5_EFFECT_SIZE], int position, int last, int level)
{
	unsigned long active = 0, levels = 0;
	int zone;

	for (zone = position; zone <= last && zone < 10; zone++)
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
	zones(effect, clamp(position, 0, 9), 9, clamp(strength, 1, 8) - 1);
}

void ae_ds5_effect_feedback_span(unsigned char effect[AE_DS5_EFFECT_SIZE], int first, int last, int strength)
{
	if (strength <= 0)
	{
		ae_ds5_effect_off(effect);
		return;
	}
	first = clamp(first, 0, 9);
	memset(effect, 0, AE_DS5_EFFECT_SIZE);
	effect[0] = 0x21;
	zones(effect, first, clamp(last, first, 9), clamp(strength, 1, 8) - 1);
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
	zones(effect, clamp(position, 0, 9), 9, clamp(amplitude, 1, 8) - 1);
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
- ramp: 1 heat: the held and firing shapes' b (strength / amplitude) goes from its table value at 0 to ramp_to at
  1; 2 charge: while charging, a hum from the held shape's zone whose amplitude goes so (the CHARGING HUM);
- the family (the owner's language, 0.3.0): HUMAN weapons are mechanical, clicks and kicks, their rumble on the LEFT
  (heavy) motor; COVENANT weapons are energy, hum and resistance with heat or charge, their rumble on the RIGHT
  (light) motor. shot_rumble: that motor's pulse at each round (0-255) and how long (ms);
- charged_kick / charged_rumble: a fully charged shot's instead (the plasma pistol's blast; it rumbles both motors);
- the weapon's state comes first, for every kind (ae_ds5_right_held): readying, overheated, reloading, empty. */
#define NO { OFF, 0, 0, 0 }
#define SM { SAME, 0, 0, 0 }
#define F(position, strength) { FEEDBACK, position, strength, 0 }
#define W(start, end, strength) { WEAPON, start, strength, end }
#define V(position, amplitude, frequency) { VIBRATION, position, amplitude, frequency }
#define HUMAN 0
#define COVENANT 1
static struct profile
{
	struct shape held, firing, kick;
	int kick_ms;
	int per_round;
	int ramp, ramp_to;
	int family;
	int shot_rumble, shot_rumble_ms;
	char const *feel;
	struct shape charged_kick;
	int charged_kick_ms;
	int charged_rumble_left, charged_rumble_right, charged_rumble_ms;
} const profiles[AE_DS5_WEAPON_KINDS] = {
	/* (owner 10-21, 0.3.7: every strength re-spaced so STRONG has room above NORMAL and LIGHT below it: a hold at most
	4 here (STRONG 6), a click at most 5 (STRONG 7), a kick at most 7 (STRONG 8) but the plasma pistol's blast) */
	/* none */ { NO, SM, NO, 0, 0, 0, 0, HUMAN, 0, 0, "off", NO, 0, 0, 0, 0 },
	/* semi (pistol): its click, a sharp snap back each shot (3.5 shots/s: 35 ms of 285), a firm left pulse */
	{ W(4, 7, 4), SM, F(0, 7), 35, 0, 0, 0, HUMAN, 150, 60, "click, snap a shot", NO, 0, 0, 0, 0 },
	/* heavy (shotgun, rockets): a heavy click, a big kick each shot (a 30 Hz jolt, 130 ms of ~700), the left motor
	full */
	{ W(3, 7, 5), SM, V(0, 7, 30), 130, 0, 0, 0, HUMAN, 255, 160, "heavy click, big kick", NO, 0, 0, 0, 0 },
	/* automatic (assault rifle): light hold; firing, round 1's buzz at 1x the rate of fire = a pulse a round, a short
	left pulse a round (40 ms of 67) */
	{ F(3, 2), V(3, 5, 0), NO, 0, 1, 0, 0, HUMAN, 90, 40, "light hold, a pulse a round", NO, 0, 0, 0, 0 },
	/* sniper: firm, a heavy thump each shot (2 shots/s: 90 ms of 500), the left motor nearly full */
	{ F(2, 4), SM, F(0, 7), 90, 0, 0, 0, HUMAN, 230, 130, "firm, heavy thump", NO, 0, 0, 0, 0 },
	/* charge (plasma pistol): a hold from zone 2; while it charges a hum from there rising to full (faster once
	charged: ae_ds5_right_held's CHARGING HUM); each ordinary shot a powerful
	pulse (a 40 Hz jolt, 70 ms, the right motor); the overcharged release a big heavy kick, longer and lower than
	the shotgun's (a full 15 Hz jolt, 240 ms), both motors */
	{ F(2, 3), SM, V(0, 7, 40), 70, 0, 2, 8, COVENANT, 160, 60, "hums as it charges, a pulse a shot, a blast charged",
		V(0, 8, 15), 240, 170, 255, 240 },
	/* burst (battle rifle): a firm click; three small pulses a burst (one a round, 15 rounds/s inside it: the trigger's
	buzz at the rate of fire), a firm pulse on both motors a round on the burst's cadence (owner 10-15: 240 for 50 ms,
	rounds 67 ms apart, three distinct pulses; 120 for 40 ms was not felt) */
	{ W(4, 7, 5), V(3, 4, 0), NO, 0, 1, 0, 0, HUMAN, 220, 50, "firm click, three pulses a burst", NO, 0, 0, 0, 0 },
	/* needler: barely there; firing, a fast light buzz (2x the rate of fire, amplitude 3), a light right tick */
	{ F(3, 2), V(3, 3, 0), NO, 0, 2, 0, 0, COVENANT, 70, 30, "fast light buzz", NO, 0, 0, 0, 0 },
	/* plasma heat (plasma rifle): resistance and hum rising with heat (locked while overheated, as every weapon) */
	{ F(2, 2), V(2, 3, 0), NO, 0, 1, 1, 6, COVENANT, 80, 40, "rises with heat, locked overheated", NO, 0, 0, 0, 0 },
	/* carry (ball, flag: no projectile, R2 swings it): free, nothing to shoot (ruled off) */
	{ NO, SM, NO, 0, 0, 0, 0, HUMAN, 0, 0, "free (melee carry)", NO, 0, 0, 0, 0 },
	/* flame (flamethrower): light hold; firing, a low 18 Hz roar rising with heat, a low left rumble */
	{ F(3, 2), V(3, 3, 18), NO, 0, 0, 1, 6, HUMAN, 60, 60, "light hold, a roar while it burns", NO, 0, 0, 0, 0 },
	/* smg (AE's SMG pack, 15 rounds/s): the assault rifle's light hold; firing, a fast buzz (30 Hz, twice the
	assault rifle's, lighter: amplitude 4), a light left tick a round */
	{ F(3, 2), V(3, 4, 30), NO, 0, 1, 0, 0, HUMAN, 70, 40, "light hold, a fast buzz", NO, 0, 0, 0, 0 },
	/* silenced smg (AE's silenced SMG pack): the SMG's hold; firing, a softer, lower buzz (20 Hz, amplitude 3), a
	softer left tick */
	{ F(3, 2), V(3, 3, 20), NO, 0, 1, 0, 0, HUMAN, 45, 40, "light hold, a soft low buzz", NO, 0, 0, 0, 0 },
	/* carbine (AE's carbine pack, 5 shots/s, semi-automatic): a quick crisp click, shorter and lighter than the
	pistol's (zones 4-6, strength 3), a short snap each shot (30 ms of 200, lighter than the pistol's), a light
	pulse on the right motor (Covenant) */
	{ W(4, 6, 3), SM, F(0, 5), 30, 0, 0, 0, COVENANT, 110, 40, "crisp light click, quick snap a shot", NO, 0, 0, 0, 0 },
	/* silenced ar (AE's silenced assault rifle pack, the assault rifle's 15 rounds/s): the assault rifle's light hold;
	firing, its pulse a round (the buzz at the rate of fire) softer (amplitude 3, the assault rifle's 5), a softer
	left pulse a round (as the silenced SMG's beside the SMG's) */
	{ F(3, 2), V(3, 3, 0), NO, 0, 1, 0, 0, HUMAN, 55, 40, "light hold, a soft pulse a round", NO, 0, 0, 0, 0 },
};
#define THROB_TICKS 8
#define THROB_ON_TICKS 2

/* the charging hum's frequency (Hz), and the fully charged one's */
#define CHARGING_HUM_HZ 25
/* a shot's rumble pulse, at least (ms) */
#define SHOT_RUMBLE_MINIMUM_MS 50
/* L2's grenade (owner 10-21, after the first re-spacing: "needs a little less resistance on grenade should be like a
hair trigger"): on every preset a hair trigger, the lightest tick that is still felt, early in the pull (zones
GRENADE_TICK_START to GRENADE_TICK_END), nothing after it; the throw has no trigger kick, only the motor's pulse */
#define GRENADE_TICK_START 1
#define GRENADE_TICK_END 2
#define GRENADE_TICK_STRENGTH 2
#define GRENADE_THROW_RUMBLE 100
/* the battle rifle's round: the light motor with the heavy one */
#define BURST_RUMBLE_RIGHT 120
#define CHARGING_HUM_LOW 1
#define CHARGING_HUM_HIGH 3
/* the charge's rumble: a pulse every CHARGE_RUMBLE_TICKS, each a little longer so they run together; the right
motor from LOW to HIGH with the charge, the left from nothing to LEFT_HIGH */
#define CHARGE_RUMBLE_TICKS 3
#define CHARGE_RUMBLE_MS 140
#define CHARGE_RUMBLE_RIGHT_LOW 70
#define CHARGE_RUMBLE_RIGHT_HIGH 210
#define CHARGE_RUMBLE_LEFT_HIGH 110
#define CHARGED_HUM_HZ 40

static int ramped(int base, int to, float amount)
{
	if (amount <= 0.0f)
		return base;
	if (amount >= 1.0f)
		return to;
	return base + (int)((float)(to - base) * amount + 0.5f);
}

/* TRIGGER STRENGTH (owner 10-21 after 0.3.7: "reajust the trigger pull on all the settings"; before it LIGHT,
NORMAL and STRONG were x0.75, x1, x1.75 of every number, so STRONG's holds ran to 8, a stiff full pull). The table is
NORMAL's:
- a hold (resistance from a zone to the end of the pull): STRONG two steps up, at most HOLD_STRONGEST (firm; a full
  pull held through a fight does not tire);
- a click (resistance that gives way): STRONG two steps up, at most CLICK_STRONGEST;
- kicks and buzzes (a vibration, a kick from zone 0 after the shot, the overheat's lock): STRONG two steps up (at
  most 8);
- LIGHT is a HAIR TRIGGER on both triggers (owner 10-16: "i want light to have triggers hair trigger like"; 10-21 after
  the first re-spacing: "light mode still pretty instence on the trigger pulls. it should feel like a hair trigger on
  both triggers on light"): every hold and click, whatever the weapon, a tiny tick early in the pull (a resistance
  over zones HAIR_START to HAIR_END only, at HAIR_STRENGTH, nothing after it), so the pull is almost free; kicks and
  buzzes at most LIGHT_PULSE_MOST */
#define STRONG_STEPS 2
#define HOLD_STRONGEST 6
#define CLICK_STRONGEST 7
#define HAIR_START 1
#define HAIR_END 2
#define HAIR_STRENGTH 2
#define LIGHT_PULSE_MOST 2

static int hair_trigger(int percent)
{
	return percent <= AE_DS5_STRENGTH_LIGHT;
}

static int strong(int percent)
{
	return percent > 100;
}

/* a hold's or click's strength (NORMAL's value) on STRONG, never above `strongest` (nor under the value) */
static int held_strong(int value, int strongest)
{
	int result = value + STRONG_STEPS;

	if (result > strongest)
		result = strongest > value ? strongest : value;
	return clamp(result, 1, 8);
}

/* LIGHT's tick: early in the pull, nothing after it */
static void hair_tick(unsigned char effect[AE_DS5_EFFECT_SIZE])
{
	ae_ds5_effect_feedback_span(effect, HAIR_START, HAIR_END, HAIR_STRENGTH);
}

/* a kick's or buzz's strength / amplitude for the preset */
static int pulse_scaled(int value, int percent)
{
	if (value <= 0)
		return value;
	if (hair_trigger(percent))
		return value < LIGHT_PULSE_MOST ? value : LIGHT_PULSE_MOST;
	if (strong(percent))
		return clamp(value + STRONG_STEPS, 1, 8);
	return clamp(value, 1, 8);
}

void ae_ds5_rumble_scaled(struct ae_ds5_rumble *rumble, int percent)
{
	int motor = percent <= AE_DS5_STRENGTH_LIGHT ? AE_DS5_RUMBLE_LIGHT : percent > 100 ? AE_DS5_RUMBLE_STRONG : 100;

	rumble->left = clamp((rumble->left * motor + 50) / 100, 0, 255);
	rumble->right = clamp((rumble->right * motor + 50) / 100, 0, 255);
}

/* the shape's bytes for the preset (the shape's numbers NORMAL's); text: its words */
static void shape_effect(unsigned char effect[AE_DS5_EFFECT_SIZE], struct shape shape, float rate_of_fire, int per_round,
	int strength, char *text, size_t text_size)
{
	int frequency, value;

	switch (shape.mode)
	{
	case FEEDBACK:
		if (shape.a == 0)
		{
			/* (from zone 0: a kick after the shot, or the overheat's lock) */
			value = pulse_scaled(shape.b, strength);
			ae_ds5_effect_feedback(effect, 0, value);
			snprintf(text, text_size, "feedback from zone 0 strength %d", value);
		}
		else if (hair_trigger(strength))
		{
			hair_tick(effect);
			snprintf(text, text_size, "hair trigger tick zones %d-%d strength %d", HAIR_START, HAIR_END, HAIR_STRENGTH);
		}
		else
		{
			value = strong(strength) ? held_strong(shape.b, HOLD_STRONGEST) : clamp(shape.b, 1, 8);
			ae_ds5_effect_feedback(effect, shape.a, value);
			snprintf(text, text_size, "feedback from zone %d strength %d", shape.a, value);
		}
		break;
	case WEAPON:
		if (hair_trigger(strength))
		{
			hair_tick(effect);
			snprintf(text, text_size, "hair trigger tick zones %d-%d strength %d", HAIR_START, HAIR_END, HAIR_STRENGTH);
		}
		else
		{
			value = strong(strength) ? held_strong(shape.b, CLICK_STRONGEST) : clamp(shape.b, 1, 8);
			ae_ds5_effect_weapon(effect, shape.a, shape.c, value);
			snprintf(text, text_size, "click zones %d-%d strength %d", shape.a, shape.c, value);
		}
		break;
	case VIBRATION:
		value = pulse_scaled(shape.b, strength);
		frequency = shape.c ? shape.c : clamp((int)(rate_of_fire * (float)(per_round > 0 ? per_round : 1) + 0.5f), 6, 60);
		ae_ds5_effect_vibration(effect, shape.a, value, frequency);
		snprintf(text, text_size, "vibration from zone %d amplitude %d at %d Hz", shape.a, value, frequency);
		break;
	default:
		ae_ds5_effect_off(effect);
		snprintf(text, text_size, "off");
		break;
	}
}

/* DRY CLICK (an empty weapon, L2 with no grenades): slack, then a weak hollow click at the bottom of the pull, once a
pull (the weapon effect's own), on every preset (an empty weapon has nothing to fire, the click is news, not
resistance): LIGHT the faintest, STRONG a step firmer */
#define DRY_STRENGTH 2
static int dry_click(unsigned char effect[AE_DS5_EFFECT_SIZE], int strength)
{
	int value = hair_trigger(strength) ? 1 : DRY_STRENGTH + (strong(strength) ? 1 : 0);

	ae_ds5_effect_weapon(effect, 7, 8, value);
	return value;
}

void ae_ds5_right_held(unsigned char effect[AE_DS5_EFFECT_SIZE], struct ae_ds5_weapon_state const *state, int strength,
	char *text, size_t text_size)
{
	struct profile const *profile = &profiles[state->kind >= 0 && state->kind < AE_DS5_WEAPON_KINDS ? state->kind : 0];
	struct shape shape = profile->held;
	char words[80];
	char const *why = "held";

	if (shape.mode == OFF)
	{
		ae_ds5_effect_off(effect);
		snprintf(text, text_size, "%s", state->kind == AE_DS5_WEAPON_CARRY ? "free (carrying)" : "off");
		return;
	}
	/* (the weapon's state first, for every kind: the owner's rules, 0.3.0) */
	if (state->readying)
	{
		/* QUICK SETTLE: free while the weapon comes up (its settle bump is an event) */
		ae_ds5_effect_off(effect);
		snprintf(text, text_size, "free (readying)");
		return;
	}
	if (state->overheated)
	{
		/* LOCKED + VENTING THROB: locked solid, with a two-tick throb every eight, until the game lets it fire */
		struct shape locked = F(0, 8), throb = V(0, 8, 8);

		shape_effect(effect, state->ticks % THROB_TICKS < THROB_ON_TICKS ? throb : locked, 0.0f, 1, strength, words,
			sizeof(words));
		snprintf(text, text_size, "locked (overheated, venting throb)");
		return;
	}
	if (state->reloading)
	{
		/* SLACK (the seat bump at the magazine's going in is an event) */
		ae_ds5_effect_off(effect);
		snprintf(text, text_size, "free (reloading)");
		return;
	}
	if (state->empty)
	{
		/* DRY CLICK */
		int value = dry_click(effect, strength);

		snprintf(text, text_size, "dry click (empty): click zones 7-8 strength %d", value);
		return;
	}
	if (state->firing && profile->firing.mode != SAME)
	{
		shape = profile->firing;
		why = "firing";
	}
	if (profile->ramp == 1)
		shape.b = (unsigned char)ramped(shape.b, profile->ramp_to, state->heat);
	else if (profile->ramp == 2 && state->charge > 0.0f)
	{
		/* CHARGING HUM (owner 10-15: "plasma pistol is missing a charge haptic": the trigger is held down while it
		charges, so a resistance building with the charge was not felt): a vibration from the hold's zone, its
		amplitude rising with the charge, kept low (owner 10-15: "lower the intensity on the triggers and mix in the
		rumble motors": the motors carry most of it, ae_ds5_charge_rumble), faster once fully charged */
		shape = (struct shape)V(profile->held.a, 0, 0);
		shape.b = (unsigned char)ramped(CHARGING_HUM_LOW, CHARGING_HUM_HIGH, state->charge);
		shape.c = (unsigned char)(state->charge >= 1.0f ? CHARGED_HUM_HZ : CHARGING_HUM_HZ);
		why = state->charge >= 1.0f ? "charged" : "charging";
	}
	shape_effect(effect, shape, state->rate_of_fire, profile->per_round, strength, words, sizeof(words));
	snprintf(text, text_size, "%s: %s", why, words);
}

int ae_ds5_event(unsigned char effect[AE_DS5_EFFECT_SIZE], struct ae_ds5_rumble *rumble, enum ae_ds5_weapon_kind kind,
	enum ae_ds5_event event, int strength)
{
	struct profile const *profile = &profiles[kind >= 0 && kind < AE_DS5_WEAPON_KINDS ? kind : 0];
	char words[80];
	struct shape shape = NO;
	int ms = 0;

	rumble->left = rumble->right = rumble->ms = 0;
	switch (event)
	{
	case AE_DS5_EVENT_SHOT_CHARGED:
		if (profile->charged_kick.mode != OFF && profile->charged_kick_ms > 0)
		{
			shape = profile->charged_kick;
			ms = profile->charged_kick_ms;
			rumble->left = profile->charged_rumble_left;
			rumble->right = profile->charged_rumble_right;
			rumble->ms = profile->charged_rumble_ms;
			break;
		}
		/* (no charged kick: an ordinary shot) */
		/* fall through */
	case AE_DS5_EVENT_SHOT:
		shape = profile->kick;
		ms = profile->kick_ms;
		if (profile->family == COVENANT)
			rumble->right = profile->shot_rumble;
		else
			rumble->left = profile->shot_rumble;
		/* (at least SHOT_RUMBLE_MINIMUM_MS, one Bluetooth report: the pad gets one every 50 ms there, and a shorter pulse could be
		merged away, owner 10-15: "the br doesn't use the rumble motors") */
		rumble->ms = profile->shot_rumble_ms < SHOT_RUMBLE_MINIMUM_MS ? SHOT_RUMBLE_MINIMUM_MS : profile->shot_rumble_ms;
		/* (the battle rifle: the light motor joins each round, owner 10-15: "consistent but weak") */
		if (kind == AE_DS5_WEAPON_BURST)
			rumble->right = BURST_RUMBLE_RIGHT;
		break;
	case AE_DS5_EVENT_MAG_IN:
	{
		/* SEAT BUMP: the magazine (or a shotgun's shell) going in, at the reload animation's own moment */
		struct shape bump = F(0, 5);

		shape = bump;
		ms = 40;
		rumble->left = 60;
		rumble->ms = 30;
		break;
	}
	case AE_DS5_EVENT_SETTLE:
	{
		/* QUICK SETTLE: the new weapon's feel arriving as it is ready */
		struct shape settle = F(0, 3);

		shape = settle;
		ms = 30;
		break;
	}
	case AE_DS5_EVENT_CHAMBER:
	{
		/* the post-shot action at the weapon's own chambering (the shotgun's pump): a mechanical click */
		struct shape pump = F(0, 6);

		shape = pump;
		ms = 45;
		rumble->left = 120;
		rumble->ms = 50;
		break;
	}
	case AE_DS5_EVENT_GRENADE_FRAG:
		/* (L2) a frag grenade leaving the hand: no trigger kick (owner 10-21: "should be like a hair trigger"; it was a
		25 Hz jolt at 7 for 80 ms, then a 30 ms click), a light pulse on the left motor */
		rumble->left = GRENADE_THROW_RUMBLE;
		rumble->ms = SHOT_RUMBLE_MINIMUM_MS;
		break;
	case AE_DS5_EVENT_GRENADE_PLASMA:
		/* (L2) a plasma grenade leaving the hand: the same, on the right motor */
		rumble->right = GRENADE_THROW_RUMBLE;
		rumble->ms = SHOT_RUMBLE_MINIMUM_MS;
		break;
	}
	ae_ds5_rumble_scaled(rumble, strength);
	if (shape.mode == OFF || ms <= 0)
	{
		ae_ds5_effect_off(effect);
		return 0;
	}
	shape_effect(effect, shape, 0.0f, 1, strength, words, sizeof(words));
	return ms;
}

int ae_ds5_kind_covenant(enum ae_ds5_weapon_kind kind)
{
	return profiles[kind >= 0 && kind < AE_DS5_WEAPON_KINDS ? kind : 0].family == COVENANT;
}

char const *ae_ds5_profile_feel(enum ae_ds5_weapon_kind kind)
{
	return profiles[kind >= 0 && kind < AE_DS5_WEAPON_KINDS ? kind : 0].feel;
}

char const *ae_ds5_left_effect(unsigned char effect[AE_DS5_EFFECT_SIZE], int zoomed, int grenades, int winding,
	int strength)
{
	/* (owner 10-21: the grenade a hair trigger on every preset, LIGHT a hair trigger on both triggers. The zoom's hold:
	LIGHT none, NORMAL feedback 2, STRONG 3; grenades in hand the hair tick (zones 1-2, strength 2) on every preset;
	wound up, free) */
	if (zoomed)
	{
		if (hair_trigger(strength))
		{
			ae_ds5_effect_off(effect);
			return "free (zoomed, light)";
		}
		ae_ds5_effect_feedback(effect, 2, strong(strength) ? 3 : 2);
		return "light resistance (zoomed)";
	}
	if (winding)
	{
		ae_ds5_effect_off(effect);
		return "grenade wound up (free)";
	}
	if (grenades > 0)
	{
		ae_ds5_effect_feedback_span(effect, GRENADE_TICK_START, GRENADE_TICK_END, GRENADE_TICK_STRENGTH);
		return "grenade ready (hair trigger tick)";
	}
	dry_click(effect, strength);
	return "no grenades (dry click)";
}

#define OVERCHARGE_FLASH_TICKS 9
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

int ae_ds5_lightbar_black(unsigned char const base[3])
{
	return base[0] < 24 && base[1] < 24 && base[2] < 24;
}

void ae_ds5_lightbar(unsigned char rgb[3], unsigned char const base[3], struct ae_ds5_powerups const *powerups,
	char *text, size_t text_size)
{
	unsigned char colour[3];
	float scale = 1.0f, white = 0.0f;
	char const *what = "colour";
	int i;

	memcpy(colour, base, 3);
	if (ae_ds5_lightbar_black(base))
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

int ae_ds5_overcharge_flash(unsigned char rgb[3], long ticks)
{
	/* (the plasma pistol's overcharged release: green, fading over 0.3 s, in 3 steps) */
	float amount;

	if (ticks < 0 || ticks >= OVERCHARGE_FLASH_TICKS)
		return 0;
	amount = 1.0f - (float)(ticks / 3) / 3.0f;
	rgb[0] = (unsigned char)(32.0f * amount + 0.5f);
	rgb[1] = (unsigned char)(255.0f * amount + 0.5f);
	rgb[2] = (unsigned char)(64.0f * amount + 0.5f);
	return 1;
}

int ae_ds5_left_powerup(unsigned char effect[AE_DS5_EFFECT_SIZE], struct ae_ds5_powerups const *powerups, int strength,
	char *text, size_t text_size)
{
	if (powerups->juggernaut_ticks >= 0 && powerups->juggernaut_ticks < JUGGERNAUT_BUZZ_TICKS)
	{
		ae_ds5_effect_vibration(effect, 0, pulse_scaled(7, strength), 25);
		snprintf(text, text_size, "juggernaut buzz (vibration amplitude %d at 25 Hz)", pulse_scaled(7, strength));
		return 1;
	}
	if (powerups->shield > 1.0f && powerups->shield_rising)
	{
		float amount = (powerups->shield - 1.0f) / 2.0f;
		int amplitude = pulse_scaled(1 + (int)((amount > 1.0f ? 1.0f : amount) * 5.0f + 0.5f), strength);

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

/* a full shield's lightbar (owner 10-15: RGB 0, 14, 117), from this vitality up */
#define SHIELD_FULL 0.999f
#define SHIELD_FULL_RED 0
#define SHIELD_FULL_GREEN 14
#define SHIELD_FULL_BLUE 117

void ae_ds5_lightbar_shield(unsigned char rgb[3], struct ae_ds5_shield const *shield, char *text, size_t text_size)
{
	/* (the overshield's layers on the lightbar in the owner's picks (10-15): pink 255,0,51, then green 14,209,0; then
	hud_unit.c's yellow and violet) */
	static unsigned long const layers[5] = { 0, 0xff0033UL, 0x0ed100UL, 0xffff00UL, 0x7f00ffUL };
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
	/* (a full shield: the owner's own colour, deep navy, so full reads apart from the meter's blue as it drains) */
	if (shield->vitality >= SHIELD_FULL)
	{
		rgb[0] = SHIELD_FULL_RED;
		rgb[1] = SHIELD_FULL_GREEN;
		rgb[2] = SHIELD_FULL_BLUE;
		snprintf(text, text_size, "shield: full");
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
		return AE_DS5_STRENGTH_LIGHT;
	if (!strcmp(name, "strong"))
		return AE_DS5_STRENGTH_STRONG;
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

/* ---- one writer a pad (ae_ds5_rules.h: the link) ---- */

/* a field's bytes in the body */
struct field_span
{
	int field;
	int at, size;
};
static struct field_span const field_spans[] =
{
	{ AE_DS5_FIELD_TRIGGERS, AE_DS5_BODY_RIGHT_TRIGGER, 2 * AE_DS5_EFFECT_SIZE },
	{ AE_DS5_FIELD_RUMBLE, AE_DS5_BODY_RUMBLE_RIGHT, 2 },
	{ AE_DS5_FIELD_LIGHTBAR, AE_DS5_BODY_RED, 3 },
	{ AE_DS5_FIELD_PLAYER_LIGHTS, AE_DS5_BODY_PLAYER_LIGHTS, 1 }
};
#define FIELD_SPANS ((int)(sizeof(field_spans) / sizeof(field_spans[0])))

enum ae_ds5_write_result ae_ds5_write_result_of_errno(int error)
{
	/* (the queue full, or a moment's shortage: the next window) */
	if (error == EAGAIN || error == EINTR || error == ENOMEM)
		return AE_DS5_WRITE_BUSY;
#ifdef EWOULDBLOCK
	if (error == EWOULDBLOCK)
		return AE_DS5_WRITE_BUSY;
#endif
#ifdef ENOBUFS
	if (error == ENOBUFS)
		return AE_DS5_WRITE_BUSY;
#endif
#ifdef ETIMEDOUT
	if (error == ETIMEDOUT)
		return AE_DS5_WRITE_BUSY;
#endif
	/* (the device went: a Bluetooth link lost, the cable pulled) */
	if (error == ENODEV || error == EIO || error == ENXIO || error == EPIPE)
		return AE_DS5_WRITE_GONE;
#ifdef ENOTCONN
	if (error == ENOTCONN)
		return AE_DS5_WRITE_GONE;
#endif
#ifdef ESHUTDOWN
	if (error == ESHUTDOWN)
		return AE_DS5_WRITE_GONE;
#endif
	return AE_DS5_WRITE_REFUSED;
}

void ae_ds5_link_init(struct ae_ds5_link *link, int bluetooth, int vibration_v2)
{
	memset(link, 0, sizeof(*link));
	link->bluetooth = bluetooth != 0;
	link->vibration_v2 = vibration_v2 != 0;
}

void ae_ds5_link_forget(struct ae_ds5_link *link)
{
	link->known = 0;
	link->open_ms = 0;
	link->pending = 0;
}

static void link_want(struct ae_ds5_link *link, int field, int at, unsigned char const *bytes, int size)
{
	memcpy(link->want + at, bytes, (size_t)size);
	link->wanted |= field;
}

void ae_ds5_link_want_triggers(struct ae_ds5_link *link, unsigned char const right[AE_DS5_EFFECT_SIZE],
	unsigned char const left[AE_DS5_EFFECT_SIZE])
{
	link_want(link, AE_DS5_FIELD_TRIGGERS, AE_DS5_BODY_RIGHT_TRIGGER, right, AE_DS5_EFFECT_SIZE);
	link_want(link, AE_DS5_FIELD_TRIGGERS, AE_DS5_BODY_LEFT_TRIGGER, left, AE_DS5_EFFECT_SIZE);
}

void ae_ds5_link_want_rumble(struct ae_ds5_link *link, int left, int right)
{
	unsigned char motors[2];

	motors[0] = (unsigned char)clamp(right, 0, 255);
	motors[1] = (unsigned char)clamp(left, 0, 255);
	link_want(link, AE_DS5_FIELD_RUMBLE, AE_DS5_BODY_RUMBLE_RIGHT, motors, 2);
}

void ae_ds5_link_want_lightbar(struct ae_ds5_link *link, int red, int green, int blue)
{
	unsigned char rgb[3];

	rgb[0] = (unsigned char)clamp(red, 0, 255);
	rgb[1] = (unsigned char)clamp(green, 0, 255);
	rgb[2] = (unsigned char)clamp(blue, 0, 255);
	link_want(link, AE_DS5_FIELD_LIGHTBAR, AE_DS5_BODY_RED, rgb, 3);
}

void ae_ds5_link_want_player_lights(struct ae_ds5_link *link, unsigned char lights)
{
	link_want(link, AE_DS5_FIELD_PLAYER_LIGHTS, AE_DS5_BODY_PLAYER_LIGHTS, &lights, 1);
}

void ae_ds5_link_force(struct ae_ds5_link *link, int fields)
{
	link->forced |= fields & AE_DS5_FIELDS;
}

void ae_ds5_link_drop(struct ae_ds5_link *link, int fields)
{
	link->wanted &= ~fields;
	link->forced &= ~fields;
}

int ae_ds5_link_dirty(struct ae_ds5_link const *link)
{
	int dirty = link->wanted & (~link->known | link->forced);
	int i;

	for (i = 0; i < FIELD_SPANS; i++)
	{
		struct field_span const *span = &field_spans[i];

		if ((link->wanted & span->field) && !(dirty & span->field) &&
			memcmp(link->want + span->at, link->sent + span->at, (size_t)span->size))
			dirty |= span->field;
	}
	return dirty & AE_DS5_FIELDS;
}

/* the body for these fields: their enable bits, the wanted bytes */
static void link_body(struct ae_ds5_link const *link, int fields, unsigned char body[AE_DS5_BODY_SIZE])
{
	memcpy(body, link->want, AE_DS5_BODY_SIZE);
	body[AE_DS5_BODY_ENABLE1] = 0;
	body[AE_DS5_BODY_ENABLE2] = 0;
	body[AE_DS5_BODY_ENABLE3] = 0;
	if (fields & AE_DS5_FIELD_TRIGGERS)
		body[AE_DS5_BODY_ENABLE1] |= AE_DS5_ENABLE1_RIGHT_TRIGGER | AE_DS5_ENABLE1_LEFT_TRIGGER;
	if (fields & AE_DS5_FIELD_RUMBLE)
	{
		body[AE_DS5_BODY_ENABLE1] |= AE_DS5_ENABLE1_HAPTICS_SELECT;
		if (link->vibration_v2)
			body[AE_DS5_BODY_ENABLE3] |= AE_DS5_ENABLE3_RUMBLE_V2;
		else
			body[AE_DS5_BODY_ENABLE1] |= AE_DS5_ENABLE1_RUMBLE;
	}
	if (fields & AE_DS5_FIELD_LIGHTBAR)
		body[AE_DS5_BODY_ENABLE2] |= AE_DS5_ENABLE2_LIGHTBAR;
	if (fields & AE_DS5_FIELD_PLAYER_LIGHTS)
		body[AE_DS5_BODY_ENABLE2] |= AE_DS5_ENABLE2_PLAYER_LIGHTS;
}

static int link_due(struct ae_ds5_link *link, unsigned long long now, unsigned char body[AE_DS5_BODY_SIZE], int window)
{
	int dirty = ae_ds5_link_dirty(link);

	if (!dirty)
	{
		/* (what waited went back to what the pad has: folded away) */
		if (link->pending)
			link->merged++;
		link->pending = 0;
		link->last_dirty = 0;
		return 0;
	}
	if (window && now < link->open_ms)
	{
		/* (waits for the window: a change of what waits since the last frame is a change folded in) */
		if (!link->pending)
		{
			link->pending = 1;
			link->deferred++;
		}
		else if (dirty != link->last_dirty || memcmp(link->want, link->last_want, AE_DS5_BODY_SIZE))
			link->merged++;
		memcpy(link->last_want, link->want, AE_DS5_BODY_SIZE);
		link->last_dirty = dirty;
		return 0;
	}
	link_body(link, dirty, body);
	return dirty;
}

int ae_ds5_link_due(struct ae_ds5_link *link, unsigned long long now, unsigned char body[AE_DS5_BODY_SIZE])
{
	return link_due(link, now, body, 1);
}

int ae_ds5_link_due_now(struct ae_ds5_link *link, unsigned long long now, unsigned char body[AE_DS5_BODY_SIZE])
{
	return link_due(link, now, body, 0);
}

void ae_ds5_link_result(struct ae_ds5_link *link, int fields, enum ae_ds5_write_result result, unsigned long long now)
{
	int i;

	fields &= AE_DS5_FIELDS;
	if (result == AE_DS5_WRITE_SENT)
	{
		for (i = 0; i < FIELD_SPANS; i++)
		{
			struct field_span const *span = &field_spans[i];

			if (fields & span->field)
				memcpy(link->sent + span->at, link->want + span->at, (size_t)span->size);
		}
		link->known |= fields;
		link->forced &= ~fields;
		link->pending = 0;
		link->last_dirty = 0;
		link->writes++;
		link->open_ms = now + (link->bluetooth ? AE_DS5_BLUETOOTH_INTERVAL_MS : 0);
		return;
	}
	link->errors++;
	if (result == AE_DS5_WRITE_BUSY)
	{
		/* (kept: tried again as the next window opens, never at once) */
		link->open_ms = now + (link->bluetooth ? AE_DS5_BLUETOOTH_INTERVAL_MS : AE_DS5_BUSY_RETRY_MS);
		if (!link->pending)
		{
			link->pending = 1;
			link->deferred++;
		}
		memcpy(link->last_want, link->want, AE_DS5_BODY_SIZE);
		link->last_dirty = fields;
		return;
	}
	if (result == AE_DS5_WRITE_GONE)
		ae_ds5_link_forget(link);
}

int ae_ds5_charge_rumble(enum ae_ds5_weapon_kind kind, float charge, long ticks, struct ae_ds5_rumble *rumble)
{
	float amount = charge < 0.0f ? 0.0f : charge > 1.0f ? 1.0f : charge;

	rumble->left = rumble->right = rumble->ms = 0;
	if (kind < 0 || kind >= AE_DS5_WEAPON_KINDS || profiles[kind].ramp != 2 || charge <= 0.0f ||
		ticks % CHARGE_RUMBLE_TICKS != 0)
		return 0;
	/* (a Covenant weapon: the light right motor leads, the heavy left one joins as the charge builds) */
	rumble->right = CHARGE_RUMBLE_RIGHT_LOW + (int)((float)(CHARGE_RUMBLE_RIGHT_HIGH - CHARGE_RUMBLE_RIGHT_LOW) * amount + 0.5f);
	rumble->left = (int)((float)CHARGE_RUMBLE_LEFT_HIGH * amount + 0.5f);
	rumble->ms = CHARGE_RUMBLE_MS;
	return 1;
}

void ae_ds5_bind(unsigned int *bound, int ports, int port, unsigned int id)
{
	int other;

	if (port < 0 || port >= ports)
		return;
	if (id)
	{
		for (other = 0; other < ports; other++)
		{
			if (other != port && bound[other] == id)
				bound[other] = 0;
		}
	}
	bound[port] = id;
}
