/* A dedicated server's lobby page: the pure rules (see the header) */
#include <stdio.h>
#include <string.h>

#include "ae_server_lobby_rules.h"

#define DOT " \xE2\x80\xA2 "

/* ---------- the wire */

static int printable_ended(const char *text, int size)
{
	int index;

	for (index = 0; index < size; index++)
	{
		unsigned char c = (unsigned char)text[index];

		if (!c)
			return 1;
		if (c < 0x20 || c > 0x7E)
			return 0;
	}
	return 0;
}

/* (the card's facts may hold the dots: U+2022, three bytes) */
static int facts_ended(const char *text, int size)
{
	int index;

	for (index = 0; index < size; index++)
	{
		unsigned char c = (unsigned char)text[index];

		if (!c)
			return 1;
		if (c == 0xE2 && index + 2 < size && (unsigned char)text[index + 1] == 0x80 &&
			(unsigned char)text[index + 2] == 0xA2)
		{
			index += 2;
			continue;
		}
		if (c < 0x20 || c > 0x7E)
			return 0;
	}
	return 0;
}

void ae_sl_printable(const char *text, char *kept, int size)
{
	int index;

	if (size <= 0)
		return;
	for (index = 0; text && text[index] && index < size - 1; index++)
	{
		unsigned char c = (unsigned char)text[index];

		kept[index] = c >= 0x20 && c <= 0x7E ? (char)c : ' ';
	}
	kept[index] = 0;
}

static void put_text(unsigned char *at, const char *text, int size)
{
	int length = 0;

	while (length < size - 1 && text[length])
		length++;

	memset(at, 0, (size_t)size);
	memcpy(at, text, (size_t)length);
}

int ae_sl_wire_encode(const struct ae_sl_wire *state, unsigned char *bytes, int size)
{
	int needed, index;
	unsigned char *at = bytes;

	if (!state || !bytes || state->count > AE_SL_ENTRIES || state->team_rule > AE_SL_TEAMS_OPEN ||
		state->join > AE_SL_JOIN_AE_MCC || (state->flags & ~AE_SL_FLAGS))
	{
		return 0;
	}
	needed = AE_SL_HEAD + state->count * AE_SL_ENTRY;
	if (size < needed)
		return 0;
	*at++ = AE_SL_WIRE_VERSION;
	*at++ = state->flags;
	*at++ = state->team_rule;
	*at++ = state->join;
	*at++ = state->min_players;
	*at++ = state->max_players;
	*at++ = state->bots_red;
	*at++ = state->bots_blue;
	*at++ = state->bots;
	*at++ = state->vote_seconds;
	*at++ = state->count;
	put_text(at, state->region, AE_SL_REGION);
	at += AE_SL_REGION;
	put_text(at, state->motd, AE_SL_MOTD);
	at += AE_SL_MOTD;
	for (index = 0; index < state->count; index++)
	{
		const struct ae_sl_entry *entry = &state->entries[index];

		if (!entry->roles || (entry->roles & ~AE_SL_ENTRY_ROLES))
			return 0;
		*at++ = entry->roles;
		put_text(at, entry->map, AE_SL_MAP);
		at += AE_SL_MAP;
		put_text(at, entry->gametype, AE_SL_GAMETYPE);
		at += AE_SL_GAMETYPE;
		put_text(at, entry->name, AE_SL_NAME);
		at += AE_SL_NAME;
		put_text(at, entry->facts, AE_SL_FACTS);
		at += AE_SL_FACTS;
	}
	return (int)(at - bytes);
}

