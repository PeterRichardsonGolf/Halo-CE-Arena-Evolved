/*
AE_WEAPON_REGISTRY.C

The weapon registry's rows (ae_weapon_registry.h) from the generated table. No engine includes.
*/

#include <ctype.h>
#include <string.h>

#include "ae_weapon_registry.h"

#define AE_WEAPON_REGISTRY_ROW(id, key, display, legacy_slot, status, fallback, feed_word, feed_picture, bot_value, \
	bot_range, tap_ticks, precision, scoped, columns_from, medal_role, trigger_family, power, replaces, menu_kind) \
	{ id, key, display, legacy_slot, status, fallback, feed_word, feed_picture, bot_value, tap_ticks, bot_range, \
		precision, scoped, columns_from, medal_role, trigger_family, power, 0, replaces, menu_kind, { NULL } },
#define AE_WEAPON_REGISTRY_ALIAS(id, word)
#define AE_WEAPON_REGISTRY_ENTRY(id, kind, key, status)

static struct ae_weapon_registry_row rows[] =
{
#include "ae_weapon_registry.inc"
};

#undef AE_WEAPON_REGISTRY_ROW
#undef AE_WEAPON_REGISTRY_ALIAS
#undef AE_WEAPON_REGISTRY_ENTRY
#define AE_WEAPON_REGISTRY_ROW(...)
#define AE_WEAPON_REGISTRY_ALIAS(id, word) { id, word },
#define AE_WEAPON_REGISTRY_ENTRY(id, kind, key, status)

static struct
{
	unsigned int id;
	char const *word;
} const alias_table[] =
{
#include "ae_weapon_registry.inc"
};

#define ROW_COUNT (sizeof(rows) / sizeof(rows[0]))
#define ALIAS_COUNT (sizeof(alias_table) / sizeof(alias_table[0]))

/* the aliases and the active flags are filled on first use: the rows are static data, the lists are made from the alias
table (the key is its first word) */
static int filled;

static int same_word(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

static void fill(void)
{
	size_t index, alias;

	if (filled)
		return;
	filled = 1;
	for (index = 0; index < ROW_COUNT; index++)
	{
		unsigned int count = 0;

		rows[index].active = (unsigned char)!strcmp(rows[index].status, "active");
		for (alias = 0; alias < ALIAS_COUNT && count < AE_WEAPON_REGISTRY_ALIASES_MAXIMUM; alias++)
			if (alias_table[alias].id == rows[index].id)
				rows[index].aliases[count++] = alias_table[alias].word;
		rows[index].aliases[count] = NULL;
	}
}

unsigned int ae_weapon_registry_count(void)
{
	return (unsigned int)ROW_COUNT;
}

struct ae_weapon_registry_row const *ae_weapon_registry_at(unsigned int index)
{
	fill();
	return index < ROW_COUNT ? &rows[index] : NULL;
}

struct ae_weapon_registry_row const *ae_weapon_registry_row(unsigned int id)
{
	size_t index;

	fill();
	for (index = 0; index < ROW_COUNT; index++)
		if (rows[index].id == id)
			return &rows[index];
	return NULL;
}

struct ae_weapon_registry_row const *ae_weapon_registry_row_of_slot(int slot)
{
	return slot >= 1 && slot <= AE_WEAPON_REGISTRY_LEGACY_LAST ? ae_weapon_registry_row((unsigned int)slot) : NULL;
}

unsigned int ae_weapon_registry_id_of_word(char const *word)
{
	size_t index, alias;

	if (!word || !word[0])
		return 0;
	fill();
	for (index = 0; index < ROW_COUNT; index++)
	{
		if (same_word(word, rows[index].key))
			return rows[index].id;
		for (alias = 0; rows[index].aliases[alias]; alias++)
			if (same_word(word, rows[index].aliases[alias]))
				return rows[index].id;
	}
	return 0;
}

int ae_weapon_registry_stock(char const *fallback)
{
	if (!fallback)
		return 0;
	if (!strcmp(fallback, "assault_rifle"))
		return 2;
	if (!strcmp(fallback, "pistol"))
		return 3;
	if (!strcmp(fallback, "shotgun"))
		return 4;
	if (!strcmp(fallback, "sniper_rifle"))
		return 5;
	if (!strcmp(fallback, "rocket_launcher"))
		return 6;
	if (!strcmp(fallback, "needler"))
		return 9;
	return 0;
}
