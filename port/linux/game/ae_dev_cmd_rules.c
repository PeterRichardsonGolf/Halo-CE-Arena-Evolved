/*
AE_DEV_CMD_RULES.C

DEV MODE's command grammar (ae_dev_cmd_rules.h).
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_dev_cmd_rules.h"
#include "ae_weapon_tune_rules.h"

#define TOKENS (AE_DEV_CMD_WORDS + 2)

struct command { const char *name, *usage; int bare; };

/* in enum ae_dev_cmd_id's order */
static struct command const commands[AE_DEV_CMD_COUNT] =
{
	{ "", "", 0 },
	{ "give", "give <weapon>|all", 1 },
	{ "spawn", "spawn <vehicle> [here|ahead|door|passdoor|<spot>]", 1 },
	{ "bots", "bots <0-127> [even|against] [recruit|easy|normal|heroic|elite|legendary|demon]", 1 },
	{ "powerup", "powerup <item> [now] [<player>]", 1 },
	{ "respawn", "respawn [me|all]", 1 },
	{ "kill", "kill [me|all|bots]", 1 },
	{ "end", "end round|game", 1 },
	{ "score", "score red|blue|me <0-9999>", 1 },
	{ "time", "time <seconds>|<m:ss>", 1 },
	{ "goto", "goto <spot> [facing <spot>] | start <n> | <x> <y> <z> [facing <degrees>]", 1 },
	{ "gametype", "gametype <name>", 1 },
	{ "option", "option <row> <value>", 1 },
	{ "mark", "mark <text>", 1 },
	{ "shot", "shot", 1 },
	{ "help", "help [command]", 0 },
	{ "list", "list weapons|vehicles|items|spots|options", 1 },
	{ "view", "view bots [on|off|<n>|aim|enemy|own] | view spawns|items|net [on|off]", 1 },
	{ "reset", "reset", 1 },
	{ "config", "config <key> <value>", 1 },
	{ "agent", "agent check <1-3> pass|fail|look|waiting [reason] | agent say <text> | agent clear", 1 },
	{ "viewmodel", "viewmodel [<x> <y> <z> <scale>|place <right> <up> <forward> <yaw> <pitch> <roll> <scale>|probe|save|clear]", 1 },
	{ "fireteam", "fireteam follow|regroup|attack|defend|watch|go|clear|status | fireteam recruit [<bot>]", 1 },
	{ "round", "round end|next|live", 1 },
	{ "zone", "zone now|small|off | zone look a|b", 1 },
	{ "down", "down <player> [here]", 1 },
	{ "revive", "revive <player>", 1 },
	{ "god", "god [me|<player>|off]", 1 },
	{ "seat", "seat <driver|gunner|passenger> [<player>] | seat", 1 },
	{ "ride", "ride [warthog|ghost|scorpion] [circle|line]", 1 },
	{ "ttk", "ttk [on|off]", 1 },
	{ "dummy", "dummy [<n>] [here|<distance>] [mate] | dummy walk here|<x> <y> <z> [to <x> <y> <z>] | dummy reach [<n>] | dummy clear", 1 },
	{ "hud", "hud | hud <i> <dx> <dy> [<scale>] | hud <i> hide|show | hud reset [<i>] | hud save", 1 },
	{ "tune", "tune | tune <key> <value> | tune save | tune reset", 1 },
	{ "call", "call ride", 1 },
	{ "roundcard", "roundcard won|lost|tied [reason]", 1 },
	{ "flag", "flag ours|theirs here|ahead|stand|carry|home", 1 },
	{ "record_medals", "record_medals <0-118> [<seed>]", 1 },
	{ "lobby_fill", "lobby_fill <1-4>", 1 },
	{ "motd_reset", "motd_reset", 1 },
	{ "motd_offline", "motd_offline", 1 },
	{ "hitch", "hitch <50-5000> [after <1-600>]", 1 },
	{ "join_lan", "join_lan", 1 },
	{ "menu", "menu settings [player <1-4>] find [<text>] | menu game_options <filter> [change <option>=<value> ...] | menu tournament [filter casual|comp] [focus <id>] | menu matchmaking [online] [focus <id>] | menu career [player <1-4>] [focus <id>] | menu campaign [seat | players <2-4>] [error] | menu coop [players <1-4>] | menu lan | menu playlist [edit <name>] | menu browser | menu gametype | menu tab | menu credits | menu network [reached|unreached|unknown] | menu bots [player <1-4>] [focus <name>] [sort games|last|record]", 1 },
	{ "botskill", "botskill [<bot> <shooting> <movement>|<bot> clear|clear]", 1 },
	{ "social", "social exp <id>|all <0-99999> | reset [<id>|all] | match <id> <0-4999>|tier <1-19> | mode <id> auto|<skill> | roster <id> [seed <n>] | pending <id> [crashed] | maps hide smp|<file>|none | calibration <profile|me> unset|new|some|alot | start <id> [map <name>] [seed <n>] [variant br|evo|magnum|none] [now] | postgame win|tie|loss|rankup|place <p> <n> [<id>] [exp <n>] | carnage <id> [seed <n>] [players <1-4>] [cursor <name>]  (each: [player <1-4>] where it applies)", 1 },
	{ "botstyle", "botstyle [<bot> <style> [<weapon>]|<bot> clear|clear]", 1 },
	{ "botrecord", "botrecord [demo [seed <n>] [bots <1-60>] | demo off | set <name> <field> <value> ... | page <name> [game] | forget] [player <1-4>]", 1 },
	{ "playlists", "playlists demo|forget|real", 1 },
};

static const char *const skills[] = { "recruit", "easy", "normal", "heroic", "elite", "legendary", "demon", NULL };
static const char *const teams[] = { "even", "against", NULL };

struct token
{
	char text[AE_DEV_CMD_WORD];
	const char *start;
	int too_long;
};

static int same_text(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	}
	return *a == *b;
}