int ae_sl_wire_decode(const unsigned char *bytes, int size, struct ae_sl_wire *state)
{
	const unsigned char *at = bytes;
	int index;

	if (!bytes || !state || size < AE_SL_HEAD || at[0] != AE_SL_WIRE_VERSION)
		return 0;
	memset(state, 0, sizeof(*state));
	at++;
	state->flags = *at++;
	state->team_rule = *at++;
	state->join = *at++;
	state->min_players = *at++;
	state->max_players = *at++;
	state->bots_red = *at++;
	state->bots_blue = *at++;
	state->bots = *at++;
	state->vote_seconds = *at++;
	state->count = *at++;
	if ((state->flags & ~AE_SL_FLAGS) || state->team_rule > AE_SL_TEAMS_OPEN || state->join > AE_SL_JOIN_AE_MCC ||
		state->count > AE_SL_ENTRIES || size != AE_SL_HEAD + state->count * AE_SL_ENTRY)
	{
		return 0;
	}
	memcpy(state->region, at, AE_SL_REGION);
	at += AE_SL_REGION;
	memcpy(state->motd, at, AE_SL_MOTD);
	at += AE_SL_MOTD;
	if (!printable_ended(state->region, AE_SL_REGION) || !printable_ended(state->motd, AE_SL_MOTD))
		return 0;
	for (index = 0; index < state->count; index++)
	{
		struct ae_sl_entry *entry = &state->entries[index];

		entry->roles = *at++;
		memcpy(entry->map, at, AE_SL_MAP);
		at += AE_SL_MAP;
		memcpy(entry->gametype, at, AE_SL_GAMETYPE);
		at += AE_SL_GAMETYPE;
		memcpy(entry->name, at, AE_SL_NAME);
		at += AE_SL_NAME;
		memcpy(entry->facts, at, AE_SL_FACTS);
		at += AE_SL_FACTS;
		if (!entry->roles || (entry->roles & ~AE_SL_ENTRY_ROLES) || !printable_ended(entry->map, AE_SL_MAP) ||
			!entry->map[0] || !printable_ended(entry->gametype, AE_SL_GAMETYPE) ||
			!printable_ended(entry->name, AE_SL_NAME) || !facts_ended(entry->facts, AE_SL_FACTS))
		{
			return 0;
		}
	}
	return 1;
}

/* ---------- the entries */

int ae_sl_entry_with(const struct ae_sl_wire *state, int role, int nth)
{
	int index;

	for (index = 0; state && index < state->count; index++)
	{
		if ((state->entries[index].roles & role) && nth-- == 0)
			return index;
	}
	return -1;
}

int ae_sl_entry_of(const struct ae_sl_wire *state, const char *map, const char *gametype)
{
	int index;

	if (!state || !map || !gametype)
		return -1;
	for (index = 0; index < state->count; index++)
	{
		if (!strcmp(state->entries[index].map, map) && !strcmp(state->entries[index].gametype, gametype))
			return index;
	}
	return -1;
}

void ae_sl_gametype_word(const char *gametype, char *text, int size)
{
	const char *name = gametype ? gametype : "";
	int length = 0, index;

	if (size <= 0)
		return;
	if (!strncmp(name, "file=", 5))
	{
		const char *slash = strrchr(name + 5, '/');
		const char *backslash = strrchr(name + 5, '\\');

		name += 5;
		if (slash && slash + 1 > name)
			name = slash + 1;
		if (backslash && backslash + 1 > name)
			name = backslash + 1;
	}
	for (index = 0; name[index] && name[index] != '+' && length < size - 1; index++)
	{
		char c = name[index];

		if (index > 0 && name[index - 1] != '_' && !strncmp(name + index, "_plus", 5))
		{
			text[length++] = '+';
			index += 4;
			continue;
		}
		if (c == '_')
			c = ' ';
		else if (c >= 'a' && c <= 'z')
			c = (char)(c - 32);
		else if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E)
			c = '?';
		text[length++] = c;
	}
	/* (a file's own name: its ".toml" or ".bin" left out) */
	if (length > 5 && !strncmp(text + length - 5, ".TOML", 5))
		length -= 5;
	text[length] = 0;
}

/* "15 MINUTES" -> "15 MIN", "1 MINUTE" -> "1 MIN"; other words as they are */
static void short_time(const char *time, char *text, int size)
{
	const char *word = strstr(time, " MINUTE");

	if (word && (!strcmp(word, " MINUTE") || !strcmp(word, " MINUTES")))
		snprintf(text, (size_t)size, "%.*s MIN", (int)(word - time), time);
	else
		snprintf(text, (size_t)size, "%s", time);
}

