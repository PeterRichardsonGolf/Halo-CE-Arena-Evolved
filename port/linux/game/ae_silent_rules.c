/*
AE_SILENT_RULES.C

SILENT MOVEMENT and SILENT WEAPONS: the rules (ae_silent_rules.h).
*/

#include <stddef.h>
#include <string.h>

#include "ae_silent_rules.h"

static int field_shift(int role)
{
	return role == AE_SILENT_ROLE_MOVEMENT ? AE_SILENT_MOVEMENT_SHIFT :
		role == AE_SILENT_ROLE_WEAPON ? AE_SILENT_WEAPONS_SHIFT : -1;
}

int ae_silent_mode(unsigned char byte, int role)
{
	int shift = field_shift(role);
	unsigned int value;

	if (shift < 0)
		return AE_SILENT_NORMAL;
	value = ((unsigned int)byte >> shift) & AE_SILENT_FIELD_MASK;
	/* (3, which the menu never writes, as the nearest: SILENT) */
	return value >= AE_SILENT_SILENT ? AE_SILENT_SILENT : (int)value;
}

unsigned char ae_silent_byte_set(unsigned char byte, int role, int mode)
{
	int shift = field_shift(role);
	unsigned int value = mode == AE_SILENT_NORMAL ? 0u : mode == AE_SILENT_JUST_ME ? 1u : 2u;

	if (shift < 0)
		return byte;
	return (unsigned char)(((unsigned int)byte & ~(AE_SILENT_FIELD_MASK << shift)) | (value << shift));
}

int ae_silent_any(unsigned char byte)
{
	return ae_silent_mode(byte, AE_SILENT_ROLE_MOVEMENT) != AE_SILENT_NORMAL ||
		ae_silent_mode(byte, AE_SILENT_ROLE_WEAPON) != AE_SILENT_NORMAL;
}

int ae_silent_muted(unsigned char byte, int role, long acting_player, long listener_player)
{
	switch (ae_silent_mode(byte, role))
	{
	case AE_SILENT_SILENT:
		return 1;
	case AE_SILENT_JUST_ME:
		/* (an owner not known is muted, as one that is not the listener) */
		return acting_player == -1 || listener_player == -1 || acting_player != listener_player;
	default:
		return 0;
	}
}

int ae_silent_weapon_role(int ready_or_put_away)
{
	return ready_or_put_away ? AE_SILENT_ROLE_WEAPON : AE_SILENT_ROLE_NONE;
}

int ae_silent_unit_action_role(int action)
{
	/* (units.c unit_animation_start_action: 3 the weapon class's ready
	animation, 4 its put-away) */
	return action == 3 || action == 4 ? AE_SILENT_ROLE_WEAPON : AE_SILENT_ROLE_NONE;
}

void ae_silent_join_mark(unsigned short *name, int length)
{
	if (!name || length < 3)
		return;
	/* (the name ends two before the end at the latest: a longer one is cut) */
	name[length - 2] = 0;
	name[length - 1] = (unsigned short)(AE_SILENT_JOIN_MARK | AE_SILENT_CAPABILITY);
}

int ae_silent_join_capability(unsigned short const *name, int length)
{
	unsigned int last;
	int index;

	if (!name || length < 3)
		return 0;
	last = name[length - 1];
	if ((last & AE_SILENT_JOIN_MARK_MASK) != AE_SILENT_JOIN_MARK)
		return 0;
	/* (the mark only after the name's end: a name that runs into it is no
	mark) */
	for (index = 0; index < length - 1; index++)
	{
		if (!name[index])
			return (int)(last & ~AE_SILENT_JOIN_MARK_MASK);
	}
	return 0;
}