static int one_of(const char *word, const char *const *list)
{
	int i;

	for (i = 0; list[i]; i++)
	{
		if (same_text(word, list[i]))
			return 1;
	}
	return 0;
}

/* splits line into up to max tokens (double quotes group words); the number of tokens, max + 1 when there were more */
static int tokenize(const char *line, struct token *tokens, int max)
{
	int count = 0;
	const char *p = line;

	for (;;)
	{
		int quoted, length = 0;

		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p || *p == '\r' || *p == '\n')
			break;
		if (count == max)
			return max + 1;
		tokens[count].start = p;
		tokens[count].too_long = 0;
		quoted = *p == '"';
		if (quoted)
			p++;
		while (*p && *p != '\r' && *p != '\n' && (quoted ? *p != '"' : (*p != ' ' && *p != '\t')))
		{
			if (length < AE_DEV_CMD_WORD - 1)
				tokens[count].text[length++] = *p;
			else
				tokens[count].too_long = 1;
			p++;
		}
		if (quoted && *p == '"')
			p++;
		tokens[count].text[length] = 0;
		count++;
	}
	return count;
}

/* the line from start to its end, trimmed, one pair of surrounding quotes taken off, into out */
static void rest_of(const char *start, char *out, int size)
{
	int length = 0;
	const char *end = start + strlen(start);

	while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
		end--;
	if (end - start >= 2 && *start == '"' && end[-1] == '"' && !memchr(start + 1, '"', end - start - 2))
	{
		start++;
		end--;
	}
	while (start < end && length < size - 1)
		out[length++] = *start++;
	out[length] = 0;
}

static int whole_number(const char *text, long low, long high, long *value)
{
	long n = 0;
	const char *p = text;

	if (!*p)
		return 0;
	for (; *p; p++)
	{
		if (*p < '0' || *p > '9')
			return 0;
		n = n * 10 + (*p - '0');
		if (n > high)
			return 0;
	}
	if (n < low)
		return 0;
	*value = n;
	return 1;
}

static int real_number(const char *text, float *value)
{
	char *end;
	double n;

	if (!*text)
		return 0;
	n = strtod(text, &end);
	if (*end || n != n || n > 1.0e6 || n < -1.0e6)
		return 0;
	*value = (float)n;
	return 1;
}

int ae_dev_cmd_seconds(const char *text, long *seconds)
{
	const char *colon = strchr(text, ':');
	long minutes, secs;
	char head[16];

	if (!colon)
		return whole_number(text, 0, 9999, seconds);
	if (colon - text < 1 || colon - text >= (long)sizeof head || strlen(colon + 1) != 2)
		return 0;
	memcpy(head, text, colon - text);
	head[colon - text] = 0;
	if (!whole_number(head, 0, 166, &minutes) || !whole_number(colon + 1, 0, 59, &secs) || minutes * 60 + secs > 9999)
		return 0;
	*seconds = minutes * 60 + secs;
	return 1;
}

int ae_dev_cmd_gate(int id, int setting, int client)
{
	if (id == AE_DEV_CMD_HELP)
		return AE_DEV_CMD_ALLOWED;
	/* (hitch stops only this machine: a client may, as the TEST RUN's joiner does) */
	if (client && id != AE_DEV_CMD_HITCH)
		return AE_DEV_CMD_CLIENT;
	return setting ? AE_DEV_CMD_ALLOWED : AE_DEV_CMD_SETTING_OFF;
}

const char *ae_dev_cmd_gate_text(int gate)
{
	switch (gate)
	{
	case AE_DEV_CMD_SETTING_OFF:
		return "DEV MODE is off on this PC (system.dev_mode)";
	case AE_DEV_CMD_CLIENT:
		return "DEV MODE: only the host runs dev commands in a network game";
	default:
		return "";
	}
}

const char *ae_dev_cmd_name(int id)
{
	return id > AE_DEV_CMD_NONE && id < AE_DEV_CMD_COUNT ? commands[id].name : "";
}

const char *ae_dev_cmd_usage(int id)
{
	return id > AE_DEV_CMD_NONE && id < AE_DEV_CMD_COUNT ? commands[id].usage : "";
}

int ae_dev_cmd_find(const char *word)
{
	int id;

	for (id = AE_DEV_CMD_NONE + 1; id < AE_DEV_CMD_COUNT; id++)
	{
		if (same_text(word, commands[id].name))
			return id;
	}
	return AE_DEV_CMD_NONE;
}

int ae_dev_cmd_complete(const char *prefix, const char **out, int max)
{
	int id, found = 0;
	size_t length = strlen(prefix);

	for (id = AE_DEV_CMD_NONE + 1; id < AE_DEV_CMD_COUNT && found < max; id++)
	{
		size_t i;

		if (!commands[id].bare)
			continue;
		for (i = 0; i < length; i++)
		{
			if (tolower((unsigned char)prefix[i]) != commands[id].name[i])
				break;
		}
		if (i == length)
			out[found++] = commands[id].name;
	}
	return found;
}

