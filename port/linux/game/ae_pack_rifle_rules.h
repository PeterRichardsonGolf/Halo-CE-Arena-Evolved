/*
AE_PACK_RIFLE_RULES.H

A map's own battle rifle under the battle rifle's pack, pure (port/linux/tests/ae_pack_rifle_rules_test.c):
ae_weapon_pack.c reads it.

Owner 10-24 (it replaces the old "left as the map made it", decision 5): when the battle rifle's pack is added
to a map that ships its own battle rifle (a H2 demake's halo2\weapons\human\battle rifle\battle rifle, CE+
X's dw2\weapons\battle_rifle\battle_rifle), the pack's REPLACES it: every reference of the map's tags to it (its
placements' palette, the item collections its spots and starts spawn from, the starting profiles, the globals'
weapon list, the scripts' references) is made the pack's, so the game spawns and gives the same battle rifle
everywhere. The map's own tag stays in the table (its name still finds it), unreferenced.

- A map's own battle rifle: a weapon tag of the map whose name, in any case, holds "battle_rifle" or "battle
  rifle" (the pack's own tags are past the map's, never asked).
- A reference is redirected when its tag index is one of the map's own battle rifles: the pack's index then.
- The log counts them by kind (the referencing tag's group, the schema's structure and field): one line each.
*/

#ifndef __AE_PACK_RIFLE_RULES_H
#define __AE_PACK_RIFLE_RULES_H

#include <string.h>

enum
{
	/* the map's own battle rifles looked for (a map has one; a few for a map with variants) */
	AE_PACK_RIFLE_OWN_MAXIMUM = 8,
	/* the kinds of reference logged, each on its own line (more are counted in the last) */
	AE_PACK_RIFLE_KINDS_MAXIMUM = 16,
	AE_PACK_RIFLE_NAME_MAXIMUM = 255,
};

/* a kind of reference: the referencing tag's group, the schema's structure and field names (static strings), the
own battle rifle's place in the list, how many there were and the first referencing tag's name (the log's) */
struct ae_pack_rifle_kind
{
	unsigned long group;
	char const *structure;
	char const *field;
	int own;
	long count;
	char const *first;
};

/* whether a weapon tag's name is a battle rifle's ("battle_rifle" or "battle rifle", in any case) */
static inline int ae_pack_rifle_name_is_battle_rifle(
	char const *name)
{
	char lowered[AE_PACK_RIFLE_NAME_MAXIMUM + 1];
	size_t length;

	if (!name)
		return 0;
	for (length = 0; name[length] && length < AE_PACK_RIFLE_NAME_MAXIMUM; length++)
		lowered[length] = (char)(name[length] >= 'A' && name[length] <= 'Z' ? name[length] + 32 : name[length]);
	lowered[length] = 0;

	return strstr(lowered, "battle_rifle") != NULL || strstr(lowered, "battle rifle") != NULL;
}

/* the place of a tag index in the map's own battle rifles, or -1 (NONE, -1, is never one) */
static inline int ae_pack_rifle_own_of(
	long tag_index,
	long const *own,
	int own_count)
{
	int index;

	if (tag_index == -1)
		return -1;
	for (index = 0; index < own_count; index++)
	{
		if (own[index] == tag_index)
			return index;
	}

	return -1;
}

/* a reference's tag index made the pack's if it is one of the map's own battle rifles: its place there (and
*tag_index the pack's), or -1 (unchanged) */
static inline int ae_pack_rifle_redirect(
	long *tag_index,
	long const *own,
	int own_count,
	long pack_index)
{
	int own_index = tag_index && pack_index != -1 ? ae_pack_rifle_own_of(*tag_index, own, own_count) : -1;

	if (own_index >= 0)
		*tag_index = pack_index;

	return own_index;
}

/* one more reference of a kind counted (a new kind while there is room, else the last one's count): the kinds
now */
static inline int ae_pack_rifle_kind_add(
	struct ae_pack_rifle_kind *kinds,
	int count,
	unsigned long group,
	char const *structure,
	char const *field,
	int own,
	char const *first)
{
	int index;

	for (index = 0; index < count; index++)
	{
		if (kinds[index].group == group && kinds[index].own == own && kinds[index].structure == structure &&
			kinds[index].field == field)
		{
			kinds[index].count++;
			return count;
		}
	}
	if (count >= AE_PACK_RIFLE_KINDS_MAXIMUM)
	{
		kinds[AE_PACK_RIFLE_KINDS_MAXIMUM - 1].count++;
		return count;
	}
	kinds[count].group = group;
	kinds[count].structure = structure;
	kinds[count].field = field;
	kinds[count].own = own;
	kinds[count].count = 1;
	kinds[count].first = first;

	return count + 1;
}

#endif
