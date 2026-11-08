/* ae_weapon_registry_test.c: the registry's rows as the consumers read them (ae_weapon_registry.h): the pins of what the
slot switches gave the 0.7.5 packs (bot values, the kill feed, medal roles, trigger families, layout words, fallbacks),
and that no active row has a column silently empty. tools/test_weapon_cache_registry.py holds the table equal to the JSON. */
#include <stdio.h>
#include <string.h>
#include "ae_weapon_cache_format.h"
#include "ae_weapon_registry.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* what ae_bots.c's switch gave each pack of ids 1-14 (value, range, tap ticks; -1: bots.c's own from the trigger) and what
ae_bots_combat.c's precision and scoped tests said (the battle rifle, the DMR and the EVO RIFLE by their rifle profile) */
static struct
{
	unsigned int id;
	short value;
	float range;
	short tap;
	int precision, scoped;
} const bot_pins[] =
{
	{ 1, 8, 11.0f, -1, 1, 1 },  /* AE_BOTS_RIFLE_VALUE / RANGE; the pull ticks are the profile's */
	{ 2, 6, 5.0f, 0, 0, 0 },    /* AE_BOTS_SMG_VALUE / SMG_RANGE */
	{ 3, 6, 6.0f, 0, 0, 0 },    /* AE_BOTS_SILENCED_SMG_RANGE */
	{ 4, 8, 11.0f, 11, 1, 1 },  /* AE_BOTS_CARBINE_*: a precision rifle, scoped */
	{ 5, 6, 7.0f, 0, 0, 0 },    /* AE_BOTS_AR_SILENCED_* */
	{ 6, 8, 11.0f, -1, 1, 0 },  /* the DMR: a rifle profile; the scoped test was the battle rifle's and the carbine's */
	{ 7, 8, 11.0f, -1, 1, 0 },  /* the EVO RIFLE */
	{ 8, 7, 6.5f, 0, 0, 0 },    /* AE_BOTS_NEEDLER_* */
	{ 9, 8, 18.0f, -1, 1, 0 },  /* the bolt sniper: bots.c's name search ("sniper"), and the DESCOPE REFLEX of its own */
	{ 10, 7, 9.0f, 14, 0, 0 },  /* AE_BOTS_MAGNUM_*: its tag name has no "pistol", so not a precision weapon */
	{ 11, 8, 9.0f, 12, 0, 0 },  /* AE_BOTS_BRUTE_SHOT_* */
	{ 12, 6, 8.0f, 0, 0, 0 },   /* AE_BOTS_AR_MODERN_* */
	{ 13, 4, 7.0f, -1, 0, 0 },  /* the sidearm: bots.c's name search of "...\\magnum\\h2\\magnum": the default 4, 7 units */
	{ 14, 8, 2.5f, -1, 0, 0 }   /* AE_BOTS_SWORD_* */
};

static struct
{
	unsigned int id;
	char const *key, *display, *fallback, *feed_word, *feed_picture, *medal, *trigger;
} const pins[] =
{
	{ 1, "battle_rifle", "BATTLE RIFLE", "pistol", "BR", "BATTLE_RIFLE", "battle_rifle", "battle_rifle" },
	{ 2, "smg", "SMG", "assault_rifle", "SMG", "SMG", "smg", "smg" },
	{ 3, "smg_silenced", "SUPPRESSED SMG", "assault_rifle", "SMG", "SMG_SILENCED", "smg", "smg_silenced" },
	{ 4, "carbine", "CARBINE", "pistol", "CARBINE", "CARBINE", "carbine", "carbine" },
	{ 5, "ar_silenced", "SUPPRESSED AR", "assault_rifle", "AR", "AR_SILENCED", "assault_rifle", "ar_silenced" },
	{ 6, "dmr", "DMR", "pistol", "DMR", "DMR", "dmr", "carbine" },
	{ 7, "evo_rifle", "EVO RIFLE", "pistol", "EVO", "EVO", "evo_rifle", "carbine" },
	{ 8, "needler", "NEEDLER", "needler", "NEEDLER", "NEEDLER", "needler", "none" },
	{ 9, "sniper_bolt", "BOLT SNIPER", "sniper_rifle", "SNIPER", "SNIPER_BOLT", "sniper", "none" },
	{ 10, "magnum_scoped", "HJ MAGNUM", "pistol", "HJ MAGNUM", "MAGNUM_SCOPED", "pistol", "none" },
	{ 11, "brute_shot", "BRUTE SHOT", "rocket_launcher", "BRUTE SHOT", "BRUTE_SHOT", "rocket", "none" },
	{ 12, "ar_modern", "COMBAT AR", "assault_rifle", "AR", "AR_MODERN", "assault_rifle", "none" },
	{ 13, "sidearm", "SIDEARM", "pistol", "SIDEARM", "SIDEARM", "pistol", "none" },
	{ 14, "energy_sword", "ENERGY SWORD", "none", "SWORD", "SWORD", "sword", "none" }
};

static int one_of(char const *value, char const *const *list)
{
	for (; *list; list++)
		if (!strcmp(value, *list))
			return 1;
	return 0;
}

