#include <stdio.h>
#include <string.h>
#include "ae_randomizer_menus_rules.h"
#include "ae_pve_menus_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct { const char *key, *value; } settings[64];
static int setting_count;

static void set(const char *key, const char *value)
{
	int each;

	for (each = 0; each < setting_count; each++)
		if (!strcmp(settings[each].key, key))
		{
			settings[each].value = value;
			return;
		}
	settings[setting_count].key = key;
	settings[setting_count++].value = value;
}

static const char *get(void *context, const char *key)
{
	int each;

	(void)context;
	for (each = 0; each < setting_count; each++)
		if (!strcmp(settings[each].key, key))
			return settings[each].value;
	return "";
}

static const char *defaults(const char *key)
{
	return !strcmp(key, "game.randomizer_extra_enemies") ? "1" : "any";
}

static int shown_count(int page, struct ae_rnd_facts const *facts)
{
	int each, count = 0;

	for (each = 0; each < ae_rnd_row_count(); each++)
		if (ae_rnd_row(each)->page == page && ae_rnd_row_shown(ae_rnd_row(each), facts))
			count++;
	return count;
}

static int shown(const char *key, struct ae_rnd_facts const *facts)
{
	int each;

	for (each = 0; each < ae_rnd_row_count(); each++)
		if (ae_rnd_row(each)->key && !strcmp(ae_rnd_row(each)->key, key))
			return ae_rnd_row_shown(ae_rnd_row(each), facts);
	return -1;
}

static struct ae_rnd_row const *row_of(const char *key)
{
	int each;

	for (each = 0; each < ae_rnd_row_count(); each++)
		if (ae_rnd_row(each)->key && !strcmp(ae_rnd_row(each)->key, key))
			return ae_rnd_row(each);
	return NULL;
}