/* the argument checks: "" when the command can run, else the usage line in error */
static void check(struct ae_dev_cmd *cmd)
{
	int n = cmd->words, bad = 0, i;

	switch (cmd->id)
	{
	case AE_DEV_CMD_GIVE:
		bad = n != 1;
		break;
	case AE_DEV_CMD_SPAWN:
		bad = n < 1 || n > 2;
		break;
	case AE_DEV_CMD_BOTS:
	{
		int team = 0, skill = 0;

		bad = n < 1 || n > 3 || !whole_number(cmd->word[0], 0, 127, &cmd->number);
		for (i = 1; !bad && i < n; i++)
		{
			if (one_of(cmd->word[i], teams) && !team)
				team = 1;
			else if (one_of(cmd->word[i], skills) && !skill)
				skill = 1;
			else
				bad = 1;
		}
		break;
	}
	case AE_DEV_CMD_POWERUP:
		/* (powerup <item> [now] [<player>]: the second word is "now" or the player) */
		bad = n < 1 || n > 3 || (n == 3 && !same_text(cmd->word[1], "now"));
		break;
	case AE_DEV_CMD_RESPAWN:
	{
		static const char *const who[] = { "me", "all", NULL };

		bad = n > 1 || (n == 1 && !one_of(cmd->word[0], who));
		break;
	}
	case AE_DEV_CMD_KILL:
	{
		static const char *const who[] = { "me", "all", "bots", NULL };

		bad = n > 1 || (n == 1 && !one_of(cmd->word[0], who));
		break;
	}
	case AE_DEV_CMD_END:
	{
		static const char *const what[] = { "round", "game", NULL };

		bad = n != 1 || !one_of(cmd->word[0], what);
		break;
	}
	case AE_DEV_CMD_SCORE:
	{
		static const char *const who[] = { "red", "blue", "me", NULL };

		bad = n != 2 || !one_of(cmd->word[0], who) || !whole_number(cmd->word[1], 0, 9999, &cmd->number);
		break;
	}
	case AE_DEV_CMD_TIME:
		bad = n != 1 || !ae_dev_cmd_seconds(cmd->word[0], &cmd->number);
		break;
	case AE_DEV_CMD_GOTO:
		if (n == 2 && same_text(cmd->word[0], "start"))
			bad = !whole_number(cmd->word[1], 0, 255, &cmd->number);
		else if (n == 3 && same_text(cmd->word[1], "facing"))
			bad = same_text(cmd->word[0], "start");
		else if (n == 5 && same_text(cmd->word[3], "facing"))
		{
			float degrees = 0.0f;

			/* (goto <x> <y> <z> facing <degrees>: the way the player faces, east 0, north 90; in position[3]'s place:
			number) */
			bad = !real_number(cmd->word[0], &cmd->position[0]) || !real_number(cmd->word[1], &cmd->position[1]) ||
				!real_number(cmd->word[2], &cmd->position[2]) || !real_number(cmd->word[4], &degrees);
			cmd->number = (long)degrees;
			cmd->has_position = !bad;
		}
		else if (n == 3)
		{
			bad = !real_number(cmd->word[0], &cmd->position[0]) || !real_number(cmd->word[1], &cmd->position[1]) ||
				!real_number(cmd->word[2], &cmd->position[2]);
			cmd->has_position = !bad;
		}
		else
			bad = n != 1 || same_text(cmd->word[0], "start");
		break;
	case AE_DEV_CMD_GAMETYPE:
	case AE_DEV_CMD_MARK:
		bad = !cmd->rest[0];
		break;
	case AE_DEV_CMD_CONFIG:
		bad = n < 2 || !cmd->rest[0];
		break;
	case AE_DEV_CMD_OPTION:
		bad = n != 2;
		break;
	case AE_DEV_CMD_AGENT:
	{
		static const char *const verdicts[] = { "pass", "fail", "look", "waiting", NULL };

		if (n >= 1 && same_text(cmd->word[0], "check"))
			bad = n < 3 || !whole_number(cmd->word[1], 1, 3, &cmd->number) || !one_of(cmd->word[2], verdicts);
		else if (n >= 1 && same_text(cmd->word[0], "say"))
			bad = !cmd->rest[0];
		else
			bad = n != 1 || !same_text(cmd->word[0], "clear");
		break;
	}
	case AE_DEV_CMD_FIRETEAM:
	{
		static const char *const orders[] = { "follow", "regroup", "attack", "defend", "watch", "go", "clear", "status", NULL };

		if (n >= 1 && same_text(cmd->word[0], "recruit"))
			bad = n > 2 || (n == 2 && !whole_number(cmd->word[1], 1, 127, &cmd->number));
		else
			bad = n != 1 || !one_of(cmd->word[0], orders);
		break;
	}
	case AE_DEV_CMD_SHOT:
	case AE_DEV_CMD_RESET:
		bad = n != 0;
		break;
	case AE_DEV_CMD_ROUND:
	{
		static const char *const what[] = { "end", "next", "live", NULL };

		bad = n != 1 || !one_of(cmd->word[0], what);
		break;
	}
	case AE_DEV_CMD_ZONE:
	{
		static const char *const what[] = { "now", "small", "off", NULL };
		static const char *const looks[] = { "a", "b", NULL };

		bad = !((n == 1 && one_of(cmd->word[0], what)) ||
			(n == 2 && same_text(cmd->word[0], "look") && one_of(cmd->word[1], looks)));
		break;
	}
	case AE_DEV_CMD_DOWN:
		bad = n < 1 || n > 2 || (n == 2 && !same_text(cmd->word[1], "here"));
		break;
	case AE_DEV_CMD_REVIVE:
		bad = n != 1;
		break;
	case AE_DEV_CMD_GOD:
		bad = n > 1;
		break;
	case AE_DEV_CMD_PLAYLISTS:
		bad = n != 1 || !(same_text(cmd->word[0], "demo") || same_text(cmd->word[0], "forget") || same_text(cmd->word[0], "real"));
		break;
	case AE_DEV_CMD_BOTRECORD:
	{
		/* (0.9.4: number = the local player 0-3; demo: seed, and position[0] the bots (0: the default)) */
		static const char *const fields[] = { "games", "wins", "ties", "kills", "deaths", "with", "vs", NULL };
		long player = 1, value = 0;
		int at = n, each;

		cmd->seed = 0;
		cmd->position[0] = 0.0f;
		if (n >= 2 && same_text(cmd->word[n - 2], "player"))
		{
			bad = !whole_number(cmd->word[n - 1], 1, 4, &player);
			at = n - 2;
		}
		if (bad || at == 0)
		{
			cmd->number = player - 1;
			break;
		}
		if (same_text(cmd->word[0], "demo"))
		{
			if (at == 2 && same_text(cmd->word[1], "off"))
				;
			else
				for (each = 1; !bad && each < at; each += 2)
				{
					if (each + 1 >= at)
						bad = 1;
					else if (same_text(cmd->word[each], "seed"))
					{
						bad = !whole_number(cmd->word[each + 1], 0, 99999999L, &value);
						cmd->seed = (unsigned long)value;
					}
					else if (same_text(cmd->word[each], "bots"))
					{
						bad = !whole_number(cmd->word[each + 1], 1, 60, &value);
						cmd->position[0] = (float)value;
					}
					else
						bad = 1;
				}
		}
		else if (same_text(cmd->word[0], "forget"))
			bad = at != 1;
		else if (same_text(cmd->word[0], "page"))
			bad = at < 2 || at > 3 || (at == 3 && !same_text(cmd->word[2], "game"));
		else if (same_text(cmd->word[0], "set"))
		{
			bad = at < 4 || (at - 2) % 2 != 0;
			for (each = 2; !bad && each + 1 < at; each += 2)
			{
				bad = !one_of(cmd->word[each], fields);
				if (!bad && same_text(cmd->word[each], "vs"))
				{
					char const *dash = strchr(cmd->word[each + 1], '-');
					char left[16];

					bad = !dash || dash == cmd->word[each + 1] || (size_t)(dash - cmd->word[each + 1]) >= sizeof(left);
					if (!bad)
					{
						memcpy(left, cmd->word[each + 1], (size_t)(dash - cmd->word[each + 1]));
						left[dash - cmd->word[each + 1]] = 0;
						bad = !whole_number(left, 0, 99999, &value) || !whole_number(dash + 1, 0, 99999, &value);
					}
				}
				else if (!bad)
					bad = !whole_number(cmd->word[each + 1], 0, 9999999L, &value);
			}
		}
		else
			bad = 1;
		cmd->number = player - 1;
		break;
	}
	case AE_DEV_CMD_BOTSTYLE:
	{
		/* (<bot> <style> [<weapon>]: the style's and the weapon's numbers in position[0] and [1]; <bot> clear; clear;
		nothing: the list. The names' order is ae_bots_style_rules.h's) */
		static const char *const styles[] = { "steady", "pusher", "anchor", "hopper", "careful", NULL };
		static const char *const weapons[] = { "none", "assault_rifle", "battle_rifle", "shotgun", "sniper", "pistol",
			"rockets", "plasma_rifle", "needler", NULL };
		int index;

		cmd->number = 0;
		cmd->position[0] = cmd->position[1] = 0.0f;
		if (n == 0 || (n == 1 && same_text(cmd->word[0], "clear")))
			break;
		bad = n < 2 || n > 3 || !whole_number(cmd->word[0], 1, AE_DEV_CMD_VIEW_BOTS_MOST, &cmd->number);
		if (!bad && n == 2 && same_text(cmd->word[1], "clear"))
			break;
		for (index = 0; !bad && styles[index] && !same_text(cmd->word[1], styles[index]); index++)
			;
		bad = bad || !styles[index];
		cmd->position[0] = (float)index;
		if (!bad && n == 3)
		{
			for (index = 0; weapons[index] && !same_text(cmd->word[2], weapons[index]); index++)
				;
			bad = !weapons[index];
			cmd->position[1] = (float)index;
		}
		break;
	}
	case AE_DEV_CMD_BOTSKILL:
	{
		/* (<bot> <shooting> <movement>: the steps 1-7 in position[0] and [1]; <bot> clear; clear; nothing: the list) */
		int step;

		cmd->number = 0;
		if (n == 0 || (n == 1 && same_text(cmd->word[0], "clear")))
			break;
		bad = (n != 2 && n != 3) || !whole_number(cmd->word[0], 1, AE_DEV_CMD_VIEW_BOTS_MOST, &cmd->number);
		if (!bad && n == 2)
			bad = !same_text(cmd->word[1], "clear");
		for (step = 0; !bad && n == 3 && step < 2; step++)
		{
			int index;

			for (index = 0; skills[index] && !same_text(cmd->word[1 + step], skills[index]); index++)
				;
			bad = !skills[index];
			cmd->position[step] = (float)(index + 1);
		}
		break;
	}
	case AE_DEV_CMD_SEAT:
	{
		static const char *const kinds[] = { "driver", "gunner", "passenger", NULL };

		bad = n > 2 || (n >= 1 && !one_of(cmd->word[0], kinds));
		break;
	}
	case AE_DEV_CMD_RIDE:
	{
		static const char *const kinds[] = { "warthog", "ghost", "scorpion", NULL };
		static const char *const paths[] = { "circle", "line", NULL };

		bad = n > 2 || (n >= 1 && !one_of(cmd->word[0], kinds)) ||
			(n == 2 && !one_of(cmd->word[1], paths));
		break;
	}
	case AE_DEV_CMD_FLAG:
	{
		static const char *const sides[] = { "ours", "theirs", NULL };
		static const char *const where[] = { "here", "ahead", "stand", "carry", "home", NULL };

		bad = n != 2 || !one_of(cmd->word[0], sides) || !one_of(cmd->word[1], where);
		break;
	}
	case AE_DEV_CMD_CALL:
	{
		static const char *const calls[] = { "ride", NULL };

		bad = n != 1 || !one_of(cmd->word[0], calls);
		break;
	}
	case AE_DEV_CMD_ROUNDCARD:
	{
		static const char *const verdicts[] = { "won", "lost", "tied", NULL };

		bad = n < 1 || n > 2 || !one_of(cmd->word[0], verdicts);
		break;
	}
	case AE_DEV_CMD_RECORD_MEDALS:
	{
		long seed = 0;

		/* (the kinds: AE_MEDAL_COUNT at most, ae_medal_ids.h's 118) */
		bad = n < 1 || n > 2 || !whole_number(cmd->word[0], 0, 118, &cmd->number) ||
			(n == 2 && !whole_number(cmd->word[1], 0, 99999999L, &seed));
		cmd->seed = (unsigned long)seed;
		break;
	}
	case AE_DEV_CMD_LOBBY_FILL:
		bad = n != 1 || !whole_number(cmd->word[0], 1, 4, &cmd->number);
		break;
	case AE_DEV_CMD_MOTD_RESET:
	case AE_DEV_CMD_MOTD_OFFLINE:
	case AE_DEV_CMD_JOIN_LAN:
		bad = n != 0;
		break;
	case AE_DEV_CMD_MENU:
	{
		int each;

		if (n >= 1 && same_text(cmd->word[0], "settings"))
		{
			/* (0.9.3 menu settings [player <1-4>] find [<text>]) */
			long player = 1;
			int at = 1;

			bad = n < 2;
			if (!bad && same_text(cmd->word[1], "player"))
			{
				bad = n < 4 || !whole_number(cmd->word[2], 1, 4, &player);
				at = 3;
			}
			bad = bad || n < at + 1 || !same_text(cmd->word[at], "find");
			cmd->number = player - 1;
			break;
		}
		/* (0.9.3 menu tournament [focus <id>]; menu matchmaking [social|online] [focus <id>] (social: the old form of
		tournament); menu career [player <1-4>] [focus <id>]: the tournament's screens staged) */
		if (n >= 1 && same_text(cmd->word[0], "tournament"))
		{
			int at = 1, ok = 1;

			/* ([filter casual|comp] [focus <id>], either or both) */
			while (at < n && ok)
			{
				if (at + 1 < n && (same_text(cmd->word[at], "focus") || (same_text(cmd->word[at], "filter") &&
					(same_text(cmd->word[at + 1], "casual") || same_text(cmd->word[at + 1], "comp")))))
					at += 2;
				else
					ok = 0;
			}
			bad = !ok || at != n;
			break;
		}
		if (n >= 1 && same_text(cmd->word[0], "matchmaking"))
		{
			int at = 1;

			if (n >= 2 && (same_text(cmd->word[1], "social") || same_text(cmd->word[1], "online")))
				at = 2;
			bad = !(n == at || (n == at + 2 && same_text(cmd->word[at], "focus")));
			break;
		}
		if (n >= 1 && same_text(cmd->word[0], "career"))
		{
			long player = 1;
			int at = 1;

			if (n >= 3 && same_text(cmd->word[1], "player"))
			{
				bad = !whole_number(cmd->word[2], 1, 4, &player);
				at = 3;
			}
			bad = bad || !(n == at || (n == at + 2 && same_text(cmd->word[at], "focus")));
			cmd->number = player - 1;
			break;
		}
		/* (0.9.4 menu campaign [seat | players <2-4>] [error]: CAMPAIGN, the players seated as guests (seat: two), the co-op start's error dialog; number = the players) */
		if (n >= 1 && same_text(cmd->word[0], "campaign"))
		{
			int seats = 0, errors = 0, counted = 0;

			cmd->number = 0;
			for (each = 1; !bad && each < n; each++)
			{
				if (same_text(cmd->word[each], "seat"))
					bad = ++seats > 1;
				else if (same_text(cmd->word[each], "error"))
					bad = ++errors > 1;
				else if (same_text(cmd->word[each], "players") && !counted && each + 1 < n &&
					cmd->word[each + 1][0] >= '2' && cmd->word[each + 1][0] <= '4' && !cmd->word[each + 1][1])
				{
					counted = 1;
					cmd->number = cmd->word[each + 1][0] - '0';
					each++;
				}
				else
					bad = 1;
			}
			bad = bad || (seats && counted);
			break;
		}
		/* (0.9.4 menu coop [players <1-4>]: the CO-OP LOBBY hosted on this PC (LAN), the players of this PC in it; number = the players) */
		if (n >= 1 && same_text(cmd->word[0], "coop"))
		{
			cmd->number = 1;
			bad = !(n == 1 || (n == 3 && same_text(cmd->word[1], "players") && cmd->word[2][0] >= '1' &&
				cmd->word[2][0] <= '4' && !cmd->word[2][1]));
			if (!bad && n == 3)
				cmd->number = cmd->word[2][0] - '0';
			break;
		}
		/* (0.9.4 menu tab; menu credits; menu network [reached|unreached|unknown]: the TAB menu's two pages opened, the network one with
		made-up facts when a case is named (the real ones need a master to answer or not)) */
		if (n >= 1 && (same_text(cmd->word[0], "credits") || same_text(cmd->word[0], "tab") || same_text(cmd->word[0], "lan") ||
			same_text(cmd->word[0], "gametype") || same_text(cmd->word[0], "browser")))
		{
			bad = n != 1;
			break;
		}
		/* (0.9.4 menu playlist [edit <name words>]: the PLAYLISTS board; edit opens that playlist's games) */
		if (n >= 1 && same_text(cmd->word[0], "playlist"))
		{
			bad = !(n == 1 || (n >= 3 && same_text(cmd->word[1], "edit")));
			break;
		}
		if (n >= 1 && same_text(cmd->word[0], "network"))
		{
			static const char *const cases[] = { "reached", "unreached", "unknown", NULL };

			bad = !(n == 1 || (n == 2 && one_of(cmd->word[1], cases)));
			break;
		}
		/* (0.9.4 menu bots [player <1-4>] [focus <name>] [sort games|last|record]: CAREER's BOTS list; number = the local
		player, seed = the sort (AE_BOT_RECORD_SORT_*)) */
		if (n >= 1 && same_text(cmd->word[0], "bots"))
		{
			static const char *const sorts[] = { "games", "last", "record", NULL };
			long player = 1;

			cmd->seed = 0;
			for (each = 1; !bad && each < n; each += 2)
			{
				if (each + 1 >= n)
					bad = 1;
				else if (same_text(cmd->word[each], "player"))
					bad = !whole_number(cmd->word[each + 1], 1, 4, &player);
				else if (same_text(cmd->word[each], "sort"))
				{
					bad = !one_of(cmd->word[each + 1], sorts);
					cmd->seed = same_text(cmd->word[each + 1], "last") ? 1ul : same_text(cmd->word[each + 1], "record") ? 2ul : 0ul;
				}
				else if (!same_text(cmd->word[each], "focus"))
					bad = 1;
			}
			cmd->number = player - 1;
			break;
		}
		/* (0.9.3 game_options <filter> then change <option>=<value> pairs, up to three) */
		bad = n < 2 || !same_text(cmd->word[0], "game_options") || (n - 2) % 2 != 0;
		for (each = 2; !bad && each < n; each += 2)
			bad = !same_text(cmd->word[each], "change") || !strchr(cmd->word[each + 1], '=');
		break;
	}
	case AE_DEV_CMD_SOCIAL:
	{
		/* (0.9.3 the offline social career; number = the local player 0-3, seed = the amount: exp, points, tier or the
		roster's seed) */
		static const char *const modes[] = { "auto", "recruit", "easy", "normal", "heroic", "elite", "legendary", "demon", NULL };
		long player = 1, amount = 0;
		int at = n;

		bad = n < 1;
		if (bad)
			break;
		/* ("player <1-4>" may end any of them that takes it) */
		if (n >= 3 && same_text(cmd->word[n - 2], "player") && !same_text(cmd->word[0], "maps") && !same_text(cmd->word[0], "mode"))
		{
			bad = !whole_number(cmd->word[n - 1], 1, 4, &player);
			at = n - 2;
		}
		if (bad)
			break;
		if (same_text(cmd->word[0], "exp"))
			bad = at != 3 || !whole_number(cmd->word[2], 0, 99999, &amount);
		else if (same_text(cmd->word[0], "reset"))
			bad = at > 2;
		else if (same_text(cmd->word[0], "match"))
		{
			if (at == 4)
				bad = !same_text(cmd->word[2], "tier") || !whole_number(cmd->word[3], 1, 19, &amount);
			else
				bad = at != 3 || !whole_number(cmd->word[2], 0, 4999, &amount);
		}
		else if (same_text(cmd->word[0], "mode"))
			bad = n != 3 || !one_of(cmd->word[2], modes);
		else if (same_text(cmd->word[0], "roster"))
		{
			bad = at != 2 && at != 4;
			if (!bad && at == 4)
				bad = !same_text(cmd->word[2], "seed") || !whole_number(cmd->word[3], 0, 99999999L, &amount);
		}
		else if (same_text(cmd->word[0], "pending"))
			bad = at != 2 && !(at == 3 && same_text(cmd->word[2], "crashed"));
		else if (same_text(cmd->word[0], "start"))
		{
			/* (start <id> [map <name>] [seed <n>] [now]: the pairs and the flag in any order) */
			int each;

			bad = at < 2;
			for (each = 2; !bad && each < at; each++)
			{
				if (same_text(cmd->word[each], "now"))
					continue;
				if (each + 1 >= at)
					bad = 1;
				else if (same_text(cmd->word[each], "seed"))
					bad = !whole_number(cmd->word[++each], 0, 99999999L, &amount);
				else if (same_text(cmd->word[each], "map"))
					each++;
				else
					bad = 1;
			}
		}
		else if (same_text(cmd->word[0], "postgame"))
		{
			/* (postgame win|tie|loss|rankup | place <p> <n>, then [<id>] [exp <n>]; position: the place (zero-based) and the
			players of a free for all, the EXP before (-1: the profile's own)) */
			static const char *const kinds[] = { "win", "tie", "loss", "place", "rankup", NULL };
			long place = 1, players = 8, before = -1;
			int each = 2;

			bad = at < 2 || !one_of(cmd->word[1], kinds);
			if (!bad && same_text(cmd->word[1], "place"))
			{
				bad = at < 4 || !whole_number(cmd->word[2], 1, 64, &place) || !whole_number(cmd->word[3], 2, 64, &players) ||
					place > players;
				each = 4;
			}
			if (!bad && each < at && !same_text(cmd->word[each], "exp"))
				each++;
			if (!bad && each < at)
				bad = each + 2 != at || !same_text(cmd->word[each], "exp") || !whole_number(cmd->word[each + 1], 0, 99999, &before);
			cmd->position[0] = (float)(place - 1);
			cmd->position[1] = (float)players;
			cmd->position[2] = (float)before;
		}
		else if (same_text(cmd->word[0], "maps"))
			bad = n != 3 || !same_text(cmd->word[1], "hide");
		else if (same_text(cmd->word[0], "calibration"))
		{
			/* (0.9.4 calibration <profile|me> unset|new|some|alot: the amount is the answer, 0 .. 3) */
			static const char *const answers[] = { "unset", "new", "some", "alot", NULL };
			int each;

			bad = at != 3 || !one_of(cmd->word[2], answers);
			for (each = 0; !bad && answers[each]; each++)
				if (same_text(cmd->word[2], answers[each]))
					amount = each;
		}
		else if (same_text(cmd->word[0], "carnage"))
		{
			/* (0.9.4 carnage <id> [seed <n>] [players <1-4>] [cursor <name>]: position[0] the players, position[1] the
			cursor's word (0: none)) */
			long players = 1;
			int each;

			cmd->position[1] = 0.0f;
			bad = at < 2;
			for (each = 2; !bad && each < at; each += 2)
			{
				if (each + 1 >= at)
					bad = 1;
				else if (same_text(cmd->word[each], "seed"))
					bad = !whole_number(cmd->word[each + 1], 0, 99999999L, &amount);
				else if (same_text(cmd->word[each], "players"))
					bad = !whole_number(cmd->word[each + 1], 1, 4, &players);
				else if (same_text(cmd->word[each], "cursor"))
					cmd->position[1] = (float)(each + 1);
				else
					bad = 1;
			}
			cmd->position[0] = (float)players;
		}
		else
			bad = 1;
		cmd->number = player - 1;
		cmd->seed = (unsigned long)amount;
		break;
	}
	case AE_DEV_CMD_HITCH:
		/* (0.9.0 TEST RUN: this machine's game stops that many milliseconds once, a hitch for CLOCK SLEW's check) */
	{
		long after = 0;

		bad = (n != 1 && n != 3) || !whole_number(cmd->word[0], 50, 5000, &cmd->number) ||
			(n == 3 && (!same_text(cmd->word[1], "after") || !whole_number(cmd->word[2], 1, 600, &after)));
		cmd->seed = (unsigned long)after;
		break;
	}
	case AE_DEV_CMD_TTK:
	{
		static const char *const state[] = { "on", "off", NULL };

		bad = n > 1 || (n == 1 && !one_of(cmd->word[0], state));
		break;
	}
	case AE_DEV_CMD_DUMMY:
	{
		float distance;
		int at = 0;

		cmd->number = 1;
		cmd->position[0] = 10.0f;
		cmd->position[1] = 0.0f;
		cmd->position[2] = 0.0f;
		if (n == 1 && same_text(cmd->word[0], "clear"))
			break;
		/* (reach [<n>]: the can't-reach report, n walkers of 1 to 4, 4 by default) */
		if (n >= 1 && same_text(cmd->word[0], "reach"))
		{
			cmd->number = 4;
			bad = n > 2 || (n == 2 && !whole_number(cmd->word[1], 1, 4, &cmd->number));
			break;
		}
		/* (walk here|<x> <y> <z> [to <x> <y> <z>]: one walking dummy) */
		if (n >= 1 && same_text(cmd->word[0], "walk"))
		{
			int goal_at = n >= 2 && same_text(cmd->word[1], "here") ? 2 : 4;

			if (goal_at == 4)
				bad = n < 4 || !real_number(cmd->word[1], &cmd->position[0]) ||
					!real_number(cmd->word[2], &cmd->position[1]) || !real_number(cmd->word[3], &cmd->position[2]);
			else
				cmd->position[0] = cmd->position[1] = cmd->position[2] = 0.0f;
			cmd->has_position = goal_at == 4 && !bad;
			if (!bad && n > goal_at)
			{
				bad = n != goal_at + 4 || !same_text(cmd->word[goal_at], "to") ||
					!real_number(cmd->word[goal_at + 1], &cmd->goal[0]) ||
					!real_number(cmd->word[goal_at + 2], &cmd->goal[1]) ||
					!real_number(cmd->word[goal_at + 3], &cmd->goal[2]);
				cmd->has_goal = !bad;
			}
			else if (!bad)
				bad = n != goal_at;
			break;
		}
		/* (a last word "mate": on the host's team) */
		if (n >= 2 && same_text(cmd->word[n - 1], "mate"))
		{
			cmd->position[2] = 1.0f;
			n--;
		}
		else if (n == 1 && same_text(cmd->word[0], "mate"))
		{
			cmd->position[2] = 1.0f;
			n = 0;
		}
		/* (a last word "away" (before mate): facing the way you face) */
		if (n >= 2 && same_text(cmd->word[n - 1], "away"))
		{
			cmd->away = 1;
			n--;
		}
		else if (n == 1 && same_text(cmd->word[0], "away"))
		{
			cmd->away = 1;
			n = 0;
		}
		/* (the count first, a whole number of 1 to 8, unless it is left out for here; then here or a distance) */
		if (n >= 1 && !same_text(cmd->word[0], "here"))
		{
			bad = !whole_number(cmd->word[0], 1, 8, &cmd->number);
			at = 1;
		}
		bad = bad || n - at > 1;
		if (!bad && n - at == 1)
		{
			if (same_text(cmd->word[at], "here"))
				cmd->position[1] = 1.0f;
			else
			{
				bad = !real_number(cmd->word[at], &distance) || distance < 1.0f || distance > 200.0f;
				cmd->position[0] = distance;
			}
		}
		break;
	}
	case AE_DEV_CMD_HUD:
	{
		float number[3];
		int index;

		if (n == 0)
			break;
		cmd->number = -1;
		if (n == 1)
		{
			bad = !(same_text(cmd->word[0], "save") || same_text(cmd->word[0], "reset"));
			break;
		}
		if (same_text(cmd->word[0], "reset"))
		{
			bad = n != 2 || !whole_number(cmd->word[1], 0, 31, &cmd->number);
			break;
		}
		/* (an element's number, then hide, show or the numbers: dx, dy and an optional scale) */
		bad = !whole_number(cmd->word[0], 0, 31, &cmd->number);
		if (bad)
			break;
		if (n == 2)
			bad = !(same_text(cmd->word[1], "hide") || same_text(cmd->word[1], "show"));
		else if (n == 3 || n == 4)
		{
			for (index = 0; index < n - 1 && !bad; index++)
				bad = !real_number(cmd->word[1 + index], &number[index]);
			if (!bad)
			{
				cmd->position[0] = number[0];
				cmd->position[1] = number[1];
				cmd->position[2] = n == 4 ? number[2] : 0.0f;
			}
		}
		else
			bad = 1;
		break;
	}
	case AE_DEV_CMD_TUNE:
	{
		struct ae_tune_key key;
		struct ae_tune_value value;
		char *letter;

		/* (the key or the word in lower case, as the engine reads them) */
		for (letter = cmd->word[0]; n >= 1 && *letter; letter++)
			*letter = (char)tolower((unsigned char)*letter);
		/* (a key and a number the rules read: whether the engine can hold it is said when it runs) */
		if (n == 1)
			bad = !(same_text(cmd->word[0], "save") || same_text(cmd->word[0], "reset"));
		else if (n == 2)
			bad = !ae_tune_key_parse(cmd->word[0], &key) || !ae_tune_value_parse(key, cmd->word[1], &value);
		else
			bad = n != 0;
		break;
	}
	case AE_DEV_CMD_HELP:
		bad = n > 1 || (n == 1 && ae_dev_cmd_find(cmd->word[0]) == AE_DEV_CMD_NONE);
		break;
	case AE_DEV_CMD_LIST:
	{
		static const char *const what[] = { "weapons", "vehicles", "items", "spots", "options", NULL };

		bad = n != 1 || !one_of(cmd->word[0], what);
		break;
	}
	case AE_DEV_CMD_VIEW:
	{
		static const char *const what[] = { "bots", "spawns", "items", "net", NULL };
		static const char *const state[] = { "on", "off", NULL };
		/* (0.7.9) view bots also takes a bot's number (pin it), aim (unpin), enemy and own (whose bots) */
		static const char *const bots_state[] = { "on", "off", "aim", "enemy", "own", NULL };

		int w, c;

		/* (the words in lower case, as the view reads them) */
		for (w = 0; w < n && w < 2; w++)
			for (c = 0; cmd->word[w][c]; c++)
				cmd->word[w][c] = (char)tolower((unsigned char)cmd->word[w][c]);
		cmd->number = 0;
		bad = n < 1 || n > 2 || !one_of(cmd->word[0], what);
		if (!bad && n == 2 && same_text(cmd->word[0], "bots") && !one_of(cmd->word[1], bots_state))
		{
			char const *digit = cmd->word[1];
			long number = 0;

			for (; *digit >= '0' && *digit <= '9' && number < 1000; digit++)
				number = number * 10 + (*digit - '0');
			bad = *digit != 0 || cmd->word[1][0] == 0 || number < 1 || number > AE_DEV_CMD_VIEW_BOTS_MOST;
			cmd->number = number;
		}
		else if (!bad && n == 2 && !same_text(cmd->word[0], "bots"))
			bad = !one_of(cmd->word[1], state);
		break;
	}
	case AE_DEV_CMD_VIEWMODEL:
	{
		static const char *const what[] = { "save", "clear", "probe", NULL };
		float scale;

		/* (the scale's range is the engine's: ae_view_model_rules.h; a value outside it is refused when it runs) */
		if (n == 8 && !strcmp(cmd->word[0], "place"))
		{
			int index;

			/* (place: the arms and gun as one piece, seven numbers) */
			for (index = 1; index < 8 && !bad; index++)
				bad = !real_number(cmd->word[index], &scale);
		}
		else if (n == 4)
			bad = !real_number(cmd->word[0], &cmd->position[0]) || !real_number(cmd->word[1], &cmd->position[1]) ||
				!real_number(cmd->word[2], &cmd->position[2]) || !real_number(cmd->word[3], &scale) ||
				(cmd->has_position = 1, 0);
		else
			bad = n > 1 || (n == 1 && !one_of(cmd->word[0], what));
		break;
	}
	default:
		bad = 1;
		break;
	}
	if (bad)
		snprintf(cmd->error, sizeof cmd->error, "usage: %s", commands[cmd->id].usage);
}