int main(void)
{
	static char const *const medal_words[] = { "none", "pistol", "plasma_pistol", "assault_rifle", "smg", "battle_rifle",
		"evo_rifle", "dmr", "carbine", "plasma_rifle", "shotgun", "sniper", "rocket", "fuel_rod", "sword", "hammer", "needler",
		"other", "melee", NULL };
	static char const *const trigger_words[] = { "none", "battle_rifle", "smg", "smg_silenced", "carbine", "ar_silenced", NULL };
	static char const *const menu_kinds[] = { "PRECISION", "AUTOMATIC", "PISTOL", "LONG RANGE", "HEAVY", "MELEE", "CLOSE", NULL };
	unsigned int index, other, count = ae_weapon_registry_count();

	CHECK(count >= 24);
	/* the item message index (16 bits, signed) of the cache's last weapon and its ammo text stays positive: the 0.7.5 slot
	scheme wrapped negative from the 17th slot (seam map S11) */
	CHECK(AE_CACHE_ITEM_STRING_INDEX_FIRST + 2 * (AE_CACHE_MAXIMUM_WEAPONS - 1) + 1 <= 0x7FFF);
	/* the 0.7.5 packs' bot numbers, and every consumer column of ids 1-14 */
	for (index = 0; index < sizeof(bot_pins) / sizeof(bot_pins[0]); index++)
	{
		struct ae_weapon_registry_row const *row = ae_weapon_registry_row(bot_pins[index].id);

		CHECK(row != NULL);
		if (!row)
			continue;
		CHECK(row->bot_value == bot_pins[index].value);
		CHECK(row->bot_range == bot_pins[index].range);
		CHECK(row->bot_tap_ticks == bot_pins[index].tap);
		CHECK(row->bot_precision == bot_pins[index].precision);
		CHECK(row->bot_scoped == bot_pins[index].scoped);
	}
	for (index = 0; index < sizeof(pins) / sizeof(pins[0]); index++)
	{
		struct ae_weapon_registry_row const *row = ae_weapon_registry_row(pins[index].id);

		CHECK(row != NULL);
		if (!row)
			continue;
		CHECK(!strcmp(row->key, pins[index].key) && !strcmp(row->display, pins[index].display));
		CHECK(!strcmp(row->stock_fallback, pins[index].fallback));
		CHECK(!strcmp(row->feed_word, pins[index].feed_word) && !strcmp(row->feed_picture, pins[index].feed_picture));
		CHECK(!strcmp(row->medal_role, pins[index].medal) && !strcmp(row->trigger_family, pins[index].trigger));
		CHECK(row->legacy_slot == pins[index].id && row->active);
		CHECK(ae_weapon_registry_row_of_slot((int)pins[index].id) == row);
	}
	/* the power weapons the medals and the bots treat as such */
	CHECK(ae_weapon_registry_row(11)->power && ae_weapon_registry_row(14)->power && !ae_weapon_registry_row(1)->power);
	/* the rows: ascending ids, unique keys and layout words, a value in every column (no silent 0 or empty) */
	for (index = 0; index < count; index++)
	{
		struct ae_weapon_registry_row const *row = ae_weapon_registry_at(index);
		unsigned int alias;

		CHECK(row != NULL);
		if (!row)
			continue;
		CHECK(index == 0 || ae_weapon_registry_at(index - 1)->id < row->id);
		CHECK(row->key[0] && row->display[0] && row->feed_word[0] && row->feed_picture[0] && row->menu_kind[0]);
		CHECK(one_of(row->medal_role, medal_words) && one_of(row->trigger_family, trigger_words) && one_of(row->menu_kind, menu_kinds));
		CHECK(row->aliases[0] && !strcmp(row->aliases[0], row->key));
		if (row->active)
		{
			CHECK(row->bot_value > 0 && row->bot_range > 0.0f);
			CHECK(ae_weapon_registry_stock(row->stock_fallback) != 0 || !strcmp(row->stock_fallback, "none"));
		}
		CHECK(ae_weapon_registry_id_of_word(row->key) == row->id);
		for (alias = 0; row->aliases[alias]; alias++)
		{
			CHECK(ae_weapon_registry_id_of_word(row->aliases[alias]) == row->id);
			for (other = 0; other < count; other++)
				if (other != index)
					CHECK(strcmp(ae_weapon_registry_at(other)->key, row->aliases[alias]));
		}
	}
	/* layout words, case aside; the owner's CLOSE kind on the auto shotgun */
	CHECK(ae_weapon_registry_id_of_word("BR") == 1 && ae_weapon_registry_id_of_word("Silenced_AR") == 5);
	CHECK(ae_weapon_registry_id_of_word("evo") == 7 && ae_weapon_registry_id_of_word("hammer") == 24);
	CHECK(ae_weapon_registry_id_of_word("") == 0 && ae_weapon_registry_id_of_word(NULL) == 0 && ae_weapon_registry_id_of_word("pistol") == 0);
	CHECK(!strcmp(ae_weapon_registry_row(21)->menu_kind, "CLOSE"));
	CHECK(ae_weapon_registry_row(99999) == NULL && ae_weapon_registry_row_of_slot(0) == NULL && ae_weapon_registry_row_of_slot(25) == NULL);
	CHECK(ae_weapon_registry_stock("assault_rifle") == 2 && ae_weapon_registry_stock("pistol") == 3 && ae_weapon_registry_stock("shotgun") == 4 &&
		ae_weapon_registry_stock("sniper_rifle") == 5 && ae_weapon_registry_stock("rocket_launcher") == 6 &&
		ae_weapon_registry_stock("needler") == 9 && ae_weapon_registry_stock("none") == 0 && ae_weapon_registry_stock(NULL) == 0);
	if (failures)
		printf("%d failures\n", failures);
	return failures != 0;
}
