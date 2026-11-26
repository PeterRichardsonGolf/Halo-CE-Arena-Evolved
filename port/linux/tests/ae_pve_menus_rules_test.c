#include <stdio.h>
#include <string.h>
#include "ae_pve_menus_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static const char *standard_get(void *context, const char *key)
{
	struct ae_pve_menus_option const *option;
	int each;

	(void)context;
	for (each = 0; (option = ae_pve_menus_coop_option(each)) != NULL; each++)
		if (!strcmp(option->key, key))
			return option->standard;
	return NULL;
}

static const char *changed_get(void *context, const char *key)
{
	return !strcmp(key, "network.coop_team_wipe") ? "revert" : standard_get(context, key);
}

int main(void)
{
	char text[256];
	const char *mode, *all, *instant;
	long fewest, most;
	int found, each;
	struct ae_pve_menus_run runs[4];
	struct ae_pve_menus_press press = { 0, 0 };

	/* multiplayer: does nothing. No multiplayer setting is one of the keys these screens write */
	CHECK(!ae_pve_menus_key_allowed("game.multiplayer_scoring"));
	CHECK(!ae_pve_menus_key_allowed("game.multiplayer_medals"));
	CHECK(!ae_pve_menus_key_allowed("game.multiplayer_score_hud"));
	CHECK(!ae_pve_menus_key_allowed("network.vehicle_riding"));
	CHECK(!ae_pve_menus_key_allowed("game.score_hud_color"));
	CHECK(!ae_pve_menus_key_allowed("network.friendly_fire"));
	CHECK(!ae_pve_menus_key_allowed("game.companions"));
	CHECK(!ae_pve_menus_key_allowed(NULL) && !ae_pve_menus_key_allowed(""));
	CHECK(ae_pve_menus_key_allowed("game.skulls_active") && ae_pve_menus_key_allowed("game.campaign_score"));
	CHECK(ae_pve_menus_key_allowed("network.coop_enemies_mode"));
	/* every co-op option's key is one the screens may write */
	for (each = 0; each < AE_PVEM_COOP_OPTIONS; each++)
		CHECK(ae_pve_menus_key_allowed(ae_pve_menus_coop_option(each)->key));

	/* how skulls are found: the keys' mapping both ways */
	CHECK(ae_pve_menus_found_from_keys(1, 0) == AE_PVEM_FOUND_ALL && ae_pve_menus_found_from_keys(1, 1) == AE_PVEM_FOUND_ALL);
	CHECK(ae_pve_menus_found_from_keys(0, 0) == AE_PVEM_FOUND_LEVELS && ae_pve_menus_found_from_keys(0, 1) == AE_PVEM_FOUND_PICKUP);
	for (found = 0; found < AE_PVEM_FOUND_COUNT; found++)
	{
		ae_pve_menus_found_to_keys(found, &mode, &all, &instant);
		CHECK(!strcmp(mode, "play"));
		CHECK(ae_pve_menus_found_from_keys(!strcmp(all, "true"), !strcmp(instant, "true")) == found);
	}
	ae_pve_menus_found_to_keys(AE_PVEM_FOUND_PICKUP, &mode, &all, &instant);
	CHECK(!strcmp(all, "false") && !strcmp(instant, "true"));
	CHECK(!strcmp(ae_pve_menus_found_name(AE_PVEM_FOUND_LEVELS), "FOUND IN LEVELS"));

	/* drop counts and chances */
	ae_pve_menus_count_text(0, text, sizeof text);
	CHECK(!strcmp(text, "BY DIFFICULTY"));
	ae_pve_menus_count_text(3, text, sizeof text);
	CHECK(!strcmp(text, "3"));
	CHECK(ae_pve_menus_chance_percent(ae_pve_menus_chance_index(100)) == 100 && ae_pve_menus_chance_index(25) == 0);
	CHECK(ae_pve_menus_chance_index(400) == 4 && ae_pve_menus_chance_index(190) == 3);

	/* MOST is never below FEWEST: the one moved pushes the other */
	fewest = 10, most = 5;
	ae_pve_menus_random_clamp(&fewest, &most, 0);
	CHECK(fewest == 10 && most == 10);
	fewest = 10, most = 5;
	ae_pve_menus_random_clamp(&fewest, &most, 1);
	CHECK(fewest == 5 && most == 5);
	fewest = -3, most = 99;
	ae_pve_menus_random_clamp(&fewest, &most, 0);
	CHECK(fewest == 0 && most == 40);
	for (each = 0; each < 200; each++)
	{
		long n = ae_pve_menus_random_pick(1, 5, 40, (unsigned long)each * 2654435761UL);

		CHECK(n >= 1 && n <= 5);
	}
	CHECK(ae_pve_menus_random_pick(1, 5, 2, 12345) <= 2);
	CHECK(ae_pve_menus_random_pick(0, 0, 40, 7) == 0);

	/* lobby words */
	ae_pve_menus_skulls_value(0, text, sizeof text);
	CHECK(!strcmp(text, "OFF"));
	ae_pve_menus_skulls_value(4, text, sizeof text);
	CHECK(!strcmp(text, "4 ON"));
	ae_pve_menus_rules_fact(0, 0, text, sizeof text);
	CHECK(!strcmp(text, "NONE"));
	ae_pve_menus_rules_fact(4, 1, text, sizeof text);
	CHECK(!strcmp(text, "4 SKULLS \xE2\x80\xA2 SCORING ON"));
	ae_pve_menus_rules_fact(1, 0, text, sizeof text);
	CHECK(!strcmp(text, "1 SKULL"));

	/* STANDARD detection of the co-op options: AE's defaults, and per_player (the config's own default) for the enemies */
	CHECK(ae_pve_menus_coop_is_standard(standard_get, NULL));
	CHECK(!ae_pve_menus_coop_is_standard(changed_get, NULL));
	CHECK(!strcmp(ae_pve_menus_coop_option(4)->key, "network.coop_enemies_mode") && !strcmp(ae_pve_menus_coop_option(4)->standard, "per_player"));
	CHECK(ae_pve_menus_coop_value_index(AE_PVEM_COOP_MODE, "multiplier") == 1 && ae_pve_menus_coop_value_index(AE_PVEM_COOP_MODE, "x") == -1);
	CHECK(ae_pve_menus_coop_value_count(AE_PVEM_COOP_FRIENDLY) == 4 && !strcmp(ae_pve_menus_coop_value_label(AE_PVEM_COOP_FRIENDLY, 2), "SHIELD ONLY"));
	CHECK(ae_pve_menus_coop_factor("per_player", 50, 2, 2) == 150 && ae_pve_menus_coop_factor("per_player", 50, 2, 4) == 250);
	CHECK(ae_pve_menus_coop_factor("multiplier", 50, 4, 3) == 400 && ae_pve_menus_coop_factor("none", 50, 4, 3) == 100);
	ae_pve_menus_coop_factor_text(150, text, sizeof text);
	CHECK(!strcmp(text, "1.5\xC3\x97"));
	ae_pve_menus_coop_factor_text(400, text, sizeof text);
	CHECK(!strcmp(text, "4\xC3\x97"));

	/* scoring: only the first row is live with scoring off */
	CHECK(ae_pve_menus_scoring_reason(AE_PVEM_SCORING_ROW_ON, 0) == NULL);
	CHECK(!strcmp(ae_pve_menus_scoring_reason(AE_PVEM_SCORING_ROW_TEAM, 0), "NEEDS SCORING"));
	CHECK(ae_pve_menus_scoring_reason(AE_PVEM_SCORING_ROW_REVERT, 1) == NULL);

	/* the leaderboard: best score first, or best time first with no-time runs last */
	runs[0] = (struct ae_pve_menus_run){ 100, 300, 1, 0, 2, 0 };
	runs[1] = (struct ae_pve_menus_run){ 500, 0, 2, 1, 3, 1 };
	runs[2] = (struct ae_pve_menus_run){ 500, 200, 3, 4, 8, 2 };
	runs[3] = (struct ae_pve_menus_run){ 50, 100, 0, 0, 0, 3 };
	ae_pve_menus_runs_sort(runs, 4, 0);
	CHECK(runs[0].profile == 2 && runs[1].profile == 1 && runs[2].profile == 0 && runs[3].profile == 3);
	ae_pve_menus_runs_sort(runs, 4, 1);
	CHECK(runs[0].profile == 3 && runs[1].profile == 2 && runs[2].profile == 0 && runs[3].profile == 1);
	ae_pve_menus_time_text(0, text, sizeof text);
	CHECK(!strcmp(text, "-"));
	ae_pve_menus_time_text(1512, text, sizeof text);
	CHECK(!strcmp(text, "25:12"));
	ae_pve_menus_time_text(3725, text, sizeof text);
	CHECK(!strcmp(text, "1:02:05"));
	ae_pve_menus_number_text(65100, text, sizeof text);
	CHECK(!strcmp(text, "65,100"));
	ae_pve_menus_number_text(999, text, sizeof text);
	CHECK(!strcmp(text, "999"));

	/* saved games' times: calendar days count one apart across a month's and a leap year's ends */
	CHECK(ae_pve_menus_day(1970, 0, 1) == 0 && ae_pve_menus_day(2000, 2, 1) - ae_pve_menus_day(2000, 1, 29) == 1);
	CHECK(ae_pve_menus_day(2026, 11, 1) - ae_pve_menus_day(2026, 10, 30) == 1 && ae_pve_menus_day(2027, 0, 1) - ae_pve_menus_day(2026, 0, 1) == 365);
	CHECK(ae_pve_menus_day(2026, 10, 25) == 20782);
	/* saved games' times */
	ae_pve_menus_when_text(100, 100, 21, 14, 10, 2026, 25, text, sizeof text);
	CHECK(!strcmp(text, "TODAY 21:14"));
	ae_pve_menus_when_text(99, 100, 21, 14, 10, 2026, 24, text, sizeof text);
	CHECK(!strcmp(text, "YESTERDAY"));
	ae_pve_menus_when_text(50, 100, 21, 14, 10, 2026, 12, text, sizeof text);
	CHECK(!strcmp(text, "12 NOV 2026"));

	/* the end card: a button held as the card appears does not dismiss it, a new press does; A and B by card */
	CHECK(!ae_pve_menus_press_step(&press, 1));
	CHECK(!ae_pve_menus_press_step(&press, 1));
	CHECK(!ae_pve_menus_press_step(&press, 0));
	CHECK(ae_pve_menus_press_step(&press, 1));
	CHECK(!ae_pve_menus_press_step(&press, 1));
	CHECK(ae_pve_menus_card_a(0, 2, 0) == AE_PVEM_CARD_NEXT_PLAYER && ae_pve_menus_card_a(0, 2, 1) == AE_PVEM_CARD_NEXT_PLAYER);
	CHECK(ae_pve_menus_card_a(1, 2, 0) == AE_PVEM_CARD_NEXT_MISSION);
	CHECK(ae_pve_menus_card_a(0, 1, 1) == AE_PVEM_CARD_FINISH && ae_pve_menus_card_a(1, 2, 1) == AE_PVEM_CARD_FINISH);
	CHECK(!strcmp(ae_pve_menus_card_a_word(AE_PVEM_CARD_NEXT_PLAYER), "Next player") && !strcmp(ae_pve_menus_card_a_word(AE_PVEM_CARD_FINISH), "Finish"));
	ae_pve_menus_card_sub("THE PILLAR OF AUTUMN", "HEROIC", 0, text, sizeof text);
	CHECK(!strcmp(text, "THE PILLAR OF AUTUMN \xE2\x80\xA2 HEROIC \xE2\x80\xA2 NO SKULLS"));
	ae_pve_menus_card_sub("A", "EASY", 4, text, sizeof text);
	CHECK(strstr(text, "4 SKULLS") != NULL);

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
