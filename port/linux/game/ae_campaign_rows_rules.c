/*
AE_CAMPAIGN_ROWS_RULES.C

The CAMPAIGN screen's row rules (ae_campaign_rows_rules.h).
*/

#include <stdio.h>
#include <string.h>

#include "ae_campaign_rows_rules.h"

enum { DIFFICULTY_EASY = 0, DIFFICULTY_NORMAL = 1 };

static char const *const difficulty_words[] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };

static char const *difficulty_word(int difficulty)
{
	return difficulty >= 0 && difficulty < 4 ? difficulty_words[difficulty] : "";
}

int ae_campaign_rows_skip_cause(int players, int difficulty)
{
	if (players > 1)
		return AE_CAMPAIGN_ROWS_SKIP_SPLIT_SCREEN;
	/* (the level's script is told EASY as NORMAL, so EASY plays the training too) */
	if (difficulty != DIFFICULTY_NORMAL && difficulty != DIFFICULTY_EASY)
		return AE_CAMPAIGN_ROWS_SKIP_DIFFICULTY;
	return AE_CAMPAIGN_ROWS_SKIP_LIVE;
}

char const *ae_campaign_rows_skip_reason(void)
{
	return "ALREADY SKIPPED";
}

void ae_campaign_rows_skip_help(int cause, int difficulty, int on, char *text, int size)
{
	switch (cause)
	{
	case AE_CAMPAIGN_ROWS_SKIP_SPLIT_SCREEN:
		snprintf(text, size, "In split screen, THE PILLAR OF AUTUMN already starts with a short wake-up.");
		break;
	case AE_CAMPAIGN_ROWS_SKIP_DIFFICULTY:
		snprintf(text, size, "On %s, THE PILLAR OF AUTUMN already starts with a short wake-up.", difficulty_word(difficulty));
		break;
	default:
		if (on)
			snprintf(text, size, "THE PILLAR OF AUTUMN starts with a short wake-up, then you play. No look test: set INVERT LOOK in PROFILES.");
		else
			snprintf(text, size, "ON: alone on EASY or NORMAL, THE PILLAR OF AUTUMN skips its training: look test, HUD check, tutorial prompts, first-contact scene.");
		break;
	}
}

int ae_campaign_rows_down_is_respawn(char const *setting)
{
	return setting && !strcmp(setting, "respawn");
}

char const *ae_campaign_rows_down_value(int respawn)
{
	return respawn ? "RESPAWN" : "CHECKPOINT";
}

/* the setting's text after one step (the two values turn over) */
char const *ae_campaign_rows_down_next(int respawn)
{
	return respawn ? "checkpoint" : "respawn";
}

char const *ae_campaign_rows_down_help(int respawn, int squad_on)
{
	if (!squad_on)
		return "Turn AI SQUAD on first: this sets what happens when a player goes down.";
	return respawn ?
		"A player who goes down comes back beside anyone standing, squad included, within 10 seconds; if no one is, back to the last checkpoint." :
		"As in co-op, a player comes back only beside another player, not the squad. With no player left standing, back to the last checkpoint.";
}

char const *ae_campaign_rows_down_reason(void)
{
	return "NEEDS AI SQUAD";
}

int ae_campaign_rows_continue_case(int squad_count, int players, int save_takes_squad)
{
	if (players > 1)
		return AE_CAMPAIGN_ROWS_CONTINUE_SPLIT;
	if (squad_count <= 0)
		return AE_CAMPAIGN_ROWS_CONTINUE_PLAIN;
	return save_takes_squad ? AE_CAMPAIGN_ROWS_CONTINUE_SQUAD : AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY;
}

void ae_campaign_rows_continue_value(char const *base, int continue_case, int squad_count, char *text, int size)
{
	if (continue_case == AE_CAMPAIGN_ROWS_CONTINUE_SQUAD)
		snprintf(text, size, "%s \xE2\x80\xA2 +%d AI", base, squad_count);
	else if (continue_case == AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY)
		snprintf(text, size, "%s \xE2\x80\xA2 SOLO ONLY", base);
	else
		snprintf(text, size, "%s", base);
}

