/*
AE_SILENT_RULES.C

SILENT MOVEMENT and SILENT WEAPONS: the rules (ae_silent_rules.h).
*/

#include <stddef.h>

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
