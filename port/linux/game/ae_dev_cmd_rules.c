/*
AE_DEV_CMD_RULES.C

DEV MODE's command grammar (ae_dev_cmd_rules.h).
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_dev_cmd_rules.h"

#define TOKENS (AE_DEV_CMD_WORDS + 2)

struct command { const char *name, *usage; int bare; };

/* in enum ae_dev_cmd_id's order */
static struct command const commands[AE_DEV_CMD_COUNT] =
{
	{ "", "", 0 },
	{ "give", "give <weapon>|all", 1 },
	{ "spawn", "spawn <vehicle> [here|ahead|<spot>]", 1 },
	{ "bots", "bots <0-127> [even|against] [recruit|easy|normal|heroic|legendary|demon]", 1 },
	{ "powerup", "powerup <item> [now]", 1 },
	{ "respawn", "respawn [me|all]", 1 },
	{ "kill", "kill [me|all|bots]", 1 },
	{ "end", "end round|game", 1 },
	{ "score", "score red|blue|me <0-9999>", 1 },
	{ "time", "time <seconds>|<m:ss>", 1 },
	{ "goto", "goto <spot> [facing <spot>] | start <n> | <x> <y> <z>", 1 },
	{ "gametype", "gametype <name>", 1 },
	{ "option", "option <row> <value>", 1 },
	{ "mark", "mark <text>", 1 },
	{ "shot", "shot", 1 },
	{ "help", "help [command]", 0 },
	{ "list", "list weapons|vehicles|items|spots|options", 1 },
	{ "view", "view bots|spawns|items|net [on|off]", 1 },
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
	{ "seat", "seat [driver|gunner|passenger]", 1 },
	{ "ttk", "ttk [on|off]", 1 },
	{ "dummy", "dummy [<n>] [here|<distance>] | dummy clear", 1 },
	{ "hud", "hud | hud <i> <dx> <dy> [<scale>] | hud <i> hide|show | hud reset [<i>] | hud save", 1 },
};

static const char *const skills[] = { "recruit", "easy", "normal", "heroic", "legendary", "demon", NULL };
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
	if (client)
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
		bad = n < 1 || n > 2 || (n == 2 && !same_text(cmd->word[1], "now"));
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
	case AE_DEV_CMD_SEAT:
	{
		static const char *const kinds[] = { "driver", "gunner", "passenger", NULL };

		bad = n > 1 || (n == 1 && !one_of(cmd->word[0], kinds));
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
		if (n == 1 && same_text(cmd->word[0], "clear"))
			break;
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

		bad = n < 1 || n > 2 || !one_of(cmd->word[0], what) || (n == 2 && !one_of(cmd->word[1], state));
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
		out->id == AE_DEV_CMD_AGENT;
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
			(out->id == AE_DEV_CMD_AGENT && i <= first + 3))
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
