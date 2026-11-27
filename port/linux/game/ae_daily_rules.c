/*
AE_DAILY_RULES.C

See ae_daily_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_daily_rules.h"

/* days since 1970-01-01 of a civil date */
static long days_from_civil(long year, long month, long day)
{
	long era, year_of_era, day_of_year, day_of_era;

	year -= month <= 2;
	era = (year >= 0 ? year : year - 399) / 400;
	year_of_era = year - era * 400;
	day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
	return era * 146097 + day_of_era - 719468;
}

static void civil_from_days(long days, long *year, long *month, long *date)
{
	long era, day_of_era, year_of_era, day_of_year, month_index;

	days += 719468;
	era = (days >= 0 ? days : days - 146096) / 146097;
	day_of_era = days - era * 146097;
	year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
	day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
	month_index = (5 * day_of_year + 2) / 153;
	*date = day_of_year - (153 * month_index + 2) / 5 + 1;
	*month = month_index < 10 ? month_index + 3 : month_index - 9;
	*year = year_of_era + era * 400 + (*month <= 2);
}

/* the day's number, 0 and FALSE for text that is no day */
static int day_number(const char *day, long *number)
{
	long year, month, date;
	char tail;

	if (!day || strlen(day) != 10 || sscanf(day, "%4ld-%2ld-%2ld%c", &year, &month, &date, &tail) != 3 || day[4] != '-' ||
		day[7] != '-' || month < 1 || month > 12 || date < 1 || date > 31)
		return 0;
	*number = days_from_civil(year, month, date);
	return 1;
}

int ae_daily_days_ago(const char *day, const char *today)
{
	long a, b;

	if (!day_number(day, &a) || !day_number(today, &b) || a > b)
		return -1;
	return (int)(b - a);
}

int ae_daily_day_shift(const char *day, int days, char *out, size_t size)
{
	long number, year, month, date;

	if (!day_number(day, &number) || size < 11)
		return 0;
	civil_from_days(number + days, &year, &month, &date);
	snprintf(out, size, "%04ld-%02ld-%02ld", year, month, date);
	return 1;
}

void ae_daily_day_label(const char *day, const char *today, char *text, size_t size)
{
	int ago = ae_daily_days_ago(day, today);
	long number;

	if (size == 0)
		return;
	text[0] = 0;
	if (!day_number(day, &number))
		return;
	if (ago == 0)
		snprintf(text, size, "TODAY");
	else if (ago == 1)
		snprintf(text, size, "YESTERDAY");
	else
		snprintf(text, size, "%.2s-%.2s", day + 5, day + 8);
}

void ae_daily_countdown_text(long seconds, int today, char *text, size_t size)
{
	long minutes = (seconds + 59) / 60;

	if (seconds <= 0)
		snprintf(text, size, "THIS DAY IS CLOSED");
	else if (!today)
		snprintf(text, size, "TAKES SCORES FOR %ld MIN", minutes);
	else if (minutes >= 60)
		snprintf(text, size, "A NEW CHALLENGE IN %ld H %ld MIN", minutes / 60, minutes % 60);
	else
		snprintf(text, size, "A NEW CHALLENGE IN %ld MIN", minutes);
}

const char *ae_daily_crumb(int online, int unreachable)
{
	if (online && unreachable)
		return "ONLINE \xC2\xB7 NOT REACHED";
	return online ? "THIS PC \xC2\xB7 ONLINE" : "THIS PC \xC2\xB7 OFFLINE";
}

const char *ae_daily_offline_note(void)
{
	return "Offline: the day and its board are this PC's. Online boards need COMMUNITY SERVICES and ONLINE DAY AND BOARDS on.";
}

void ae_daily_board_head(int today, int online, char *text, size_t size)
{
	snprintf(text, size, "%s BOARD \xE2\x80\xA2 %s", today ? "TODAY'S" : "ITS", online ? "ONLINE" : "THIS PC'S PROFILES");
}

void ae_daily_play_text(const char *label, char *text, size_t size)
{
	snprintf(text, size, "PLAY %s'S CHALLENGE", label);
}

const char *ae_daily_day_help(void)
{
	return "Left and right go to the days before: their challenges stay playable, and a run counts for its day.";
}

const char *ae_daily_difficulty_name(int difficulty)
{
	static const char *const names[4] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };

	return difficulty >= 0 && difficulty < 4 ? names[difficulty] : "";
}