void ae_sl_facts(int side, const char *start, const char *goal, const char *time, char *text, int size)
{
	char part[40];
	int length = 0;

	if (size <= 0)
		return;
	text[0] = 0;
	if (side > 0)
		length += snprintf(text + length, (size_t)(size - length), "%d V %d", side, side);
	else
		length += snprintf(text + length, (size_t)(size - length), "FREE FOR ALL");
	if (start && start[0] && length < size)
		length += snprintf(text + length, (size_t)(size - length), DOT "%s", start);
	if (goal && goal[0] && length < size)
		length += snprintf(text + length, (size_t)(size - length), DOT "%s", goal);
	if (time && time[0] && strcmp(time, "NO TIME LIMIT") && length < size)
	{
		short_time(time, part, (int)sizeof(part));
		snprintf(text + length, (size_t)(size - length), DOT "%s", part);
	}
	/* (cut: never inside the dot's three bytes) */
	length = (int)strlen(text);
	while (length > 0 && ((unsigned char)text[length - 1] == 0xE2 || (unsigned char)text[length - 1] == 0x80 ||
		text[length - 1] == ' '))
		text[--length] = 0;
}

/* ---------- the page's words */

int ae_sl_state(int downloading, int countdown_seconds, int vote_heard, int vote_winner)
{
	if (downloading)
		return AE_SL_STATE_DOWNLOAD;
	if (countdown_seconds >= 0)
		return AE_SL_STATE_COUNTDOWN;
	if (vote_heard && vote_winner >= 0)
		return AE_SL_STATE_RESULT;
	if (vote_heard)
		return AE_SL_STATE_VOTE;
	return AE_SL_STATE_WAITING;
}

const char *ae_sl_join_word(int join)
{
	switch (join)
	{
	case AE_SL_JOIN_ANYONE: return "ANYONE";
	case AE_SL_JOIN_AE_MCC: return "AE+MCC";
	}
	return "AE";
}

void ae_sl_people_line(int people, int maximum, int bots, char *text, int size)
{
	if (size <= 0)
		return;
	if (bots > 0)
		snprintf(text, (size_t)size, "%d OF %d PEOPLE" DOT "%d BOT%s", people, maximum, bots, bots == 1 ? "" : "S");
	else
		snprintf(text, (size_t)size, "%d OF %d PEOPLE", people, maximum);
}

void ae_sl_team_count(int people, int bots, char *text, int size)
{
	if (size <= 0)
		return;
	if (bots > 0)
		snprintf(text, (size_t)size, "%d + %d BOT%s", people, bots, bots == 1 ? "" : "S");
	else
		snprintf(text, (size_t)size, "%d", people);
}

void ae_sl_bots_line(int bots, char *text, int size)
{
	if (size <= 0)
		return;
	if (bots <= 0)
		text[0] = 0;
	else
		snprintf(text, (size_t)size, "+ %d BOT%s, LEAVING AS PEOPLE JOIN", bots, bots == 1 ? "" : "S");
}

void ae_sl_waiting_line(int needed, int people, int bots, char *text, int size)
{
	int in_now = people + (bots > 0 ? bots : 0);

	if (size <= 0)
		return;
	if (needed <= 1 && people <= 0)
		snprintf(text, (size_t)size, "Starts once someone is in.");
	else if (in_now >= needed && people > 0)
		snprintf(text, (size_t)size, "Everyone it waits for is in. It starts in a moment.");
	else if (bots > 0)
		snprintf(text, (size_t)size, "Starts when %d players are in, bots counted. %d %s in now.", needed, in_now,
			in_now == 1 ? "is" : "are");
	else if (people == 1)
		snprintf(text, (size_t)size, "Starts when %d people are in. 1 is in now.", needed);
	else
		snprintf(text, (size_t)size, "Starts when %d people are in. %d are in now.", needed, people);
}

