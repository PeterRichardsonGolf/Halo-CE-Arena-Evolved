#include <stdio.h>
#include <string.h>
#include "ae_campaign_rows_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char text[256];
	int difficulty;

	/* SKIP TRAINING: live with one human on NORMAL (an AI squad does not matter); the first cause wins (split screen, difficulty) */
	CHECK(ae_campaign_rows_skip_cause(1, 1) == AE_CAMPAIGN_ROWS_SKIP_LIVE);
	CHECK(ae_campaign_rows_skip_cause(2, 2) == AE_CAMPAIGN_ROWS_SKIP_SPLIT_SCREEN);
	CHECK(ae_campaign_rows_skip_cause(2, 1) == AE_CAMPAIGN_ROWS_SKIP_SPLIT_SCREEN);
	CHECK(ae_campaign_rows_skip_cause(4, 1) == AE_CAMPAIGN_ROWS_SKIP_SPLIT_SCREEN);
	CHECK(ae_campaign_rows_skip_cause(1, 2) == AE_CAMPAIGN_ROWS_SKIP_DIFFICULTY);
	for (difficulty = 0; difficulty < 4; difficulty++)
		CHECK((ae_campaign_rows_skip_cause(1, difficulty) == AE_CAMPAIGN_ROWS_SKIP_LIVE) == (difficulty <= 1));
	CHECK(ae_campaign_rows_skip_cause(1, 0) == AE_CAMPAIGN_ROWS_SKIP_LIVE && ae_campaign_rows_skip_cause(1, 3) == AE_CAMPAIGN_ROWS_SKIP_DIFFICULTY);
	CHECK(ae_campaign_rows_skip_cause(2, 0) == AE_CAMPAIGN_ROWS_SKIP_SPLIT_SCREEN);
	CHECK(!strcmp(ae_campaign_rows_skip_reason(), "ALREADY SKIPPED"));
	ae_campaign_rows_skip_help(AE_CAMPAIGN_ROWS_SKIP_DIFFICULTY, 2, 0, text, sizeof text);
	CHECK(!strcmp(text, "On HEROIC, THE PILLAR OF AUTUMN already starts with a short wake-up."));
	ae_campaign_rows_skip_help(AE_CAMPAIGN_ROWS_SKIP_DIFFICULTY, 0, 1, text, sizeof text);
	CHECK(!strncmp(text, "On EASY, ", 9));
	ae_campaign_rows_skip_help(AE_CAMPAIGN_ROWS_SKIP_DIFFICULTY, 3, 0, text, sizeof text);
	CHECK(!strncmp(text, "On LEGENDARY, ", 14));
	ae_campaign_rows_skip_help(AE_CAMPAIGN_ROWS_SKIP_SPLIT_SCREEN, 1, 0, text, sizeof text);
	CHECK(!strncmp(text, "In split screen, ", 17));
	ae_campaign_rows_skip_help(AE_CAMPAIGN_ROWS_SKIP_LIVE, 1, 0, text, sizeof text);
	CHECK(!strncmp(text, "ON: alone on EASY or NORMAL", 27));
	ae_campaign_rows_skip_help(AE_CAMPAIGN_ROWS_SKIP_LIVE, 1, 1, text, sizeof text);
	CHECK(!strncmp(text, "THE PILLAR OF AUTUMN starts with a short wake-up, then you play.", 64));

	/* CONTINUE's case */
	CHECK(ae_campaign_rows_continue_case(0, 1, 0) == AE_CAMPAIGN_ROWS_CONTINUE_PLAIN);
	CHECK(ae_campaign_rows_continue_case(0, 1, 1) == AE_CAMPAIGN_ROWS_CONTINUE_PLAIN);
	CHECK(ae_campaign_rows_continue_case(3, 1, 1) == AE_CAMPAIGN_ROWS_CONTINUE_SQUAD);
	CHECK(ae_campaign_rows_continue_case(3, 1, 0) == AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY);
	CHECK(ae_campaign_rows_continue_case(3, 2, 1) == AE_CAMPAIGN_ROWS_CONTINUE_SPLIT);
	CHECK(ae_campaign_rows_continue_case(0, 3, 0) == AE_CAMPAIGN_ROWS_CONTINUE_SPLIT);
	/* the dialog is off in this build: A never asks */
	CHECK(!ae_campaign_rows_dialog_enabled());
	/* CONTINUE with an AI SQUAD set is played alone while a saved game cannot take the squad (SOLO ONLY) */
	CHECK(!ae_campaign_rows_continue_hosts_squad(1, AE_CAMPAIGN_SAVE_TAKES_SQUAD));
	CHECK(ae_campaign_rows_continue_hosts_squad(1, 1) && !ae_campaign_rows_continue_hosts_squad(0, 1));
	CHECK(ae_campaign_rows_continue_case(2, 1, AE_CAMPAIGN_SAVE_TAKES_SQUAD) == AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY);
	CHECK(!ae_campaign_rows_continue_asks(AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY));
	CHECK(!ae_campaign_rows_continue_asks(AE_CAMPAIGN_ROWS_CONTINUE_SQUAD) && !ae_campaign_rows_continue_asks(AE_CAMPAIGN_ROWS_CONTINUE_PLAIN) &&
		!ae_campaign_rows_continue_asks(AE_CAMPAIGN_ROWS_CONTINUE_SPLIT));
	ae_campaign_rows_continue_value("LEVEL 3 \xE2\x80\xA2 HEROIC", AE_CAMPAIGN_ROWS_CONTINUE_SQUAD, 3, text, sizeof text);
	CHECK(!strcmp(text, "LEVEL 3 \xE2\x80\xA2 HEROIC \xE2\x80\xA2 +3 AI"));
	ae_campaign_rows_continue_value("LEVEL 3 \xE2\x80\xA2 HEROIC", AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY, 3, text, sizeof text);
	CHECK(!strcmp(text, "LEVEL 3 \xE2\x80\xA2 HEROIC \xE2\x80\xA2 SOLO ONLY"));
	ae_campaign_rows_continue_value("MISSION 2 \xE2\x80\xA2 HEROIC", AE_CAMPAIGN_ROWS_CONTINUE_PLAIN, 0, text, sizeof text);
	CHECK(!strcmp(text, "MISSION 2 \xE2\x80\xA2 HEROIC"));
	ae_campaign_rows_continue_value("LEVEL 3 \xE2\x80\xA2 HEROIC", AE_CAMPAIGN_ROWS_CONTINUE_SPLIT, 3, text, sizeof text);
	CHECK(!strcmp(text, "LEVEL 3 \xE2\x80\xA2 HEROIC"));
	ae_campaign_rows_continue_help(AE_CAMPAIGN_ROWS_CONTINUE_SQUAD, 3, 0, text, sizeof text);
	CHECK(!strcmp(text, "Goes on from your saved game, at its difficulty, with your AI squad of 3."));
	ae_campaign_rows_continue_help(AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY, 3, 0, text, sizeof text);
	CHECK(!strcmp(text, "{A} continues this saved game alone. The AI squad joins a new mission, not a saved game."));
	ae_campaign_rows_continue_help_dialog(3, text, sizeof text);
	CHECK(!strncmp(text, "Your AI squad cannot join this saved game. {A}:", 47));
	CHECK(ae_campaign_rows_squad_cause(0, 1) == AE_CAMPAIGN_ROWS_SQUAD_OK && ae_campaign_rows_squad_cause(1, 1) == AE_CAMPAIGN_ROWS_SQUAD_WORKSHOP &&
		ae_campaign_rows_squad_cause(0, 2) == AE_CAMPAIGN_ROWS_SQUAD_SPLIT && ae_campaign_rows_squad_cause(1, 3) == AE_CAMPAIGN_ROWS_SQUAD_WORKSHOP);
	CHECK(!ae_campaign_rows_squad_reason(AE_CAMPAIGN_ROWS_SQUAD_OK) && !ae_campaign_rows_squad_help(AE_CAMPAIGN_ROWS_SQUAD_OK));
	CHECK(!strcmp(ae_campaign_rows_squad_reason(AE_CAMPAIGN_ROWS_SQUAD_SPLIT), "SOLO ONLY"));
	CHECK(!strcmp(ae_campaign_rows_workshop_reason(), "STOCK CAMPAIGN ONLY"));
	CHECK(!strcmp(ae_campaign_rows_workshop_help(), "The AI squad joins the Halo CE campaign's missions. Workshop campaigns play without it for now."));
	ae_campaign_rows_continue_help(AE_CAMPAIGN_ROWS_CONTINUE_PLAIN, 0, 0, text, sizeof text);
	CHECK(!strcmp(text, "Goes on from your saved game, at its difficulty."));
	/* the longest value fits the screen's 48-byte text */
	ae_campaign_rows_continue_value("LEVEL 10 \xE2\x80\xA2 LEGENDARY", AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY, 3, text, sizeof text);
	CHECK(strlen(text) < 48);
	ae_campaign_rows_continue_value("MISSION 12 \xE2\x80\xA2 LEGENDARY", AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY, 3, text, sizeof text);
	CHECK(strlen(text) < 48);

	/* the dialog */
	CHECK(!strcmp(ae_campaign_rows_dialog_title(), "THIS SAVED GAME IS SOLO ONLY"));
	ae_campaign_rows_dialog_body("THE TRUTH AND RECONCILIATION", 2, text, sizeof text);
	CHECK(!strcmp(text, "Your AI squad cannot join this saved game. With your squad, THE TRUTH AND RECONCILIATION starts over on HEROIC, and its first checkpoint replaces this save."));
	CHECK(!strcmp(ae_campaign_rows_dialog_choice(0), "CONTINUE ALONE") && !strcmp(ae_campaign_rows_dialog_choice(1), "START THE MISSION WITH MY SQUAD"));
	CHECK(ae_campaign_rows_dialog_action(0) == AE_CAMPAIGN_ROWS_ACT_CONTINUE_ALONE);
	CHECK(ae_campaign_rows_dialog_action(1) == AE_CAMPAIGN_ROWS_ACT_START_WITH_SQUAD);
	CHECK(ae_campaign_rows_dialog_action(-1) == AE_CAMPAIGN_ROWS_ACT_NONE && ae_campaign_rows_dialog_action(2) == AE_CAMPAIGN_ROWS_ACT_NONE);

	/* PLAYER DOWN */
	CHECK(ae_campaign_rows_down_is_respawn("respawn") && !ae_campaign_rows_down_is_respawn("checkpoint") &&
		!ae_campaign_rows_down_is_respawn("") && !ae_campaign_rows_down_is_respawn(NULL) && !ae_campaign_rows_down_is_respawn("Respawn "));
	CHECK(!strcmp(ae_campaign_rows_down_value(0), "CHECKPOINT") && !strcmp(ae_campaign_rows_down_value(1), "RESPAWN"));
	CHECK(!strcmp(ae_campaign_rows_down_next(0), "respawn") && !strcmp(ae_campaign_rows_down_next(1), "checkpoint"));
	CHECK(!strcmp(ae_campaign_rows_down_help(0, 1), "As in co-op, a player comes back only beside another player, not the squad. With no player left standing, back to the last checkpoint."));
	CHECK(!strcmp(ae_campaign_rows_down_help(1, 1), "A player who goes down comes back beside anyone standing, squad included, within 10 seconds; if no one is, back to the last checkpoint."));
	CHECK(!strcmp(ae_campaign_rows_down_help(1, 0), "Turn AI SQUAD on first: this sets what happens when a player goes down."));
	CHECK(!strcmp(ae_campaign_rows_down_reason(), "NEEDS AI SQUAD"));

	/* the squad MY PARTY counts: none beside a CONTINUE that plays alone */
	CHECK(ae_campaign_rows_party_squad(3, 0, AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY) == 3);
	CHECK(ae_campaign_rows_party_squad(3, 1, AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY) == 0);
	CHECK(ae_campaign_rows_party_squad(3, 1, AE_CAMPAIGN_ROWS_CONTINUE_SQUAD) == 3);
	CHECK(ae_campaign_rows_party_squad(3, 1, AE_CAMPAIGN_ROWS_CONTINUE_PLAIN) == 3);
	CHECK(ae_campaign_rows_party_squad(0, 1, AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY) == 0);
	CHECK(!strcmp(ae_campaign_rows_squad_help(AE_CAMPAIGN_ROWS_SQUAD_SPLIT), "The AI squad joins a new mission you start alone. Split screen plays without it for now."));
	CHECK(!strcmp(ae_campaign_rows_squad_ai_help(), "A squad of allied Spartans, played by the game, joins each new mission you start alone: left / right choose how many."));
	/* MY PARTY's count */
	ae_campaign_rows_party_count(1, 4, 0, text, sizeof text);
	CHECK(!strcmp(text, "1/4"));
	ae_campaign_rows_party_count(1, 4, 3, text, sizeof text);
	CHECK(!strcmp(text, "1 + 3 AI"));
	ae_campaign_rows_party_count(2, 4, 0, text, sizeof text);
	CHECK(!strcmp(text, "2/4"));

	/* DOOR STACK: "every" is EVERY DOOR, anything else ON ORDER; the help names what each does, and asks for the squad first */
	CHECK(ae_campaign_rows_door_is_every("every") && !ae_campaign_rows_door_is_every("order") &&
		!ae_campaign_rows_door_is_every("") && !ae_campaign_rows_door_is_every(NULL));
	CHECK(!strcmp(ae_campaign_rows_door_value(0), "ON ORDER") && !strcmp(ae_campaign_rows_door_value(1), "EVERY DOOR"));
	CHECK(!strcmp(ae_campaign_rows_door_next(0), "every") && !strcmp(ae_campaign_rows_door_next(1), "order"));
	CHECK(!strcmp(ae_campaign_rows_door_help(0, 1), "Mark a door with DEFEND or SMART and your squad lines up beside it; ATTACK sends them in. Otherwise they follow you through."));
	CHECK(!strcmp(ae_campaign_rows_door_help(1, 1), "Your squad lines up beside any closed door you wait at, and goes in when you walk through or call ATTACK."));
	CHECK(!strcmp(ae_campaign_rows_door_help(1, 0), "Turn AI SQUAD on first: this sets how your squad takes doors."));
	/* FORMATIONS: ON / OFF and its help by value; dimmed without a squad */
	CHECK(!strcmp(ae_campaign_rows_formations_value(1), "ON") && !strcmp(ae_campaign_rows_formations_value(0), "OFF"));
	CHECK(strstr(ae_campaign_rows_formations_help(1, 1), "forms up by itself") != NULL);
	CHECK(strstr(ae_campaign_rows_formations_help(0, 1), "no formation of its own") != NULL);
	CHECK(!strcmp(ae_campaign_rows_formations_help(1, 0), "Turn AI SQUAD on first: this sets whether your squad forms up by itself."));

	if (!failures)
		printf("ae_campaign_rows_rules: ok\n");
	return failures != 0;
}
