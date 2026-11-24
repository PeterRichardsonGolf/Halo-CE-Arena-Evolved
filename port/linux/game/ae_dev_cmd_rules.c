/*
AE_DEV_CMD_RULES.C

DEV MODE's command grammar (ae_dev_cmd_rules.h).
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_dev_cmd_rules.h"
#include "ae_maps_hub_rules.h"
#include "ae_steam_rules.h"
#include "ae_weapon_tune_rules.h"

/* the medals' keys, in their numbers' order (ae_medals.h) */
static const char *const medal_keys[] =
{
#define AE_MEDAL(name, key, points) key,
#include "ae_medal_ids.h"
#undef AE_MEDAL
};

#define TOKENS (AE_DEV_CMD_WORDS + 2)

struct command { const char *name, *usage; int bare; };

/* in enum ae_dev_cmd_id's order */
static struct command const commands[AE_DEV_CMD_COUNT] =
{
	{ "", "", 0 },
	{ "give", "give <weapon>|all | give <weapon> to <bot|name>", 1 },
	{ "spawn", "spawn <vehicle> [here|ahead|door|passdoor|<spot>]", 1 },
	{ "bots", "bots <0-127> [even|against] [recruit|easy|normal|heroic|elite|legendary|demon]", 1 },
	{ "powerup", "powerup <item> [now] [<player>] | powerup <item> ahead", 1 },
	{ "respawn", "respawn [me|all]", 1 },
	{ "kill", "kill [me|all|bots] | kill <victim> by <killer> | kill me by <bot|name> [after <s> | every <s> [times <n>]] | kill dummy [head|chest|legs] [bullet|melee|blast] [front|back]", 1 },
	{ "end", "end round|game", 1 },
	{ "score", "score red|blue|me <0-9999>", 1 },
	{ "time", "time <seconds>|<m:ss>", 1 },
	{ "goto", "goto <spot> [facing <spot>] | start <n> | <x> <y> <z> [facing <degrees>] | goto bot <x> <y> <z>", 1 },
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
	{ "fireteam", "fireteam follow|regroup|attack|defend|watch|go|clear|status|smart | fireteam smart here|<name> | fireteam recruit [<bot>]", 1 },
	{ "round", "round end|next|live", 1 },
	{ "zone", "zone now|small|off | zone look a|b", 1 },
	{ "down", "down <player> [here]", 1 },
	{ "revive", "revive <player>", 1 },
	{ "god", "god [me|<player>|off]", 1 },
	{ "seat", "seat <driver|gunner|passenger> [<player>] | seat", 1 },
	{ "ride", "ride [warthog|ghost|scorpion] [circle|line]", 1 },
	{ "ttk", "ttk [on|off]", 1 },
	{ "dummy", "dummy [<n>] [here|<distance>] [mate] | dummy walk here|<x> <y> <z> [to <x> <y> <z>] | dummy run <x> <y> <z> to <x> <y> <z> [loop] | dummy reach [<n>] | dummy clear", 1 },
	{ "hud", "hud | hud <i> <dx> <dy> [<scale>] | hud <i> hide|show | hud reset [<i>] | hud save", 1 },
	{ "tune", "tune | tune <key> <value> | tune save | tune reset", 1 },
	{ "call", "call ride", 1 },
	{ "roundcard", "roundcard won|lost|tied [reason]", 1 },
	{ "flag", "flag ours|theirs here|ahead|stand|carry|home", 1 },
	{ "record_medals", "record_medals <0-118> [<seed>] [page 1-7] [counted <n>] [kinds <1-7>] [ring <kind>]", 1 },
	{ "lobby_fill", "lobby_fill <1-4>", 1 },
	{ "motd_reset", "motd_reset", 1 },
	{ "motd_offline", "motd_offline", 1 },
	{ "hitch", "hitch <50-5000> [after <1-600>]", 1 },
	{ "join_lan", "join_lan", 1 },
	{ "camera", "camera third|first [<1-4>]", 1 },
	{ "menu", "menu settings [player <1-4>] find [<text>] | menu game_options <filter> [change <option>=<value> ...] | menu tournament [filter casual|comp] [focus <id>] | menu matchmaking [online] [focus <id>] | menu career [player <1-4>] [focus <id>] | menu campaign [seat | players <2-4>] [error] [workshop [demo|fault|none|saved|damaged]] [save <1-10> <0-3>] [focus skip|continue|squad|down|dialog] [difficulty <0-3>] | menu coop [players <1-4>] | menu hostvote [lan] [now|join] | menu lan | menu steam <state> [<percent>] | menu playlist [edit <name>] | menu browser | menu gametype | menu tab | menu credits | menu network [reached|unreached|unknown] | menu bots [player <1-4>] [focus <name>] [sort games|last|record] | menu records [player <1-4>] | menu music [song|playlist] [mcc|standin|none] | menu maps [installed|required|browse|workshop|mods] [<state>] | menu profiles [edit [colours|plain]] [walk <ms>] [turn <deg>] | menu profiles progress export|files|import | menu classic <page>", 1 },
	{ "botskill", "botskill [<bot> <shooting> <movement>|<bot> clear|clear]", 1 },
	{ "social", "social exp <id>|all <0-99999> | reset [<id>|all] | match <id> <0-4999>|tier <1-19> | mode <id> auto|<skill> | roster <id> [seed <n>] | pending <id> [crashed] | maps hide smp|<file>|none | calibration <profile|me> unset|new|some|alot | start <id> [map <name>] [seed <n>] [variant br|evo|magnum|protomag|singleshot|none] [now] | start <id> vote [seed <n>] [votes <a,b,c>] [left <0-10>] [party <1-4>] | postgame win|tie|loss|rankup|place <p> <n> [<id>] [exp <n>] | carnage <id> [seed <n>] [players <1-4>] [cursor <name>]  (each: [player <1-4>] where it applies)", 1 },
	{ "botstyle", "botstyle [<bot> <style> [<weapon>]|<bot> clear|clear]", 1 },
	{ "botrecord", "botrecord [demo [seed <n>] [bots <1-60>] | demo off | set <name> <field> <value> ... (games wins ties kills deaths with vs counted counted_v3 seconds caps returns grabs carrier hill ball plants defuses laps) | page <name>|#<n> [game] [page <1-7>] | forget] [player <1-4>]", 1 },
	{ "playlists", "playlists demo|forget|real", 1 },
	{ "nav", "nav dump|status|show [off|routes|all]", 1 },
	{ "item", "item respawn <entry> in <1-3600>", 1 },
	{ "pgcr_demo", "pgcr_demo <4v4|8v8|ffa8> [slayer|flag|ball|hill|race|bomb] [details]", 1 },
	{ "unseen", "unseen [me|<player>|off]", 1 },
	{ "netext", "netext status", 1 },
	{ "mods", "mods list | on <name> | off <name> | order <name> <1-256>", 1 },
	{ "archive", "archive status | list [<text>] | get <row> | cancel", 1 },
	{ "maps", "maps list [<source>:] [<text>] | missing | scan | delete <map>[@<pack>] | move <map>[@<pack>] <folder>|.", 1 },
	{ "split", "split join|leave|pad|card|force <2-4>", 1 },
	{ "biped", "biped [list|use <1-4>|off|check <1-4>]", 1 },
	{ "campaign_score", "campaign_score status|on|off|reset | campaign_score add <enemy> <style>", 1 },
	{ "replay", "replay list|status|info <n>|play <n>|eyes <player>|free|pause|speed <25-400>|seek <s>|skip ahead|skip back|stop", 1 },
	{ "inject", "inject status|maps|open <level>|list <category>|take <category> <number|name>|close|on|off|clear", 1 },
	{ "randomizer", "randomizer status|on|off|seed <n>|reroll|save|pack", 1 },
	{ "profile_move", "profile_move list | sample [<name> [<games>]] | export <name> | preview|import <file> <name> [merge|replace] | undo <name>", 1 },
	{ "weaponset", "weaponset casual|comp|btb|off|auto", 1 },
	{ "ammo", "ammo <0-100>", 1 },
	{ "spawner", "spawner wait <1-600> | spawner red <1-600> | spawner ready", 1 },
	{ "medal", "medal <key> [me|<player>]", 1 },
	{ "companion", "companion [<1-4>|off|reset|kill|dry|down|revert|won|wait] | companion near <1-20>", 1 },
	{ "campaign", "campaign <1-10> [<0-3>] [won <seconds>] | campaign continue", 1 },
	{ "campaign_weapons", "campaign_weapons status|on|off|share <0-100>|squad standard|ae|goto [<n>]", 1 },
	{ "firefight", "firefight start [<map>] [<variant>] | wave <1-5> | clear | skull <name>", 1 },
	{ "range", "range [<profile>]", 1 },
	{ "skip_training", "skip_training status|on|off", 1 },
	{ "script_refuse", "script_refuse status|off|<script>", 1 },
	{ "pad", "pad status | world a|b|c|other|dualsense|keyboard|off | battery <0-100>|charging [<0-100>]|charged|wired|off | mode wired|bluetooth|puck|off | hold on|off | reset | open settings|buttons | open controls [trackpad|extra|gyro]  (each: [player <1-4>])", 1 },
	{ "actor", "actor <variant> [<2-60>] | actor kill [blast]", 1 },
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

int ae_dev_cmd_votes(const char *text, int *votes)
{
	int values[3], at;
	const char *p = text;

	for (at = 0; at < 3; at++)
	{
		int digits = 0, value = 0;

		while (*p >= '0' && *p <= '9' && digits < 3)
		{
			value = value * 10 + (*p++ - '0');
			digits++;
		}
		if (!digits || value > 16 || (at < 2 && *p++ != ','))
			return 0;
		values[at] = value;
	}
	if (*p)
		return 0;
	if (votes)
		for (at = 0; at < 3; at++)
			votes[at] = values[at];
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

int ae_dev_cmd_during_replay(int id)
{
	if (id == AE_DEV_CMD_REPLAY || id == AE_DEV_CMD_HELP)
		return AE_DEV_CMD_REPLAY_ALLOWED;
	if (id == AE_DEV_CMD_RESET)
		return AE_DEV_CMD_REPLAY_NOTHING;
	return AE_DEV_CMD_REPLAY_REFUSED;
}

const char *ae_dev_cmd_replay_refusal(void)
{
	return "a replay plays: its game stays as recorded, only replay commands run";
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

int ae_dev_cmd_medal(const char *key)
{
	static const char prefix[] = "medal_";
	int medal;

	for (medal = 0; key && medal < (int)(sizeof(medal_keys) / sizeof(medal_keys[0])); medal++)
	{
		const char *a = medal_keys[medal], *b = key;

		/* (a key without its "medal_" is the same medal's) */
		if (strncmp(b, prefix, sizeof(prefix) - 1) && !strncmp(a, prefix, sizeof(prefix) - 1))
			a += sizeof(prefix) - 1;
		while (*a && *b && tolower((unsigned char)*b) == *a)
		{
			a++;
			b++;
		}
		if (!*a && !*b)
			return medal;
	}
	return -1;
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
		if (n == 3)
			bad = !same_text(cmd->word[1], "to") || same_text(cmd->word[0], "all");
		else
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
		/* (powerup <item> [now] [<player>]: the second word is "now" or the player; powerup <item> ahead: the item
		left lying in front of the host, so "ahead" is no player's name and takes no word after it) */
		bad = n < 1 || n > 3 || (n == 3 && (!same_text(cmd->word[1], "now") || same_text(cmd->word[2], "ahead")));
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

		/* (rc1: kill dummy [head|chest|legs] [bullet|melee|blast] [front|back]: a staged ragdoll kill; each kind at most
		once, in any order) */
		if (n >= 1 && same_text(cmd->word[0], "dummy"))
		{
			static const char *const part[] = { "head", "chest", "legs", NULL };
			static const char *const kind[] = { "bullet", "melee", "blast", NULL };
			static const char *const side[] = { "front", "back", NULL };
			int seen_part = 0, seen_kind = 0, seen_side = 0, i;

			for (i = 1; i < n && !bad; i++)
			{
				if (one_of(cmd->word[i], part) && !seen_part)
					seen_part = 1;
				else if (one_of(cmd->word[i], kind) && !seen_kind)
					seen_kind = 1;
				else if (one_of(cmd->word[i], side) && !seen_side)
					seen_side = 1;
				else
					bad = 1;
			}
		}
		/* (0.9.4: kill me by bot | kill me by <name>: a kill with a killer, for the death camera's TEST RUN check) */
		/* (kill <victim> by <killer>: the victim and the killer each a player's name or number, for a server with no
		player of its own; the real damage path, so the kill, its credit and its medals run) */
		else if (n == 3)
			bad = !same_text(cmd->word[1], "by") || same_text(cmd->word[0], cmd->word[2]) || same_text(cmd->word[0], "all") ||
				same_text(cmd->word[0], "bots");
		/* (rc2: kill me by <who> after <1-600> | every <1-600> [times <2-20>]: the kill is pending, made once that many
		seconds on, or again every that many seconds, times kills in all (default 2); number: the seconds between, seed:
		the seconds to the first, times: the kills) */
		else if (n == 5 || n == 7)
		{
			long seconds = 0, times = 1;

			bad = !same_text(cmd->word[0], "me") || !same_text(cmd->word[1], "by") ||
				!whole_number(cmd->word[4], 1, 600, &seconds);
			if (!bad && n == 5 && same_text(cmd->word[3], "after"))
				cmd->seed = (unsigned long)seconds;
			else if (!bad && same_text(cmd->word[3], "every"))
			{
				times = 2;
				if (n == 7)
					bad = !same_text(cmd->word[5], "times") || !whole_number(cmd->word[6], 2, 20, &times);
				cmd->seed = (unsigned long)seconds;
				cmd->number = seconds;
			}
			else
				bad = 1;
			cmd->times = times;
		}
		else
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
		else if (n == 4 && same_text(cmd->word[0], "bot"))
		{
			/* (1.0 rc2 goto bot <x> <y> <z>: the nearest living bot moved there, a staged scene's) */
			bad = !real_number(cmd->word[1], &cmd->position[0]) || !real_number(cmd->word[2], &cmd->position[1]) ||
				!real_number(cmd->word[3], &cmd->position[2]);
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
		static const char *const orders[] = { "follow", "regroup", "attack", "defend", "watch", "go", "clear", "status", "smart", NULL };

		if (n >= 1 && same_text(cmd->word[0], "recruit"))
			bad = n > 2 || (n == 2 && !whole_number(cmd->word[1], 1, 127, &cmd->number));
		/* (smart here: a tap on the host's own spot; smart <word>: on the nearest object whose tag's name has the word) */
		else if (n == 2 && same_text(cmd->word[0], "smart"))
			bad = !cmd->word[1][0];
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
	case AE_DEV_CMD_UNSEEN:
		bad = n > 1;
		break;
	case AE_DEV_CMD_MODS:
		/* (mods list | on <name> | off <name> | order <name> <priority>: number = the priority) */
		bad = !((n == 1 && same_text(cmd->word[0], "list")) ||
			(n == 2 && (same_text(cmd->word[0], "on") || same_text(cmd->word[0], "off"))) ||
			(n == 3 && same_text(cmd->word[0], "order") && whole_number(cmd->word[2], 1, 256, &cmd->number)));
		break;
	case AE_DEV_CMD_ARCHIVE:
		/* (1.0 the community map archive: number = get's row, 1 ...) */
		bad = !(n == 1 && (same_text(cmd->word[0], "status") || same_text(cmd->word[0], "cancel") ||
			same_text(cmd->word[0], "list"))) &&
			!(n == 2 && same_text(cmd->word[0], "list")) &&
			!(n == 2 && same_text(cmd->word[0], "get") && whole_number(cmd->word[1], 1, 20000, &cmd->number));
		break;
	case AE_DEV_CMD_MAPS:
		/* (the words are checked again by the library's own parser: ae_map_library_command_parse) */
		bad = n < 1 || !(same_text(cmd->word[0], "list") ||
			((same_text(cmd->word[0], "missing") || same_text(cmd->word[0], "scan")) && n == 1) ||
			(same_text(cmd->word[0], "delete") && n == 2) || (same_text(cmd->word[0], "move") && n >= 3));
		break;
	case AE_DEV_CMD_SPLIT:
		/* (number = the controller, 1-3) */
		bad = n != 2 || !(same_text(cmd->word[0], "join") || same_text(cmd->word[0], "leave") ||
			same_text(cmd->word[0], "pad") || same_text(cmd->word[0], "card") || same_text(cmd->word[0], "force")) ||
			!whole_number(cmd->word[1], 2, 4, &cmd->number);
		cmd->number--;
		break;
	case AE_DEV_CMD_BIPED:
	{
		/* (number = the slot of use and check) */
		bad = n > 2 || (n == 0 ? 0 : same_text(cmd->word[0], "list") || same_text(cmd->word[0], "off") ? n != 1 :
			!(same_text(cmd->word[0], "use") || same_text(cmd->word[0], "check")) || n != 2 ||
			!whole_number(cmd->word[1], 1, 4, &cmd->number));
		break;
	}
	case AE_DEV_CMD_CAMPAIGN_SCORE:
	{
		/* (status, on, off, reset; add <enemy> <style>: the words are kept as typed, in lower case) */
		int verb = n >= 1 && (same_text(cmd->word[0], "status") || same_text(cmd->word[0], "on") ||
			same_text(cmd->word[0], "off") || same_text(cmd->word[0], "reset"));

		bad = !((verb && n == 1) || (n == 3 && same_text(cmd->word[0], "add")));
		break;
	}
	case AE_DEV_CMD_REPLAY:
	{
		static const char *const alone[] = { "list", "status", "free", "pause", "stop", NULL };

		/* (number: info's and play's recording (1 the newest), speed's percent, seek's second) */
		bad = n < 1 || n > 2;
		if (!bad && one_of(cmd->word[0], alone))
			bad = n != 1;
		else if (!bad && (same_text(cmd->word[0], "info") || same_text(cmd->word[0], "play")))
			bad = n != 2 || !whole_number(cmd->word[1], 1, 500, &cmd->number);
		else if (!bad && same_text(cmd->word[0], "eyes"))
			bad = n != 2;
		else if (!bad && same_text(cmd->word[0], "speed"))
			bad = n != 2 || !whole_number(cmd->word[1], 25, 400, &cmd->number);
		else if (!bad && same_text(cmd->word[0], "seek"))
			bad = n != 2 || !whole_number(cmd->word[1], 0, 86400, &cmd->number);
		else if (!bad && same_text(cmd->word[0], "skip"))
			bad = n != 2 || !(same_text(cmd->word[1], "ahead") || same_text(cmd->word[1], "back"));
		else
			bad = 1;
		break;
	}
	case AE_DEV_CMD_INJECT:
		/* (number = list's and take's category, 0 to 8; take's tag, a number or a part of its path, is looked for in
		the open map's list when it runs) */
		bad = !((n == 1 && (same_text(cmd->word[0], "status") || same_text(cmd->word[0], "maps") ||
			same_text(cmd->word[0], "close") || same_text(cmd->word[0], "on") || same_text(cmd->word[0], "off") ||
			same_text(cmd->word[0], "clear"))) ||
			(n == 2 && same_text(cmd->word[0], "open")) ||
			(n == 2 && same_text(cmd->word[0], "list") && whole_number(cmd->word[1], 0, 8, &cmd->number)) ||
			(n == 3 && same_text(cmd->word[0], "take") && whole_number(cmd->word[1], 0, 8, &cmd->number)));
		break;
	case AE_DEV_CMD_RANDOMIZER:
		/* (number = seed's value, 0 to 999999999) */
		bad = !((n == 1 && (same_text(cmd->word[0], "status") || same_text(cmd->word[0], "on") ||
			same_text(cmd->word[0], "off") || same_text(cmd->word[0], "reroll") || same_text(cmd->word[0], "save") || same_text(cmd->word[0], "pack"))) ||
			(n == 2 && same_text(cmd->word[0], "seed") && whole_number(cmd->word[1], 0, 999999999, &cmd->number)));
		break;
	case AE_DEV_CMD_PROFILE_MOVE:
		/* (number = sample's games, 1 to 100000) */
		bad = !(n == 0 || (n == 1 && (same_text(cmd->word[0], "list") || same_text(cmd->word[0], "sample"))) ||
			(n == 2 && (same_text(cmd->word[0], "export") || same_text(cmd->word[0], "undo") ||
			same_text(cmd->word[0], "sample"))) ||
			(n == 3 && same_text(cmd->word[0], "sample") && whole_number(cmd->word[2], 1, 100000, &cmd->number)) ||
			((n == 3 || n == 4) && (same_text(cmd->word[0], "import") || same_text(cmd->word[0], "preview")) &&
			(n == 3 || same_text(cmd->word[3], "merge") || same_text(cmd->word[3], "replace"))));
		break;
	case AE_DEV_CMD_ITEM:
		/* (1.0 rc2 item respawn <entry> in <seconds>: number = the seconds) */
		bad = n != 4 || !same_text(cmd->word[0], "respawn") || !same_text(cmd->word[2], "in") ||
			!whole_number(cmd->word[3], 1, 3600, &cmd->number);
		break;
	case AE_DEV_CMD_PGCR_DEMO:
	{
		static const char *const modes[] = { "slayer", "flag", "ball", "hill", "race", "bomb", NULL };
		int each;

		/* (a made-up post-game report: its shape, optionally its game kind and the DETAILS page) */
		bad = n < 1 || n > 3 || !((cmd->word[0][0] >= '1' && cmd->word[0][0] <= '9' && strchr(cmd->word[0], 'v')) ||
			!strncmp(cmd->word[0], "ffa", 3));
		for (each = 1; !bad && each < n; each++)
			bad = !(one_of(cmd->word[each], modes) || same_text(cmd->word[each], "details")) ||
				(each == 2 && one_of(cmd->word[1], modes) == one_of(cmd->word[2], modes));
		break;
	}
	case AE_DEV_CMD_NETEXT:
		bad = n != 1 || !same_text(cmd->word[0], "status");
		break;
	case AE_DEV_CMD_NAV:
	{
		static const char *const shows[] = { "off", "routes", "all", NULL };

		/* (nav show: number = what the overlay draws, 0 off, 1 routes, 2 all; none given is all) */
		if (n >= 1 && same_text(cmd->word[0], "show"))
		{
			bad = n > 2 || (n == 2 && !one_of(cmd->word[1], shows));
			cmd->number = n == 1 || same_text(cmd->word[1], "all") ? 2 : same_text(cmd->word[1], "routes") ? 1 : 0;
		}
		else
			bad = n != 1 || !(same_text(cmd->word[0], "dump") || same_text(cmd->word[0], "status"));
		break;
	}
	case AE_DEV_CMD_PLAYLISTS:
		bad = n != 1 || !(same_text(cmd->word[0], "demo") || same_text(cmd->word[0], "forget") || same_text(cmd->word[0], "real"));
		break;
	case AE_DEV_CMD_BOTRECORD:
	{
		/* (0.9.4: number = the local player 0-3; demo: seed, and position[0] the bots (0: the default)) */
		static const char *const fields[] = { "games", "wins", "ties", "kills", "deaths", "with", "vs", "counted", "counted_v3", "seconds",
			"caps", "returns", "grabs", "carrier", "hill", "ball", "plants", "defuses", "laps", NULL };
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
		{
			/* (page <name> [game] [page <1-7>]: position[1] the record page (0-2), or 3-6 the detail window's page over it,
			position[0] unused here) */
			int rest = 2;

			cmd->position[1] = 0.0f;
			bad = at < 2;
			if (!bad && rest < at && same_text(cmd->word[rest], "game"))
				rest++;
			if (!bad && rest < at)
			{
				bad = rest + 2 != at || !same_text(cmd->word[rest], "page") || !whole_number(cmd->word[rest + 1], 1, 7, &value);
				cmd->position[1] = (float)(value - 1);
			}
		}
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
		long seed = 0, value = 0;
		int each = 1;

		/* (the kinds: AE_MEDAL_COUNT at most, ae_medal_ids.h's 118; then a seed, then page <1-3>, counted <n> and kinds <1-7> in any
		order: position[0] the page (0-2, or 3-6 the detail window's page over the record), position[1] the games the made-up numbers
		cover (-1: every one), position[2] the game kinds played (0: the usual three)) */
		cmd->position[0] = 0.0f;
		cmd->position[1] = -1.0f;
		cmd->position[2] = 0.0f;
		bad = n < 1 || !whole_number(cmd->word[0], 0, 118, &cmd->number);
		if (!bad && n >= 2 && !same_text(cmd->word[1], "page") && !same_text(cmd->word[1], "counted") &&
			!same_text(cmd->word[1], "kinds") && !same_text(cmd->word[1], "ring"))
		{
			bad = !whole_number(cmd->word[1], 0, 99999999L, &seed);
			each = 2;
		}
		for (; !bad && each < n; each += 2)
		{
			if (each + 1 >= n)
				bad = 1;
			else if (same_text(cmd->word[each], "page"))
			{
				bad = !whole_number(cmd->word[each + 1], 1, 7, &value);
				cmd->position[0] = (float)(value - 1);
			}
			else if (same_text(cmd->word[each], "counted"))
			{
				bad = !whole_number(cmd->word[each + 1], 0, 9999999L, &value);
				cmd->position[1] = (float)value;
			}
			else if (same_text(cmd->word[each], "kinds"))
			{
				bad = !whole_number(cmd->word[each + 1], 1, 7, &value);
				cmd->position[2] = (float)value;
			}
			else if (same_text(cmd->word[each], "ring"))
			{
				/* (the kind ringed on MODES: the command reads its word) */
				static const char *const kinds[] = { "slayer", "flag", "ball", "hill", "race", "bomb", "other", NULL };

				bad = !one_of(cmd->word[each + 1], kinds);
			}
			else
				bad = 1;
		}
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
				else if (same_text(cmd->word[at], "party"))
					at++;
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
		/* (menu profiles [edit [colours|plain]] [walk <ms>] [turn <deg>]: PROFILES, the first profile's EDIT page over it;
		colours: its colours have the focus; plain: as on a PC with no Spartan model) */
		if (n >= 1 && same_text(cmd->word[0], "profiles"))
		{
			/* (walk <0-9999> and turn <-360..360> in any order after the rest: him held at that millisecond of his walk and
			turned that many degrees) */
			int tails;
			long whole;
			float degrees;

			for (tails = 0; tails < 2 && n >= 3 && (same_text(cmd->word[n - 2], "walk") || same_text(cmd->word[n - 2], "turn")); tails++)
			{
				if (same_text(cmd->word[n - 2], "walk") ? !whole_number(cmd->word[n - 1], 0, 9999, &whole) :
					!(real_number(cmd->word[n - 1], &degrees) && degrees >= -360.0f && degrees <= 360.0f))
					break;
				n -= 2;
			}
			bad = !(n == 1 || (n >= 2 && n <= 3 && same_text(cmd->word[1], "edit") &&
				(n == 2 || same_text(cmd->word[2], "colours") || same_text(cmd->word[2], "plain"))) ||
				(n == 3 && same_text(cmd->word[1], "progress") && (same_text(cmd->word[2], "export") ||
				same_text(cmd->word[2], "files") || same_text(cmd->word[2], "import"))));
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
		/* (menu campaign [seat | players <2-4>] [error] [workshop [demo|fault|none|saved|damaged]]: CAMPAIGN, the players seated as guests (seat: two), the co-op start's error dialog, a custom campaign scene; number = the players) */
		if (n >= 1 && same_text(cmd->word[0], "campaign"))
		{
			int seats = 0, errors = 0, counted = 0, workshop = 0;
			long scratch;

			cmd->number = 0;
			for (each = 1; !bad && each < n; each++)
			{
				if (same_text(cmd->word[each], "seat"))
					bad = ++seats > 1;
				else if (same_text(cmd->word[each], "error"))
					bad = ++errors > 1;
				else if (same_text(cmd->word[each], "workshop"))
				{
					bad = ++workshop > 1;
					if (each + 1 < n && (same_text(cmd->word[each + 1], "demo") || same_text(cmd->word[each + 1], "fault") ||
						same_text(cmd->word[each + 1], "none") || same_text(cmd->word[each + 1], "saved") ||
						same_text(cmd->word[each + 1], "damaged")))
						each++;
				}
				else if (same_text(cmd->word[each], "save") && each + 2 < n && whole_number(cmd->word[each + 1], 1, 10, &scratch) &&
					whole_number(cmd->word[each + 2], 0, 3, &scratch))
					each += 2;
				else if (same_text(cmd->word[each], "focus") && each + 1 < n && (same_text(cmd->word[each + 1], "skip") ||
					same_text(cmd->word[each + 1], "continue") || same_text(cmd->word[each + 1], "squad") ||
					same_text(cmd->word[each + 1], "dialog") || same_text(cmd->word[each + 1], "down")))
					each++;
				else if (same_text(cmd->word[each], "difficulty") && each + 1 < n && whole_number(cmd->word[each + 1], 0, 3, &scratch))
					each++;
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
		/* (rc2 menu hostvote [lan] [now|join]: CREATE GAME hosting a lobby (this PC only, or on the LAN) with the playlist in use, its
		MAP VOTE opened as if a game had just ended: now at once, join when a second person is in the lobby; number = 1 lan + 2 now + 4 join) */
		if (n >= 1 && same_text(cmd->word[0], "hostvote"))
		{
			int each_word;

			cmd->number = 0;
			for (each_word = 1; !bad && each_word < n; each_word++)
			{
				if (same_text(cmd->word[each_word], "lan") && !(cmd->number & 1))
					cmd->number |= 1;
				else if (same_text(cmd->word[each_word], "now") && !(cmd->number & 6))
					cmd->number |= 2;
				else if (same_text(cmd->word[each_word], "join") && !(cmd->number & 6))
					cmd->number |= 4;
				else
					bad = 1;
			}
			break;
		}
		/* (menu classic <page>: a screen of the PC menus (the classic widget menus) opened by name, "profile" the profile
		menu, ae_settings/hud/hud a page of its ARENA EVOLVED section; the words are lower case letters, digits, _ and /) */
		if (n >= 1 && same_text(cmd->word[0], "classic"))
		{
			int at;

			bad = n != 2 || !cmd->word[1][0];
			for (at = 0; !bad && cmd->word[1][at]; at++)
				bad = !((cmd->word[1][at] >= 'a' && cmd->word[1][at] <= 'z') || (cmd->word[1][at] >= '0' &&
					cmd->word[1][at] <= '9') || cmd->word[1][at] == '_' || cmd->word[1][at] == '/');
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
		/* (rc2 menu music [song|playlist] [mcc|standin|none]: MENU MUSIC, on ONE SONG or MY PLAYLIST when named; the songs from the
		installation (mcc), a table of stand-in songs (standin) or none, as no MCC (none); number = the screen, 0 MENU MUSIC, 1 ONE SONG,
		2 MY PLAYLIST; seed = the source, 0 as it is, 1 mcc, 2 standin, 3 none) */
		if (n >= 1 && same_text(cmd->word[0], "music"))
		{
			int each_word;

			cmd->number = 0;
			cmd->seed = 0;
			for (each_word = 1; !bad && each_word < n; each_word++)
			{
				if (same_text(cmd->word[each_word], "song") || same_text(cmd->word[each_word], "playlist"))
					cmd->number = same_text(cmd->word[each_word], "song") ? 1 : 2;
				else if (same_text(cmd->word[each_word], "mcc"))
					cmd->seed = 1;
				else if (same_text(cmd->word[each_word], "standin"))
					cmd->seed = 2;
				else if (same_text(cmd->word[each_word], "none"))
					cmd->seed = 3;
				else
					bad = 1;
			}
			break;
		}
		/* (1.0 menu steam <state> [<percent>]: STEAM ACCOUNT staged (ae_steam_rules.h's states), or first-start: the first-start
		page; number = DOWNLOADING's percent (0: 37)) */
		if (n >= 1 && same_text(cmd->word[0], "steam"))
		{
			int stage = 0;

			cmd->number = 0;
			bad = n < 2 || n > 3 || !(same_text(cmd->word[1], "first-start") || same_text(cmd->word[1], "open") || ae_steam_stage_find(cmd->word[1], &stage));
			if (!bad && n == 3)
				bad = stage != AE_STEAM_STAGE_DOWNLOADING || same_text(cmd->word[1], "first-start") || same_text(cmd->word[1], "open") ||
					!whole_number(cmd->word[2], 1, 100, &cmd->number);
			break;
		}
		/* (1.0 menu maps [<page> [<state>]]: MAPS & MODS staged from made-up data; number = the page (AE_HUB_TAB_*), seed = its
		state (AE_HUB_STATE_*); no page: INSTALLED) */
		if (n >= 1 && same_text(cmd->word[0], "maps"))
		{
			int tab = AE_HUB_TAB_INSTALLED, state = AE_HUB_STATE_LIST;

			bad = n > 3;
			if (!bad && n >= 2)
			{
				tab = ae_hub_tab_find(cmd->word[1]);
				bad = tab < 0;
			}
			if (!bad && n == 3)
				bad = !ae_hub_stage_find(tab, cmd->word[2], &state);
			if (!bad && n < 3)
				ae_hub_stage_find(tab, "list", &state);
			cmd->number = tab;
			cmd->seed = state;
			break;
		}
		/* (rc5 menu changemap [folder] [all] [version <n>] [<map words>]: CHANGE MAP on the filter ALL with the first map whose name has the
		words in it focused, or its folder's row with `folder`) */
		if (n >= 1 && same_text(cmd->word[0], "changemap"))
			break;
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
		/* ((rc2) menu records [player <1-4>]: CAREER's RECORDS, the players beside the bots; the same words) */
		if (n >= 1 && (same_text(cmd->word[0], "bots") || same_text(cmd->word[0], "records")))
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
				if (same_text(cmd->word[each], "now") || same_text(cmd->word[each], "vote") || same_text(cmd->word[each], "postgame") ||
					same_text(cmd->word[each], "online") || same_text(cmd->word[each], "still"))
					continue;
				if (each + 1 >= at)
					bad = 1;
				else if (same_text(cmd->word[each], "party"))
				{
					long party;

					bad = !whole_number(cmd->word[++each], 1, 4, &party);
				}
				else if (same_text(cmd->word[each], "hold"))
				{
					long hold_seconds;

					/* (a number of seconds, or starting: the lobby's STARTING stage, one second in) */
					each++;
					bad = !(each < cmd->words && same_text(cmd->word[each], "starting")) &&
						!whole_number(cmd->word[each], 0, 30, &hold_seconds);
				}
				else if (same_text(cmd->word[each], "bots"))
				{
					long party_bots;

					bad = !whole_number(cmd->word[++each], 0, 7, &party_bots);
				}
				else if (same_text(cmd->word[each], "left"))
					{
					long seconds_left;

					bad = !whole_number(cmd->word[++each], 0, 20, &seconds_left);
				}
				else if (same_text(cmd->word[each], "votes"))
					bad = !ae_dev_cmd_votes(cmd->word[++each], NULL);
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
		else if (same_text(cmd->word[0], "party"))
			bad = at != 2;
		else if (same_text(cmd->word[0], "recent"))
		{
			/* (rc5 recent show|clear <id|all>, recent set <id> <map,map,...>) */
			bad = at < 3 || (same_text(cmd->word[1], "set") ? at != 4 : at != 3 || (!same_text(cmd->word[1], "show") &&
				!same_text(cmd->word[1], "clear")));
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
	case AE_DEV_CMD_CAMERA:
		/* (rc2: camera third|first [view]: the dev third-person camera on or off, as D-PAD DOWN does; number: the view, 0 to 3) */
		bad = (n != 1 && n != 2) || (!same_text(cmd->word[0], "third") && !same_text(cmd->word[0], "first")) ||
			(n == 2 && !whole_number(cmd->word[1], 1, 4, &cmd->number));
		if (!bad)
			cmd->number = n == 2 ? cmd->number - 1 : 0;
		break;
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
		/* (run <x> <y> <z> to <x> <y> <z> [loop]: one runner of the other team, there and, with loop, back again and again) */
		if (n >= 1 && same_text(cmd->word[0], "run"))
		{
			bad = (n != 8 && n != 9) || !real_number(cmd->word[1], &cmd->position[0]) ||
				!real_number(cmd->word[2], &cmd->position[1]) || !real_number(cmd->word[3], &cmd->position[2]) ||
				!same_text(cmd->word[4], "to") || !real_number(cmd->word[5], &cmd->goal[0]) ||
				!real_number(cmd->word[6], &cmd->goal[1]) || !real_number(cmd->word[7], &cmd->goal[2]) ||
				(n == 9 && !same_text(cmd->word[8], "loop"));
			cmd->has_position = cmd->has_goal = !bad;
			cmd->times = !bad && n == 9 ? 1 : 0;
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
	case AE_DEV_CMD_WEAPONSET:
	{
		static const char *const names[] = { "casual", "comp", "btb", "off", "auto", NULL };

		bad = n != 1 || !one_of(cmd->word[0], names);
		break;
	}
	case AE_DEV_CMD_AMMO:
		/* (rc2 ammo <percent>: number = the percent of the weapon in hand's full ammunition) */
		bad = n != 1 || !whole_number(cmd->word[0], 0, 100, &cmd->number);
		break;
	case AE_DEV_CMD_SPAWNER:
		/* (rc2 spawner wait|red <seconds>, spawner ready: number = the seconds) */
		bad = !((n == 2 && (same_text(cmd->word[0], "wait") || same_text(cmd->word[0], "red")) &&
			whole_number(cmd->word[1], 1, 600, &cmd->number)) || (n == 1 && same_text(cmd->word[0], "ready")));
		break;
	case AE_DEV_CMD_MEDAL:
		/* (rc5 medal <key> [<player>]: number = the medal) */
		cmd->number = n >= 1 ? ae_dev_cmd_medal(cmd->word[0]) : -1;
		bad = n < 1 || n > 2 || cmd->number < 0;
		break;
	case AE_DEV_CMD_CAMPAIGN:
		/* (number = the level, 1 to 10; the difficulty is word 1, 0 to 3; `campaign continue`: number 0, the saved game) */
		if (n == 1 && same_text(cmd->word[0], "continue"))
		{
			cmd->number = 0;
			bad = 0;
			break;
		}
		bad = !(n >= 1 && n <= 4 && whole_number(cmd->word[0], 1, 10, &cmd->number));
		if (!bad && n == 2)
			bad = strlen(cmd->word[1]) != 1 || cmd->word[1][0] < '0' || cmd->word[1][0] > '3';
		if (!bad && n == 3)
			bad = !same_text(cmd->word[1], "won");
		if (!bad && n == 4)
			bad = strlen(cmd->word[1]) != 1 || cmd->word[1][0] < '0' || cmd->word[1][0] > '3' || !same_text(cmd->word[2], "won");
		break;
	case AE_DEV_CMD_CAMPAIGN_WEAPONS:
		/* (number = share's percent, 0 to 100, or goto's weapon, 1 to 64 (1 when none is given)) */
		cmd->number = 1;
		bad = !((n == 1 && (same_text(cmd->word[0], "status") || same_text(cmd->word[0], "on") || same_text(cmd->word[0], "off") ||
			same_text(cmd->word[0], "goto"))) ||
			(n == 2 && same_text(cmd->word[0], "share") && whole_number(cmd->word[1], 0, 100, &cmd->number)) ||
			(n == 2 && same_text(cmd->word[0], "goto") && whole_number(cmd->word[1], 1, 64, &cmd->number)) ||
			(n == 2 && same_text(cmd->word[0], "squad") && (same_text(cmd->word[1], "standard") || same_text(cmd->word[1], "ae"))));
		break;
	case AE_DEV_CMD_COMPANION:
		/* (number = how many, 1 to 4; 2 when none is given) */
		cmd->number = 2;
		bad = !(n == 0 || (n == 1 && (same_text(cmd->word[0], "off") || same_text(cmd->word[0], "reset") || same_text(cmd->word[0], "kill") || same_text(cmd->word[0], "dry") || same_text(cmd->word[0], "down") || same_text(cmd->word[0], "revert") || same_text(cmd->word[0], "won") || same_text(cmd->word[0], "wait") || whole_number(cmd->word[0], 1, 4, &cmd->number))) ||
			(n == 2 && same_text(cmd->word[0], "near") && whole_number(cmd->word[1], 1, 20, &cmd->number)));
		break;
	case AE_DEV_CMD_SKIP_TRAINING:
		bad = !(n == 1 && (same_text(cmd->word[0], "status") || same_text(cmd->word[0], "on") || same_text(cmd->word[0], "off")));
		break;
	case AE_DEV_CMD_SCRIPT_REFUSE:
		/* (status, off, or the name of the script to refuse) */
		bad = n != 1;
		break;
	case AE_DEV_CMD_ACTOR:
		/* (number: how far ahead it stands, world units, 5 unless given; kill: those placed, shot or, with blast, by a
		blast at their feet) */
		cmd->number = 5;
		if (same_text(cmd->word[0], "kill") && (n == 1 || (n == 2 && same_text(cmd->word[1], "blast"))))
			bad = 0;
		else
			bad = (n != 1 && n != 2) || (n == 2 && !whole_number(cmd->word[1], 2, 60, &cmd->number));
		break;
	case AE_DEV_CMD_PAD:
	{
		static const char *const worlds[] = { "a", "b", "c", "other", "dualsense", "keyboard", "off", NULL };
		static const char *const modes[] = { "wired", "bluetooth", "puck", "off", NULL };
		static const char *const places[] = { "settings", "controls", "buttons", NULL };
		static const char *const focuses[] = { "trackpad", "extra", "gyro", NULL };
		long player = 1;

		/* (number = the local player, 0 to 3; times = the battery's percent, -1 for none) */
		cmd->times = -1;
		if (n >= 3 && same_text(cmd->word[n - 2], "player"))
		{
			bad = !whole_number(cmd->word[n - 1], 1, 4, &player);
			n -= 2;
		}
		cmd->number = player - 1;
		if (bad || n < 1)
			bad = 1;
		else if (same_text(cmd->word[0], "status") || same_text(cmd->word[0], "reset"))
			bad = n != 1;
		else if (same_text(cmd->word[0], "world"))
			bad = n != 2 || !one_of(cmd->word[1], worlds);
		else if (same_text(cmd->word[0], "mode"))
			bad = n != 2 || !one_of(cmd->word[1], modes);
		else if (same_text(cmd->word[0], "open"))
			bad = !((n == 2 && one_of(cmd->word[1], places)) ||
				(n == 3 && same_text(cmd->word[1], "controls") && one_of(cmd->word[2], focuses)));
		else if (same_text(cmd->word[0], "hold"))
			bad = !(n == 2 && (same_text(cmd->word[1], "on") || same_text(cmd->word[1], "off")));
		else if (same_text(cmd->word[0], "battery"))
		{
			if (n == 2 && (same_text(cmd->word[1], "off") || same_text(cmd->word[1], "charged") || same_text(cmd->word[1], "wired") ||
				same_text(cmd->word[1], "charging")))
				bad = 0;
			else if (n == 2)
				bad = !whole_number(cmd->word[1], 0, 100, &cmd->times);
			else if (n == 3)
				bad = !same_text(cmd->word[1], "charging") || !whole_number(cmd->word[2], 0, 100, &cmd->times);
			else
				bad = 1;
		}
		else
			bad = 1;
		break;
	}
	case AE_DEV_CMD_FIREFIGHT:
		/* (firefight start [<map>] [<variant>]: a Firefight game of this machine alone, on the map named, else the map
		being played; the variant's built-in name, else "firefight") */
		if (n >= 1 && same_text(cmd->word[0], "wave"))
			bad = n != 2 || !whole_number(cmd->word[1], 1, 5, &cmd->number);
		else if (n >= 1 && same_text(cmd->word[0], "skull"))
			bad = n != 2;
		else if (n == 1 && same_text(cmd->word[0], "clear"))
			bad = 0;
		else
			bad = n < 1 || n > 3 || !same_text(cmd->word[0], "start");
		break;
	case AE_DEV_CMD_RANGE:
	{
		/* (range [<profile>]: a profile folder's name, letters, digits, _ and -) */
		char const *letter = n == 1 ? cmd->word[0] : "";

		bad = n > 1;
		for (; !bad && *letter; letter++)
			bad = !isalnum((unsigned char)*letter) && *letter != '_' && *letter != '-';
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
		out->id == AE_DEV_CMD_AGENT || out->id == AE_DEV_CMD_MENU || out->id == AE_DEV_CMD_MAPS;
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

int ae_dev_cmd_powerup_place(struct ae_dev_cmd const *cmd, const char **player)
{
	if (player)
		*player = NULL;
	if (cmd->words == 2 && same_text(cmd->word[1], "ahead"))
		return AE_DEV_POWERUP_AHEAD;
	if (cmd->words == 3 || (cmd->words == 2 && !same_text(cmd->word[1], "now")))
	{
		if (player)
			*player = cmd->word[cmd->words - 1];
		return AE_DEV_POWERUP_PLAYER;
	}
	return AE_DEV_POWERUP_FEET;
}

int ae_dev_cmd_runs_in_campaign(struct ae_dev_cmd const *cmd)
{
	switch (cmd->id)
	{
	case AE_DEV_CMD_COMPANION:
	case AE_DEV_CMD_CAMPAIGN_WEAPONS:
	case AE_DEV_CMD_GOTO:
	case AE_DEV_CMD_SPAWN:
	case AE_DEV_CMD_SEAT:
	case AE_DEV_CMD_RIDE:
	case AE_DEV_CMD_FIRETEAM:
		return 1;
	case AE_DEV_CMD_POWERUP:
		return ae_dev_cmd_powerup_place(cmd, NULL) == AE_DEV_POWERUP_AHEAD;
	default:
		return 0;
	}
}

int ae_dev_cmd_seat_wanted(const char *kind, int driver, int gunner)
{
	if (!strcmp(kind, "driver"))
		return driver != 0;
	if (!strcmp(kind, "gunner"))
		return gunner != 0 && driver == 0;
	return driver == 0 && gunner == 0;
}
