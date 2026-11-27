/* ae_forge_rules.h */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_forge_rules.h"

int ae_forge_rules_is_forge_line(char const *text)
{
	return text != 0 && text[0] == '!' && text[1] == 'F';
}

int ae_forge_rules_line_accepted(int session, char const *text)
{
	return session != 0 && ae_forge_rules_is_forge_line(text);
}

int ae_forge_rules_session_may_start(int is_client, int joined_at_forge_version, int local_request)
{
	return is_client ? joined_at_forge_version != 0 : local_request != 0;
}

long ae_forge_rules_network_version(int session, long normal_version, long forge_version)
{
	return session != 0 ? forge_version : normal_version;
}

int ae_forge_rules_key_taken(int session, int vk, int const *forge_vks, int count)
{
	int each;

	if (!session || !forge_vks || vk <= 0)
		return 0;
	for (each = 0; each < count; each++)
	{
		if (forge_vks[each] == vk)
			return 1;
	}
	return 0;
}

/* the pairs of one vehicle under two names (forge_runtime_tags.c's vehi_pairs) */
static char const *const alias_pairs[][2] =
{
	{ "vehicles\\warthog\\warthog", "vehicles\\warthog\\mp_warthog" },
	{ "vehicles\\ghost\\ghost", "vehicles\\ghost\\ghost_mp" },
	{ "vehicles\\scorpion\\scorpion", "vehicles\\scorpion\\scorpion_mp" },
	{ "vehicles\\banshee\\banshee", "vehicles\\banshee\\banshee_mp" },
	{ "vehicles\\c_gun_turret\\c_gun_turret", "vehicles\\c_gun_turret\\c_gun_turret_mp" },
	{ "vehicles\\c gun turret\\c gun turret", "vehicles\\c gun turret\\c gun turret_mp" },
};

/* the same path, compared without case and with either slash */
static int same_tag(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
	{
		char x = *a == '/' ? '\\' : (*a >= 'A' && *a <= 'Z' ? (char)(*a + 32) : *a);
		char y = *b == '/' ? '\\' : (*b >= 'A' && *b <= 'Z' ? (char)(*b + 32) : *b);

		if (x != y)
			return 0;
	}
	return *a == *b;
}

int ae_forge_rules_tag_alias(char const *tag, char *other, int size)
{
	int each, side, at;
	char const *name;

	if (!tag || !other || size <= 0)
		return 0;
	for (each = 0; each < (int)(sizeof(alias_pairs) / sizeof(alias_pairs[0])); each++)
	{
		for (side = 0; side < 2; side++)
		{
			if (!same_tag(tag, alias_pairs[each][side]))
				continue;
			name = alias_pairs[each][1 - side];
			if ((int)strlen(name) >= size)
				return 0;
			for (at = 0; name[at]; at++)
				other[at] = strchr(tag, '/') ? (name[at] == '\\' ? '/' : name[at]) : name[at];
			other[at] = 0;
			return 1;
		}
	}
	return 0;
}

int ae_forge_rules_gametype_plays(unsigned flags, int engine)
{
	if (flags == 0 || (flags & 0x1Fu) == 0x1Fu || (flags & 0x0FFFu) == 0x0FFFu || engine == 0)
		return 1;
	if (engine < 1 || engine > 5)
		return 0;
	return (flags & (1u << (engine - 1))) != 0;
}

void ae_forge_rules_gravity_correct(float vector[3], float const pull[3], float amount)
{
	vector[0] += pull[0] * amount;
	vector[1] += pull[1] * amount;
	vector[2] += (pull[2] + 1.0f) * amount;
}

float ae_forge_rules_ground_gravity(float world, float const pull[3])
{
	float strength = (float)sqrt((double)(pull[0] * pull[0] + pull[1] * pull[1] + pull[2] * pull[2]));

	return world * (strength > 1.0f ? strength : 1.0f);
}

int ae_forge_rules_pull_lifts(float const pull[3], float const ground_normal[3])
{
	return pull[0] * ground_normal[0] + pull[1] * ground_normal[1] + pull[2] * ground_normal[2] > 0.001f;
}

int ae_forge_rules_gravity_grace(int in_volume, long vehicle, long now, long grace, long *last_vehicle, long *last_time)
{
	if (in_volume)
	{
		*last_vehicle = vehicle;
		*last_time = now;
		return 1;
	}
	return *last_vehicle == vehicle && now - *last_time >= 0 && now - *last_time < grace;
}

int ae_forge_rules_gravity_noted(int in_volume, int grounded, int noted)
{
	if (in_volume)
		return 1;
	return grounded ? 0 : noted;
}