int ae_dev_cmd_parse(const char *line, int bare, struct ae_dev_cmd *out)
{
	struct token tokens[TOKENS];
	int count, first = 0, i, long_word = 0;
	int takes_rest;

	memset(out, 0, sizeof *out);
	if (!line)
		return 0;
	count = tokenize(line, tokens, TOKENS);
	if (count == 0)
		return 0;
	if (same_text(tokens[0].text, "dev"))
	{
		if (count == 1)
		{
			out->id = AE_DEV_CMD_HELP;
			return 1;
		}
		if (ae_dev_cmd_find(tokens[1].text) == AE_DEV_CMD_NONE)
		{
			out->id = AE_DEV_CMD_HELP;
			snprintf(out->error, sizeof out->error, "no such dev command (dev lists them)");
			return 1;
		}
		first = 1;
	}
	else if (!bare || !commands[ae_dev_cmd_find(tokens[0].text)].bare)
		return 0;
	out->id = ae_dev_cmd_find(tokens[first].text);
	takes_rest = out->id == AE_DEV_CMD_GAMETYPE || out->id == AE_DEV_CMD_MARK || out->id == AE_DEV_CMD_CONFIG ||
		out->id == AE_DEV_CMD_AGENT || out->id == AE_DEV_CMD_MENU;
	if (count > TOKENS && !takes_rest)
	{
		snprintf(out->error, sizeof out->error, "usage: %s", commands[out->id].usage);
		return 1;
	}
	if (count > TOKENS)
		count = TOKENS;
	for (i = first + 1; i < count && out->words < AE_DEV_CMD_WORDS; i++)
	{
		char *w = out->word[out->words++];
		size_t c;

		/* (a long word in a rest-of-line command's text is only cut there: option's key still counts) */
		if (!takes_rest || (out->id == AE_DEV_CMD_CONFIG && i == first + 1) ||
			(out->id == AE_DEV_CMD_AGENT && i <= first + 3) || (out->id == AE_DEV_CMD_MENU && i <= first + 2))
			long_word |= tokens[i].too_long;
		strcpy(w, tokens[i].text);
		for (c = 0; w[c]; c++)
			w[c] = (char)tolower((unsigned char)w[c]);
	}
	if (out->id == AE_DEV_CMD_CONFIG)
	{
		if (count > first + 2)
			rest_of(tokens[first + 2].start, out->rest, sizeof out->rest);
	}
	else if (out->id == AE_DEV_CMD_AGENT)
	{
		/* (say's text after "say"; a check's reason after "check <n> <verdict>") */
		int from = count > first + 1 && same_text(tokens[first + 1].text, "say") ? first + 2 : first + 4;

		if (count > from)
			rest_of(tokens[from].start, out->rest, sizeof out->rest);
	}
	else if (out->id == AE_DEV_CMD_MENU)
	{
		/* (the text after "settings find", or after "settings player <n> find") */
		int from = out->words >= 2 && !strcmp(out->word[1], "player") ? first + 5 : first + 3;

		if (count > from)
			rest_of(tokens[from].start, out->rest, sizeof out->rest);
	}
	else if (takes_rest && count > first + 1)
		rest_of(tokens[first + 1].start, out->rest, sizeof out->rest);
	if (long_word)
	{
		snprintf(out->error, sizeof out->error, "a word is longer than %d letters", AE_DEV_CMD_WORD - 1);
		return 1;
	}
	check(out);
	return 1;
}
