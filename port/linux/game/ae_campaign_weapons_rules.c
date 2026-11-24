/*
AE_CAMPAIGN_WEAPONS_RULES.C

AE WEAPONS IN LEVELS, the pure rules (ae_campaign_weapons_rules.h).
*/

#include <ctype.h>
#include <string.h>

#include "ae_campaign_weapons_rules.h"

static struct
{
	char const *leaf;
	char const *name;
} const kinds[AE_CAMPAIGN_WEAPONS_KIND_COUNT] =
{
	{ "", "none" },
	{ "assault rifle", "assault rifle" },
	{ "pistol", "pistol" },
	{ "sniper rifle", "sniper rifle" },
	{ "needler", "needler" },
	{ "rocket launcher", "rocket launcher" },
};

static unsigned int const assault_rifles[] = { 12, 5, 15 };
static unsigned int const pistols[] = { 13, 17 };
static unsigned int const sniper_rifles[] = { 9 };
static unsigned int const needlers[] = { 8 };
static unsigned int const rocket_launchers[] = { 11 };

static unsigned long mix(unsigned long value)
{
	value &= 0xFFFFFFFFUL;
	value ^= value >> 16;
	value = (value * 0x45D9F3BUL) & 0xFFFFFFFFUL;
	value ^= value >> 16;
	value = (value * 0x45D9F3BUL) & 0xFFFFFFFFUL;
	value ^= value >> 16;
	return value;
}

static int same_leaf(char const *path, char const *leaf)
{
	char const *at = path, *scan;
	size_t length = strlen(leaf), each;

	for (scan = path; *scan; scan++)
		if (*scan == '\\' || *scan == '/')
			at = scan + 1;
	if (strlen(at) != length)
		return 0;
	for (each = 0; each < length; each++)
		if (tolower((unsigned char)at[each]) != leaf[each])
			return 0;
	return 1;
}

int ae_campaign_weapons_kind_of(char const *tag_name)
{
	int kind;

	if (!tag_name)
		return AE_CAMPAIGN_WEAPONS_NONE;
	for (kind = 1; kind < AE_CAMPAIGN_WEAPONS_KIND_COUNT; kind++)
		if (same_leaf(tag_name, kinds[kind].leaf))
			return kind;
	return AE_CAMPAIGN_WEAPONS_NONE;
}

char const *ae_campaign_weapons_kind_name(int kind)
{
	return kind >= 0 && kind < AE_CAMPAIGN_WEAPONS_KIND_COUNT ? kinds[kind].name : "none";
}

int ae_campaign_weapons_share_clamp(long share)
{
	return share < 0 || share > 100 ? AE_CAMPAIGN_WEAPONS_SHARE_DEFAULT : (int)share;
}

static unsigned long mix_bits(unsigned long seed, float value)
{
	unsigned long bits = 0;

	if (sizeof bits >= sizeof value)
		memcpy(&bits, &value, sizeof value);
	return mix(seed ^ bits);
}

unsigned long ae_campaign_weapons_seed(char const *map_name, float x, float y, float z, unsigned long salt)
{
	unsigned long seed = mix(salt + 0x9E3779B9UL);
	char const *scan;

	for (scan = map_name ? map_name : ""; *scan; scan++)
		seed = mix(seed + (unsigned long)tolower((unsigned char)*scan));
	seed = mix_bits(seed, x);
	seed = mix_bits(seed, y);
	return mix_bits(seed, z);
}

int ae_campaign_weapons_share_takes(unsigned long seed, int percent)
{
	if (percent <= 0)
		return 0;
	if (percent >= 100)
		return 1;
	return (int)(mix(seed + 17UL) % 100UL) < percent;
}

unsigned int ae_campaign_weapons_pick(int kind, unsigned long seed)
{
	unsigned long turn = mix(seed + 31UL);

	switch (kind)
	{
	case AE_CAMPAIGN_WEAPONS_ASSAULT_RIFLE: return assault_rifles[turn % (sizeof assault_rifles / sizeof assault_rifles[0])];
	case AE_CAMPAIGN_WEAPONS_PISTOL: return pistols[turn % (sizeof pistols / sizeof pistols[0])];
	case AE_CAMPAIGN_WEAPONS_SNIPER_RIFLE: return sniper_rifles[0];
	case AE_CAMPAIGN_WEAPONS_NEEDLER: return needlers[0];
	case AE_CAMPAIGN_WEAPONS_ROCKET_LAUNCHER: return rocket_launchers[0];
	default: return 0;
	}
}

int ae_campaign_weapons_loadout_of_word(char const *word)
{
	if (word && strlen(word) == 2 && tolower((unsigned char)word[0]) == 'a' && tolower((unsigned char)word[1]) == 'e')
		return AE_CAMPAIGN_WEAPONS_LOADOUT_AE;
	return AE_CAMPAIGN_WEAPONS_LOADOUT_STANDARD;
}

void ae_campaign_weapons_squad(int slot, unsigned int *primary, unsigned int *sidearm)
{
	/* COMBAT AR + SIDEARM, BATTLE RIFLE + HEAVY REVOLVER, TACTICAL SMG + SIDEARM, CARBINE + HEAVY REVOLVER */
	static unsigned int const gear[AE_CAMPAIGN_WEAPONS_SQUAD_MOST][2] = { { 12, 13 }, { 1, 17 }, { 15, 13 }, { 4, 17 } };

	if (slot < 0)
		slot = 0;
	slot %= AE_CAMPAIGN_WEAPONS_SQUAD_MOST;
	*primary = gear[slot][0];
	*sidearm = gear[slot][1];
}

int ae_campaign_weapons_level_of(int enabled, int share)
{
	if (!enabled)
		return AE_CAMPAIGN_WEAPONS_LEVEL_OFF;
	share = ae_campaign_weapons_share_clamp(share);
	return share <= 30 ? AE_CAMPAIGN_WEAPONS_LEVEL_SOME : share <= 60 ? AE_CAMPAIGN_WEAPONS_LEVEL_MOST : AE_CAMPAIGN_WEAPONS_LEVEL_ALL;
}

int ae_campaign_weapons_level_share(int level)
{
	return level == AE_CAMPAIGN_WEAPONS_LEVEL_SOME ? 30 : level == AE_CAMPAIGN_WEAPONS_LEVEL_MOST ? 60 :
		level == AE_CAMPAIGN_WEAPONS_LEVEL_ALL ? 100 : 0;
}

char const *ae_campaign_weapons_level_name(int level)
{
	return level == AE_CAMPAIGN_WEAPONS_LEVEL_SOME ? "SOME" : level == AE_CAMPAIGN_WEAPONS_LEVEL_MOST ? "MOST" :
		level == AE_CAMPAIGN_WEAPONS_LEVEL_ALL ? "ALL" : "OFF";
}

int ae_campaign_weapons_level_step(int level, int direction)
{
	if (level < 0 || level >= AE_CAMPAIGN_WEAPONS_LEVELS)
		level = AE_CAMPAIGN_WEAPONS_LEVEL_OFF;
	return (level + (direction < 0 ? AE_CAMPAIGN_WEAPONS_LEVELS - 1 : 1)) % AE_CAMPAIGN_WEAPONS_LEVELS;
}

int ae_campaign_weapons_join_needs_cache(int campaign_game, int weapons_on, int squad_is_ae, int squad_count)
{
	return campaign_game && (weapons_on || (squad_is_ae && squad_count > 0));
}
