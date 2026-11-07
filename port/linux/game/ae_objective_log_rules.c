/*
AE_OBJECTIVE_LOG_RULES.C

See ae_objective_log_rules.h. The event table below is the log's contract: a key added or moved is a change of the log's
format (tools/ae_test/obj_report.py reads this table, and the unit test pins it).
*/

#include "ae_objective_log_rules.h"

#include <stdio.h>
#include <string.h>

#define WHO "p b team x y z"

static struct { char const *ev; char const *keys; } const ae_obj_events[] =
{
	{ "game_start", "map teams players bots" },
	{ "game_end", "winner score_r score_b" },
	/* o1 o2: the mode's objective, compact. ctf: flag0 flag1 as home | carried:<p> | dropped:<x>,<y>,<z>;
	oddball: the holders (p, ';' apart) or free:<x>,<y>,<z>, and -; koth: the hill <x>,<y>,<z> and its state;
	terr: the owners by territory (r, b, n; ','-apart) and the held counts r/b/n; bomb and snd: the bomb as carried:<p> |
	loose:<x>,<y>,<z> | planted:<goal team> | none, and the round */
	{ "snap", "score_r score_b time_s time_left_s o1 o2" },
	{ "ps", WHO " hp shield job role goal objective d_obj state seat ft order" },
	{ "job_change", "p b team from to util_from util_to forced d_obj" },
	{ "obj_blocked", "p b team job objective d_obj reason since_s" },
	{ "flag_take", "flag " WHO " from prev" },
	{ "flag_drop", "flag " WHO " why held_s" },
	{ "flag_return", "flag how by b x y z" },
	{ "flag_capture", "flag " WHO " carry_s dist" },
	{ "ball_take", WHO " prev" },
	{ "ball_drop", WHO " why held_s" },
	{ "jugg_change", "from to" },
	{ "hill_new", "x y z" },
	{ "hill_move", "px py pz x y z" },
	{ "hill_state", "state team occ_r occ_b" },
	{ "hill_enter", WHO },
	{ "hill_leave", WHO },
	{ "terr_capture", "terr hill team from_team score_r score_b" },
	{ "terr_state", "terr hill state team occ_r occ_b progress" },
	{ "bomb_take", WHO },
	{ "bomb_drop", WHO " why" },
	{ "plant_done", WHO },
	{ "defuse_start", WHO },
	{ "defuse_cancel", "p b team held_s" },
	{ "defuse_done", WHO },
	{ "blast_kill", WHO },
	{ "oob_kill", WHO },
	{ "bomb_score", WHO " score_r score_b" },
	{ "bomb_reset", "why x y z" },
	{ "round_start", "round attackers score_r score_b" },
	{ "round_end", "round winner why score_r score_b" },
	/* 0.7.7 follow-ups. plant_start: the carrier is in the goal and the arming begins; plant_cancel: it left before the
	plant (held_s). bomb_reset: the bomb sent home (why: out_of_bounds | out_of_reach | untouched; x y z where it lay) */
	{ "plant_start", WHO },
	{ "plant_cancel", "p b team held_s" },
	/* vehicles and items: a player boards (seat driver | gunner | passenger; veh = the vehicle's tag name) or leaves it
	(held_s in it); an item (a weapon's tag name) picked up or swapped in, not the start kit, a flag, a ball or the bomb */
	{ "veh_board", WHO " veh seat" },
	{ "veh_exit", WHO " veh seat held_s" },
	{ "item_take", WHO " item" },
	/* elimination: a round (max = the match's rounds, switched = sides switched, wins by team or the top player) */
	{ "elim_round_start", "round max_rounds switched wins_r wins_b" },
	{ "elim_round_end", "round winner wins_r wins_b" },
	{ "elim_down", WHO " zone" },
	{ "elim_out", WHO " zone" },
	{ "elim_revive", WHO " by revives" },
	{ "zone_phase", "round phase radius left_s" },
	/* race: a flag touched (by order, team = the flag's), a lap done (lap_s = its time) */
	{ "race_flag", WHO " flag" },
	{ "race_lap", WHO " lap lap_s" },
	/* a bot found stuck (the events level; debug.bot_events adds the long ev=stuck line besides) */
	{ "stuck", "p b team x y z count job goal_name role d_goal d_obj fighting tether" },
};

/* appended to the bot event line of a stuck bot (ev=stuck), after its existing words */
static char const ae_obj_stuck_words[] = "team mode job goal_name role d_goal d_obj fighting tether";

static char const *const ae_obj_mode_names[AE_OBJ_MODES] =
{
	"slayer", "ctf", "ctf1", "assault", "oddball", "tag", "jugg", "koth", "terr", "race", "bomb", "snd", "elim", "exterm", "attrit",
};

int ae_obj_rules_level(char const *text, int bot_events)
{
	int level = AE_OBJ_OFF;

	if (text)
	{
		if (!strcmp(text, "events"))
			level = AE_OBJ_EVENTS;
		else if (!strcmp(text, "snap"))
			level = AE_OBJ_SNAP;
		else if (!strcmp(text, "full"))
			level = AE_OBJ_FULL;
	}
	return bot_events ? AE_OBJ_FULL : level;
}

char const *ae_obj_rules_mode_name(int mode)
{
	return mode >= 0 && mode < AE_OBJ_MODES ? ae_obj_mode_names[mode] : "slayer";
}

