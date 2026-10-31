/*
AE_BOT_SETTINGS.C

The bot settings as the AE menus' rows (ae_bot_settings.h). Moved here from CREATE GAME (0.3.9's lobby panel and LOBBY
SETUP) for the pause menu's BOTS (0.4.0): the values, words and help unchanged.
*/

#include <stdio.h>
#include <string.h>

#include "ae_bot_settings.h"
#include "ae_sound.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
long config_integer(char const *name);
int config_boolean(char const *name);
char const *config_string(char const *name);
int config_write(char const *name, char const *value);

/* BOTS: OFF and 1-31, then the big games' 47, 63, 95 and 127 (bots.count; typed, any count up to 127), past 31 with a
warning (owner 10-26: big bot games run, but the bots lose their way on big maps), BOT SKILL: bots.skill's levels
(wording and help: tools/port_settings.py's Bots screen) */
enum { BOTS_ADVISED_MAXIMUM = 31, BOTS_SETTING_MAXIMUM = 127, BOT_SKILLS = 6, BOT_SKILL_DEFAULT = 2 };
static const short bots_big_stops[] = { 47, 63, 95, 127 };
enum { BOTS_STOPS = BOTS_ADVISED_MAXIMUM + 1 + (int)(sizeof(bots_big_stops) / sizeof(bots_big_stops[0])) };
static const char *const bot_skill_values[BOT_SKILLS] = { "recruit", "easy", "normal", "heroic", "legendary", "demon" };
static const char *const bot_skill_names[BOT_SKILLS] = { "RECRUIT", "EASY", "NORMAL", "HEROIC", "LEGENDARY", "DEMON" };
#define BOTS_HELP "How many bots play your games, 31 or fewer best; a person who joins a full game takes a bot's " \
	"place. With bots, {X} (LOBBY SETUP) has more."
#define BOTS_MANY_HELP "Past 31 bots the game runs, but the bots get lost and stuck on big maps, drive poorly and " \
	"swamp the fights: 31 or fewer play best. OFFLINE takes up to 120."
#define BOT_SKILL_HELP "How soon the bots react, how well they aim and how far they see."
/* (0.3.9) LOBBY SETUP's bot rows: the settings bots.shooting_skill, bots.movement_skill ("same": BOT SKILL's),
bots.playfight, bots.teams; their wording tools/port_settings.py's classic BOTS screen's */
/* (MOVEMENT SKILL has one more, last: STANDING, the bots stay where they spawn, for the owner's tests, 10-30) */
enum { BOT_PARTS = BOT_SKILLS + 1, BOT_MOVEMENTS = BOT_PARTS + 1 };
static const char *const bot_part_values[BOT_MOVEMENTS] = { "same", "recruit", "easy", "normal", "heroic", "legendary",
	"demon", "standing" };
static const char *const bot_part_names[BOT_MOVEMENTS] = { "AS BOT SKILL", "RECRUIT", "EASY", "NORMAL", "HEROIC",
	"LEGENDARY", "DEMON", "STANDING" };
static const char *const bot_teams_values[2] = { "even", "against" };
static const char *const bot_teams_names[2] = { "EVEN", "AGAINST YOU" };
#define SHOOTING_HELP "The bots' aim, reactions, grenades and blows: as BOT SKILL, or a level of their own."
#define MOVEMENT_HELP "The bots' strafing, jumping, crouching and retreating: as BOT SKILL, or a level of their own."
#define STANDING_HELP "STANDING: the bots stay where they spawn; they only turn, aim and shoot (with PLAYFIGHT on, not " \
	"even that): targets for tests. At once, in game too."
static const char *const playfight_helps[2] = {
	"Off: the bots fight back.",
	"On: the bots move, strafe and aim at you, but never shoot, throw or strike: aim practice. At once, in game too."
};
static const char *const bot_teams_helps[2] = {
	"In team games, bots on both teams, evening them with the players.",
	"In team games, every bot on the team the players are not on."
};
static const char *const labels[AE_BOT_SETTINGS] = { "BOTS", "BOT SKILL", "SHOOTING SKILL", "MOVEMENT SKILL",
	"PLAYFIGHT", "BOT TEAMS" };
static const char *const settings[AE_BOT_SETTINGS] = { "bots.count", "bots.skill", "bots.shooting_skill",
	"bots.movement_skill", "bots.playfight", "bots.teams" };
static const char *const logged[AE_BOT_SETTINGS] = { "bots", "bot skill", "shooting skill", "movement skill",
	"playfight", "teams" };

long ae_bot_count(void)
{
	long count = config_integer("bots.count");

	return count < 0 ? 0 : count > BOTS_SETTING_MAXIMUM ? BOTS_SETTING_MAXIMUM : count;
}

/* a setting's value among its values (the fallback when it has none of them) */
static short setting_index(const char *name, const char *const *values, short count, short fallback)
{
	char const *now = config_string(name);
	short index;

	for (index = 0; now && index < count; index++)
	{
		if (!strcmp(now, values[index]))
			return index;
	}
	return fallback;
}

/* the value's index now (BOTS: its count) */
static short setting_now(int kind)
{
	switch (kind)
	{
	case AE_BOT_COUNT: return (short)ae_bot_count();
	case AE_BOT_SKILL: return setting_index("bots.skill", bot_skill_values, BOT_SKILLS, BOT_SKILL_DEFAULT);
	case AE_BOT_SHOOTING: return setting_index("bots.shooting_skill", bot_part_values, BOT_PARTS, 0);
	case AE_BOT_MOVEMENT: return setting_index("bots.movement_skill", bot_part_values, BOT_MOVEMENTS, 0);
	case AE_BOT_PLAYFIGHT: return (short)(config_boolean("bots.playfight") != 0);
	default: return setting_index("bots.teams", bot_teams_values, 2, 0);
	}
}