int ae_daily_online_on(int enabled, const char *services)
{
	return enabled && services && !strcmp(services, "yes");
}

const char *ae_daily_online_help(void)
{
	return "Asks Content Evolved's community server for the day's challenge and its boards, and sends it a random install key, "
		"your profile's name and your scores in a daily challenge. OFF: nothing leaves this PC.";
}

const char *ae_daily_share_help(void)
{
	return "A mission won with SCORING on, by one player on this PC, is also sent to the mission's online board with your "
		"profile's name. A daily challenge's run always goes to its day's board while ONLINE DAY AND BOARDS is on.";
}

const char *ae_daily_server_help(void)
{
	return "DEFAULT: Content Evolved's community server. CUSTOM: another community's server, set in SERVER ADDRESS. "
		"Needs ONLINE DAY AND BOARDS on.";
}

const char *ae_daily_server_address_help(void)
{
	return "The https address of another community's server, as https://example.com. Used when SERVER is CUSTOM.";
}

int ae_daily_server_clean(const char *typed, char *out, size_t size)
{
	char work[AE_DAILY_SERVER_MAXIMUM + 16];
	size_t length, host, at;

	if (size)
		out[0] = 0;
	if (!typed || size < 2)
		return 0;
	while (*typed == ' ')
		typed++;
	if (strlen(typed) > AE_DAILY_SERVER_MAXIMUM)
		return 0;
	if (!strstr(typed, "://"))
		snprintf(work, sizeof(work), "https://%s", typed);
	else
		snprintf(work, sizeof(work), "%s", typed);
	length = strlen(work);
	while (length > 0 && (work[length - 1] == ' ' || work[length - 1] == '/'))
		work[--length] = 0;
	if (strncmp(work, "https://", 8) || length <= 8)
		return 0;
	for (at = 8; at < length; at++)
	{
		unsigned char letter = (unsigned char)work[at];

		if (letter <= ' ' || letter >= 127 || letter == '"' || letter == '\\' || letter == '<' || letter == '>')
			return 0;
	}
	/* (a host: something before any path, and not another scheme's separator) */
	host = strcspn(work + 8, "/?#");
	if (host == 0 || work[8] == ':' || strstr(work + 8, "://"))
		return 0;
	if (length + 1 > size)
		return 0;
	memcpy(out, work, length + 1);
	return 1;
}

void ae_daily_server_address_shown(const char *mode, const char *address, char *text, size_t size)
{
	if (!size)
		return;
	if (!mode || strcmp(mode, "custom"))
		snprintf(text, size, "USES DEFAULT");
	else if (!address || !address[0])
		snprintf(text, size, "NOT SET");
	else
		snprintf(text, size, "%s", address);
}

int ae_daily_key_allowed(const char *key)
{
	static const char *const keys[] = { "game.daily_challenge", "community.enabled", "community.upload_scores",
		"community.server_mode", "community.custom_server" };
	size_t each;

	for (each = 0; key && each < sizeof(keys) / sizeof(keys[0]); each++)
		if (!strcmp(key, keys[each]))
			return 1;
	return 0;
}

void ae_daily_randomizer_summary(char const (*details)[64], int count, char *text, size_t size)
{
	static const char *const words[4] = { "ENEMIES", "WEAPONS", "PICKUPS", "GO CRAZY" };
	int each, kept = 0;

	if (size == 0)
		return;
	text[0] = 0;
	for (each = 0; each < count && each < 4; each++)
	{
		if (strstr(details[each], "NOT RANDOMIZED") || strstr(details[each], "GO CRAZY: OFF"))
			continue;
		snprintf(text + strlen(text), size - strlen(text), "%s%s", kept++ ? " \xE2\x80\xA2 " : "", words[each]);
	}
	if (!kept)
		snprintf(text, size, "OFF");
}