int ae_obj_rules_event_count(void)
{
	return (int)(sizeof(ae_obj_events) / sizeof(ae_obj_events[0]));
}

char const *ae_obj_rules_event_name(int index)
{
	return index >= 0 && index < ae_obj_rules_event_count() ? ae_obj_events[index].ev : NULL;
}

char const *ae_obj_rules_event_keys(char const *ev)
{
	int index;

	for (index = 0; ev && index < ae_obj_rules_event_count(); index++)
	{
		if (!strcmp(ae_obj_events[index].ev, ev))
			return ae_obj_events[index].keys;
	}
	return NULL;
}

char const *ae_obj_rules_stuck_keys(void)
{
	return ae_obj_stuck_words;
}

int ae_obj_rules_bomb_reset(char const *message, char *why, size_t size, double *x, double *y, double *z)
{
	char const *word;
	float fx = 0.0f, fy = 0.0f, fz = 0.0f;
	char const *at;

	if (!message || strncmp(message, "reset: ", 7))
		return 0;
	word = !strncmp(message + 7, "out of bounds", 13) ? "out_of_bounds" : !strncmp(message + 7, "out of reach", 12) ?
		"out_of_reach" : !strncmp(message + 7, "untouched", 9) ? "untouched" : NULL;
	if (!word)
		return 0;
	at = strchr(message, '(');
	if (at && sscanf(at, "(%f %f %f)", &fx, &fy, &fz) != 3)
		fx = fy = fz = 0.0f;
	snprintf(why, size, "%s", word);
	*x = fx;
	*y = fy;
	*z = fz;
	return 1;
}

int ae_obj_rules_item_logged(char const *name)
{
	return name && name[0] && strcmp(name, "flag") && strcmp(name, "ball") && strcmp(name, "bomb") &&
		strcmp(name, "oddball");
}

char const *ae_obj_rules_seat_name(int seat)
{
	return seat == 0 ? "driver" : seat == 1 ? "gunner" : "passenger";
}

void ae_obj_fields_begin(struct ae_obj_fields *fields)
{
	fields->text[0] = 0;
	fields->length = 0;
}

static void ae_obj_fields_add(struct ae_obj_fields *fields, char const *key, char const *value)
{
	char word[160];
	size_t length, index;
	int written = snprintf(word, sizeof(word), " %s=", key);

	if (written < 0 || (size_t)written >= sizeof(word))
		return;
	length = (size_t)written;
	for (index = 0; value[index] && length + 1 < sizeof(word); index++)
		word[length++] = value[index] == ' ' || value[index] == '\t' || value[index] == '\n' ? '_' : value[index];
	word[length] = 0;
	if (fields->length + length + 1 > sizeof(fields->text))
		return;
	memcpy(fields->text + fields->length, word, length + 1);
	fields->length += length;
}

void ae_obj_fields_long(struct ae_obj_fields *fields, char const *key, long value)
{
	char text[32];

	snprintf(text, sizeof(text), "%ld", value);
	ae_obj_fields_add(fields, key, text);
}

void ae_obj_fields_real(struct ae_obj_fields *fields, char const *key, double value)
{
	char text[40];

	snprintf(text, sizeof(text), "%.1f", value);
	ae_obj_fields_add(fields, key, text);
}

void ae_obj_fields_text(struct ae_obj_fields *fields, char const *key, char const *value)
{
	ae_obj_fields_add(fields, key, value && value[0] ? value : "none");
}

void ae_obj_fields_point(struct ae_obj_fields *fields, double x, double y, double z)
{
	ae_obj_fields_real(fields, "x", x);
	ae_obj_fields_real(fields, "y", y);
	ae_obj_fields_real(fields, "z", z);
}

size_t ae_obj_rules_line(char *out, size_t size, long tick, int host, int mode, char const *ev,
	struct ae_obj_fields const *fields)
{
	int written = snprintf(out, size, "ae obj: t=%ld host=%d mode=%s ev=%s%s", tick, host ? 1 : 0,
		ae_obj_rules_mode_name(mode), ev, fields ? fields->text : "");

	return written < 0 ? 0 : (size_t)written >= size ? size - 1 : (size_t)written;
}

int ae_obj_rules_line_conforms(char const *line)
{
	char const *at = strstr(line, " ev=");
	char event[40];
	char const *keys, *cursor;
	size_t length = 0;

	if (strncmp(line, "ae obj: t=", 10) || !at)
		return 0;
	at += 4;
	while (at[length] && at[length] != ' ' && length + 1 < sizeof(event))
	{
		event[length] = at[length];
		length++;
	}
	event[length] = 0;
	keys = ae_obj_rules_event_keys(event);
	if (!keys)
		return 0;
	cursor = at + length;
	while (*keys)
	{
		size_t key_length = 0;

		while (keys[key_length] && keys[key_length] != ' ')
			key_length++;
		if (*cursor != ' ' || strncmp(cursor + 1, keys, key_length) || cursor[1 + key_length] != '=')
			return 0;
		cursor += 1 + key_length;
		while (*cursor && *cursor != ' ')
			cursor++;
		keys += key_length;
		if (*keys == ' ')
			keys++;
	}
	return *cursor == 0;
}