const char *ae_bot_setting_label(int kind)
{
	return kind >= 0 && kind < AE_BOT_SETTINGS ? labels[kind] : "";
}

const char *ae_bot_setting_text(int kind)
{
	static char text[8];
	short now = setting_now(kind);

	switch (kind)
	{
	case AE_BOT_COUNT:
		if (!now)
			return "OFF";
		snprintf(text, sizeof(text), "%d", now);
		return text;
	case AE_BOT_SKILL: return bot_skill_names[now];
	case AE_BOT_SHOOTING:
	case AE_BOT_MOVEMENT:
		return bot_part_names[now];
	case AE_BOT_PLAYFIGHT:
		return now ? "ON" : "OFF";
	default:
		return bot_teams_names[now];
	}
}

int ae_bot_setting_warns(int kind)
{
	return kind == AE_BOT_COUNT && ae_bot_count() > BOTS_ADVISED_MAXIMUM;
}

/* BOTS' stop (its index among OFF, 1-31 and the big games' counts) for a count: a count between stops is the stop
below it, stepped from there */
static short bots_stop(long count)
{
	short stop = (short)(count > BOTS_ADVISED_MAXIMUM ? BOTS_ADVISED_MAXIMUM : count), big;

	for (big = 0; big < (short)(sizeof(bots_big_stops) / sizeof(bots_big_stops[0])); big++)
		if (count >= bots_big_stops[big])
			stop = (short)(BOTS_ADVISED_MAXIMUM + 1 + big);
	return stop;
}

static long bots_stop_count(short stop)
{
	return stop <= BOTS_ADVISED_MAXIMUM ? stop : bots_big_stops[stop - BOTS_ADVISED_MAXIMUM - 1];
}

const char *ae_bot_setting_help(int kind)
{
	switch (kind)
	{
	case AE_BOT_COUNT: return ae_bot_setting_warns(AE_BOT_COUNT) ? BOTS_MANY_HELP : BOTS_HELP;
	case AE_BOT_SKILL: return BOT_SKILL_HELP;
	case AE_BOT_SHOOTING: return SHOOTING_HELP;
	case AE_BOT_MOVEMENT: return setting_now(AE_BOT_MOVEMENT) == BOT_MOVEMENTS - 1 ? STANDING_HELP : MOVEMENT_HELP;
	case AE_BOT_PLAYFIGHT: return playfight_helps[setting_now(AE_BOT_PLAYFIGHT)];
	default: return bot_teams_helps[setting_now(AE_BOT_TEAMS)];
	}
}

int ae_bot_setting_step(int kind, short direction, const char *log_prefix)
{
	static const char *const on_off[2] = { "false", "true" };
	char text[8];
	short current = setting_now(kind), next;
	const char *value;

	if (kind < 0 || kind >= AE_BOT_SETTINGS)
		return 0;
	if (kind == AE_BOT_COUNT)
	{
		/* (a count between stops, typed or from config.toml: right goes to the next stop, left to the stop below) */
		short stop = bots_stop(current);

		if (bots_stop_count(stop) != current && direction < 0)
			next = stop;
		else
			next = ae_value_step(BOTS_STOPS, stop, direction);
		snprintf(text, sizeof(text), "%ld", bots_stop_count(next));
		value = text;
		next = (short)bots_stop_count(next);
	}
	else
	{
		short count = kind == AE_BOT_SKILL ? BOT_SKILLS : kind == AE_BOT_SHOOTING ? BOT_PARTS :
			kind == AE_BOT_MOVEMENT ? BOT_MOVEMENTS : 2;

		next = ae_value_step(count, current, direction);
		value = kind == AE_BOT_SKILL ? bot_skill_values[next] : kind == AE_BOT_SHOOTING || kind == AE_BOT_MOVEMENT ?
			bot_part_values[next] : kind == AE_BOT_PLAYFIGHT ? on_off[next] : bot_teams_values[next];
	}
	if (next == current || !config_write(settings[kind], value))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return 0;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("%s: %s %s", log_prefix, logged[kind], ae_bot_setting_text(kind));
	return 1;
}

int ae_bot_count_typing_ok(const char *typed)
{
	int length = 0;

	if (!typed)
		return 0;
	for (; *typed; typed++, length++)
		if (*typed < '0' || *typed > '9' || length >= 3)
			return 0;
	return 1;
}

int ae_bot_count_typed(const char *typed, char *value, size_t size)
{
	long count = 0;
	const char *at;

	if (!value || !size || !ae_bot_count_typing_ok(typed) || !typed[0])
		return 0;
	for (at = typed; *at; at++)
		count = count * 10 + (*at - '0');
	if (count > BOTS_SETTING_MAXIMUM)
		count = BOTS_SETTING_MAXIMUM;
	snprintf(value, size, "%ld", count);
	return 1;
}

int ae_bot_setting_typed(const char *typed, const char *log_prefix)
{
	char value[8];

	if (!ae_bot_count_typed(typed, value, sizeof(value)) || !config_write(settings[AE_BOT_COUNT], value))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return 0;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("%s: %s %s", log_prefix, logged[AE_BOT_COUNT], ae_bot_setting_text(AE_BOT_COUNT));
	return 1;
}
