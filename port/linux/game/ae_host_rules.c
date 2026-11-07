/*
AE_HOST_RULES.C

The HOST RULES panel's model (ae_host_rules.h).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_host_rules.h"

struct rule
{
	char const *label;
	char const *setting;
	short section;
	short count;
	/* the words shown, and the settings' text each is (numbers: the number) */
	char const *words[8];
	char const *values[8];
	/* numbers: the unit after the value ("SECONDS", "M"); NULL for words */
	char const *unit;
	char const *help;
};

static struct rule const rules[AE_HR_ROWS] =
{
	[AE_HR_TEXT_CHAT] = { "TEXT CHAT", "network.text_chat", AE_HR_SECTION_CHAT, 3, { "ON", "LINKS OK", "OFF" },
		{ "on", "links", "off" }, NULL,
		"Text chat in the game: ON refuses links in lines, LINKS OK allows them." },
	[AE_HR_LOBBY_VOICE] = { "LOBBY VOICE", "network.voice_lobby", AE_HR_SECTION_CHAT, 2, { "ON", "OFF" },
		{ "true", "false" }, NULL, "Voice in the lobby, before and after a game: everyone hears everyone." },
	[AE_HR_GAME_VOICE] = { "GAME VOICE", "network.voice_mode", AE_HR_SECTION_CHAT, 5,
		{ "OFF", "TEAM NEAR", "ANYONE NEAR", "TEAM", "TEAM + NEAR ENEMIES" },
		{ "off", "team_proximity", "team_enemy_proximity", "team_global", "team_global_enemy_proximity" }, NULL,
		"Voice in a game: every teammate, and enemies within VOICE RANGE." },
	[AE_HR_VOICE_RANGE] = { "VOICE RANGE", "network.voice_proximity", AE_HR_SECTION_CHAT, 6,
		{ "5 M", "10 M", "15 M", "20 M", "30 M", "50 M" }, { "5", "10", "15", "20", "30", "50" }, "M",
		"How near a player must be to be heard by the near voices of GAME VOICE." },
	[AE_HR_TEAM_CHANGING] = { "TEAM CHANGING", "network.team_change", AE_HR_SECTION_PLAYERS, 3,
		{ "BALANCING ONLY", "ALLOWED", "NOT ALLOWED" }, { "balanced", "allow", "off" }, NULL,
		"Whether players change team in a team game: BALANCING ONLY lets them when the teams stay even." },
	[AE_HR_CLIENT_TIMEOUT] = { "CLIENT TIMEOUT", "network.client_timeout_seconds", AE_HR_SECTION_PLAYERS, 7,
		{ "5 SECONDS", "10 SECONDS", "15 SECONDS", "30 SECONDS", "60 SECONDS", "120 SECONDS", "300 SECONDS" },
		{ "5", "10", "15", "30", "60", "120", "300" }, "SECONDS",
		"How long you wait for a player you hear nothing from before dropping them. Longer keeps a stalled player, "
		"and freezes the game for the others while it waits." },
	[AE_HR_VOTEKICK] = { "VOTE TO KICK", "network.votekick", AE_HR_SECTION_VOTES, 2, { "ON", "OFF" },
		{ "true", "false" }, NULL, "Players can vote to kick a player (the scoreboard's right-click, F1 yes, F2 no)." },
	[AE_HR_VOTES_NEEDED] = { "VOTES NEEDED", "network.votekick_threshold", AE_HR_SECTION_VOTES, 4,
		{ "SCALED", "50 %", "60 %", "75 %" }, { "dynamic", "50", "60", "75" }, NULL,
		"The yes votes a vote needs. SCALED: more than half up to 8 players, 40 % up to 32, 30 % beyond. A percent "
		"counts the players who may vote." },
	[AE_HR_VOTE_END] = { "VOTE TO END GAME", "network.vote_end_game", AE_HR_SECTION_VOTES, 2, { "ON", "OFF" },
		{ "true", "false" }, NULL, "Players can vote to end the game with its scores (!endgame in the chat)." },
};

static char const *const section_names[AE_HR_SECTIONS] = { "CHAT AND VOICE", "PLAYERS", "VOTES" };

char const *ae_host_rule_label(int row)
{
	return row >= 0 && row < AE_HR_ROWS ? rules[row].label : "";
}

int ae_host_rule_section(int row)
{
	return row >= 0 && row < AE_HR_ROWS ? rules[row].section : 0;
}

char const *ae_host_rule_section_name(int section)
{
	return section >= 0 && section < AE_HR_SECTIONS ? section_names[section] : "";
}