/* PLAYER LOADOUT's picker: Content Evolved's nine, then AE's seven, then the playing set's weapons; steps skip empty slots */
static void loadout_picker(void)
{
	struct ae_rnd_facts facts = { get, NULL, 0, 0, 0 };
	struct ae_rnd_row const *primary = row_of("game.randomizer_loadout_primary"), *secondary = row_of("game.randomizer_loadout_secondary");
	static char labels[AE_RND_LOADOUT_SET_SLOTS][48], values[AE_RND_LOADOUT_SET_SLOTS][24];
	char expect[16];
	int each, offered, at;

	CHECK(primary && secondary);
	/* (the flamethrower shows with GO CRAZY's FLAMETHROWER: on, all nine of Content Evolved's are offered) */
	set("game.randomizer_crazy", "true");
	set("game.randomizer_crazy_flamethrower", "true");
	/* the first nine are Content Evolved's, then AE's seven by registry id, in the order ae_rnd_loadout_ae_id_at gives */
	CHECK(!strcmp(primary->choices[0].value, "pistol") && !strcmp(primary->choices[8].value, "flamethrower"));
	CHECK(!strcmp(secondary->choices[0].value, "none") && !strcmp(secondary->choices[9].value, "flamethrower"));
	for (each = 0; each < AE_RND_LOADOUT_AE_COUNT; each++)
	{
		snprintf(expect, sizeof(expect), "ae:%u", ae_rnd_loadout_ae_id_at(each));
		CHECK(!strcmp(primary->choices[9 + each].value, expect) && !strcmp(secondary->choices[10 + each].value, expect));
		CHECK(ae_rnd_loadout_ae_id(expect) == ae_rnd_loadout_ae_id_at(each));
	}
	CHECK(!strcmp(primary->choices[9].label, "COMBAT AR") && !strcmp(primary->choices[10].label, "SUPPRESSED AR") &&
		!strcmp(primary->choices[11].label, "TACTICAL SMG") && !strcmp(primary->choices[12].label, "SIDEARM") &&
		!strcmp(primary->choices[13].label, "HEAVY REVOLVER") && !strcmp(primary->choices[14].label, "BOLT SNIPER") &&
		!strcmp(primary->choices[15].label, "BRUTE SHOT"));
	CHECK(primary->choice_count == 9 + AE_RND_LOADOUT_AE_COUNT + AE_RND_LOADOUT_SET_SLOTS &&
		secondary->choice_count == 10 + AE_RND_LOADOUT_AE_COUNT + AE_RND_LOADOUT_SET_SLOTS);
	/* no set: the slots are not offered, a step wraps from AE's last to the first */
	ae_rnd_loadout_set_weapons((char const (*)[48])labels, (char const (*)[24])values, 0);
	for (each = 0, offered = 0; each < primary->choice_count; each++)
		offered += ae_rnd_choice_offered(primary, each, &facts);
	CHECK(offered == 9 + AE_RND_LOADOUT_AE_COUNT);
	CHECK(ae_rnd_choice_step(primary, 15, 1, &facts) == 0 && ae_rnd_choice_step(primary, 0, -1, &facts) == 15);
	/* two weapons of a set: offered after AE's, in order; more than the slots are left out */
	snprintf(labels[0], sizeof(labels[0]), "SPIRIT PISTOL");
	snprintf(values[0], sizeof(values[0]), "tag:5eed0001");
	snprintf(labels[1], sizeof(labels[1]), "BRUTE CARBINE");
	snprintf(values[1], sizeof(values[1]), "tag:5eed0002");
	ae_rnd_loadout_set_weapons((char const (*)[48])labels, (char const (*)[24])values, 2);
	for (each = 0, offered = 0; each < primary->choice_count; each++)
		offered += ae_rnd_choice_offered(primary, each, &facts);
	CHECK(offered == 9 + AE_RND_LOADOUT_AE_COUNT + 2);
	CHECK(ae_rnd_choice_step(primary, 15, 1, &facts) == 16 && ae_rnd_choice_step(primary, 16, 1, &facts) == 17 &&
		ae_rnd_choice_step(primary, 17, 1, &facts) == 0);
	CHECK(ae_rnd_choice_index(primary, "tag:5eed0002") == 17 && !strcmp(primary->choices[16].label, "SPIRIT PISTOL"));
	/* a value the picker lacks is shown as not available, never as the first choice */
	set("game.randomizer_loadout_primary", "tag:5eed0002");
	CHECK(!strcmp(ae_rnd_row_value_text(primary, &facts), "BRUTE CARBINE"));
	ae_rnd_loadout_set_weapons((char const (*)[48])labels, (char const (*)[24])values, 0);
	CHECK(!strcmp(ae_rnd_row_value_text(primary, &facts), "NOT AVAILABLE"));
	set("game.randomizer_loadout_primary", "ae:12");
	CHECK(!strcmp(ae_rnd_row_value_text(primary, &facts), "COMBAT AR"));
	set("game.randomizer_loadout_primary", "ae:99");
	CHECK(!strcmp(ae_rnd_row_value_text(primary, &facts), "NOT AVAILABLE"));
	set("game.randomizer_loadout_primary", "needler");
	CHECK(!strcmp(ae_rnd_row_value_text(primary, &facts), "NEEDLER"));
	/* the value kinds, and the AE ids: only the seven */
	CHECK(ae_rnd_loadout_value_kind("pistol") == AE_RND_LOADOUT_VALUE_STOCK && ae_rnd_loadout_value_kind("none") == AE_RND_LOADOUT_VALUE_NONE &&
		ae_rnd_loadout_value_kind("ae:12") == AE_RND_LOADOUT_VALUE_AE && ae_rnd_loadout_value_kind("tag:00ff") == AE_RND_LOADOUT_VALUE_SET &&
		ae_rnd_loadout_value_kind("") == AE_RND_LOADOUT_VALUE_UNKNOWN && ae_rnd_loadout_value_kind(NULL) == AE_RND_LOADOUT_VALUE_UNKNOWN);
	CHECK(ae_rnd_loadout_ae_id("ae:12") == 12 && ae_rnd_loadout_ae_id("ae:11") == 11 && ae_rnd_loadout_ae_id("ae:8") == 0 &&
		ae_rnd_loadout_ae_id("ae:") == 0 && ae_rnd_loadout_ae_id("ae:12x") == 0 && ae_rnd_loadout_ae_id("pistol") == 0 &&
		ae_rnd_loadout_ae_id("ae:99999999999") == 0 && ae_rnd_loadout_ae_id_at(7) == 0 && ae_rnd_loadout_ae_id_at(-1) == 0);
	/* the fall back: a primary of AE's or of a set that the level lacks sends the game to the level's own loadout; a stock one, "none"
	and a found weapon do not */
	CHECK(ae_rnd_loadout_primary_missing("ae:12", 0) && ae_rnd_loadout_primary_missing("tag:5eed0001", 0));
	CHECK(!ae_rnd_loadout_primary_missing("ae:12", 1) && !ae_rnd_loadout_primary_missing("tag:5eed0001", 1));
	CHECK(!ae_rnd_loadout_primary_missing("pistol", 0) && !ae_rnd_loadout_primary_missing("none", 0) && !ae_rnd_loadout_primary_missing("", 0) &&
		!ae_rnd_loadout_primary_missing(NULL, 0));
	(void)at;
}