static int same_word(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
	{
		char x = *a >= 'A' && *a <= 'Z' ? (char)(*a + 32) : *a;
		char y = *b >= 'A' && *b <= 'Z' ? (char)(*b + 32) : *b;

		if (x != y)
			return 0;
	}
	return *a == *b;
}

/* (a volume the Monitor's feet stand in: GRAVITY a column from just under the ground up; KILL and the damage ones a small box; ONE WAY
a thin slab, its top just above the ground, which a jump rises through and comes down onto) */
static struct
{
	char const *name;
	struct ae_forge_volume_made made;
} const volume_kinds[AE_FORGE_VOLUME_KINDS] =
{
	{ "gravity", { AE_FORGE_VOLUME_GRAVITY, { 12.0f, 12.0f, 40.0f }, 19.0f, 1, 25.0f, 0.0f, 1000.0f } },
	{ "kill", { AE_FORGE_VOLUME_KILL, { 6.0f, 6.0f, 6.0f }, 0.0f, 0, 0.0f, 0.0f, 0.0f } },
	{ "damage", { AE_FORGE_VOLUME_DAMAGE, { 6.0f, 6.0f, 6.0f }, 0.0f, 2, 25.0f, 1.0f, 100.0f } },
	{ "hurt", { AE_FORGE_VOLUME_HURT, { 6.0f, 6.0f, 6.0f }, 0.0f, 2, 25.0f, 1.0f, 100.0f } },
	{ "oneway", { AE_FORGE_VOLUME_ONE_WAY, { 8.0f, 8.0f, 0.2f }, 0.2f, 0, 0.0f, 0.0f, 0.0f } },
	/* (an AI PATROL volume: the ground in it, over the feet, a room to roam) */
	{ "patrol", { AE_FORGE_VOLUME_PATROL, { 24.0f, 24.0f, 12.0f }, 5.0f, 3, 10.0f, 1.0f, 100.0f } },
};

int ae_forge_rules_volume_kind(char const *name)
{
	int kind;

	if (!name)
		return AE_FORGE_VOLUME_NONE;
	for (kind = 0; kind < AE_FORGE_VOLUME_KINDS; kind++)
	{
		if (same_word(name, volume_kinds[kind].name))
			return kind;
	}
	return AE_FORGE_VOLUME_NONE;
}

int ae_forge_rules_volume_made(int kind, struct ae_forge_volume_made *made)
{
	if (kind < 0 || kind >= AE_FORGE_VOLUME_KINDS || !made)
		return 0;
	*made = volume_kinds[kind].made;
	return 1;
}

int ae_forge_rules_team_slot(char const *name)
{
	if (!name)
		return -1;
	return same_word(name, "red") ? 0 : same_word(name, "blue") ? 1 : same_word(name, "ffa") ? 2 : -1;
}

int ae_forge_rules_atmosphere(char const *name)
{
	static char const *const names[] = { "default", "clear", "light", "heavy", "flood" };
	int each;

	if (!name)
		return -1;
	for (each = 0; each < 5; each++)
	{
		if (same_word(name, names[each]))
			return each;
	}
	return -1;
}

int ae_forge_rules_tag_matches(char const *path, char const *asked)
{
	char const *leaf, *scan, *at;
	size_t length;

	if (!path || !asked || !asked[0])
		return 0;
	leaf = path;
	for (scan = path; *scan; scan++)
	{
		if (*scan == '\\' || *scan == '/')
			leaf = scan + 1;
	}
	length = strlen(asked);
	for (at = leaf; *at; at++)
	{
		size_t each;

		for (each = 0; each < length && at[each]; each++)
		{
			char x = at[each] >= 'A' && at[each] <= 'Z' ? (char)(at[each] + 32) : at[each];
			char y = asked[each] >= 'A' && asked[each] <= 'Z' ? (char)(asked[each] + 32) : asked[each];

			if (x != y)
				break;
		}
		if (each == length)
			return 1;
	}
	return 0;
}

int ae_forge_rules_ai_team(char const *name)
{
	static char const *const names[] = { "own", "player", "human", "covenant", "flood", "sentinel", "players", "red", "blue" };
	int each;

	if (!name)
		return -1;
	for (each = 0; each < 9; each++)
	{
		if (same_word(name, names[each]))
			return each;
	}
	return -1;
}

int ae_forge_rules_ai_goal(char const *name)
{
	static char const *const names[] = { "none", "patrol", "route", "hunt" };
	int each;

	if (!name)
		return -1;
	for (each = 0; each < 4; each++)
	{
		if (same_word(name, names[each]))
			return each;
	}
	return -1;
}