void ae_campaign_rows_continue_help(int continue_case, int squad_count, int workshop, char *text, int size)
{
	(void)workshop;
	if (continue_case == AE_CAMPAIGN_ROWS_CONTINUE_SQUAD)
		snprintf(text, size, "Goes on from your saved game, at its difficulty, with your AI squad of %d.", squad_count);
	else if (continue_case == AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY && ae_campaign_rows_dialog_enabled())
		ae_campaign_rows_continue_help_dialog(squad_count, text, size);
	else if (continue_case == AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY)
		snprintf(text, size, "{A} continues this saved game alone. The AI squad joins a new mission, not a saved game.");
	else
		snprintf(text, size, "Goes on from your saved game, at its difficulty.");
}

int ae_campaign_rows_dialog_enabled(void)
{
	return 0;
}

int ae_campaign_rows_continue_asks(int continue_case)
{
	return ae_campaign_rows_dialog_enabled() && continue_case == AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY;
}

void ae_campaign_rows_continue_help_dialog(int squad_count, char *text, int size)
{
	(void)squad_count;
	snprintf(text, size, "Your AI squad cannot join this saved game. {A}: continue alone, or start the mission with your squad.");
}

int ae_campaign_rows_squad_cause(int workshop, int players)
{
	return workshop ? AE_CAMPAIGN_ROWS_SQUAD_WORKSHOP : players > 1 ? AE_CAMPAIGN_ROWS_SQUAD_SPLIT : AE_CAMPAIGN_ROWS_SQUAD_OK;
}

char const *ae_campaign_rows_squad_reason(int cause)
{
	return cause == AE_CAMPAIGN_ROWS_SQUAD_WORKSHOP ? ae_campaign_rows_workshop_reason() :
		cause == AE_CAMPAIGN_ROWS_SQUAD_SPLIT ? "SOLO ONLY" : NULL;
}

char const *ae_campaign_rows_squad_help(int cause)
{
	return cause == AE_CAMPAIGN_ROWS_SQUAD_WORKSHOP ? ae_campaign_rows_workshop_help() :
		cause == AE_CAMPAIGN_ROWS_SQUAD_SPLIT ? "The AI squad joins a new mission you start alone. Split screen plays without it for now." : NULL;
}

char const *ae_campaign_rows_squad_ai_help(void)
{
	return "A squad of allied Spartans, played by the game, joins each new mission you start alone: left / right choose how many.";
}

int ae_campaign_rows_party_squad(int squad_count, int continue_focused, int continue_case)
{
	return continue_focused && continue_case == AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY ? 0 : squad_count;
}

char const *ae_campaign_rows_workshop_reason(void)
{
	return "STOCK CAMPAIGN ONLY";
}

char const *ae_campaign_rows_workshop_help(void)
{
	return "The AI squad joins the Halo CE campaign's missions. Workshop campaigns play without it for now.";
}

char const *ae_campaign_rows_dialog_title(void)
{
	return "THIS SAVED GAME IS SOLO ONLY";
}

void ae_campaign_rows_dialog_body(char const *level_name, int difficulty, char *text, int size)
{
	snprintf(text, size, "Your AI squad cannot join this saved game. With your squad, %s starts over on %s, and its first "
		"checkpoint replaces this save.", level_name, difficulty_word(difficulty));
}

char const *ae_campaign_rows_dialog_choice(int choice)
{
	return choice == 0 ? "CONTINUE ALONE" : choice == 1 ? "START THE MISSION WITH MY SQUAD" : "";
}

int ae_campaign_rows_dialog_action(int choice)
{
	return choice == 0 ? AE_CAMPAIGN_ROWS_ACT_CONTINUE_ALONE : choice == 1 ? AE_CAMPAIGN_ROWS_ACT_START_WITH_SQUAD :
		AE_CAMPAIGN_ROWS_ACT_NONE;
}

void ae_campaign_rows_party_count(int players, int players_max, int squad_count, char *text, int size)
{
	if (squad_count > 0)
		snprintf(text, size, "%d + %d AI", players, squad_count);
	else
		snprintf(text, size, "%d/%d", players, players_max);
}

int ae_campaign_rows_continue_hosts_squad(int squad_asked, int save_takes_squad)
{
	return squad_asked && save_takes_squad;
}