char const *ae_host_rule_setting(int row)
{
	return row >= 0 && row < AE_HR_ROWS ? rules[row].setting : "";
}

int ae_host_rule_count(int row)
{
	if (row < 0 || row >= AE_HR_ROWS)
		return 0;
	return rules[row].count;
}

char const *ae_host_rule_word(int row, int index)
{
	if (row < 0 || row >= AE_HR_ROWS || index < 0 || index >= rules[row].count)
		return "";
	return rules[row].words[index];
}

static char const *row_value(int row, int index)
{
	return rules[row].values[index];
}

int ae_host_rule_index(int row, char const *text, long percent, int nearest)
{
	int index, best = AE_HR_CUSTOM;
	long distance = 0, number;
	char *end;

	if (row < 0 || row >= AE_HR_ROWS || !text)
		return AE_HR_CUSTOM;
	if (row == AE_HR_VOTES_NEEDED)
	{
		if (!strcmp(text, "dynamic"))
			return 0;
		if (strcmp(text, "static"))
			return AE_HR_CUSTOM;
		for (index = 1; index < rules[row].count; index++)
			if (percent == atol(rules[row].values[index]))
				return index;
		return AE_HR_CUSTOM;
	}
	if (!rules[row].unit)
	{
		for (index = 0; index < rules[row].count; index++)
			if (!strcmp(text, rules[row].values[index]))
				return index;
		return AE_HR_CUSTOM;
	}
	number = strtol(text, &end, 10);
	if (end == text)
		return AE_HR_CUSTOM;
	for (index = 0; index < rules[row].count; index++)
	{
		long value = atol(row_value(row, index)), gap = value > number ? value - number : number - value;

		if (gap == 0)
			return index;
		if (best == AE_HR_CUSTOM || gap < distance)
		{
			best = index;
			distance = gap;
		}
	}
	return nearest ? best : AE_HR_CUSTOM;
}

void ae_host_rule_value_text(int row, char const *text, long percent, char *out, int size)
{
	int index = ae_host_rule_index(row, text, percent, 0);

	if (size <= 0)
		return;
	out[0] = 0;
	if (row < 0 || row >= AE_HR_ROWS)
		return;
	if (index != AE_HR_CUSTOM)
		snprintf(out, (size_t)size, "%s", ae_host_rule_word(row, index));
	else if (row == AE_HR_VOTES_NEEDED)
		snprintf(out, (size_t)size, "CUSTOM (%ld %%)", percent);
	else if (rules[row].unit && text)
		snprintf(out, (size_t)size, "%ld %s", atol(text), rules[row].unit);
	else
		snprintf(out, (size_t)size, "%s", text ? text : "");
}

int ae_host_rule_writes(int row, int index, struct ae_host_rule_write writes[2])
{
	if (row < 0 || row >= AE_HR_ROWS || index < 0 || index >= rules[row].count)
		return 0;
	if (row == AE_HR_VOTES_NEEDED)
	{
		writes[0].name = "network.votekick_threshold";
		snprintf(writes[0].value, sizeof(writes[0].value), "%s", index == 0 ? "dynamic" : "static");
		if (index == 0)
			return 1;
		writes[1].name = "network.votekick_percent";
		snprintf(writes[1].value, sizeof(writes[1].value), "%s", rules[row].values[index]);
		return 2;
	}
	writes[0].name = rules[row].setting;
	snprintf(writes[0].value, sizeof(writes[0].value), "%s", row_value(row, index));
	return 1;
}

char const *ae_host_rule_help(int row, int locked)
{
	static char text[320];

	if (row < 0 || row >= AE_HR_ROWS)
		return "";
	if (row == AE_HR_TEXT_CHAT && locked)
		return "Text chat is off in a PUBLIC game a player hosts, whatever this says. Turn PUBLIC off, or host on "
			"LAN, to use it. Kept for the games this PC hosts.";
	snprintf(text, sizeof(text), "%s Kept for the games this PC hosts.", rules[row].help);
	return text;
}

void ae_host_rules_summary(char const *chat, char const *team, char const *voice, int locked, char *out, int size)
{
	int index;

	if (size <= 0)
		return;
	index = ae_host_rule_index(AE_HR_TEAM_CHANGING, team, 0, 0);
	snprintf(out, (size_t)size, "TEAMS %s, VOICE %s", index == 1 ? "FREE" : index == 2 ? "LOCKED" : "BALANCED",
		voice && !strcmp(voice, "off") ? "OFF" : "ON");
	if (locked || (chat && !strcmp(chat, "off")))
	{
		int used = (int)strlen(out);

		snprintf(out + used, (size_t)(size - used), ", CHAT OFF");
	}
}