int ae_forge_rules_ai_pack(int team, int goal, int respawn_seconds)
{
	team = team < 0 ? 0 : team > 15 ? 15 : team;
	goal = goal < 0 ? 0 : goal > 15 ? 15 : goal;
	respawn_seconds = respawn_seconds < 0 ? 0 : respawn_seconds > 600 ? 600 : respawn_seconds;
	return team | (goal << 4) | (respawn_seconds << 8);
}

int ae_forge_rules_ai_unpack_team(int packed)
{
	return packed & 15;
}

int ae_forge_rules_ai_unpack_goal(int packed)
{
	return (packed >> 4) & 15;
}

int ae_forge_rules_ai_unpack_respawn(int packed)
{
	return (packed >> 8) & 1023;
}

int ae_forge_rules_ai_link(int asked, int const *numbers, int count)
{
	int each, lowest = 0;

	if (asked > 0)
		return asked;
	for (each = 0; each < count; each++)
	{
		if (numbers[each] > 0 && (!lowest || numbers[each] < lowest))
			lowest = numbers[each];
	}
	return lowest;
}

int ae_forge_rules_ready(int session, int in_level, int edits_queued, int modes_pending, int ai_pending, char *text, int size)
{
	if (!session)
	{
		snprintf(text, (size_t)size, "forge: no session (forge start <map> first)");
		return 0;
	}
	if (!in_level)
		snprintf(text, (size_t)size, "forge: being made: waiting for the level");
	else if (edits_queued > 0)
		snprintf(text, (size_t)size, "forge: being made: %d edits to carry out", edits_queued);
	else if (modes_pending > 0)
		snprintf(text, (size_t)size, "forge: being made: waiting for the mode");
	else if (ai_pending)
		snprintf(text, (size_t)size, "forge: being made: waiting for the AI to stand");
	else
	{
		snprintf(text, (size_t)size, "forge: ready");
		return 1;
	}
	return -1;
}

int ae_forge_rules_ff_kind(char const *name)
{
	static char const *const names[AE_FORGE_FF_KINDS] = { "infantry", "vehicles", "vehicle", "ghost", "heavy", "zone", "pad", "player",
		"generator", "rack", "drop" };
	int each;

	if (!name)
		return AE_FORGE_FF_NONE;
	for (each = 0; each < AE_FORGE_FF_KINDS; each++)
	{
		if (same_word(name, names[each]))
			return each;
	}
	return AE_FORGE_FF_NONE;
}

int ae_forge_rules_ff_round(char const *name)
{
	if (!name)
		return -1;
	if (same_word(name, "all"))
		return 0;
	return name[0] >= '1' && name[0] <= '5' && !name[1] ? name[0] - '0' : -1;
}

int ae_forge_rules_ff_pack(int kind, int round, int wave)
{
	return (kind & 255) | ((round & 15) << 8) | ((wave & 15) << 12);
}

int ae_forge_rules_ff_unpack_kind(int packed)
{
	return packed & 255;
}

int ae_forge_rules_ff_unpack_round(int packed)
{
	return (packed >> 8) & 15;
}

int ae_forge_rules_ff_unpack_wave(int packed)
{
	return (packed >> 12) & 15;
}

int ae_forge_rules_address(char const *text)
{
	int part, digits, value;
	char const *at = text;

	if (!text)
		return 0;
	for (part = 0; part < 5; part++)
	{
		digits = 0;
		value = 0;
		while (*at >= '0' && *at <= '9' && digits < 6)
		{
			value = value * 10 + (*at - '0');
			digits++;
			at++;
		}
		if (!digits || (part < 4 && value > 255) || (part == 4 && (value < 1 || value > 65535)))
			return 0;
		if (part < 3 && *at++ != '.')
			return 0;
		if (part == 3 && *at++ != ':')
			return 0;
	}
	return *at == 0;
}

int ae_forge_rules_net_name(char const *text)
{
	int each;

	if (!text || !text[0])
		return 0;
	for (each = 0; text[each]; each++)
	{
		char c = text[each];

		if (each >= 40 || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
			return 0;
	}
	return 1;
}

int ae_forge_rules_raw_line(char const *text)
{
	int each;

	if (!text || text[0] != '!' || (text[1] != 'F' && text[1] != 'f') || !((text[2] >= 'A' && text[2] <= 'Z') || (text[2] >= 'a' && text[2] <= 'z')))
		return 0;
	for (each = 3; text[each]; each++)
	{
		char c = text[each];

		if (each >= 63 || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ':' || c == '.' || c == '-'))
			return 0;
	}
	return 1;
}
