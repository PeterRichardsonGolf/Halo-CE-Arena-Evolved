#include <stdio.h>
#include <string.h>
#include "ae_dev_cmd_state.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a config of three keys, as config_set changes it */
static char config[3][2][64] = { { "bots.count", "0" }, { "bots.skill", "normal" }, { "game.fall_damage", "true" } };
static int sets;

static int config_put(char const *key, char const *value, void *context)
{
	int index;

	(void)context;
	sets++;
	for (index = 0; index < 3; index++)
	{
		if (!strcmp(config[index][0], key))
		{
			snprintf(config[index][1], sizeof config[index][1], "%s", value);
			return 1;
		}
	}
	return 0;
}

int main(void)
{
	struct ae_dev_game_state game;
	struct ae_dev_settings settings;
	char long_key[80];

	memset(&game, 0, sizeof game);
	memset(&settings, 0, sizeof settings);

	/* (a reset in this game finishes; one asked at tick 0 of the last game, with the new game also at tick 0, never
	does: the generation, not the clock) */
	ae_dev_game_new(&game);
	ae_dev_game_reset_begin(&game);
	CHECK(ae_dev_game_reset_current(&game));
	ae_dev_game_reset_end(&game);
	CHECK(!ae_dev_game_reset_current(&game));
	ae_dev_game_reset_begin(&game);
	ae_dev_game_new(&game);
	CHECK(!ae_dev_game_reset_current(&game));
	/* (the same with only the generation moving, as if the tick never saw the new game's start) */
	ae_dev_game_reset_begin(&game);
	game.generation++;
	CHECK(!ae_dev_game_reset_current(&game));
	CHECK(!game.reset_pending);

	/* (rules kept at the first option only; put back in their own game; never into the next game's gametype) */
	CHECK(ae_dev_game_options_keep(&game));
	CHECK(!ae_dev_game_options_keep(&game));
	CHECK(ae_dev_game_options_take(&game));
	CHECK(!ae_dev_game_options_take(&game));
	CHECK(ae_dev_game_options_keep(&game));
	game.generation++;
	CHECK(!ae_dev_game_options_take(&game));
	CHECK(ae_dev_game_options_keep(&game));
	ae_dev_game_new(&game);
	CHECK(!ae_dev_game_options_take(&game));

	/* (settings: the value before the first change kept once; system.dev_mode never; a key too long never) */
	CHECK(ae_dev_settings_keep(&settings, "bots.count", "0"));
	CHECK(!ae_dev_settings_keep(&settings, "bots.count", "4"));
	CHECK(!ae_dev_settings_wants(&settings, "system.dev_mode"));
	CHECK(!ae_dev_settings_keep(&settings, "system.dev_mode", "true"));
	memset(long_key, 'k', sizeof long_key - 1);
	long_key[sizeof long_key - 1] = 0;
	CHECK(!ae_dev_settings_keep(&settings, long_key, "1"));
	CHECK(ae_dev_settings_keep(&settings, "bots.skill", "normal"));
	snprintf(config[0][1], sizeof config[0][1], "4");
	snprintf(config[1][1], sizeof config[1][1], "legendary");

	/* (DEV MODE off: everything kept goes back at once and nothing stays kept, so a later reset, after the player
	changed a setting by hand, puts back nothing stale) */
	CHECK(ae_dev_settings_back(&settings, config_put, NULL) == 2);
	CHECK(!strcmp(config[0][1], "0") && !strcmp(config[1][1], "normal"));
	snprintf(config[0][1], sizeof config[0][1], "7");
	sets = 0;
	CHECK(ae_dev_settings_back(&settings, config_put, NULL) == 0 && sets == 0);
	CHECK(!strcmp(config[0][1], "7"));

	/* (full: the 17th key is not kept) */
	{
		int index;
		char key[16];

		for (index = 0; index < AE_DEV_SETTINGS; index++)
		{
			snprintf(key, sizeof key, "k.%d", index);
			CHECK(ae_dev_settings_keep(&settings, key, "v"));
		}
		CHECK(!ae_dev_settings_keep(&settings, "k.last", "v"));
	}
	/* (the dummy command's bots settings: kept once, put back once; bots.count only when it is still what the dummy set) */
	{
		struct ae_dev_dummy_settings dummy;
		char count[16], teams[16];

		memset(&dummy, 0, sizeof dummy);
		CHECK(!ae_dev_dummy_settings_take(&dummy, 3, count, sizeof count, teams, sizeof teams));
		CHECK(ae_dev_dummy_settings_keep(&dummy, "0", "even"));
		ae_dev_dummy_settings_set(&dummy, 3);
		/* (a second dummy: the first values stay) */
		CHECK(!ae_dev_dummy_settings_keep(&dummy, "3", "against"));
		ae_dev_dummy_settings_set(&dummy, 4);
		CHECK(ae_dev_dummy_settings_take(&dummy, 4, count, sizeof count, teams, sizeof teams));
		CHECK(!strcmp(count, "0") && !strcmp(teams, "even"));
		CHECK(!ae_dev_dummy_settings_take(&dummy, 4, count, sizeof count, teams, sizeof teams));
		/* (bots.count changed by hand since: left alone, bots.teams still goes back) */
		CHECK(ae_dev_dummy_settings_keep(&dummy, "2", "against"));
		ae_dev_dummy_settings_set(&dummy, 3);
		CHECK(ae_dev_dummy_settings_take(&dummy, 6, count, sizeof count, teams, sizeof teams));
		CHECK(!count[0] && !strcmp(teams, "against"));
	}
	if (failures)
		printf("%d failures\n", failures);
	return failures != 0;
}