void ae_silent_join_features(unsigned short *name, int length, unsigned int bits)
{
	int index;

	bits &= 0xFFu;
	if (!name || length < 5 || !bits)
		return;
	/* (only after a mark: a name not marked is left as it is) */
	if ((name[length - 1] & AE_SILENT_JOIN_MARK_MASK) != AE_SILENT_JOIN_MARK)
		return;
	/* (the name ends four before the end at the latest; a shorter one keeps its end, the characters between it and
	the feature mark 0) */
	for (index = 0; index < length - 4; index++)
	{
		if (!name[index])
			break;
	}
	for (; index <= length - 4; index++)
		name[index] = 0;
	name[length - 3] = (unsigned short)(AE_SILENT_JOIN_FEATURE_MARK | bits);
}

unsigned int ae_silent_join_features_told(unsigned short const *name, int length)
{
	int index;

	if (!name || length < 5 || !ae_silent_join_capability(name, length))
		return 0;
	if ((name[length - 3] & AE_SILENT_JOIN_FEATURE_MARK_MASK) != AE_SILENT_JOIN_FEATURE_MARK)
		return 0;
	/* (the feature mark only after the name's end) */
	for (index = 0; index < length - 3; index++)
	{
		if (!name[index])
			break;
	}
	if (index >= length - 3)
		return 0;
	return name[length - 3] & ~AE_SILENT_JOIN_FEATURE_MARK_MASK;
}

unsigned int ae_silent_join_feature_grenades(int version)
{
	if (version < 1)
		return 0;
	if (version > 7)
		version = 7;
	return AE_SILENT_JOIN_FEATURE_GRENADES | ((unsigned int)version << AE_SILENT_JOIN_FEATURE_GRENADE_VERSION_SHIFT);
}

int ae_silent_join_grenade_version(unsigned int bits)
{
	if (!(bits & AE_SILENT_JOIN_FEATURE_GRENADES))
		return 0;
	return (int)((bits & AE_SILENT_JOIN_FEATURE_GRENADE_VERSION_MASK) >> AE_SILENT_JOIN_FEATURE_GRENADE_VERSION_SHIFT);
}

int ae_silent_join_refused(unsigned char byte, int capability, int local)
{
	return !local && ae_silent_any(byte) && capability < AE_SILENT_CAPABILITY;
}

char const *ae_silent_mode_name(int mode)
{
	return mode == AE_SILENT_JUST_ME ? "just me" : mode == AE_SILENT_SILENT ? "silent" : "normal";
}

char const *ae_silent_role_name(int role)
{
	return role == AE_SILENT_ROLE_MOVEMENT ? "movement" : role == AE_SILENT_ROLE_WEAPON ? "weapons" : "other";
}

int ae_silent_split_screen(unsigned char byte)
{
	return ae_silent_mode(byte, AE_SILENT_ROLE_MOVEMENT) == AE_SILENT_SILENT &&
		ae_silent_mode(byte, AE_SILENT_ROLE_WEAPON) == AE_SILENT_SILENT;
}

void ae_silent_scope_push(struct ae_silent_scopes *scopes, int role, long player)
{
	if (scopes->depth >= AE_SILENT_SCOPE_DEPTH)
	{
		scopes->overflow++;
		return;
	}
	scopes->scopes[scopes->depth].handle = -1;
	scopes->scopes[scopes->depth].role = (unsigned char)(role > AE_SILENT_ROLE_NONE && role < NUMBER_OF_AE_SILENT_ROLES ?
		role : AE_SILENT_ROLE_NONE);
	scopes->scopes[scopes->depth].player = player;
	scopes->depth++;
}

void ae_silent_scope_pop(struct ae_silent_scopes *scopes)
{
	if (scopes->overflow > 0)
		scopes->overflow--;
	else if (scopes->depth > 0)
		scopes->depth--;
}

int ae_silent_scope_role(struct ae_silent_scopes const *scopes, long *player)
{
	if (player)
		*player = -1;
	if (scopes->depth <= 0)
		return AE_SILENT_ROLE_NONE;
	if (player)
		*player = scopes->scopes[scopes->depth - 1].player;
	return scopes->scopes[scopes->depth - 1].role;
}

