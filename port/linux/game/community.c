/*
COMMUNITY.C

The daily challenges and the online leaderboards (community.h), from the
community server (the Content Evolved community server, https://oceapi.watanoge.com, or
community.custom_server's: api-opence) or, offline (community.enabled false),
from this machine.

- The online leaderboards: each campaign level's, every install's best (the
  most points), the level won with scoring on by one player of this machine
  alone sent as it is won (community.upload_scores); and each day's
  challenge's. The menus' LEADERBOARD shows them (menu_functions.c).
- The daily challenge: the server makes one each day (its own day, which
  ends at its midnight), a campaign level at a difficulty with the
  randomizer's setup, the skulls on and a seed, the same for every player.
  Its settings are set for the run alone (config_override: config.toml is
  left as it is) and taken away when the main menu is back; the randomizer
  rolls the seed in place of its own (randomizer.c), so that everyone meets
  the same enemies. Its runs go to the day's leaderboard whenever they are
  played (a past day's too); the server keeps the day's own top three, while
  it lasted, on top. A level won as a challenge's comes back to its day's
  leaderboard, over its screen (menu_functions.c).
- Offline, the day is this machine's (its midnight ends it) and its challenge
  is made here (local_challenge: the server's choices, utils/halo_daily.js,
  rolled from the day, so every offline game has the same one that day); its
  leaderboard is this machine's profiles', each profile's best of each day
  in its daily_records.txt, beside its blam.sav.

Arena Evolved's changes of this file: the main menu's news (GitHub's releases) is not in it, and a challenge's
settings are given back as soon as a game that is not a campaign level's runs, so that no multiplayer game plays with
them (community_update); a challenge's run is always scored (game.campaign_score, this run only), and a day's board is read
whatever SCORING is set to. The online part asks nothing until community.enabled and the community services' consent
(network.community_services "yes") are both on (port/linux/src/community_net.c); both are off by default.

Every request is made on a thread of its own (port/linux/src/
community_net.c) and its answer picked up each frame (community_update):
nothing here waits on the network. The answers are JSON ({"data": ...} or
{"error": {"type": ...}}), read by the small reader below.
*/

#include "cseries.h"
#include "interface/player_ui.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "saved games/player_profile.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "text/unicode.h"

#include "community.h"
#include "ae_daily_rules.h"
#include "halo_skulls.h"
#include "randomizer.h"
#include "service_rank.h"
#include "../src/community_net.h"
#include "../src/port_config.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <xtl.h>

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
boolean player_profile_get_enclosing_directory_path(long profile, char *full_path);

/* the newest challenge this build plays (the server's CHALLENGE_VERSION) */
#define COMMUNITY_CHALLENGE_VERSION 1
#define COMMUNITY_STATUS_LENGTH 160
#define COMMUNITY_RESULT_LENGTH 64
#define COMMUNITY_BODY_LENGTH 2048
/* how long the main menu may stay after a challenge was begun with its
level not loaded, before its settings are given back */
#define COMMUNITY_LOAD_TIMEOUT_MILLISECONDS 120000UL
/* the settings a challenge may set, at most */
#define COMMUNITY_MAXIMUM_SETTINGS 48
/* each profile's offline daily records (local_records_*), and how many of
the latest days they keep */
#define COMMUNITY_RECORDS_FILE "daily_records.txt"
#define COMMUNITY_RECORDS_DAYS 60
#define COMMUNITY_RECORDS_LENGTH 4096
/* the profiles this machine has, at most, and an index whose profile reads
(menu_functions.c's: saved_game_files.c's) */
#define COMMUNITY_MAXIMUM_PROFILES 32
#define COMMUNITY_PROFILE_VALID_BIT 0x80000000UL

/* ---------- JSON: a value is read where it starts, in place */

