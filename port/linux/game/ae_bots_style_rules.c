/*
AE_BOTS_STYLE_RULES.C

Bot play styles, as pure rules (ae_bots_style_rules.h).
*/

#include "ae_bots_style_rules.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

static struct
{
	char const *name;
	int aggression, jumpiness, patience;
	int weight;	/* of 10 */
} const presets[AE_STYLE_PRESETS] =
{
	{ "STEADY", 0, 0, 0, 3 },
	{ "PUSHER", 2, 1, -1, 2 },
	{ "ANCHOR", -2, -1, 2, 2 },
	{ "HOPPER", 1, 2, -1, 1 },
	{ "CAREFUL", -1, 0, 1, 2 },
};

/* each weapon's name, and the tag path parts it is known by (the first that matches wins: the plasma pistol is not a
pistol's, the plasma rifle not the assault rifle's) */
static struct
{
	char const *name;
	char const *tag;
} const weapons[AE_STYLE_WEAPONS] =
{
	{ "none", NULL },
	{ "assault_rifle", "assault rifle" },
	{ "battle_rifle", "battle" },
	{ "shotgun", "shotgun" },
	{ "sniper", "sniper" },
	{ "pistol", "pistol" },
	{ "rockets", "rocket" },
	{ "plasma_rifle", "plasma rifle" },
	{ "needler", "needler" },
};

static int same_text(char const *a, char const *b)
{
	while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return !*a && !*b;
}

int ae_bots_style_preset(int preset, int weapon, struct ae_bots_style *style)
{
	if (preset < 0 || preset >= AE_STYLE_PRESETS || weapon < 0 || weapon >= AE_STYLE_WEAPONS)
		return 0;
	style->preset = preset;
	style->aggression = presets[preset].aggression;
	style->jumpiness = presets[preset].jumpiness;
	style->patience = presets[preset].patience;
	style->weapon = weapon;
	return 1;
}

void ae_bots_style_from_hash(unsigned long hash, struct ae_bots_style *style)
{
	int roll = (int)((hash & 0xFFFFUL) % 10), preset = 0, weapon = 0;
	unsigned long weapon_roll = (hash >> 16) % (3 * (AE_STYLE_WEAPONS - 1));

	while (preset < AE_STYLE_PRESETS - 1 && roll >= presets[preset].weight)
	{
		roll -= presets[preset].weight;
		preset++;
	}
	/* (a favourite 2 in 3: the rolls past the first third map onto the weapons) */
	if (weapon_roll >= (unsigned long)(AE_STYLE_WEAPONS - 1))
		weapon = 1 + (int)(weapon_roll % (AE_STYLE_WEAPONS - 1));
	ae_bots_style_preset(preset, weapon, style);
}

char const *ae_bots_style_preset_name(int preset)
{
	return preset >= 0 && preset < AE_STYLE_PRESETS ? presets[preset].name : "?";
}

char const *ae_bots_style_weapon_name(int weapon)
{
	return weapon >= 0 && weapon < AE_STYLE_WEAPONS ? weapons[weapon].name : "?";
}

int ae_bots_style_preset_of_name(char const *name)
{
	int preset;

	for (preset = 0; name && preset < AE_STYLE_PRESETS; preset++)
	{
		if (same_text(name, presets[preset].name))
			return preset;
	}
	return -1;
}

int ae_bots_style_weapon_of_name(char const *name)
{
	int weapon;

	for (weapon = 0; name && weapon < AE_STYLE_WEAPONS; weapon++)
	{
		if (same_text(name, weapons[weapon].name))
			return weapon;
	}
	return -1;
}

int ae_bots_style_weapon_of_tag(char const *tag_path)
{
	int weapon;

	if (!tag_path)
		return AE_STYLE_WEAPON_NONE;
	/* (the plasma ones first: "plasma pistol" holds "pistol", "plasma rifle" is no assault rifle) */
	if (strstr(tag_path, "plasma pistol"))
		return AE_STYLE_WEAPON_NONE;
	if (strstr(tag_path, "plasma rifle"))
		return AE_STYLE_WEAPON_PLASMA_RIFLE;
	for (weapon = 1; weapon < AE_STYLE_WEAPONS; weapon++)
	{
		if (strstr(tag_path, weapons[weapon].tag) || (weapon == AE_STYLE_WEAPON_BATTLE_RIFLE && strstr(tag_path, "battle_rifle")))
			return weapon;
	}
	return AE_STYLE_WEAPON_NONE;
}

static int clamp_step(int step)
{
	return step < -AE_STYLE_STEP_MOST ? -AE_STYLE_STEP_MOST : step > AE_STYLE_STEP_MOST ? AE_STYLE_STEP_MOST : step;
}

float ae_bots_style_confidence(struct ae_bots_style const *style, float confidence)
{
	return style ? confidence + AE_STYLE_CONFIDENCE * (float)clamp_step(style->aggression) : confidence;
}

long ae_bots_style_jump_delay(struct ae_bots_style const *style, long ticks)
{
	if (!style || ticks <= 0)
		return ticks;
	return (long)((float)ticks * (1.0f - AE_STYLE_JUMP * (float)clamp_step(style->jumpiness)) + 0.5f);
}

long ae_bots_style_halt(struct ae_bots_style const *style, long ticks)
{
	if (!style || ticks <= 0)
		return ticks;
	return (long)((float)ticks * (1.0f + AE_STYLE_HALT * (float)clamp_step(style->patience)) + 0.5f);
}

int ae_bots_style_weapon_value(struct ae_bots_style const *style, char const *tag_path, int value)
{
	if (!style || style->weapon == AE_STYLE_WEAPON_NONE || value < 0)
		return value;
	return ae_bots_style_weapon_of_tag(tag_path) == style->weapon ? value + AE_STYLE_FAVOURITE : value;
}