void ae_silent_table_set(struct ae_silent_tag *table, int capacity, long handle, int role, long player)
{
	int slot;

	if (handle == -1)
		return;
	slot = (int)(handle & 0xFFFF);
	if (slot >= capacity)
		return;
	table[slot].handle = handle;
	table[slot].role = (unsigned char)(role > AE_SILENT_ROLE_NONE && role < NUMBER_OF_AE_SILENT_ROLES ?
		role : AE_SILENT_ROLE_NONE);
	table[slot].player = table[slot].role == AE_SILENT_ROLE_NONE ? -1 : player;
}

int ae_silent_table_get(struct ae_silent_tag const *table, int capacity, long handle, long *player)
{
	int slot;

	if (player)
		*player = -1;
	if (handle == -1)
		return AE_SILENT_ROLE_NONE;
	slot = (int)(handle & 0xFFFF);
	if (slot >= capacity || table[slot].handle != handle || table[slot].role == AE_SILENT_ROLE_NONE)
		return AE_SILENT_ROLE_NONE;
	if (player)
		*player = table[slot].player;
	return table[slot].role;
}

/* ---- the weapon cache's block (ae_silent_rules.h): characters counted from the last one, 0 = the AE mark, 3 = the cache
mark, 4 = the generation, 5 = the digest's low half, 6 = its high half; the name ends at length - 8 (index 24 of 32) at the
latest */

static int cache_index(int length, int from_end)
{
	return length - 1 - from_end;
}

void ae_silent_join_cache(unsigned short *name, int length, unsigned int format, unsigned int generation,
	unsigned char const *digest4)
{
	int index, cut;

	if (!name || !digest4 || length < AE_SILENT_JOIN_CACHE_MINIMUM_LENGTH)
		return;
	/* (only a marked name: the block is read with the AE mark) */
	if ((name[length - 1] & AE_SILENT_JOIN_MARK_MASK) != AE_SILENT_JOIN_MARK)
		return;
	/* (the name ends where it did, or at index length - 8 at the latest; its end to the block's mark, the level
	character's and the feature mark's places left alone, zeroed) */
	for (cut = 0; cut < length - 8 && name[cut]; cut++)
		;
	for (index = cut; index <= cache_index(length, AE_SILENT_JOIN_CACHE_FROM_END_MARK); index++)
		name[index] = 0;
	name[cache_index(length, AE_SILENT_JOIN_CACHE_FROM_END_MARK)] =
		(unsigned short)(AE_SILENT_JOIN_CACHE_MARK | (format & 0xFFu));
	name[cache_index(length, AE_SILENT_JOIN_CACHE_FROM_END_GENERATION)] = (unsigned short)(generation & 0xFFFFu);
	name[cache_index(length, AE_SILENT_JOIN_CACHE_FROM_END_DIGEST_LOW)] =
		(unsigned short)(digest4[0] | ((unsigned int)digest4[1] << 8));
	name[cache_index(length, AE_SILENT_JOIN_CACHE_FROM_END_DIGEST_HIGH)] =
		(unsigned short)(digest4[2] | ((unsigned int)digest4[3] << 8));
}

int ae_silent_join_cache_told(unsigned short const *name, int length, unsigned int *format, unsigned int *generation,
	unsigned char *digest4)
{
	unsigned int mark, low, high;
	int index;

	if (!name || length < AE_SILENT_JOIN_CACHE_MINIMUM_LENGTH)
		return 0;
	if ((name[length - 1] & AE_SILENT_JOIN_MARK_MASK) != AE_SILENT_JOIN_MARK)
		return 0;
	mark = name[cache_index(length, AE_SILENT_JOIN_CACHE_FROM_END_MARK)];
	if ((mark & AE_SILENT_JOIN_CACHE_MARK_MASK) != AE_SILENT_JOIN_CACHE_MARK)
		return 0;
	/* (the name ends before the block: a zero at index length - 8 or before) */
	for (index = 0; index <= length - 8; index++)
	{
		if (!name[index])
			break;
	}
	if (index > length - 8)
		return 0;
	low = name[cache_index(length, AE_SILENT_JOIN_CACHE_FROM_END_DIGEST_LOW)];
	high = name[cache_index(length, AE_SILENT_JOIN_CACHE_FROM_END_DIGEST_HIGH)];
	if (format)
		*format = mark & 0xFFu;
	if (generation)
		*generation = name[cache_index(length, AE_SILENT_JOIN_CACHE_FROM_END_GENERATION)];
	if (digest4)
	{
		digest4[0] = (unsigned char)(low & 0xFFu);
		digest4[1] = (unsigned char)(low >> 8);
		digest4[2] = (unsigned char)(high & 0xFFu);
		digest4[3] = (unsigned char)(high >> 8);
	}
	return 1;
}

