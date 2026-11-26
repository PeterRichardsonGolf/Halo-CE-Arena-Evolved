#include <stdio.h>
#include <string.h>

#include "ae_firefight_menus_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char text[160];
	int round, kind, rule, choice;
	unsigned char content;

	/* the settings the screens may write: Firefight's alone, no multiplayer setting among them */
	CHECK(ae_ffm_key_allowed("game.campaign_score") && ae_ffm_key_allowed("game.firefight_objectives_round") &&
		ae_ffm_key_allowed("game.firefight_objectives_set_chance") && ae_ffm_key_allowed("game.firefight_objective_bomb"));
	CHECK(!ae_ffm_key_allowed(NULL) && !ae_ffm_key_allowed("") && !ae_ffm_key_allowed("game.firefight_pack") &&
		!ae_ffm_key_allowed("game.multiplayer_score") && !ae_ffm_key_allowed("network.vehicle_riding") &&
		!ae_ffm_key_allowed("game.skulls_active") && !ae_ffm_key_allowed("game.firefight_enemy_aim_error") &&
		!ae_ffm_key_allowed("game.firefight_objective_unknown") && !ae_ffm_key_allowed("bots.count"));

	/* the objectives: every kind's key is allowed and its own, a label and a help; the section's switch and chance */
	CHECK(ae_ffm_objective_count(AE_FFM_WAVE) == 8 && ae_ffm_objective_count(AE_FFM_ROUND) == 9 && ae_ffm_objective_count(2) == 0);
	for (round = 0; round < 2; round++)
	{
		int other;

		CHECK(ae_ffm_key_allowed(ae_ffm_objectives_key(round)) && ae_ffm_key_allowed(ae_ffm_chance_key(round)));
		CHECK(ae_ffm_objective(round, -1) == NULL && ae_ffm_objective(round, ae_ffm_objective_count(round)) == NULL);
		for (kind = 0; kind < ae_ffm_objective_count(round); kind++)
		{
			struct ae_ffm_objective const *row = ae_ffm_objective(round, kind);

			CHECK(row && row->key && ae_ffm_key_allowed(row->key) && row->label[0] && row->help[0]);
			for (other = kind + 1; other < ae_ffm_objective_count(round); other++)
				CHECK(strcmp(row->key, ae_ffm_objective(round, other)->key) != 0);
		}
		CHECK(ae_ffm_objectives_help(round, 1)[0] && ae_ffm_objectives_help(round, 0)[0] && ae_ffm_chance_help(round)[0]);
	}
	CHECK(strcmp(ae_ffm_objectives_key(0), ae_ffm_objectives_key(1)) && strcmp(ae_ffm_chance_key(0), ae_ffm_chance_key(1)));
	CHECK(strcmp(ae_ffm_objective(0, 0)->key, ae_ffm_objective(1, 0)->key));
	CHECK(ae_ffm_chance_percent(0) == 10 && ae_ffm_chance_percent(9) == 100 && ae_ffm_chance_percent(-3) == 10 && ae_ffm_chance_percent(40) == 100);
	CHECK(ae_ffm_chance_index(10) == 0 && ae_ffm_chance_index(50) == 4 && ae_ffm_chance_index(100) == 9 && ae_ffm_chance_index(0) == 0 &&
		ae_ffm_chance_index(999) == 9 && ae_ffm_chance_index(-5) == 0 && ae_ffm_chance_index(84) == 7);
	CHECK(!strcmp(ae_ffm_objectives_value(0, 0), "OFF") && !strcmp(ae_ffm_objectives_value(1, 0), "WAVE") &&
		!strcmp(ae_ffm_objectives_value(0, 1), "ROUND") && !strcmp(ae_ffm_objectives_value(1, 1), "BOTH"));

	/* SCALE WITH PLAYERS and VEHICLES */
	CHECK(ae_ffm_scale_percent(0) == 0 && ae_ffm_scale_percent(4) == 50 && ae_ffm_scale_percent(9) == 0 && ae_ffm_scale_index(10) == 2 &&
		ae_ffm_scale_index(0) == 0 && ae_ffm_scale_index(33) == 3 && ae_ffm_scale_index(100) == 4 && !strcmp(ae_ffm_scale_text(3), "20%") &&
		!ae_ffm_scale_text(7)[0]);
	CHECK(!strcmp(ae_ffm_vehicles_name(0), "ALL AT SPAWN") && !strcmp(ae_ffm_vehicles_name(2), "NONE") && !ae_ffm_vehicles_name(3)[0] &&
		ae_ffm_vehicles_help[0] && ae_ffm_scale_help[0]);

	/* MAP CONTENT: the mode and the kinds' choices live in separate bits, the engine's own values */
	CHECK(ae_ffm_content_mode(0) == AE_FFM_MODE_VANILLA && ae_ffm_content_mode(1) == AE_FFM_MODE_VANILLA &&
		ae_ffm_content_mode(2) == AE_FFM_MODE_INCLUDE && ae_ffm_content_mode(3) == AE_FFM_MODE_ONLY && ae_ffm_content_mode(0x7C) == AE_FFM_MODE_VANILLA);
	content = ae_ffm_content_with_mode(0x14, AE_FFM_MODE_ONLY);
	CHECK(content == 0x17 && ae_ffm_content_mode(content) == AE_FFM_MODE_ONLY && ae_ffm_content_with_mode(content, AE_FFM_MODE_VANILLA) == 0x14 &&
		ae_ffm_content_with_mode(0x01, AE_FFM_MODE_INCLUDE) == 0x02);
	/* each kind's choice reads back, and changes only its own bits */
	for (kind = 0; kind < AE_FFM_CONTENT_KINDS; kind++)
		for (choice = 0; choice < AE_FFM_CHOICES; choice++)
		{
			int other;

			content = ae_ffm_content_with_choice(0x02, kind, choice);
			CHECK(ae_ffm_content_choice(content, kind) == choice);
			for (other = 0; other < AE_FFM_CONTENT_KINDS; other++)
				if (other != kind)
					CHECK(ae_ffm_content_choice(content, other) == AE_FFM_CHOICE_ALL);
			CHECK(ae_ffm_content_mode(content) == AE_FFM_MODE_INCLUDE);
		}
	CHECK(ae_ffm_content_with_choice(0x02, AE_FFM_KIND_ENEMIES, AE_FFM_CHOICE_NONE) == 0x12 &&
		ae_ffm_content_with_choice(0x02, AE_FFM_KIND_WEAPONS, AE_FFM_CHOICE_CUSTOM) == 0x0A &&
		ae_ffm_content_with_choice(0x02, AE_FFM_KIND_VEHICLES, AE_FFM_CHOICE_CUSTOM) == 0x82 &&
		ae_ffm_content_with_choice(0x02, AE_FFM_KIND_VEHICLES, AE_FFM_CHOICE_NONE) == 0x42 &&
		ae_ffm_content_with_choice(0x72, AE_FFM_KIND_VEHICLES, AE_FFM_CHOICE_ALL) == 0x32 && ae_ffm_content_with_choice(5, 9, 1) == 5);
	CHECK(ae_ffm_has_kind(5, AE_FFM_KIND_ENEMIES) && !ae_ffm_has_kind(5, AE_FFM_KIND_WEAPONS) && ae_ffm_has_kind(5, AE_FFM_KIND_VEHICLES) &&
		!ae_ffm_has_kind(7, 3) && !ae_ffm_has_kind(7, -1));
	for (kind = 0; kind < AE_FFM_CONTENT_KINDS; kind++)
		CHECK(ae_ffm_kind_row(kind)[0] && ae_ffm_kind_list_row(kind)[0] && ae_ffm_kind_help(kind)[0] && ae_ffm_kind_list_help(kind)[0]);
	CHECK(!ae_ffm_kind_row(5)[0] && !ae_ffm_mode_name(7)[0] && !ae_ffm_choice_name(-1)[0] && ae_ffm_mode_help()[0] && ae_ffm_loadout_help()[0] &&
		!strcmp(ae_ffm_mode_name(1), "INCLUDE MAP") && !strcmp(ae_ffm_choice_name(2), "NONE") && ae_ffm_needs_content[0]);
	for (rule = 0; rule < AE_FFM_HEALTH_RULES; rule++)
	{
		CHECK(ae_ffm_health_row(rule)[0] && ae_ffm_health_help(rule)[0] && ae_ffm_health_choices(rule) >= 2);
		for (choice = 0; choice < ae_ffm_health_choices(rule); choice++)
			CHECK(ae_ffm_health_choice(rule, choice)[0]);
		CHECK(!ae_ffm_health_choice(rule, ae_ffm_health_choices(rule))[0]);
	}
	CHECK(ae_ffm_health_choices(AE_FFM_HEALTH_REGEN) == 2 && ae_ffm_health_choices(AE_FFM_HEALTH_SHIELDS) == 3 && !strcmp(ae_ffm_health_choice(0, 0), "MAP'S") &&
		!strcmp(ae_ffm_health_choice(2, 0), "OFF") && ae_ffm_health_choices(9) == 0);

	/* the lobby's value: what is taken of the map */
	ae_ffm_content_summary(7, ae_ffm_content_with_mode(0, AE_FFM_MODE_INCLUDE), 0, 0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "INCLUDE MAP: AI, WEAPONS, VEHICLES"));
	ae_ffm_content_summary(7, ae_ffm_content_with_choice(ae_ffm_content_with_choice(ae_ffm_content_with_mode(0, AE_FFM_MODE_ONLY),
		AE_FFM_KIND_WEAPONS, AE_FFM_CHOICE_CUSTOM), AE_FFM_KIND_VEHICLES, AE_FFM_CHOICE_NONE), 1, 1, 1, text, sizeof(text));
	CHECK(!strcmp(text, "ONLY MAP: AI, WEAPONS*, LOADOUT, HEALTH"));
	ae_ffm_content_summary(7, 0, 1, 1, 1, text, sizeof(text));
	CHECK(!strcmp(text, "VANILLA ONLY, LOADOUT, HEALTH"));
	ae_ffm_content_summary(5, ae_ffm_content_with_choice(ae_ffm_content_with_choice(ae_ffm_content_with_mode(0, AE_FFM_MODE_INCLUDE),
		AE_FFM_KIND_ENEMIES, AE_FFM_CHOICE_NONE), AE_FFM_KIND_VEHICLES, AE_FFM_CHOICE_NONE), 0, 0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "VANILLA ONLY"));
	ae_ffm_content_summary(0, 0, 1, 0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "HEALTH RULES: MAP'S"));
	ae_ffm_content_summary(0, 0, 1, 1, 0, text, sizeof(text));
	CHECK(!strcmp(text, "HEALTH RULES: CHANGED"));
	ae_ffm_content_summary(0, 0, 0, 0, 0, text, sizeof(text));
	CHECK(!text[0]);
	ae_ffm_content_summary(7, 2, 0, 0, 0, text, 12);
	CHECK(strlen(text) < 12);
	ae_ffm_content_summary(7, 2, 0, 0, 0, NULL, 0);

	/* a tag's name as a row shows it */
	ae_ffm_tag_label("weapons\\sniper\\sniper rifle", text, sizeof(text));
	CHECK(!strcmp(text, "SNIPER RIFLE"));
	ae_ffm_tag_label("vehicles/ghost/ghost", text, sizeof(text));
	CHECK(!strcmp(text, "GHOST"));
	ae_ffm_tag_label("a very long weapon name indeed", text, 8);
	CHECK(!strcmp(text, "A VERY "));
	ae_ffm_tag_label(NULL, text, sizeof(text));
	CHECK(!text[0]);

	if (failures)
		return 1;
	printf("ae_firefight_menus_rules_test: ok\n");
	return 0;
}