/* the settings a community challenge may set for its run: the offline challenge's own (community.c local_challenge_rolls),
exact names, none of the player's skull progress (the unlock mode, the unlocked list, ALL SKULLS, INSTANT UNLOCK) */
int ae_daily_challenge_key_allowed(const char *key)
{
	static const char *const keys[] = { "game.randomizer", "game.randomizer_enemies", "game.randomizer_allies",
		"game.randomizer_pool", "game.randomizer_type", "game.randomizer_pickups", "game.randomizer_pickup_pool",
		"game.randomizer_pickup_type", "game.randomizer_enemy_weapons", "game.randomizer_weapon_pool",
		"game.randomizer_weapon_type", "game.randomizer_crazy", "game.randomizer_crazy_weapons",
		"game.randomizer_crazy_enemies", "game.randomizer_crazy_flamethrower", "game.randomizer_crazy_pods",
		"game.randomizer_grunt_rank", "game.randomizer_elite_rank", "game.randomizer_jackal_rank",
		"game.randomizer_hunter_rank", "game.randomizer_flood_combat_rank", "game.skulls", "game.skulls_active",
		"game.skulls_lose_on_death", "game.skulls_drop_count" };
	size_t each;

	for (each = 0; key && each < sizeof(keys) / sizeof(keys[0]); each++)
		if (!strcmp(key, keys[each]))
			return 1;
	return 0;
}

/* whether a challenge's settings stay when a map loads: the menus' own (ui) and the challenge's level (its next round, a co-op host's) keep them */
int ae_daily_map_keeps_settings(const char *map_key, const char *challenge_key)
{
	if (!map_key || !challenge_key || !challenge_key[0])
		return 0;
	return !strcmp(map_key, "ui") || !strcmp(map_key, challenge_key);
}

/* whether text is a day exactly as the server names it: YYYY-MM-DD, digits only, a month 01-12 and a date 01-31 (it goes into a request's path) */
int ae_daily_day_valid(const char *day)
{
	int index, month, date;

	if (!day || strlen(day) != 10)
		return 0;
	for (index = 0; index < 10; index++)
	{
		if (index == 4 || index == 7 ? day[index] != '-' : day[index] < '0' || day[index] > '9')
			return 0;
	}
	month = (day[5] - '0') * 10 + (day[6] - '0');
	date = (day[8] - '0') * 10 + (day[9] - '0');
	return month >= 1 && month <= 12 && date >= 1 && date <= 31;
}

/* ---------- the window of a server not reached */

int ae_daily_unreached_wanted(int online, int unreachable, int asked)
{
	return online && unreachable && !asked;
}

int ae_daily_unreached_choices(int custom, int *ids, const char **words)
{
	static const char *const names[AE_DAILY_UNREACHED_CHOICES] = { "TRY THE DEFAULT SERVER", "CHANGE THE ADDRESS", "PLAY OFFLINE", "TRY AGAIN", "BACK" };
	int each, count = 0;

	for (each = 0; each < AE_DAILY_UNREACHED_CHOICES; each++)
	{
		if (each == AE_DAILY_UNREACHED_DEFAULT && !custom)
			continue;
		ids[count] = each;
		words[count] = names[each];
		count++;
	}
	return count;
}

const char *ae_daily_unreached_reason(const char *status)
{
	if (!status || !status[0])
		return "The server did not answer.";
	if (strstr(status, "not a community server"))
		return "That address is not a community server (a bad answer).";
	if (strstr(status, "refused") || strstr(status, "database") || strstr(status, "trouble"))
		return "The server answered, and could not give the day.";
	return "The server did not answer.";
}

void ae_daily_unreached_body(const char *server, const char *status, int custom, char *text, size_t size)
{
	snprintf(text, size, "The daily challenge could not be fetched from\n%s\n\n%s\n\n%sTRY AGAIN asks the same server.\nPLAY OFFLINE turns ONLINE off and plays this PC's day: nothing is sent.",
		server && server[0] ? server : "the community server", ae_daily_unreached_reason(status),
		custom ? "TRY THE DEFAULT SERVER asks Content Evolved's server instead.\n" : "CHANGE THE ADDRESS asks another community server.\n");
}

int ae_daily_retry_shown(int online, int state)
{
	/* (state as the daily screen reads it: 2 is no day, not reached or refused) */
	return online && state == 2;
}

int ae_daily_unreached_write(int id, const char **key, const char **value)
{
	if (id == AE_DAILY_UNREACHED_DEFAULT)
	{
		*key = "community.server_mode";
		*value = "default";
		return 1;
	}
	if (id == AE_DAILY_UNREACHED_OFFLINE)
	{
		*key = "community.enabled";
		*value = "false";
		return 1;
	}
	return 0;
}