void ae_silent_join_mcc(unsigned short *name, int length, int has_mcc)
{
	if (!name || length < AE_SILENT_JOIN_MCC_MINIMUM_LENGTH)
		return;
	if ((name[length - 1] & AE_SILENT_JOIN_MARK_MASK) != AE_SILENT_JOIN_MARK)
		return;
	name[length - 9] = 0;
	name[length - 8] = (unsigned short)(AE_SILENT_JOIN_MCC_MARK | (has_mcc ? 1u : 0u));
}

int ae_silent_join_mcc_told(unsigned short const *name, int length)
{
	int index;

	if (!name || length < AE_SILENT_JOIN_MCC_MINIMUM_LENGTH)
		return -1;
	if ((name[length - 1] & AE_SILENT_JOIN_MARK_MASK) != AE_SILENT_JOIN_MARK)
		return -1;
	if ((name[length - 8] & AE_SILENT_JOIN_MCC_MARK_MASK) != AE_SILENT_JOIN_MCC_MARK)
		return -1;
	for (index = 0; index <= length - 9; index++)
		if (!name[index])
			return (int)(name[length - 8] & 1u);
	return -1;
}

int ae_silent_join_cache_decide(int uses_cache, int told, unsigned int client_format, unsigned int client_generation,
	unsigned char const *client_digest4, unsigned int host_format, unsigned int host_generation,
	unsigned char const *host_digest4)
{
	if (!uses_cache)
		return AE_CACHE_JOIN_ACCEPT;
	if (!told)
		return AE_CACHE_JOIN_NO_BLOCK;
	if (client_format != host_format)
		return AE_CACHE_JOIN_FORMAT;
	if (client_generation < host_generation)
		return AE_CACHE_JOIN_CLIENT_OLDER;
	if (client_generation > host_generation)
		return AE_CACHE_JOIN_HOST_OLDER;
	if (!client_digest4 || !host_digest4 || memcmp(client_digest4, host_digest4, 4))
		return AE_CACHE_JOIN_DIFFERENT_BUILD;
	return AE_CACHE_JOIN_ACCEPT;
}

unsigned int ae_cache_rejection_word(int cache_case)
{
	if (cache_case == AE_CACHE_JOIN_NO_BLOCK)
		return AE_PACKS_REJECTION_CODE;
	return (unsigned int)(AE_CACHE_REJECTION_CODE | ((unsigned int)cache_case << 8)) & 0xFFFFu;
}

int ae_cache_rejection_case(unsigned int word)
{
	if ((word & 0xFFu) != AE_CACHE_REJECTION_CODE || word > 0xFFFFu)
		return -1;
	return (int)((word >> 8) & 0xFFu);
}

unsigned int ae_held_rejection_word(long seconds)
{
	long minutes = (seconds + 59) / 60;

	if (seconds <= 0 || minutes > AE_HELD_MAXIMUM_MINUTES)
		return 6; /* (_rejection_code_blacklisted_machine) */
	return (unsigned int)(AE_HELD_REJECTION_CODE | ((unsigned int)minutes << 8));
}

int ae_held_rejection_minutes(unsigned int word)
{
	if ((word & 0xFFu) != AE_HELD_REJECTION_CODE || word > 0xFFFFu || (word >> 8) == 0)
		return -1;
	return (int)(word >> 8);
}