int main(void)
{
	struct ae_rnd_facts facts = { get, NULL, 0, 0, 0 };
	char text[256], line[1200];
	struct ae_rnd_setup setup, back;
	struct ae_rnd_setups setups;
	int each;

	/* multiplayer: does nothing. These keys belong to the randomizer and its setups; no multiplayer key is one of them */
	CHECK(!ae_randomizer_menus_key("game.multiplayer_scoring") && !ae_randomizer_menus_key("network.vehicle_riding"));
	CHECK(!ae_randomizer_menus_key("game.skulls") && !ae_randomizer_menus_key("game.firefight_objectives_round"));
	CHECK(!ae_randomizer_menus_key(NULL) && !ae_randomizer_menus_key(""));
	CHECK(ae_randomizer_menus_key("game.randomizer") && ae_randomizer_menus_key("game.randomizer_profile_8"));
	CHECK(!ae_randomizer_menus_key("game.randomizer_profile_9") && ae_randomizer_menus_key("game.tag_injection"));
	for (each = 0; each < ae_rnd_row_count(); each++)
		CHECK(!ae_rnd_row(each)->key || (ae_randomizer_menus_key(ae_rnd_row(each)->key) && !ae_pve_menus_key_allowed(ae_rnd_row(each)->key)));
	for (each = 0; each < AE_RND_SETUP_KEYS; each++)
		CHECK(!ae_rnd_setup_key(each)[0] || ae_randomizer_menus_key(ae_rnd_setup_key(each)));

	/* hidden rows: everything off shows the switches and EXTRA ENEMIES only */
	set("game.randomizer_enemies", "false");
	set("game.randomizer_allies", "false");
	set("game.randomizer_enemy_weapons", "false");
	set("game.randomizer_pickups", "false");
	CHECK(shown_count(AE_RND_PAGE_AI, &facts) == 5);   /* EXTRA ENEMIES, the ENEMIES and ALLIES headers and their two switches */
	CHECK(shown("game.randomizer_pool", &facts) == 0 && shown("game.randomizer_enemies", &facts) == 1);
	CHECK(shown_count(AE_RND_PAGE_WEAPONS, &facts) == 1 && shown_count(AE_RND_PAGE_PICKUPS, &facts) == 1);
	set("game.randomizer_enemies", "true");
	CHECK(shown("game.randomizer_pool", &facts) == 1 && shown("game.randomizer_type", &facts) == 0);
	set("game.randomizer_pool", "one");
	set("game.randomizer_type", "elite");
	CHECK(shown("game.randomizer_type", &facts) == 1 && shown("game.randomizer_elite_rank", &facts) == 1);
	CHECK(shown("game.randomizer_grunt_rank", &facts) == 0 && shown("game.randomizer_custom_type", &facts) == 0);
	set("game.randomizer_type", "tag_injector");
	CHECK(shown("game.randomizer_custom_type", &facts) == 1 && shown("game.randomizer_elite_rank", &facts) == 0);
	set("game.randomizer_enemies", "false");
	CHECK(shown("game.randomizer_type", &facts) == 0 && shown("game.randomizer_custom_type", &facts) == 0);
	set("game.randomizer_allies", "true");
	set("game.randomizer_ally_pool", "as_enemies");
	CHECK(shown("game.randomizer_ally_pool", &facts) == 1 && shown("game.randomizer_ally_type", &facts) == 0);
	set("game.randomizer_ally_pool", "one");
	set("game.randomizer_ally_type", "grunt");
	CHECK(shown("game.randomizer_ally_type", &facts) == 1 && shown("game.randomizer_ally_grunt_rank", &facts) == 1);
	set("game.randomizer_loadout", "set");
	CHECK(shown("game.randomizer_loadout_primary", &facts) == 1);
	set("game.randomizer_loadout", "random");
	CHECK(shown("game.randomizer_loadout_primary", &facts) == 0 && shown("game.randomizer_loadout", &facts) == 1);

	/* choices offered: ITEM SET only with a set, ENGINEER / SENTINEL and FLAMETHROWER only with GO CRAZY's parts */
	{
		struct ae_rnd_row const *type = row_of("game.randomizer_type"), *pickup = row_of("game.randomizer_pickup_type");
		int engineer = ae_rnd_choice_index(type, "engineer"), item = ae_rnd_choice_index(type, "tag_injector");
		int flame = ae_rnd_choice_index(pickup, "flamethrower");

		CHECK(!ae_rnd_choice_offered(type, engineer, &facts) && !ae_rnd_choice_offered(type, item, &facts));
		CHECK(!ae_rnd_choice_offered(pickup, flame, &facts) && ae_rnd_choice_offered(type, ae_rnd_choice_index(type, "grunt"), &facts));
		set("game.randomizer_crazy", "true");
		set("game.randomizer_crazy_enemies", "true");
		set("game.randomizer_crazy_flamethrower", "false");
		CHECK(ae_rnd_choice_offered(type, engineer, &facts) && !ae_rnd_choice_offered(pickup, flame, &facts));
		set("game.randomizer_crazy_flamethrower", "true");
		CHECK(ae_rnd_choice_offered(pickup, flame, &facts));
		facts.actor_types = 2;
		facts.weapon_types = 0;
		CHECK(ae_rnd_choice_offered(type, item, &facts) && !ae_rnd_choice_offered(pickup, ae_rnd_choice_index(pickup, "tag_injector"), &facts));
		/* stepping passes over what is not offered and wraps */
		set("game.randomizer_crazy", "false");
		facts.actor_types = 0;
		CHECK(ae_rnd_choice_step(type, ae_rnd_choice_index(type, "marine"), 1, &facts) == ae_rnd_choice_index(type, "grunt"));
		CHECK(ae_rnd_choice_step(type, ae_rnd_choice_index(type, "grunt"), -1, &facts) == ae_rnd_choice_index(type, "marine"));
		CHECK(ae_rnd_choice_index(type, "nonsense") == 0);
	}

	/* the summaries */
	set("game.randomizer", "false");
	ae_rnd_lobby_summary(&facts, text, sizeof(text));
	CHECK(!strcmp(text, "OFF"));
	set("game.randomizer", "true");
	set("game.randomizer_enemies", "false");
	set("game.randomizer_allies", "false");
	set("game.randomizer_loadout", "default");
	set("game.randomizer_extra_enemies", "1");
	ae_rnd_lobby_summary(&facts, text, sizeof(text));
	CHECK(!strcmp(text, "ON"));
	set("game.randomizer_enemies", "true");
	set("game.randomizer_pickups", "true");
	ae_rnd_lobby_summary(&facts, text, sizeof(text));
	CHECK(!strcmp(text, "ENEMIES \xE2\x80\xA2 PICKUPS"));
	set("game.randomizer_allies", "true");
	set("game.randomizer_enemy_weapons", "true");
	set("game.randomizer_extra_enemies", "2");
	ae_rnd_lobby_summary(&facts, text, sizeof(text));
	CHECK(!strcmp(text, "4 CHANGES"));
	ae_rnd_ai_summary(&facts, text, sizeof(text));
	CHECK(!strcmp(text, "ALL \xE2\x80\xA2 2X"));
	set("game.randomizer_extra_enemies", "1");
	set("game.randomizer_pickup_pool", "any");
	ae_rnd_weapon_summary(&facts, 1, text, sizeof(text));
	CHECK(!strcmp(text, "ON"));
	set("game.randomizer_pickup_pool", "one");
	set("game.randomizer_pickup_type", "shotgun");
	ae_rnd_weapon_summary(&facts, 1, text, sizeof(text));
	CHECK(!strcmp(text, "ONE TYPE: SHOTGUN"));
	set("game.randomizer_pickups", "false");
	ae_rnd_weapon_summary(&facts, 1, text, sizeof(text));
	CHECK(!strcmp(text, "OFF") && !strcmp(ae_rnd_loadout_summary(&facts), "DEFAULT"));

	/* setups: CE's text format reads back, a short one takes defaults, names are cleaned */
	memset(&setup, 0, sizeof(setup));
	snprintf(setup.name, sizeof(setup.name), "NEEDLER NIGHTMARE");
	for (each = 0; each < AE_RND_SETUP_KEYS; each++)
		if (ae_rnd_setup_key(each)[0])
			snprintf(setup.value[each], sizeof(setup.value[each]), "v%d", each);
	CHECK(ae_rnd_setup_format(&setup, line, sizeof(line)) > 0);
	CHECK(!strncmp(line, "NEEDLER NIGHTMARE;v0;v1;", 24));
	CHECK(ae_rnd_setup_parse(line, &back, defaults) && !memcmp(&setup, &back, sizeof(setup)));
	CHECK(ae_rnd_setup_format(&setup, line, 40) == 0);
	CHECK(ae_rnd_setup_parse("OLD;true;false", &back, defaults) && !strcmp(back.value[0], "true") && !strcmp(back.value[1], "false"));
	CHECK(!strcmp(back.value[2], "any") && !strcmp(back.value[19], "1") && back.value[15][0] == 0);
	CHECK(!ae_rnd_setup_parse("", &back, defaults) && !ae_rnd_setup_parse(";true", &back, defaults));
	snprintf(text, sizeof(text), "  my_setup-1 !?  ");
	CHECK(ae_rnd_setup_name_clean(text) && !strcmp(text, "my_setup-1"));
	snprintf(text, sizeof(text), " ?! ");
	CHECK(!ae_rnd_setup_name_clean(text));
	CHECK(!strcmp(ae_rnd_setup_value(&setup, "game.randomizer_enemies"), "v0") && ae_rnd_setup_value(&setup, "game.randomizer") == NULL);

	/* eight are kept: a name again replaces itself, a ninth drops the oldest, a delete moves the rest up */
	memset(&setups, 0, sizeof(setups));
	for (each = 0; each < 8; each++)
	{
		snprintf(setup.name, sizeof(setup.name), "S%d", each);
		CHECK(ae_rnd_setups_would_drop(&setups, setup.name) == -1);
		CHECK(ae_rnd_setups_keep(&setups, &setup) == each);
	}
	CHECK(setups.count == 8);
	snprintf(setup.name, sizeof(setup.name), "s3");
	CHECK(ae_rnd_setups_would_drop(&setups, setup.name) == -1 && ae_rnd_setups_keep(&setups, &setup) == 3 && setups.count == 8);
	snprintf(setup.name, sizeof(setup.name), "NINTH");
	CHECK(ae_rnd_setups_would_drop(&setups, setup.name) == 0);
	CHECK(ae_rnd_setups_keep(&setups, &setup) == 7 && setups.count == 8 && !strcmp(setups.setup[0].name, "S1") && !strcmp(setups.setup[7].name, "NINTH"));
	CHECK(ae_rnd_setups_remove(&setups, 0) && setups.count == 7 && !strcmp(setups.setup[0].name, "S2"));
	CHECK(!ae_rnd_setups_remove(&setups, 7) && !ae_rnd_setups_remove(&setups, -1));

	loadout_picker();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
