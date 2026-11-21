/* ae_host_rules.c: CREATE GAME's HOST RULES rows, their words and what a pick writes */
#include <stdio.h>
#include <string.h>

#include "ae_host_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_host_rule_write writes[2];
	char text[64];
	int row;

	for (row = 0; row < AE_HR_ROWS; row++)
	{
		CHECK(ae_host_rule_count(row) >= 2);
		CHECK(ae_host_rule_label(row)[0] != 0);
		CHECK(ae_host_rule_help(row, 0)[0] != 0);
		CHECK(strstr(ae_host_rule_help(row, 0), "this PC hosts") != NULL);
	}
	/* words from the config's text */
	CHECK(ae_host_rule_index(AE_HR_TEXT_CHAT, "links", 0, 0) == 1);
	CHECK(ae_host_rule_index(AE_HR_GAME_VOICE, "team_global_enemy_proximity", 0, 0) == 4);
	CHECK(ae_host_rule_index(AE_HR_TEAM_CHANGING, "allow", 0, 0) == 1);
	CHECK(ae_host_rule_index(AE_HR_VOTEKICK, "false", 0, 0) == 1);
	CHECK(ae_host_rule_index(AE_HR_TEXT_CHAT, "bogus", 0, 0) == AE_HR_CUSTOM);
	/* numbers: "15.0" is 15; 17 is custom, nearest on a step */
	CHECK(ae_host_rule_index(AE_HR_VOICE_RANGE, "15.0", 0, 0) == 2);
	CHECK(ae_host_rule_index(AE_HR_CLIENT_TIMEOUT, "15", 0, 0) == 2);
	CHECK(ae_host_rule_index(AE_HR_CLIENT_TIMEOUT, "20", 0, 0) == AE_HR_CUSTOM);
	CHECK(ae_host_rule_index(AE_HR_CLIENT_TIMEOUT, "20", 0, 1) == 2);
	ae_host_rule_value_text(AE_HR_CLIENT_TIMEOUT, "20", 0, text, (int)sizeof(text));
	CHECK(!strcmp(text, "20 SECONDS"));
	ae_host_rule_value_text(AE_HR_VOICE_RANGE, "15.0", 0, text, (int)sizeof(text));
	CHECK(!strcmp(text, "15 M"));
	/* VOTES NEEDED: scaled, the three percents, a custom one shows its number */
	CHECK(ae_host_rule_index(AE_HR_VOTES_NEEDED, "dynamic", 50, 0) == 0);
	CHECK(ae_host_rule_index(AE_HR_VOTES_NEEDED, "static", 60, 0) == 2);
	CHECK(ae_host_rule_index(AE_HR_VOTES_NEEDED, "static", 66, 0) == AE_HR_CUSTOM);
	ae_host_rule_value_text(AE_HR_VOTES_NEEDED, "static", 66, text, (int)sizeof(text));
	CHECK(!strcmp(text, "CUSTOM (66 %)"));
	ae_host_rule_value_text(AE_HR_VOTES_NEEDED, "dynamic", 66, text, (int)sizeof(text));
	CHECK(!strcmp(text, "SCALED"));
	/* writes */
	CHECK(ae_host_rule_writes(AE_HR_VOTES_NEEDED, 0, writes) == 1 && !strcmp(writes[0].value, "dynamic"));
	CHECK(ae_host_rule_writes(AE_HR_VOTES_NEEDED, 3, writes) == 2 && !strcmp(writes[0].value, "static") &&
		!strcmp(writes[1].name, "network.votekick_percent") && !strcmp(writes[1].value, "75"));
	CHECK(ae_host_rule_writes(AE_HR_GAME_VOICE, 1, writes) == 1 && !strcmp(writes[0].name, "network.voice_mode") &&
		!strcmp(writes[0].value, "team_proximity"));
	CHECK(ae_host_rule_writes(AE_HR_CLIENT_TIMEOUT, 6, writes) == 1 && !strcmp(writes[0].value, "300"));
	CHECK(ae_host_rule_writes(AE_HR_TEXT_CHAT, 3, writes) == 0);
	/* the help names the lock */
	CHECK(strstr(ae_host_rule_help(AE_HR_TEXT_CHAT, 1), "PUBLIC") != NULL);
	/* the summary */
	ae_host_rules_summary("on", "balanced", "team_global", 0, text, (int)sizeof(text));
	CHECK(!strcmp(text, "TEAMS BALANCED, VOICE ON"));
	ae_host_rules_summary("on", "off", "off", 1, text, (int)sizeof(text));
	CHECK(!strcmp(text, "TEAMS LOCKED, VOICE OFF, CHAT OFF"));
	ae_host_rules_summary("off", "allow", "off", 0, text, 12);
	CHECK(strlen(text) < 12);
	if (failures)
		return 1;
	printf("ae_host_rules ok\n");
	return 0;
}