static char const *json_space(char const *text)
{
	while (text && (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n'))
		text++;
	return text;
}

/* past the value at text (its end), NULL for none */
static char const *json_skip(char const *text)
{
	long depth = 0;
	boolean in_string = FALSE;

	text = json_space(text);
	if (!text || !*text)
		return NULL;
	if (*text != '{' && *text != '[' && *text != '"')
	{
		/* (a number, true, false or null) */
		while (*text && *text != ',' && *text != '}' && *text != ']' && *text != ' ' && *text != '\r' &&
			*text != '\n' && *text != '\t')
		{
			text++;
		}
		return text;
	}
	for (; *text; text++)
	{
		if (in_string)
		{
			if (*text == '\\' && text[1])
				text++;
			else if (*text == '"')
			{
				in_string = FALSE;
				if (!depth)
					return text + 1;
			}
			continue;
		}
		if (*text == '"')
			in_string = TRUE;
		else if (*text == '{' || *text == '[')
			depth++;
		else if (*text == '}' || *text == ']')
		{
			if (--depth == 0)
				return text + 1;
		}
	}
	return NULL;
}

/* the value of the object's key, NULL for none */
static char const *json_member(char const *object, char const *key)
{
	size_t key_length = strlen(key);
	char const *text = json_space(object);

	if (!text || *text != '{')
		return NULL;
	text++;
	for (;;)
	{
		char const *name;
		char const *value;

		text = json_space(text);
		if (!text || *text != '"')
			return NULL;
		name = text + 1;
		text = json_skip(text);
		if (!text)
			return NULL;
		value = json_space(text);
		if (!value || *value != ':')
			return NULL;
		value = json_space(value + 1);
		if ((size_t)(text - 1 - name) == key_length && !strncmp(name, key, key_length))
			return value;
		text = json_space(json_skip(value));
		if (!text || *text != ',')
			return NULL;
		text++;
	}
}

/* the value at a path of keys ("data.mine.score"), NULL for none */
static char const *json_path(char const *text, char const *path)
{
	char key[32];

	while (text && *path)
	{
		size_t length = strcspn(path, ".");

		if (length >= sizeof(key))
			return NULL;
		memcpy(key, path, length);
		key[length] = 0;
		text = json_member(text, key);
		path += length;
		if (*path == '.')
			path++;
	}
	return text;
}

/* the array's element after `element` (NULL: its first), NULL past its last */
static char const *json_next(char const *array, char const *element)
{
	char const *text;

	if (!element)
	{
		text = json_space(array);
		if (!text || *text != '[')
			return NULL;
		text = json_space(text + 1);
		return text && *text != ']' ? text : NULL;
	}
	text = json_space(json_skip(element));
	if (!text || *text != ',')
		return NULL;
	return json_space(text + 1);
}

static long json_long(char const *value, long otherwise)
{
	value = json_space(value);
	if (!value || (*value != '-' && (*value < '0' || *value > '9')))
		return otherwise;
	return strtol(value, NULL, 10);
}

static unsigned long json_unsigned(char const *value)
{
	value = json_space(value);
	return value && *value >= '0' && *value <= '9' ? strtoul(value, NULL, 10) : 0;
}

static boolean json_boolean(char const *value)
{
	value = json_space(value);
	return value && !strncmp(value, "true", 4);
}

/* the string's text, as UTF-8 (\u escapes of the basic plane), "" for none */
static void json_text(char const *value, char *text, size_t size)
{
	size_t length = 0;

	value = json_space(value);
	if (size)
		text[0] = 0;
	if (!value || *value != '"' || !size)
		return;
	for (value++; *value && *value != '"'; value++)
	{
		unsigned long character = (unsigned char)*value;
		size_t room = size - 1 - length;

		if (*value == '\\' && value[1])
		{
			value++;
			switch (*value)
			{
			case 'n': character = '\n'; break;
			case 't': character = ' '; break;
			case 'r': character = '\r'; break;
			case 'u':
				character = 0;
				if (isxdigit((unsigned char)value[1]) && isxdigit((unsigned char)value[2]) &&
					isxdigit((unsigned char)value[3]) && isxdigit((unsigned char)value[4]))
				{
					char digits[5];

					memcpy(digits, value + 1, 4);
					digits[4] = 0;
					character = strtoul(digits, NULL, 16);
					value += 4;
				}
				break;
			default: character = (unsigned char)*value; break;
			}
			if (character >= 0x80)
			{
				/* (as UTF-8, the rest of this file's text) */
				if (room < (character < 0x800 ? 2u : 3u))
					break;
				if (character < 0x800)
				{
					text[length++] = (char)(0xC0 | (character >> 6));
				}
				else
				{
					text[length++] = (char)(0xE0 | (character >> 12));
					text[length++] = (char)(0x80 | ((character >> 6) & 0x3F));
				}
				text[length++] = (char)(0x80 | (character & 0x3F));
				continue;
			}
		}
		if (!room)
			break;
		if (character)
			text[length++] = (char)character;
	}
	text[length] = 0;
}

/* UTF-8 to the game's text (the basic plane; what it has not, '?') */
static void wide_from_utf8(char const *text, wchar_t *wide, long count)
{
	long length = 0;
	unsigned char const *cursor = (unsigned char const *)text;

	while (*cursor && length < count - 1)
	{
		unsigned long character = *cursor++;

		if (character >= 0xF0)
		{
			character = '?';
			while ((*cursor & 0xC0) == 0x80)
				cursor++;
		}
		else if (character >= 0xE0 && (cursor[0] & 0xC0) == 0x80 && (cursor[1] & 0xC0) == 0x80)
		{
			character = ((character & 0x0F) << 12) | ((cursor[0] & 0x3F) << 6) | (cursor[1] & 0x3F);
			cursor += 2;
		}
		else if (character >= 0xC0 && (cursor[0] & 0xC0) == 0x80)
		{
			character = ((character & 0x1F) << 6) | (cursor[0] & 0x3F);
			cursor++;
		}
		else if (character >= 0x80)
			character = '?';
		wide[length++] = (wchar_t)character;
	}
	wide[length] = 0;
}

static void json_wide(char const *value, wchar_t *wide, long count)
{
	char text[COMMUNITY_NAME_LENGTH * 3 + 1];

	json_text(value, text, sizeof(text));
	wide_from_utf8(text, wide, count);
}

/* the game's text as a JSON string's, quoted, UTF-8 */
static void json_quote_wide(wchar_t const *wide, char *text, size_t size)
{
	size_t length = 0;

	if (size < 3)
		return;
	text[length++] = '"';
	for (; wide && *wide && length + 8 < size; wide++)
	{
		unsigned long character = (unsigned short)*wide;

		if (character == '"' || character == '\\')
		{
			text[length++] = '\\';
			text[length++] = (char)character;
		}
		else if (character < 0x20)
			continue;
		else if (character < 0x80)
			text[length++] = (char)character;
		else if (character < 0x800)
		{
			text[length++] = (char)(0xC0 | (character >> 6));
			text[length++] = (char)(0x80 | (character & 0x3F));
		}
		else
		{
			text[length++] = (char)(0xE0 | (character >> 12));
			text[length++] = (char)(0x80 | ((character >> 6) & 0x3F));
			text[length++] = (char)(0x80 | (character & 0x3F));
		}
	}
	text[length++] = '"';
	text[length] = 0;
}

/* ---------- times: the server's ISO 8601 instants (2026-10-22T06:00:00.000Z) */

/* days since 1970-01-01 of a civil date (Howard Hinnant's) */
static long days_from_civil(long year, long month, long day)
{
	long era, year_of_era, day_of_year, day_of_era;

	/* (a date from a server or a host: kept to 2000-2100, so that none of this overflows) */
	year = PIN(year, 2000, 2100);
	month = PIN(month, 1, 12);
	day = PIN(day, 1, 31);
	year -= month <= 2;
	era = (year >= 0 ? year : year - 399) / 400;
	year_of_era = year - era * 400;
	day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
	return era * 146097 + day_of_era - 719468;
}

/* the civil date (YYYY-MM-DD) of a count of days since 1970-01-01 */
static void day_from_days(long days, char *day, size_t size)
{
	long era, day_of_era, year_of_era, day_of_year, month_index, year, month, date;

	/* (the days of 2000 to 2100) */
	days = PIN(days, 10957, 47846);
	days += 719468;
	era = (days >= 0 ? days : days - 146096) / 146097;
	day_of_era = days - era * 146097;
	year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
	day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
	month_index = (5 * day_of_year + 2) / 153;
	date = day_of_year - (153 * month_index + 2) / 5 + 1;
	month = month_index < 10 ? month_index + 3 : month_index - 9;
	year = year_of_era + era * 400 + (month <= 2);
	snprintf(day, size, "%04ld-%02ld-%02ld", year, month, date);
}

/* the instant's seconds since 1970, 0 for none */
static long long iso_seconds(char const *value)
{
	char text[32];
	long year, month, day, hour, minute, second;

	json_text(value, text, sizeof(text));
	if (sscanf(text, "%ld-%ld-%ldT%ld:%ld:%ld", &year, &month, &day, &hour, &minute, &second) != 6)
		return 0;
	if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60)
		return 0;
	return (long long)days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
}

/* ---------- the campaign's levels, by their key (levels\a10\a10: a10) */

static void level_key(char const *level_name, char *key, size_t size)
{
	char const *last = level_name ? level_name : "";
	char const *cursor;
	size_t length = 0;

	for (cursor = last; *cursor; cursor++)
	{
		if (*cursor == '\\' || *cursor == '/')
			last = cursor + 1;
	}
	for (; *last && length + 1 < size; last++)
	{
		char c = *last;

		if (c >= 'A' && c <= 'Z')
			c = (char)(c + 32);
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
			key[length++] = c;
	}
	key[length] = 0;
}

static short level_index_of(char const *key)
{
	short level;

	for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
	{
		char other[COMMUNITY_LEVEL_KEY_LENGTH];

		level_key(main_get_solo_level_name(level), other, sizeof(other));
		if (!strcmp(other, key))
			return level;
	}
	return NONE;
}

/* ---------- requests the screens wait on */

struct community_request
{
	int ticket;
	short state;
	/* (no community server answered: none there, or not one) */
	boolean unreachable;
	/* (what it was asked for: a later answer to another is dropped) */
	char asked[64];
};

static struct
{
	struct community_request daily;
	struct community_challenge challenge;
	/* the challenge's settings, as the server sent them */
	short setting_count;
	char setting_names[COMMUNITY_MAXIMUM_SETTINGS][48];
	char setting_values[COMMUNITY_MAXIMUM_SETTINGS][160];
	boolean today;

	struct community_request history;
	short history_count;
	struct community_history_day history_days[COMMUNITY_HISTORY_DAYS];

	struct community_request board;
	short board_count;
	struct community_row board_rows[COMMUNITY_BOARD_ROWS];
	struct community_row board_mine;
	long board_players;
	/* (a day's: its top three while it lasted, kept on top) */
	short board_day_top_count;
	struct community_row board_day_top[COMMUNITY_DAY_TOP];

	/* the challenge being played: its settings set, until the main menu
	is back after its level was loaded */
	boolean running;
	boolean running_loaded;
	/* (when it began: a level that never loaded gives the settings back) */
	unsigned long running_since;
	struct community_challenge running_challenge;
	/* (its level won here, for the menus to come back to: a random run's or
	a challenge's) */
	boolean won;
	boolean won_random;
	/* (the day of the challenge last won here) */
	char won_day[COMMUNITY_DAY_LENGTH];

	/* (a network co-op client) its host's challenge, as the level is won:
	its day (days since 1970, 0 for none) and flags */
	long network_day;
	long network_flags;

	/* the scores sent (each for a summary card's slot), and what each came to */
	int submit_tickets[COMMUNITY_RESULT_SLOTS];
	short submit_slots[COMMUNITY_RESULT_SLOTS];
	/* (a challenge's: its day, whose leaderboard is asked for again if it
	was answered before the score was taken; "" for a level's) */
	char submit_days[COMMUNITY_RESULT_SLOTS][COMMUNITY_DAY_LENGTH];
	wchar_t result[COMMUNITY_RESULT_SLOTS][COMMUNITY_RESULT_LENGTH];

	wchar_t status[COMMUNITY_STATUS_LENGTH];
	char answer[COMMUNITY_NET_ANSWER_MAXIMUM];
} community;

boolean community_enabled(void)
{
	return community_net_enabled() != 0;
}

boolean community_scoring(void)
{
	return config_boolean("game.campaign_score") != 0;
}

/* a profile's player on the server: the install's key, and this, a hash of
the profile's name (split screen's players each their own) */
#define COMMUNITY_PLAYER_LENGTH 9

static void player_id(wchar_t const *name, char *player, size_t size)
{
	unsigned long hash = 2166136261UL;

	for (; name && *name; name++)
	{
		hash = (hash ^ (unsigned long)(unsigned short)*name) * 16777619UL;
		hash &= 0xFFFFFFFFUL;
	}
	snprintf(player, size, "%08lx", hash);
}

static long local_profile(void);

/* player 1's profile's player (whose best and place the screens show) */
static void local_player_id(char *player, size_t size)
{
	struct player_profile profile;
	long profile_index = local_profile();

	if (profile_index == NONE || !player_profile_get(profile_index, &profile))
	{
		player[0] = 0;
		return;
	}
	player_id(profile.player_name, player, size);
}

static void request_start(struct community_request *request, char const *asked, char const *path, char const *body)
{
	if (request->ticket)
		community_net_forget(request->ticket);
	request->ticket = 0;
	request->unreachable = FALSE;
	snprintf(request->asked, sizeof(request->asked), "%s", asked);
	if (!community_enabled())
	{
		request->state = _community_off;
		return;
	}
	{
		char player[COMMUNITY_PLAYER_LENGTH];

		local_player_id(player, sizeof(player));
		request->ticket = community_net_send(path, body, player);
	}
	request->state = request->ticket ? _community_loading : _community_unavailable;
	request->unreachable = !request->ticket;
}

/* the server's error's type ("closed"), "" for none */
static void error_type(char const *answer, char *type, size_t size)
{
	json_text(json_path(answer, "error.type"), type, size);
}

static void status_set(wchar_t const *text)
{
	ustrncpy(community.status, text, COMMUNITY_STATUS_LENGTH - 1);
	community.status[COMMUNITY_STATUS_LENGTH - 1] = 0;
}

/* the request's answer, if it has come: its body's data (NULL for none) in
*data; FALSE while it is still waited on */
static boolean request_poll(struct community_request *request, char const **data)
{
	int status = 0;
	int result;

	*data = NULL;
	if (request->state != _community_loading)
		return FALSE;
	result = community_net_poll(request->ticket, &status, community.answer, sizeof(community.answer));
	if (result == _community_net_waiting)
		return FALSE;
	request->ticket = 0;
	if (result != _community_net_answered)
	{
		request->state = _community_unavailable;
		request->unreachable = TRUE;
		status_set(L"The community server can't be reached right now.\rCheck your connection, or try again later.");
		return TRUE;
	}
	*data = json_member(community.answer, "data");
	if (status != 200 || !*data)
	{
		char type[32];

		request->state = _community_refused;
		error_type(community.answer, type, sizeof(type));
		/* (an answer that is not a community server's, or one in trouble,
		is no answer: what the window of a server not reached is for) */
		request->unreachable = !type[0] || status >= 500;
		if (!strcmp(type, "not_found"))
			status_set(L"There is no challenge for that day.");
		else if (!type[0])
			status_set(L"That address is not a community server.");
		else if (!strcmp(type, "database"))
			status_set(L"The community server is up, but its database is not.");
		else if (status >= 500)
			status_set(L"The community server is having trouble.\rTry again later.");
		else
			status_set(L"The community server refused the request.");
		platform_log("community: answered %d (%s)", status, type[0] ? type : "no error type");
		*data = NULL;
		return TRUE;
	}
	request->state = _community_ready;
	return TRUE;
}

/* ---------- offline: the day, its challenge and its records this machine's */

/* this machine's day (YYYY-MM-DD), days from today (-1: yesterday), and
the seconds left in today */
static void local_day(long offset, char *day, size_t size, long *seconds_left)
{
	time_t now = time(NULL);
	struct tm local = *localtime(&now);

	if (seconds_left)
		*seconds_left = 86400 - (local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec);
	day_from_days(days_from_civil(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday) + offset, day, size);
}

/* the roll of the day's offline challenge: the same day, the same rolls */
static unsigned long local_roll_state;

static void local_roll_seed(char const *day)
{
	unsigned long hash = 2166136261UL;
	char const *text = "opence daily ";

	for (; *text; text++)
		hash = (hash ^ (unsigned char)*text) * 16777619UL;
	for (; *day; day++)
		hash = (hash ^ (unsigned char)*day) * 16777619UL;
	local_roll_state = hash ? hash : 1;
}

/* 0 to limit - 1 (xorshift32) */
static long local_roll(long limit)
{
	local_roll_state ^= (local_roll_state << 13) & 0xFFFFFFFFUL;
	local_roll_state ^= local_roll_state >> 17;
	local_roll_state ^= (local_roll_state << 5) & 0xFFFFFFFFUL;
	local_roll_state &= 0xFFFFFFFFUL;
	return limit > 0 ? (long)(local_roll_state % (unsigned long)limit) : 0;
}

struct local_choice
{
	char const *name;
	short weight;
	char const *label;
};

static short local_pick(struct local_choice const *choices, short count)
{
	long total = 0;
	long roll;
	short index;

	for (index = 0; index < count; index++)
		total += choices[index].weight;
	roll = local_roll(total);
	for (index = 0; index < count; index++)
	{
		roll -= choices[index].weight;
		if (roll < 0)
			return index;
	}
	return (short)(count - 1);
}

/* the server's tables (utils/halo_daily.js) */
static struct local_choice const local_skulls[] = {
	{ "anger", 3 }, { "armistice", 1 }, { "assassins", 2 }, { "bandanna", 1 }, { "black_eye", 2 }, { "blind", 1 },
	{ "boom", 2 }, { "catch", 3 }, { "confetti_party", 1 }, { "efficient", 1 }, { "envy", 1 }, { "eye_patch", 2 },
	{ "famine", 3 }, { "floor_is_lava", 1 }, { "fog", 3 }, { "foreign", 2 }, { "ghost", 2 }, { "give_and_take", 1 },
	{ "grunt_birthday_party", 1 }, { "grunt_funeral", 2 }, { "johnny_ammo", 2 }, { "leadhead", 2 },
	{ "m_rating", 1 }, { "malfunction", 2 }, { "mythic", 3 }, { "need_glasses", 1 }, { "outbreak", 2 },
	{ "pinata", 1 }, { "protected", 2 }, { "recession", 2 }, { "rip_and_tear", 1 }, { "scarab", 1 },
	{ "sputnik", 2 }, { "streaking", 1 }, { "swarm", 2 }, { "thats_just_wrong", 2 }, { "thunderstorm", 3 },
	{ "tough_luck", 3 }, { "woe_is_me", 1 },
};
static struct local_choice const local_skull_counts[] = {
	{ "0", 1 }, { "1", 3 }, { "2", 4 }, { "3", 5 }, { "4", 4 }, { "5", 3 }, { "6", 2 }, { "7", 1 }, { "8", 1 } };
static struct local_choice const local_difficulties[] = { { "1", 3 }, { "2", 5 }, { "3", 2 } };
static struct local_choice const local_pools[] = {
	{ "any", 6, "ALL KINDS" }, { "covenant", 4, "COVENANT ONLY" }, { "flood", 4, "FLOOD ONLY" },
	{ "human", 1, "MARINES ONLY" }, { "one", 4, NULL } };
static struct local_choice const local_types[] = {
	{ "grunt", 3, "GRUNTS" }, { "elite", 3, "ELITES" }, { "jackal", 2, "JACKALS" }, { "hunter", 1, "HUNTERS" },
	{ "flood_combat", 2, "COMBAT FORMS" }, { "sentinel", 1, "SENTINELS" }, { "marine", 1, "MARINES" } };
static char const *const local_type_ranks[][7] = {
	{ "any", "minor", "major", "specops", NULL }, { "any", "minor", "major", "specops", "zealot", "stealth", NULL },
	{ "any", "minor", "major", NULL }, { "any", NULL }, { "any", "human", "elite", NULL }, { NULL }, { NULL } };
static char const *const local_rank_settings[] = {
	"game.randomizer_grunt_rank", "game.randomizer_elite_rank", "game.randomizer_jackal_rank",
	"game.randomizer_hunter_rank", "game.randomizer_flood_combat_rank" };
/* (power and the last two weapons: the actors' only) */
static struct local_choice const local_weapon_pools[] = {
	{ "any", 5, "ANY" }, { "covenant", 2, "COVENANT" }, { "human", 2, "HUMAN" }, { "precision", 1, "PRECISION" },
	{ "automatic", 1, "AUTOMATIC" }, { "one", 1, NULL }, { "power", 1, "POWER" } };
static struct local_choice const local_weapons[] = {
	{ "pistol", 1, "PISTOL" }, { "assault_rifle", 1, "ASSAULT RIFLE" }, { "shotgun", 1, "SHOTGUN" },
	{ "sniper_rifle", 1, "SNIPER RIFLE" }, { "rocket_launcher", 1, "ROCKET LAUNCHER" },
	{ "plasma_pistol", 1, "PLASMA PISTOL" }, { "plasma_rifle", 1, "PLASMA RIFLE" }, { "needler", 1, "NEEDLER" },
	{ "fuel_rod", 1, "FUEL ROD" }, { "energy_sword", 1, "ENERGY SWORD" } };

static void local_setting(char const *name, char const *value)
{
	if (community.setting_count >= COMMUNITY_MAXIMUM_SETTINGS)
		return;
	snprintf(community.setting_names[community.setting_count], sizeof(community.setting_names[0]), "%s", name);
	snprintf(community.setting_values[community.setting_count], sizeof(community.setting_values[0]), "%s", value);
	community.setting_count++;
}

/* a weapon pool, for the actors or the pickups, and its one weapon; its label */
static void local_weapons_pick(boolean actors, char const **pool, char const **kind, char *label, size_t size)
{
	short pool_index = local_pick(local_weapon_pools, (short)(NUMBEROF(local_weapon_pools) - (actors ? 0 : 1)));

	*pool = local_weapon_pools[pool_index].name;
	*kind = NULL;
	if (strcmp(*pool, "one"))
	{
		snprintf(label, size, "%s", local_weapon_pools[pool_index].label);
		return;
	}
	pool_index = (short)local_roll((long)NUMBEROF(local_weapons) - (actors ? 0 : 2));
	*kind = local_weapons[pool_index].name;
	snprintf(label, size, "%s ONLY", local_weapons[pool_index].label);
}

/* the day's level, as the day's rolls choose it first */
static short local_level_roll(long day_number)
{
	char day[COMMUNITY_DAY_LENGTH];

	day_from_days(day_number, day, sizeof(day));
	local_roll_seed(day);
	return (short)local_roll(NUMBER_OF_SINGLE_PLAYER_LEVELS);
}

/* the day's level, never the day before's: each day's roll from the first
offline day on (January 1st of LOCAL_FIRST_DAY_YEAR), the next level where it is the day
before's; the last one worked out kept, as the history asks for the days in
a row */
#define LOCAL_FIRST_DAY_YEAR 2026
static short local_level(long day_number)
{
	static long cached_day = LONG_MIN;
	static short cached_level = NONE;
	long first = days_from_civil(LOCAL_FIRST_DAY_YEAR, 1, 1);
	long day;
	short level;

	if (day_number <= first)
		return local_level_roll(day_number);
	if (cached_day != LONG_MIN && cached_day <= day_number)
	{
		day = cached_day;
		level = cached_level;
	}
	else
	{
		day = first;
		level = local_level_roll(first);
	}
	while (day < day_number)
	{
		short roll = local_level_roll(++day);

		level = roll == level ? (short)((roll + 1) % NUMBER_OF_SINGLE_PLAYER_LEVELS) : roll;
	}
	cached_day = day_number;
	cached_level = level;
	return level;
}

/* the offline challenge of a day: into community.challenge and its settings
(only what the history wants, with settings FALSE) */
static void local_challenge_rolls(boolean settings);

static void local_challenge(char const *day, boolean settings)
{
	struct community_challenge *challenge = &community.challenge;
	long year, month, date;

	memset(challenge, 0, sizeof(*challenge));
	snprintf(challenge->day, sizeof(challenge->day), "%s", day);
	/* (the level, never the day before's; then the rest of the day's rolls) */
	if (sscanf(day, "%ld-%ld-%ld", &year, &month, &date) != 3)
		return;
	challenge->level_index = local_level(days_from_civil(year, month, date));
	local_level_roll(days_from_civil(year, month, date));
	local_challenge_rolls(settings);
}

/* the rest of community.challenge (its day and level set), rolled as the
rolls go on (local_roll): its difficulty, randomizer and skulls; and with
settings, those to set for its run */
static void local_challenge_rolls(boolean settings)
{
	struct community_challenge *challenge = &community.challenge;
	char today[COMMUNITY_DAY_LENGTH];
	char enemies[64], pickups_label[48], weapons_label[48];
	char const *pickup_pool = NULL, *pickup_kind = NULL, *weapon_pool = NULL, *weapon_kind = NULL;
	short pool, type = 0, skull_count, index;
	char const *rank = "any";
	boolean allies, pickups, weapons, crazy, crazy_weapons, crazy_enemies, crazy_flame;
	long pods = 30;
	long seconds_left;

	level_key(main_get_solo_level_name(challenge->level_index), challenge->level, sizeof(challenge->level));
	challenge->difficulty = (short)atoi(local_difficulties[local_pick(local_difficulties,
		NUMBEROF(local_difficulties))].name);
	challenge->version = COMMUNITY_CHALLENGE_VERSION;
	pool = local_pick(local_pools, NUMBEROF(local_pools));
	if (!strcmp(local_pools[pool].name, "one"))
	{
		short ranks = 0;

		type = local_pick(local_types, NUMBEROF(local_types));
		while (local_type_ranks[type][ranks])
			ranks++;
		rank = ranks ? local_type_ranks[type][local_roll(ranks)] : "any";
		snprintf(enemies, sizeof(enemies), "%s%s%s ONLY", strcmp(rank, "any") ? rank : "",
			strcmp(rank, "any") ? " " : "", local_types[type].label);
		for (index = 0; enemies[index]; index++)
		{
			if (enemies[index] >= 'a' && enemies[index] <= 'z')
				enemies[index] = (char)(enemies[index] - 32);
		}
	}
	else
		snprintf(enemies, sizeof(enemies), "%s", local_pools[pool].label);
	allies = local_roll(100) < 25;
	pickups = local_roll(100) < 65;
	if (pickups)
		local_weapons_pick(FALSE, &pickup_pool, &pickup_kind, pickups_label, sizeof(pickups_label));
	weapons = local_roll(100) < 65;
	if (weapons)
		local_weapons_pick(TRUE, &weapon_pool, &weapon_kind, weapons_label, sizeof(weapons_label));
	crazy = local_roll(100) < 15;
	crazy_weapons = crazy && local_roll(100) < 70;
	crazy_enemies = crazy && local_roll(100) < 70;
	crazy_flame = crazy && local_roll(100) < 50;
	if (crazy)
		pods = 10 * (1 + local_roll(10));
	/* (the skulls, without two that change the shields) */
	skull_count = (short)atoi(local_skull_counts[local_pick(local_skull_counts, NUMBEROF(local_skull_counts))].name);
	{
		struct local_choice remaining[NUMBEROF(local_skulls)];
		static char const *const shields[] = { "black_eye", "give_and_take", "streaking", "rip_and_tear" };
		short left = NUMBEROF(local_skulls);

		memcpy(remaining, local_skulls, sizeof(remaining));
		while (challenge->skull_count < skull_count && challenge->skull_count < COMMUNITY_MAXIMUM_SKULLS && left)
		{
			short chosen = local_pick(remaining, left);
			boolean shield = FALSE;
			short shield_index;

			snprintf(challenge->skulls[challenge->skull_count++], sizeof(challenge->skulls[0]), "%s",
				remaining[chosen].name);
			for (shield_index = 0; shield_index < (short)NUMBEROF(shields); shield_index++)
				shield |= !strcmp(remaining[chosen].name, shields[shield_index]);
			remaining[chosen] = remaining[--left];
			for (index = 0; shield && index < left; index++)
			{
				for (shield_index = 0; shield_index < (short)NUMBEROF(shields); shield_index++)
				{
					if (!strcmp(remaining[index].name, shields[shield_index]))
					{
						remaining[index--] = remaining[--left];
						break;
					}
				}
			}
		}
		/* (in order, as the server has them) */
		for (index = 1; index < challenge->skull_count; index++)
		{
			short place;
			char moved[24];

			snprintf(moved, sizeof(moved), "%s", challenge->skulls[index]);
			for (place = index; place > 0 && strcmp(challenge->skulls[place - 1], moved) > 0; place--)
				snprintf(challenge->skulls[place], sizeof(challenge->skulls[0]), "%s", challenge->skulls[place - 1]);
			snprintf(challenge->skulls[place], sizeof(challenge->skulls[0]), "%s", moved);
		}
	}
	challenge->seed = (unsigned long)local_roll(0x7FFFFFFFL) + 1;
	snprintf(challenge->details[0], COMMUNITY_DETAIL_LENGTH, "ENEMIES: %s%s", enemies, allies ? ", ALLIES TOO" : "");
	snprintf(challenge->details[1], COMMUNITY_DETAIL_LENGTH, "ENEMY WEAPONS: %s",
		weapons ? weapons_label : "NOT RANDOMIZED");
	snprintf(challenge->details[2], COMMUNITY_DETAIL_LENGTH, "PICKUPS: %s",
		pickups ? pickups_label : "NOT RANDOMIZED");
	challenge->detail_count = 3;
	if (crazy)
		snprintf(challenge->details[challenge->detail_count++], COMMUNITY_DETAIL_LENGTH, "GO CRAZY: ON");
	local_day(0, today, sizeof(today), &seconds_left);
	challenge->open = !strcmp(challenge->day, today);
	challenge->seconds_left = challenge->open ? seconds_left : 0;
	challenge->answered_at = system_milliseconds();
	if (!settings)
		return;
	community.setting_count = 0;
	local_setting("game.randomizer", "true");
	local_setting("game.randomizer_enemies", "true");
	local_setting("game.randomizer_allies", allies ? "true" : "false");
	local_setting("game.randomizer_pool", local_pools[pool].name);
	local_setting("game.randomizer_type", local_types[type].name);
	local_setting("game.randomizer_pickups", pickups ? "true" : "false");
	local_setting("game.randomizer_pickup_pool", pickups ? pickup_pool : "any");
	local_setting("game.randomizer_pickup_type", pickup_kind ? pickup_kind : "pistol");
	local_setting("game.randomizer_enemy_weapons", weapons ? "true" : "false");
	local_setting("game.randomizer_weapon_pool", weapons ? weapon_pool : "any");
	local_setting("game.randomizer_weapon_type", weapon_kind ? weapon_kind : "plasma_rifle");
	local_setting("game.randomizer_crazy", crazy ? "true" : "false");
	local_setting("game.randomizer_crazy_weapons", crazy_weapons ? "true" : "false");
	local_setting("game.randomizer_crazy_enemies", crazy_enemies ? "true" : "false");
	local_setting("game.randomizer_crazy_flamethrower", crazy_flame ? "true" : "false");
	{
		char text[16];

		snprintf(text, sizeof(text), "%ld", pods);
		local_setting("game.randomizer_crazy_pods", text);
	}
	{
		char text[COMMUNITY_MAXIMUM_SKULLS * 24];
		size_t length = 0;

		text[0] = 0;
		for (index = 0; index < challenge->skull_count; index++)
			length += (size_t)snprintf(text + length, sizeof(text) - length, "%s%s", index ? "," : "", challenge->skulls[index]);
		local_setting("game.skulls", challenge->skull_count ? "true" : "false");
		local_setting("game.skulls_active", text);
	}
	local_setting("game.skulls_unlock_mode", "play");
	local_setting("game.skulls_all_unlocked", "true");
	local_setting("game.skulls_instant_unlock", "false");
	local_setting("game.skulls_lose_on_death", "false");
	local_setting("game.skulls_drop_count", "0");
	for (index = 0; index < (short)NUMBEROF(local_rank_settings); index++)
	{
		local_setting(local_rank_settings[index],
			!strcmp(local_pools[pool].name, "one") && type == index ? rank : "any");
	}
}

/* a profile's best of a day, offline */
struct local_record
{
	long score, seconds, skulls, medals, attempts;
};

static boolean local_records_path(long profile_index, char *path, size_t size)
{
	char directory[256];

	if (profile_index == NONE || !player_profile_get_enclosing_directory_path(profile_index, directory) ||
		!directory[0])
	{
		return FALSE;
	}
	snprintf(path, size, "%s%s", directory, COMMUNITY_RECORDS_FILE);
	return TRUE;
}

/* the profile's records, as text ("day:score:seconds:skulls:medals:attempts;"
each, oldest first; "" for none) */
static void local_records_read(long profile_index, char *text, size_t size)
{
	char path[300];
	HANDLE file;
	unsigned long read = 0;

	text[0] = 0;
	if (!local_records_path(profile_index, path, sizeof(path)))
		return;
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return;
	if (!ReadFile(file, text, (unsigned long)(size - 1), &read, NULL))
		read = 0;
	CloseHandle(file);
	text[read] = 0;
}

static boolean local_record_find(char const *records, char const *day, struct local_record *record)
{
	size_t length = strlen(day);
	char const *entry;

	for (entry = records; entry && *entry; entry = strchr(entry, ';') ? strchr(entry, ';') + 1 : NULL)
	{
		if (!strncmp(entry, day, length) && entry[length] == ':' &&
			sscanf(entry + length + 1, "%ld:%ld:%ld:%ld:%ld", &record->score, &record->seconds, &record->skulls,
				&record->medals, &record->attempts) == 5)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/* the run kept where it beats the profile's best of the day (and the
attempt counted either way): TRUE when it did */
static boolean local_record_add(long profile_index, char const *day, struct community_run const *run)
{
	char records[COMMUNITY_RECORDS_LENGTH];
	char text[COMMUNITY_RECORDS_LENGTH];
	char path[300];
	struct local_record best;
	boolean improved;
	char const *entry;
	size_t length = 0;
	short kept = 0, count = 0;
	HANDLE file;
	unsigned long written = 0;

	local_records_read(profile_index, records, sizeof(records));
	if (!local_record_find(records, day, &best))
	{
		memset(&best, 0, sizeof(best));
		improved = TRUE;
	}
	else
		improved = run->score > best.score || (run->score == best.score && run->seconds < best.seconds);
	if (improved)
	{
		best.score = run->score;
		best.seconds = run->seconds;
		best.skulls = run->skulls;
		best.medals = run->medals;
	}
	best.attempts++;
	/* (the others, the latest COMMUNITY_RECORDS_DAYS - 1 of them) */
	for (entry = records; *entry; entry = strchr(entry, ';') ? strchr(entry, ';') + 1 : entry + strlen(entry))
		count++;
	text[0] = 0;
	for (entry = records; *entry; entry = strchr(entry, ';') ? strchr(entry, ';') + 1 : entry + strlen(entry))
	{
		size_t entry_length = strchr(entry, ';') ? (size_t)(strchr(entry, ';') - entry) : strlen(entry);

		kept++;
		if (!entry_length || (!strncmp(entry, day, strlen(day)) && entry[strlen(day)] == ':') ||
			count - kept >= COMMUNITY_RECORDS_DAYS - 1 || length + entry_length + 2 >= sizeof(text))
		{
			continue;
		}
		memcpy(text + length, entry, entry_length);
		length += entry_length;
		text[length++] = ';';
		text[length] = 0;
	}
	snprintf(text + length, sizeof(text) - length, "%s:%ld:%ld:%ld:%ld:%ld;", day, best.score, best.seconds,
		best.skulls, best.medals, best.attempts);
	if (!local_records_path(profile_index, path, sizeof(path)))
		return improved;
	file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (file == INVALID_HANDLE_VALUE || !WriteFile(file, text, (unsigned long)strlen(text), &written, NULL))
		platform_log("community: could not write the profile's %s", COMMUNITY_RECORDS_FILE);
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
	return improved;
}

/* player 1's profile (the active one, else the one used last) */
static long local_profile(void)
{
	long profile_index = player_ui_get_active_player_profile_index(0);

	return profile_index != NONE ? profile_index : player_ui_get_player1_last_used_profile_index();
}

/* the day's offline leaderboard, every profile's of this machine with a
record of it, into the board; and how many */
static void local_board(char const *day)
{
	long profiles[COMMUNITY_MAXIMUM_PROFILES];
	word count = COMMUNITY_MAXIMUM_PROFILES;
	long mine = local_profile();
	short index;

	community.board_count = 0;
	community.board_day_top_count = 0;
	community.board_players = 0;
	memset(&community.board_mine, 0, sizeof(community.board_mine));
	player_profiles_enumerate_available_to_local_player_index(NONE, &count, profiles, FALSE);
	for (index = 0; index < (short)count; index++)
	{
		struct player_profile profile;
		struct local_record record;
		struct community_row row;
		char records[COMMUNITY_RECORDS_LENGTH];
		short place;

		if (!((unsigned long)profiles[index] & COMMUNITY_PROFILE_VALID_BIT) || !player_profile_get(profiles[index], &profile))
			continue;
		local_records_read(profiles[index], records, sizeof(records));
		if (!local_record_find(records, day, &record))
			continue;
		memset(&row, 0, sizeof(row));
		ustrncpy(row.name, profile.player_name, COMMUNITY_NAME_LENGTH - 1);
		row.score = record.score;
		row.seconds = record.seconds;
		row.difficulty = NONE;
		row.skulls = record.skulls;
		row.medals = record.medals;
		community.board_players++;
		/* (in its place: the most points, then the shortest time) */
		for (place = community.board_count; place > 0 &&
			(community.board_rows[place - 1].score < row.score ||
				(community.board_rows[place - 1].score == row.score && community.board_rows[place - 1].seconds > row.seconds));
			place--)
		{
			if (place < COMMUNITY_BOARD_ROWS)
				community.board_rows[place] = community.board_rows[place - 1];
		}
		if (place < COMMUNITY_BOARD_ROWS)
		{
			community.board_rows[place] = row;
			if (community.board_count < COMMUNITY_BOARD_ROWS)
				community.board_count++;
		}
		if (profiles[index] == mine)
			community.board_mine = row;
	}
	for (index = 0; index < community.board_count; index++)
	{
		community.board_rows[index].rank = index + 1;
		if (community.board_mine.name[0] && !ustrncmp(community.board_rows[index].name, community.board_mine.name,
			COMMUNITY_NAME_LENGTH) && community.board_rows[index].score == community.board_mine.score)
		{
			community.board_mine.rank = index + 1;
		}
	}
}

/* the profile's best of the challenge shown, offline */
static void local_mine(void)
{
	char records[COMMUNITY_RECORDS_LENGTH];
	struct local_record record;

	local_records_read(local_profile(), records, sizeof(records));
	if (!local_record_find(records, community.challenge.day, &record))
		return;
	community.challenge.has_mine = TRUE;
	community.challenge.mine_score = record.score;
	community.challenge.mine_seconds = record.seconds;
	community.challenge.mine_attempts = record.attempts;
}

/* ---------- the daily challenge */

static short skull_of_key(char const *key);

static void challenge_read(char const *data)
{
	struct community_challenge *challenge = &community.challenge;
	char const *element;
	char const *config;
	char text[COMMUNITY_DETAIL_LENGTH];
	long long server_time, closes_at;

	memset(challenge, 0, sizeof(*challenge));
	challenge->online = TRUE;
	json_text(json_member(data, "day"), challenge->day, sizeof(challenge->day));
	json_text(json_member(data, "level"), challenge->level, sizeof(challenge->level));
	challenge->level_index = level_index_of(challenge->level);
	/* (the day goes into the paths of its requests: only a day as written is kept, else the challenge has no level to play) */
	if (!ae_daily_day_valid(challenge->day))
	{
		challenge->day[0] = 0;
		challenge->level_index = NONE;
	}
	challenge->difficulty = (short)PIN(json_long(json_member(data, "difficulty"), 1), 0, 3);
	challenge->version = json_long(json_member(data, "version"), 1);
	challenge->seed = json_unsigned(json_member(data, "seed"));
	for (element = json_next(json_member(data, "skulls"), NULL);
		element && challenge->skull_count < COMMUNITY_MAXIMUM_SKULLS; element = json_next(NULL, element))
	{
		json_text(element, challenge->skulls[challenge->skull_count], sizeof(challenge->skulls[0]));
		/* (only a skull this game has: its key goes back to the server in the scores' body) */
		if (skull_of_key(challenge->skulls[challenge->skull_count]) != NONE)
			challenge->skull_count++;
	}
	for (element = json_next(json_member(data, "details"), NULL);
		element && challenge->detail_count < COMMUNITY_MAXIMUM_DETAILS; element = json_next(NULL, element))
	{
		json_text(element, text, sizeof(text));
		snprintf(challenge->details[challenge->detail_count++], COMMUNITY_DETAIL_LENGTH, "%s", text);
	}
	challenge->open = json_boolean(json_member(data, "open"));
	server_time = iso_seconds(json_member(data, "serverTime"));
	closes_at = iso_seconds(json_member(data, "closesAt"));
	challenge->seconds_left = challenge->open && server_time && closes_at > server_time ?
		(long)(closes_at - server_time) : 0;
	challenge->answered_at = system_milliseconds();
	if (json_member(data, "mine"))
	{
		challenge->has_mine = TRUE;
		challenge->mine_score = json_long(json_path(data, "mine.score"), 0);
		challenge->mine_seconds = json_long(json_path(data, "mine.seconds"), 0);
		challenge->mine_attempts = json_long(json_path(data, "mine.attempts"), 0);
	}
	/* (its settings, the exact keys of the offline challenge's only: never the skull progress, set here as it sets them; scoring the
	player's own choice) */
	community.setting_count = 0;
	config = json_space(json_member(data, "config"));
	if (config && *config == '{')
	{
		char const *cursor = config + 1;

		while (community.setting_count < COMMUNITY_MAXIMUM_SETTINGS - 3)
		{
			char name[48];
			char const *value;

			cursor = json_space(cursor);
			if (!cursor || *cursor != '"')
				break;
			json_text(cursor, name, sizeof(name));
			cursor = json_space(json_skip(cursor));
			if (!cursor || *cursor != ':')
				break;
			value = json_space(cursor + 1);
			if (ae_daily_challenge_key_allowed(name))
			{
				snprintf(community.setting_names[community.setting_count], sizeof(community.setting_names[0]), "%s",
					name);
				json_text(value, community.setting_values[community.setting_count],
					sizeof(community.setting_values[0]));
				community.setting_count++;
			}
			cursor = json_space(json_skip(value));
			if (!cursor || *cursor != ',')
				break;
			cursor++;
		}
	}
	local_setting("game.skulls_unlock_mode", "play");
	local_setting("game.skulls_all_unlocked", "true");
	local_setting("game.skulls_instant_unlock", "false");
}

void community_daily_open(char const *day)
{
	char path[64];

	community.status[0] = 0;
	community.today = day == NULL;
	/* (offline: made here, at once) */
	if (!community_enabled())
	{
		char today[COMMUNITY_DAY_LENGTH];

		if (community.daily.ticket)
			community_net_forget(community.daily.ticket);
		community.daily.ticket = 0;
		local_day(0, today, sizeof(today), NULL);
		local_challenge(day ? day : today, TRUE);
		local_mine();
		community.daily.state = _community_ready;
		return;
	}
	if (day)
		snprintf(path, sizeof(path), "/daily/%s", day);
	else
		snprintf(path, sizeof(path), "/daily");
	request_start(&community.daily, day ? day : "today", path, NULL);
	if (community.daily.state == _community_unavailable)
		status_set(L"The community server can't be reached right now.");
}

short community_daily_state(void)
{
	return community.daily.state;
}

boolean community_daily_unreachable(void)
{
	return (community.daily.state == _community_unavailable || community.daily.state == _community_refused) &&
		community.daily.unreachable;
}

struct community_challenge const *community_daily(void)
{
	return community.daily.state == _community_ready ? &community.challenge : NULL;
}

boolean community_daily_is_today(void)
{
	return community.today;
}

wchar_t const *community_status_text(void)
{
	return community.status;
}

long community_daily_seconds_left(void)
{
	struct community_challenge const *challenge = community_daily();
	long elapsed;

	if (!challenge || !challenge->open)
		return 0;
	elapsed = (long)((system_milliseconds() - challenge->answered_at) / 1000);
	return MAX(challenge->seconds_left - elapsed, 0);
}

void community_day_text(char const *day, boolean short_form, wchar_t *text, long count)
{
	static char const *const weekdays[] = {
		"THURSDAY", "FRIDAY", "SATURDAY", "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY" };
	static char const *const months[] = {
		"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER",
		"NOVEMBER", "DECEMBER" };
	long year, month, date;

	if (!day || sscanf(day, "%ld-%ld-%ld", &year, &month, &date) != 3 || month < 1 || month > 12)
	{
		usnprintf(text, count, L"%hs", day ? day : "");
		return;
	}
	if (short_form)
	{
		usnprintf(text, count, L"%.3hs %ld", months[month - 1], date);
		return;
	}
	/* (1970-01-01 was a Thursday) */
	usnprintf(text, count, L"%hs, %hs %ld, %ld",
		weekdays[((days_from_civil(year, month, date) % 7) + 7) % 7], months[month - 1], date, year);
}

boolean community_daily_begin(char const **map_name, short *difficulty)
{
	struct community_challenge const *challenge = community_daily();
	short index;

	if (!challenge)
		return FALSE;
	if (challenge->version > COMMUNITY_CHALLENGE_VERSION)
	{
		status_set(L"This challenge needs a newer version of the game.");
		return FALSE;
	}
	if (challenge->level_index == NONE || !challenge->seed)
	{
		status_set(L"This challenge's level is not one this game has.");
		return FALSE;
	}
	config_overrides_clear();
	for (index = 0; index < community.setting_count; index++)
	{
		if (!config_override(community.setting_names[index], community.setting_values[index]))
			platform_log("community: the challenge's %s could not be set", community.setting_names[index]);
	}
	/* (the run is scored, for its board, whatever SCORING is set to: this run only, as the rest) */
	if (!config_override("game.campaign_score", "true"))
		platform_log("community: the challenge's scoring could not be set");
	community.running = TRUE;
	community.running_loaded = FALSE;
	community.running_since = system_milliseconds();
	community.running_challenge = *challenge;
	community_result_clear();
	*map_name = main_get_solo_level_name(challenge->level_index);
	*difficulty = challenge->difficulty;
	platform_log("community: playing the %s of %s (%s, difficulty %d, seed %lu)",
		challenge->random ? "random run" : "challenge", challenge->day, challenge->level, challenge->difficulty,
		challenge->seed);
	return TRUE;
}

boolean community_random_begin(char const **map_name, short *difficulty)
{
	static struct community_challenge shown;
	static char shown_names[COMMUNITY_MAXIMUM_SETTINGS][48];
	static char shown_values[COMMUNITY_MAXIMUM_SETTINGS][160];
	static unsigned long salt;
	struct community_request shown_request = community.daily;
	short shown_count = community.setting_count;
	boolean begun;

	/* (the challenge shown kept as it was: the run made in its place, set
	for the run, then the shown one back) */
	shown = community.challenge;
	memcpy(shown_names, community.setting_names, sizeof(shown_names));
	memcpy(shown_values, community.setting_values, sizeof(shown_values));
	local_roll_state = (system_milliseconds() ^ ((unsigned long)time(NULL) << 7) ^ (++salt * 2654435761UL)) &
		0xFFFFFFFFUL;
	if (!local_roll_state)
		local_roll_state = 1;
	memset(&community.challenge, 0, sizeof(community.challenge));
	local_day(0, community.challenge.day, sizeof(community.challenge.day), NULL);
	community.challenge.level_index = (short)local_roll(NUMBER_OF_SINGLE_PLAYER_LEVELS);
	local_challenge_rolls(TRUE);
	community.challenge.random = TRUE;
	community.daily.state = _community_ready;
	begun = community_daily_begin(map_name, difficulty);
	community.challenge = shown;
	memcpy(community.setting_names, shown_names, sizeof(shown_names));
	memcpy(community.setting_values, shown_values, sizeof(shown_values));
	community.setting_count = shown_count;
	community.daily = shown_request;
	return begun;
}

void community_daily_cancel(void)
{
	if (!community.running)
		return;
	community.running = FALSE;
	config_overrides_clear();
}

boolean community_challenge_running(void)
{
	return community.running;
}

void community_daily_won(void)
{
	if (!community.running)
		return;
	community.won = TRUE;
	community.won_random = community.running_challenge.random;
	snprintf(community.won_day, sizeof(community.won_day), "%s", community.running_challenge.day);
}

char const *community_daily_won_day(void)
{
	return community.won_day;
}

boolean community_daily_won_take(boolean *random)
{
	boolean won = community.won;

	*random = community.won_random;
	community.won = FALSE;
	return won;
}

unsigned long community_randomizer_seed(char const *scenario_name)
{
	char key[COMMUNITY_LEVEL_KEY_LENGTH];

	if (!community.running)
		return 0;
	level_key(scenario_name, key, sizeof(key));
	return !strcmp(key, community.running_challenge.level) ? community.running_challenge.seed : 0;
}

/* ---------- the past challenges */

/* a skull by its key ("famine"), NONE for one this build has not */
static short skull_of_key(char const *key)
{
	short skull;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		if (!strcmp(skull_descriptions[skull].key, key))
			return skull;
	}
	return NONE;
}

static void history_read(char const *data)
{
	char const *element;
	char today[COMMUNITY_DAY_LENGTH];

	json_text(json_member(data, "today"), today, sizeof(today));
	community.history_count = 0;
	for (element = json_next(json_member(data, "days"), NULL);
		element && community.history_count < COMMUNITY_HISTORY_DAYS; element = json_next(NULL, element))
	{
		struct community_history_day *day = &community.history_days[community.history_count++];
		char level[COMMUNITY_LEVEL_KEY_LENGTH];
		char const *skull;

		memset(day, 0, sizeof(*day));
		json_text(json_member(element, "day"), day->day, sizeof(day->day));
		if (!ae_daily_day_valid(day->day))
			day->day[0] = 0;
		day->today = !strcmp(day->day, today);
		json_text(json_member(element, "level"), level, sizeof(level));
		day->level_index = level_index_of(level);
		day->difficulty = (short)PIN(json_long(json_member(element, "difficulty"), 1), 0, 3);
		for (skull = json_next(json_member(element, "skulls"), NULL);
			skull && day->skull_count < COMMUNITY_MAXIMUM_SKULLS; skull = json_next(NULL, skull))
		{
			char key[24];

			json_text(skull, key, sizeof(key));
			day->skulls[day->skull_count++] = skull_of_key(key);
		}
		day->open = json_boolean(json_member(element, "open"));
		day->players = json_long(json_member(element, "players"), 0);
		day->has_best = json_long(json_member(element, "bestScore"), LONG_MIN) != LONG_MIN;
		day->best_score = json_long(json_member(element, "bestScore"), 0);
		json_wide(json_member(element, "bestName"), day->best_name, COMMUNITY_NAME_LENGTH);
		day->has_mine = json_long(json_path(element, "mine.score"), LONG_MIN) != LONG_MIN;
		day->mine_score = json_long(json_path(element, "mine.score"), 0);
		day->mine_rank = json_long(json_path(element, "mine.rank"), 0);
	}
}

void community_history_open(void)
{
	char path[64];

	community.status[0] = 0;
	/* (offline: the days before, made here, and this machine's records of them) */
	if (!community_enabled())
	{
		struct community_challenge shown = community.challenge;
		short board_count = community.board_count;
		struct community_row board_rows[COMMUNITY_BOARD_ROWS];
		struct community_row board_mine = community.board_mine;
		long board_players = community.board_players;
		short index;

		memcpy(board_rows, community.board_rows, sizeof(board_rows));
		if (community.history.ticket)
			community_net_forget(community.history.ticket);
		community.history.ticket = 0;
		community.history_count = 0;
		for (index = 0; index < COMMUNITY_HISTORY_DAYS; index++)
		{
			struct community_history_day *day = &community.history_days[community.history_count++];

			memset(day, 0, sizeof(*day));
			/* (newest first: today, then the days before) */
			local_day(-index, day->day, sizeof(day->day), NULL);
			day->today = index == 0;
			local_challenge(day->day, FALSE);
			day->level_index = community.challenge.level_index;
			day->difficulty = community.challenge.difficulty;
			day->skull_count = community.challenge.skull_count;
			{
				short skull;

				for (skull = 0; skull < day->skull_count; skull++)
					day->skulls[skull] = skull_of_key(community.challenge.skulls[skull]);
			}
			day->open = community.challenge.open;
			local_board(day->day);
			day->players = community.board_players;
			day->has_best = community.board_count > 0;
			if (day->has_best)
			{
				day->best_score = community.board_rows[0].score;
				ustrncpy(day->best_name, community.board_rows[0].name, COMMUNITY_NAME_LENGTH - 1);
			}
			day->has_mine = community.board_mine.name[0] != 0;
			day->mine_score = community.board_mine.score;
			day->mine_rank = community.board_mine.rank;
		}
		community.challenge = shown;
		community.board_count = board_count;
		memcpy(community.board_rows, board_rows, sizeof(board_rows));
		community.board_mine = board_mine;
		community.board_players = board_players;
		community.history.state = _community_ready;
		return;
	}
	snprintf(path, sizeof(path), "/daily/history?limit=%d", COMMUNITY_HISTORY_DAYS);
	request_start(&community.history, "history", path, NULL);
	if (community.history.state == _community_unavailable)
		status_set(L"The community server can't be reached right now.");
}

short community_history_state(void)
{
	return community.history.state;
}

short community_history_count(void)
{
	return community.history.state == _community_ready ? community.history_count : 0;
}

struct community_history_day const *community_history_day(short index)
{
	return index >= 0 && index < community_history_count() ? &community.history_days[index] : NULL;
}

/* ---------- the online leaderboards */

static void row_read(char const *element, struct community_row *row)
{
	memset(row, 0, sizeof(*row));
	if (!element)
		return;
	row->rank = json_long(json_member(element, "rank"), 0);
	json_wide(json_member(element, "name"), row->name, COMMUNITY_NAME_LENGTH);
	row->score = json_long(json_member(element, "score"), 0);
	row->seconds = json_long(json_member(element, "seconds"), 0);
	row->difficulty = json_long(json_member(element, "difficulty"), NONE);
	row->skulls = json_long(json_member(element, "skulls"), NONE);
	row->medals = json_long(json_member(element, "medals"), NONE);
}

static void board_read(char const *data)
{
	char const *element;

	community.board_count = 0;
	for (element = json_next(json_member(data, "rows"), NULL);
		element && community.board_count < COMMUNITY_BOARD_ROWS; element = json_next(NULL, element))
	{
		row_read(element, &community.board_rows[community.board_count++]);
	}
	row_read(json_member(data, "me"), &community.board_mine);
	community.board_players = json_long(json_member(data, "players"), 0);
	community.board_day_top_count = 0;
	for (element = json_next(json_member(data, "dayTop"), NULL);
		element && community.board_day_top_count < COMMUNITY_DAY_TOP; element = json_next(NULL, element))
	{
		row_read(element, &community.board_day_top[community.board_day_top_count++]);
	}
}

static void board_open(char const *asked, char const *path, boolean needs_scoring)
{
	community.status[0] = 0;
	community.board_count = 0;
	community.board_day_top_count = 0;
	memset(&community.board_mine, 0, sizeof(community.board_mine));
	community.board_players = 0;
	/* (no level's scores asked for with scoring off; a day's board is asked for as it is: a challenge's run is always scored) */
	if (needs_scoring && !community_scoring())
	{
		if (community.board.ticket)
			community_net_forget(community.board.ticket);
		community.board.ticket = 0;
		community.board.state = _community_off;
		status_set(L"Scoring is off (Settings > Campaign > Scoring).");
		return;
	}
	request_start(&community.board, asked, path, NULL);
	if (community.board.state == _community_off)
		status_set(L"Online leaderboards are off (Settings > Campaign > Daily Challenge).");
	else if (community.board.state == _community_unavailable)
		status_set(L"The community server can't be reached right now.");
}

void community_board_open_level(char const *level_name)
{
	char key[COMMUNITY_LEVEL_KEY_LENGTH];
	char path[96];

	level_key(level_name, key, sizeof(key));
	snprintf(path, sizeof(path), "/levels/%s/leaderboard?limit=%d", key, COMMUNITY_BOARD_ROWS);
	board_open(key, path, TRUE);
}

void community_board_open_day(char const *day)
{
	char path[96];

	/* (offline: this machine's profiles') */
	if (!community_enabled())
	{
		if (community.board.ticket)
			community_net_forget(community.board.ticket);
		community.board.ticket = 0;
		community.status[0] = 0;
		local_board(day);
		community.board.state = _community_ready;
		return;
	}
	snprintf(path, sizeof(path), "/daily/%s/leaderboard?limit=%d", day, COMMUNITY_BOARD_ROWS);
	board_open(day, path, FALSE);
}

short community_board_state(void)
{
	return community.board.state;
}

short community_board_count(void)
{
	return community.board.state == _community_ready ? community.board_count : 0;
}

struct community_row const *community_board_row(short index)
{
	return index >= 0 && index < community_board_count() ? &community.board_rows[index] : NULL;
}

struct community_row const *community_board_mine(void)
{
	return community.board.state == _community_ready && community.board_mine.rank ? &community.board_mine : NULL;
}

short community_board_day_top_count(void)
{
	return community.board.state == _community_ready ? community.board_day_top_count : 0;
}

struct community_row const *community_board_day_top(short index)
{
	return index >= 0 && index < community_board_day_top_count() ? &community.board_day_top[index] : NULL;
}

long community_board_players(void)
{
	return community.board.state == _community_ready ? community.board_players : 0;
}

/* ---------- scores sent */

static void result_set(short slot, wchar_t const *text)
{
	if (slot < 0 || slot >= COMMUNITY_RESULT_SLOTS)
		return;
	ustrncpy(community.result[slot], text, COMMUNITY_RESULT_LENGTH - 1);
	community.result[slot][COMMUNITY_RESULT_LENGTH - 1] = 0;
}

static void submit(char const *path, char const *body, char const *player, char const *day, short slot)
{
	short index;
	int ticket;

	for (index = 0; index < (short)NUMBEROF(community.submit_tickets); index++)
	{
		if (!community.submit_tickets[index])
			break;
	}
	if (index == (short)NUMBEROF(community.submit_tickets))
	{
		platform_log("community: a score is already being sent; %s not sent", path);
		return;
	}
	ticket = community_net_send(path, body, player);
	if (!ticket)
	{
		result_set(slot, L"ONLINE: NOT SENT (NO CONNECTION)");
		return;
	}
	community.submit_tickets[index] = ticket;
	community.submit_slots[index] = slot;
	snprintf(community.submit_days[index], sizeof(community.submit_days[0]), "%s", day ? day : "");
	result_set(slot, L"ONLINE: SENDING...");
}

/* the challenge of the level won (key), if it is one: this machine's (its
lobby's, a network co-op host's), or its host's (a client: the day and
flags it was told, the roll the host advertised, the skulls it plays
with). Its leaderboard takes it whenever it is played */
static boolean run_challenge(char const *key, short difficulty, struct community_challenge *challenge)
{
	short skull;

	if (community.running)
	{
		if (strcmp(key, community.running_challenge.level))
			return FALSE;
		*challenge = community.running_challenge;
		return TRUE;
	}
	if (!community.network_day)
		return FALSE;
	memset(challenge, 0, sizeof(*challenge));
	day_from_days(community.network_day, challenge->day, sizeof(challenge->day));
	snprintf(challenge->level, sizeof(challenge->level), "%s", key);
	challenge->level_index = level_index_of(key);
	challenge->difficulty = difficulty;
	challenge->version = community.network_flags & 0xFF;
	challenge->online = (community.network_flags & COMMUNITY_NETWORK_ONLINE) != 0;
	challenge->seed = randomizer_roll();
	for (skull = 0; skull < NUMBER_OF_SKULLS && challenge->skull_count < COMMUNITY_MAXIMUM_SKULLS; skull++)
	{
		if (skull_active(skull))
		{
			snprintf(challenge->skulls[challenge->skull_count++], sizeof(challenge->skulls[0]), "%s",
				skull_descriptions[skull].key);
		}
	}
	return TRUE;
}

boolean community_daily_run(void)
{
	return community.running || community.network_day != 0;
}

void community_network_challenge(long *day, long *flags)
{
	long year, month, date;

	*day = 0;
	*flags = 0;
	/* (a random run is no challenge its clients send) */
	if (!community.running || community.running_challenge.random ||
		sscanf(community.running_challenge.day, "%ld-%ld-%ld", &year, &month, &date) != 3)
	{
		return;
	}
	*day = days_from_civil(year, month, date);
	*flags = (community.running_challenge.version & 0xFF) |
		(community.running_challenge.online ? COMMUNITY_NETWORK_ONLINE : 0);
}

void community_network_challenge_set(long day, long flags)
{
	community.network_day = day;
	community.network_flags = flags;
}

void community_level_won(char const *level_name, struct community_run const *run, wchar_t const *profile_name,
	long profile_index, boolean alone, short slot)
{
	char key[COMMUNITY_LEVEL_KEY_LENGTH];
	char name[COMMUNITY_NAME_LENGTH * 3 + 4];
	char player[COMMUNITY_PLAYER_LENGTH];
	char body[COMMUNITY_BODY_LENGTH];
	struct community_challenge challenge;
	boolean daily;
	long length;

	/* (no score kept or sent with scoring off) */
	if (!community_scoring())
		return;
	level_key(level_name, key, sizeof(key));
	daily = run_challenge(key, (short)run->difficulty, &challenge);
	/* (a random run's: no leaderboard) */
	if (daily && challenge.random)
	{
		result_set(slot, L"RANDOM RUN: NOT RANKED");
		return;
	}
	if (daily && slot == 0)
		service_rank_on_daily_challenge_completed(run->score);
	/* (offline: a challenge's run kept in the profile's records, and its
	place among this machine's profiles') */
	if (daily && !challenge.online)
	{
		boolean improved;
		wchar_t text[COMMUNITY_RESULT_LENGTH];

		improved = local_record_add(profile_index, challenge.day, run);
		local_board(challenge.day);
		usnprintf(text, COMMUNITY_RESULT_LENGTH, L"THIS MACHINE: #%ld%s",
			community.board_mine.rank ? community.board_mine.rank : 1, improved ? L"  NEW BEST" : L"");
		result_set(slot, text);
		return;
	}
	if (!community_enabled())
	{
		if (daily)
			result_set(slot, L"OFFLINE: NOT SENT");
		return;
	}
	/* (another level's: one player's of a game of this machine's alone) */
	if (!daily && (!alone || !config_boolean("community.upload_scores")))
		return;
	json_quote_wide(profile_name, name, sizeof(name));
	if (!key[0] || !strcmp(name, "\"\""))
		return;
	player_id(profile_name, player, sizeof(player));
	length = snprintf(body, sizeof(body), "{\"name\":%s,\"level\":\"%s\",\"score\":%ld,\"seconds\":%ld,"
		"\"difficulty\":%ld,\"skulls\":%ld,\"skullsPercent\":%ld,\"medals\":%ld,\"kills\":%ld,\"deaths\":%ld,"
		"\"players\":%ld,\"platform\":\"%s\"", name, key, run->score, MAX(run->seconds, 1), run->difficulty,
		run->skulls, run->skulls_percent, run->medals, run->kills, run->deaths, MAX(run->players, 1),
		community_net_platform());
	if (daily)
	{
		char path[64];
		short skull;

		length += snprintf(body + length, sizeof(body) - (size_t)length, ",\"seed\":%lu,\"version\":%ld,\"skullSet\":[",
			challenge.seed, challenge.version);
		for (skull = 0; skull < challenge.skull_count && length < (long)sizeof(body) - 32; skull++)
		{
			length += snprintf(body + length, sizeof(body) - (size_t)length, "%s\"%s\"", skull ? "," : "",
				challenge.skulls[skull]);
		}
		snprintf(body + length, sizeof(body) - (size_t)length, "]}");
		snprintf(path, sizeof(path), "/daily/%s/scores", challenge.day);
		submit(path, body, player, challenge.day, slot);
		return;
	}
	snprintf(body + length, sizeof(body) - (size_t)length, "}");
	submit("/levels/scores", body, player, NULL, slot);
}

wchar_t const *community_result_text(short slot)
{
	return slot >= 0 && slot < COMMUNITY_RESULT_SLOTS ? community.result[slot] : L"";
}

void community_result_clear(void)
{
	short slot;

	for (slot = 0; slot < COMMUNITY_RESULT_SLOTS; slot++)
		community.result[slot][0] = 0;
}

static void submit_poll(void)
{
	short index;

	for (index = 0; index < (short)NUMBEROF(community.submit_tickets); index++)
	{
		int ticket = community.submit_tickets[index];
		int status = 0;
		int result;
		char const *data;
		short slot = community.submit_slots[index];
		wchar_t text[COMMUNITY_RESULT_LENGTH];

		if (!ticket)
			continue;
		result = community_net_poll(ticket, &status, community.answer, sizeof(community.answer));
		if (result == _community_net_waiting)
			continue;
		community.submit_tickets[index] = 0;
		if (result != _community_net_answered)
		{
			result_set(slot, L"ONLINE: SERVER UNAVAILABLE, NOT SENT");
			continue;
		}
		data = json_member(community.answer, "data");
		if (status == 200 && data)
		{
			long rank = json_long(json_path(data, "me.rank"), 0);
			long players = json_long(json_member(data, "players"), 0);

			if (rank)
			{
				usnprintf(text, COMMUNITY_RESULT_LENGTH, L"ONLINE: #%ld OF %ld%s", rank, players,
					json_boolean(json_member(data, "improved")) ? L"  NEW BEST" : L"");
				result_set(slot, text);
			}
			else
				result_set(slot, L"ONLINE: SENT");
			/* (the day's leaderboard, already shown: asked for again, with it) */
			if (community.submit_days[index][0] && community.board.state != _community_loading &&
				!strcmp(community.board.asked, community.submit_days[index]))
			{
				community_board_open_day(community.submit_days[index]);
			}
		}
		else
		{
			char type[32];

			error_type(community.answer, type, sizeof(type));
			if (!strcmp(type, "closed"))
				result_set(slot, L"ONLINE: THE CHALLENGE ENDED WHILE YOU PLAYED");
			else if (!strcmp(type, "settings_mismatch"))
				result_set(slot, L"ONLINE: SETTINGS CHANGED, NOT COUNTED");
			else
			{
				usnprintf(text, COMMUNITY_RESULT_LENGTH, L"ONLINE: NOT ACCEPTED (%hs)", type[0] ? type : "error");
				result_set(slot, text);
			}
			platform_log("community: a score was refused: %d %s", status, type);
		}
	}
}

/* ---------- the main menu's news: none in this build (Content Evolved's asks GitHub for this fork's releases; the
menus here have no news, and nothing is asked of any server but the community server's, with the player's consent) */

short community_news_count(void)
{
	return 0;
}

char const *community_news_name(short index)
{
	(void)index;
	return "";
}

char const *community_news_date(short index)
{
	(void)index;
	return "";
}

long community_news_build(short index)
{
	(void)index;
	return NONE;
}

char const *community_news_text(short index)
{
	(void)index;
	return "";
}

long community_news_latest_build(void)
{
	return NONE;
}

long community_build_number(void)
{
	return 0;
}

/* ---------- each frame */

/* a map is loading (every game's, game_load): one that is neither the menus' nor the challenge's level takes the challenge's settings away
now, before its tags or a lobby's broadcast read them */
void community_map_loading(char const *map_name)
{
	char key[48];

	if (!community.running)
		return;
	level_key(map_name, key, sizeof(key));
	if (ae_daily_map_keeps_settings(key, community.running_challenge.level))
		return;
	community.running = FALSE;
	community.running_loaded = FALSE;
	config_overrides_clear();
	platform_log("community: the challenge's settings are taken away (the map %s is not its level)", key);
}

void community_update(boolean main_menu)
{
	char const *data;

	if (request_poll(&community.daily, &data) && data)
		challenge_read(data);
	if (request_poll(&community.history, &data) && data)
		history_read(data);
	if (request_poll(&community.board, &data) && data)
		board_read(data);
	submit_poll();
	/* (the challenge's settings, until its level was played and the main
	menu is back) */
	/* (a client's host's challenge, until it is back at the menus) */
	if (main_menu)
		community.network_day = 0;
	/* (a game that is no campaign level's takes none of them: a multiplayer game never plays with a challenge's settings) */
	if (community.running && !main_menu)
	{
		struct scenario *scenario = global_scenario_try_and_get();

		if (scenario && scenario->type != _scenario_type_solo)
		{
			community.running = FALSE;
			community.running_loaded = FALSE;
			config_overrides_clear();
			platform_log("community: the challenge's settings are taken away (a game that is no campaign level's)");
		}
	}
	if (community.running)
	{
		if (!main_menu)
			community.running_loaded = TRUE;
		/* (a network co-op lobby's, while it is hosted and the level has been played: its next round is the challenge again;
		one hosted before that goes on to the load's time limit) */
		else if (global_network_game_server_get() && community.running_loaded)
			community.running_since = system_milliseconds();
		else if (community.running_loaded ||
			system_milliseconds() - community.running_since > COMMUNITY_LOAD_TIMEOUT_MILLISECONDS)
		{
			community.running = FALSE;
			community.running_loaded = FALSE;
			config_overrides_clear();
			platform_log("community: the challenge's settings are taken away");
		}
	}
}