void ae_sl_votes_word(int votes, char *text, int size)
{
	if (size > 0)
		snprintf(text, (size_t)size, "%d VOTE%s", votes, votes == 1 ? "" : "S");
}

void ae_sl_won_line(int votes, int total, char *text, int size)
{
	if (size <= 0)
		return;
	if (total > 0)
		snprintf(text, (size_t)size, "%d of %d vote%s. The game starts in a moment.", votes, total, total == 1 ? "" : "s");
	else
		snprintf(text, (size_t)size, "The game starts in a moment.");
}

int ae_sl_pips(int people, int most_votes)
{
	int pips = people > most_votes ? people : most_votes;

	if (pips < 1)
		pips = 1;
	return pips > 8 ? 8 : pips;
}

void ae_sl_help_text(const struct ae_sl_help *help, char *text, int size)
{
	const char *map = help->map && help->map[0] ? help->map : "The map";

	if (size <= 0)
		return;
	text[0] = 0;
	if (help->focus == AE_SL_FOCUS_PLAYER)
	{
		if (help->player_bot)
			snprintf(text, (size_t)size, "%s is a bot: it leaves the game as people join.", help->player && help->player[0] ?
				help->player : "This");
		else
			snprintf(text, (size_t)size, "{X} or {A}: %s's card.", help->player && help->player[0] ? help->player : "the player");
		return;
	}
	if (help->focus == AE_SL_FOCUS_SERVER)
	{
		snprintf(text, (size_t)size, "The server's page: the next game, a line to everyone, a restart.");
		return;
	}
	switch (help->state)
	{
	case AE_SL_STATE_DOWNLOAD:
		snprintf(text, (size_t)size, help->pack_map ? "%s is a pack map: the server sends it. {B} leaves the server and "
			"stops the download." : "The server sends %s. {B} leaves the server and stops the download.", map);
		return;
	case AE_SL_STATE_VOTE:
		if (help->focus == AE_SL_FOCUS_LIST)
		{
			snprintf(text, (size_t)size, help->split ? "One vote for this PC. Each controller changes its own team with "
				"{Y}." : "{A} votes for a game. You can change your vote until the time is up.");
			return;
		}
		break;
	case AE_SL_STATE_RESULT:
		snprintf(text, (size_t)size, "The vote is over. %s is next.", map);
		return;
	case AE_SL_STATE_COUNTDOWN:
		if (help->team_rule == AE_SL_TEAMS_LOCKED || !help->team_game)
			snprintf(text, (size_t)size, "The game starts when the count ends.");
		else
			snprintf(text, (size_t)size, "The game starts when the count ends. Team changes go on until then.");
		return;
	}
	if (!help->team_game)
		snprintf(text, (size_t)size, "A free for all: no teams. The server starts the game itself.");
	else if (help->team_rule == AE_SL_TEAMS_LOCKED)
		snprintf(text, (size_t)size, "This server puts players on teams itself.");
	else if (help->split)
		snprintf(text, (size_t)size, "Each controller changes its own team with {Y}.");
	else if (help->team_rule == AE_SL_TEAMS_BALANCED)
		snprintf(text, (size_t)size, "Left or right: the other team. The server allows it while the teams stay even.");
	else
		snprintf(text, (size_t)size, "Left or right: the other team. The server allows team changes before a game.");
}

/* ---------- the title */

int ae_sl_title_runs(const char *title, struct ae_sl_run *runs, int maximum)
{
	int count = 0, index;

	for (index = 0; title && title[index]; index++)
	{
		int numerals = (title[index] >= '0' && title[index] <= '9') || title[index] == '#';

		if (count > 0 && runs[count - 1].numerals == numerals)
		{
			runs[count - 1].length++;
			continue;
		}
		if (count >= maximum)
			break;
		runs[count].start = index;
		runs[count].length = 1;
		runs[count].numerals = numerals;
		count++;
	}
	return count;
}
