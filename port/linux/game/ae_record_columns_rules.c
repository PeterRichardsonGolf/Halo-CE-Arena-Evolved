/* The SERVICE RECORD detail window's words (see the header). */
#include <stdio.h>
#include <string.h>

#include "ae_pgcr_rules.h"
#include "ae_record_focus.h"
#include "ae_record_medals_rules.h"

#include "ae_record_columns_rules.h"

/* the objective kinds in the columns' order */
static int const objective_kinds[AE_RC_OBJECTIVE_KINDS] =
{
	AE_RS_KIND_FLAG, AE_RS_KIND_HILL, AE_RS_KIND_BALL, AE_RS_KIND_BOMB, AE_RS_KIND_RACE
};

/* the medals each objective kind awards (the medal rules give them in that kind's games only) */
static int const flag_medals[] =
{
	AE_MEDAL_FLAG_CAPTURED, AE_MEDAL_FLAG_RETURNED, AE_MEDAL_FLAG_TAKEN, AE_MEDAL_FLAG_CARRIER_KILL, AE_MEDAL_FLAG_RUNNER,
	AE_MEDAL_FLAG_CHAMPION, AE_MEDAL_FLAG_KILL, AE_MEDAL_FLAG_JOUST, AE_MEDAL_GOAL_LINE_STAND, -1
};
static int const hill_medals[] =
{
	AE_MEDAL_HILL_DOMINANCE, AE_MEDAL_HILL_DEFENSE, AE_MEDAL_HILL_OFFENSE, AE_MEDAL_HILL_GUARDIAN, AE_MEDAL_HAIL_TO_THE_KING, -1
};
static int const ball_medals[] =
{
	AE_MEDAL_BALL_KILL, AE_MEDAL_BALL_HOLDER, AE_MEDAL_BALL_KEEPER, AE_MEDAL_BALL_MASTER, AE_MEDAL_BALL_HOG, AE_MEDAL_MAGIC_HANDS,
	AE_MEDAL_BALL_CARRIER_KILL, AE_MEDAL_FIRST_TOUCH, AE_MEDAL_FUMBLE, AE_MEDAL_STRAIGHT_BALLING, -1
};
static int const race_medals[] = { AE_MEDAL_FIRST_PLACE, AE_MEDAL_POLE_POSITION, AE_MEDAL_REDLINED_IT, -1 };
static int const bomb_medals[] = { -1 };
static int const slayer_medals[] = { AE_MEDAL_PERFECTION, AE_MEDAL_STEAKTACULAR, -1 };
static int const other_medals[] = { -1 };

/* MODES' kinds in the columns' (and the chips') order */
static int const mode_kinds[AE_RC_MODE_COLUMNS] =
{
	AE_RS_KIND_SLAYER, AE_RS_KIND_FLAG, AE_RS_KIND_HILL, AE_RS_KIND_BALL, AE_RS_KIND_BOMB, AE_RS_KIND_RACE, AE_RS_KIND_OTHER
};
static int const assist_medals[] = { AE_MEDAL_WINGMAN, AE_MEDAL_PROTECTOR, AE_MEDAL_DRIVER, -1 };

char const *ae_rc_page_name(int page)
{
	switch (page)
	{
	case AE_RC_MODES: return "MODES";
	case AE_RC_COMBAT: return "COMBAT";
	case AE_RC_WEAPONS_MAPS: return "WEAPONS & MAPS";
	case AE_RC_MEDALS: return "MEDALS";
	default: return "";
	}
}

int ae_rc_page_step(int page, int direction)
{
	if (page < 0 || page >= AE_RC_PAGES)
		return AE_RC_MODES;
	return (page + (direction < 0 ? AE_RC_PAGES - 1 : 1)) % AE_RC_PAGES;
}

int ae_rc_page_of_box(int box, int *column)
{
	int page = -1, at = -1;

	switch (box)
	{
	case AE_RF_LAST: case AE_RF_OBJECTIVES: case AE_RF_KINDS: case AE_RF_TOTALS: page = AE_RC_MODES; break;
	case AE_RF_HIGHLIGHTS: case AE_RF_IDENTITY: page = AE_RC_WEAPONS_MAPS; at = 0; break;
	case AE_RF_ACCURACY: page = AE_RC_COMBAT; at = 0; break;
	case AE_RF_KILL_KINDS: page = AE_RC_COMBAT; at = 1; break;
	case AE_RF_DAMAGE: page = AE_RC_COMBAT; at = 2; break;
	case AE_RF_PER_GAME: case AE_RF_ASSISTS: page = AE_RC_COMBAT; at = 3; break;
	case AE_RF_WEAPONS: page = AE_RC_WEAPONS_MAPS; at = 0; break;
	case AE_RF_MAPS: page = AE_RC_WEAPONS_MAPS; at = 1; break;
	default: break;
	}
	if (column)
		*column = at;
	return page;
}

char const *ae_rc_kind_word(int kind)
{
	switch (kind)
	{
	case AE_RS_KIND_FLAG: return "FLAG";
	case AE_RS_KIND_HILL: return "HILL";
	case AE_RS_KIND_BALL: return "BALL";
	case AE_RS_KIND_BOMB: return "BOMB";
	case AE_RS_KIND_RACE: return "RACE";
	default: return "";
	}
}

void ae_rc_clock(long seconds, char *out, size_t size)
{
	if (seconds < 0)
		seconds = 0;
	if (seconds >= 3600)
		snprintf(out, size, "%ld:%02ld:%02ld", seconds / 3600, seconds / 60 % 60, seconds % 60);
	else
		snprintf(out, size, "%ld:%02ld", seconds / 60, seconds % 60);
}

int ae_rc_per_game_known(struct ae_rv_in const *in, int v3)
{
	struct ae_rs_stats const *x = in->x;

	if (!x || x->counted <= 0)
		return 0;
	/* (a bot's kind lines are counted with its numbers; a player's count every game) */
	if (in->spree_from_record)
		return v3 ? x->counted_v3 > 0 && x->counted_v3 == x->counted : 1;
	return v3 ? x->counted_v3 > 0 && x->counted_v3 == in->games : x->counted == in->games;
}

static void upper(char const *text, char *out, size_t size)
{
	size_t at = 0;

	for (; text && *text && at + 1 < size; text++)
		out[at++] = *text >= 'a' && *text <= 'z' ? (char)(*text - 32) : *text;
	if (size)
		out[at] = 0;
}

/* "1 GAME", "1,204 GAMES" */
static void games_words(long games, char *out, size_t size)
{
	char number[24];

	ae_rs_number_text(games, number, sizeof(number));
	snprintf(out, size, "%s GAME%s", number, games == 1 ? "" : "S");
}

/* a over b with one decimal: "0.9" */
static void tenth(long a, long b, char *out, size_t size)
{
	long tenths = b > 0 ? (a * 10 + b / 2) / b : 0;
	char whole[24];

	ae_rs_number_text(tenths / 10, whole, sizeof(whole));
	snprintf(out, size, "%s.%ld", whole, tenths % 10);
}

/* ---------- the header band */

void ae_rc_head(struct ae_rv_in const *in, struct ae_sr_last const *last, char const *name, struct ae_rc_head *out)
{
	struct ae_rs_stats const *x = in->x;
	int have = in->games > 0;

	memset(out, 0, sizeof(*out));
	if (have)
		ae_rs_number_text(in->games, out->games, sizeof(out->games));
	else
		snprintf(out->games, sizeof(out->games), "%s", AE_RV_DASH);
	if (have)
		snprintf(out->win_rate, sizeof(out->win_rate), "%ld%%", ae_rs_percent(in->wins, in->games));
	else
		snprintf(out->win_rate, sizeof(out->win_rate), "%s", AE_RV_DASH);
	if (have)
		ae_rs_ratio_text(ae_rs_ratio100(in->kills, in->deaths), out->kd, sizeof(out->kd));
	else
		snprintf(out->kd, sizeof(out->kd), "%s", AE_RV_DASH);
	if (in->spree_from_record ? have : x && ae_rs_known(x))
		ae_rs_number_text(in->spree_from_record ? in->best_spree : x->best_spree, out->best_spree, sizeof(out->best_spree));
	else
		snprintf(out->best_spree, sizeof(out->best_spree), "%s", AE_RV_DASH);
	if (last && last->has)
	{
		char map[AE_RS_MAP_NAME];

		upper(last->map, map, sizeof(map));
		snprintf(out->last, sizeof(out->last), "LAST: %s ON %s, %s", last->result == 0 ? "WON" : last->result == 2 ? "TIED" :
			"LOST", map[0] ? map : "A MAP", ae_sr_mode_name(last->mode));
	}
	snprintf(out->initial, sizeof(out->initial), "?");
	for (; name && *name; name++)
		if ((*name >= 'a' && *name <= 'z') || (*name >= 'A' && *name <= 'Z') || (*name >= '0' && *name <= '9'))
		{
			out->initial[0] = *name >= 'a' && *name <= 'z' ? (char)(*name - 32) : *name;
			out->initial[1] = 0;
			break;
		}
}

/* ---------- MODES */

int ae_rc_favourite_kind(struct ae_rv_in const *in)
{
	int kind, best = -1;

	for (kind = 0; kind < AE_RS_KINDS; kind++)
	{
		struct ae_sr_line const *line = &in->kind[kind];

		if (line->games > 0 && (best < 0 || line->games > in->kind[best].games ||
			(line->games == in->kind[best].games && line->kills > in->kind[best].kills)))
			best = kind;
	}
	return best;
}

int ae_rc_chips(struct ae_rv_in const *in, struct ae_rc_chip *out)
{
	int index, count = 0, best = ae_rc_favourite_kind(in);

	for (index = 0; index < AE_RC_MODE_COLUMNS; index++)
	{
		int kind = mode_kinds[index];
		struct ae_sr_line const *line = &in->kind[kind];

		if (line->games <= 0)
			continue;
		out[count].kind = kind;
		snprintf(out[count].name, sizeof(out[count].name), "%s", ae_sr_mode_name(kind));
		games_words(line->games, out[count].games, sizeof(out[count].games));
		snprintf(out[count].count, sizeof(out[count].count), "%s  %ld%%", out[count].games, ae_rs_percent(line->wins, line->games));
		out[count].on = kind == best;
		count++;
	}
	return count;
}

int ae_rc_chips_light(struct ae_rc_chip *chips, int count, int kind)
{
	int index, lit = -1;

	for (index = 0; index < count; index++)
	{
		chips[index].on = kind >= 0 && kind < AE_RS_KINDS && chips[index].kind == kind;
		if (chips[index].on)
			lit = index;
	}
	return lit;
}

static void hero(struct ae_rc_column *column, char const *label, char const *value, char const *side)
{
	struct ae_rc_hero *h = &column->hero[column->hero_count++];

	snprintf(h->label, sizeof(h->label), "%s", label);
	snprintf(h->value, sizeof(h->value), "%s", value);
	snprintf(h->side, sizeof(h->side), "%s", side ? side : "");
	h->dash = !strcmp(value, AE_RV_DASH);
	if (h->dash)
		h->side[0] = 0;
}

/* a hero of a count: the total (or the dash) and its per game when the divisor is right */
static void hero_count(struct ae_rc_column *column, char const *label, long total, int known, long games, int per_game)
{
	char value[32], side[40], per[24];

	if (!known)
	{
		hero(column, label, AE_RV_DASH, NULL);
		return;
	}
	ae_rs_number_text(total, value, sizeof(value));
	side[0] = 0;
	if (per_game && games > 0)
	{
		tenth(total, games, per, sizeof(per));
		snprintf(side, sizeof(side), "%s A GAME", per);
	}
	hero(column, label, value, side);
}

/* a hero of a time in seconds: a clock, and its per game as a clock */
static void hero_clock(struct ae_rc_column *column, char const *label, long seconds, int known, long games, int per_game)
{
	char value[32], side[40], per[24];

	if (!known)
	{
		hero(column, label, AE_RV_DASH, NULL);
		return;
	}
	ae_rc_clock(seconds, value, sizeof(value));
	side[0] = 0;
	if (per_game && games > 0)
	{
		ae_rc_clock((seconds + games / 2) / games, per, sizeof(per));
		snprintf(side, sizeof(side), "%s A GAME", per);
	}
	hero(column, label, value, side);
}

static void hero_ratio(struct ae_rc_column *column, char const *label, long a, long b)
{
	char value[24];

	ae_rs_ratio_text(ae_rs_ratio100(a, b), value, sizeof(value));
	hero(column, label, value, NULL);
}

static void row_text(struct ae_rc_column *column, char const *label, char const *value)
{
	struct ae_rc_row *r;

	if (column->row_count >= AE_RC_ROWS)
		return;
	r = &column->row[column->row_count++];
	snprintf(r->label, sizeof(r->label), "%s", label);
	snprintf(r->value, sizeof(r->value), "%s", value);
	r->dash = !strcmp(value, AE_RV_DASH);
}

static void row_count(struct ae_rc_column *column, char const *label, long value, int known)
{
	char text[32];

	if (known)
		ae_rs_number_text(value, text, sizeof(text));
	else
		snprintf(text, sizeof(text), "%s", AE_RV_DASH);
	row_text(column, label, text);
}

static void row_ratio(struct ae_rc_column *column, char const *label, long a, long b)
{
	char text[24];

	ae_rs_ratio_text(ae_rs_ratio100(a, b), text, sizeof(text));
	row_text(column, label, text);
}

/* the medals of a list earned, rarest first (the MEDALS page's order) */
static void column_medals(struct ae_rc_column *column, unsigned long const *counts, int const *list, int category)
{
	int ids[AE_MEDAL_COUNT], count, index, at;

	column->medal_count = 0;
	if (!counts)
		return;
	count = ae_rm_list(counts, AE_RM_SORT_RAREST, category, ids);
	column->medals_earned = 0;
	for (index = 0; index < count; index++)
	{
		int keep = !list;

		if (!counts[ids[index]])
			continue;
		for (at = 0; list && list[at] >= 0; at++)
			keep |= list[at] == ids[index];
		if (!keep)
			continue;
		column->medals_earned++;
		if (column->medal_count >= AE_RC_COLUMN_MEDALS)
			continue;
		column->medal[column->medal_count] = ids[index];
		column->medal_n[column->medal_count++] = counts[ids[index]];
	}
	if (!column->medal_count)
		snprintf(column->medals_empty, sizeof(column->medals_empty), "%s", "None earned yet.");
}

/* "FROM 196 OF 218 GAMES" after the words, or nothing while every game is covered */
static void coverage(char *out, size_t size, char const *what, long counted, long games)
{
	size_t used = strlen(out);
	char a[24], b[24];

	if (games <= 0 || counted >= games || used + 1 >= size)
		return;
	ae_rs_number_text(counted > 0 ? counted : 0, a, sizeof(a));
	ae_rs_number_text(games, b, sizeof(b));
	snprintf(out + used, size - used, "%s%s FROM %s OF %s GAMES", used ? "; " : "", what, a, b);
}

static void mode_column(struct ae_rv_in const *in, int kind, struct ae_rc_column *column)
{
	struct ae_rs_stats const *x = in->x;
	struct ae_sr_line const *line = &in->kind[kind];
	int counted = x && ae_rs_known(x), v3 = x && ae_rs_known_v3(x);
	int per2 = ae_rc_per_game_known(in, 0), per3 = ae_rc_per_game_known(in, 1);
	/* (a bot's numbers cover its counted games, a player's notes are of every game kept) */
	long base = in->spree_from_record ? (x ? x->counted : 0) : in->games;
	long games = line->games, losses = line->games - line->wins;
	char games_text[24], text[32];
	int const *medals = flag_medals;

	memset(column, 0, sizeof(*column));
	column->kind = kind;
	snprintf(column->name, sizeof(column->name), "%s", ae_sr_mode_name(kind));
	games_words(games, games_text, sizeof(games_text));
	snprintf(column->games, sizeof(column->games), "%s  %ld W  %ld L", games_text, line->wins, losses > 0 ? losses : 0);
	snprintf(column->meter_word, sizeof(column->meter_word), "%ld%% WON", ae_rs_percent(line->wins, games));
	column->meter = 1;
	column->fill = games > 0 ? (float)line->wins / (float)games : 0.0f;
	switch (kind)
	{
	case AE_RS_KIND_FLAG:
		hero_count(column, "CAPTURES", x ? x->flag_caps : 0, counted, games, per2);
		hero_count(column, "RETURNS", x ? x->flag_returns : 0, counted, games, per2);
		hero_count(column, "CARRIERS KILLED", in->medals ? (long)in->medals[AE_MEDAL_FLAG_CARRIER_KILL] : 0, in->medals != NULL,
			games, 1);
		row_count(column, "GRABS", x ? x->flag_grabs : 0, counted);
		row_count(column, "KILLS AS CARRIER", x ? x->as_carrier : 0, v3);
		if (v3)
			ae_rc_clock(x->carrier_seconds, text, sizeof(text));
		else
			snprintf(text, sizeof(text), "%s", AE_RV_DASH);
		row_text(column, "TIME AS CARRIER", text);
		coverage(column->note, sizeof(column->note), "CAPTURES, RETURNS AND GRABS", x ? x->counted : 0, base);
		coverage(column->note, sizeof(column->note), "KILLS AND TIME AS CARRIER", x ? x->counted_v3 : 0, base);
		medals = flag_medals;
		break;
	case AE_RS_KIND_HILL:
		hero_clock(column, "HILL TIME", x ? x->hill_ticks / AE_PGCR_TICKS_PER_SECOND : 0, counted, games, per2);
		hero_count(column, "KILLS", line->kills, 1, games, 1);
		hero_ratio(column, "K/D", line->kills, line->deaths);
		row_count(column, "DEATHS", line->deaths, 1);
		row_count(column, "ASSISTS", line->assists, 1);
		coverage(column->note, sizeof(column->note), "HILL TIME", x ? x->counted : 0, base);
		medals = hill_medals;
		break;
	case AE_RS_KIND_BALL:
		hero_clock(column, "BALL TIME", x ? x->ball_ticks / AE_PGCR_TICKS_PER_SECOND : 0, counted, games, per2);
		hero_count(column, "CARRIER KILLS", x ? x->ball_carrier_kills : 0, counted, games, per2);
		hero_count(column, "KILLS AS CARRIER", x ? x->as_ball_carrier : 0, counted, games, per2);
		row_ratio(column, "K/D", line->kills, line->deaths);
		row_count(column, "ASSISTS", line->assists, 1);
		coverage(column->note, sizeof(column->note), "BALL NUMBERS", x ? x->counted : 0, base);
		medals = ball_medals;
		break;
	case AE_RS_KIND_BOMB:
		hero_count(column, "PLANTS", x ? x->bomb_plants : 0, v3, games, per3);
		hero_count(column, "DEFUSES", x ? x->bomb_defuses : 0, v3, games, per3);
		hero_ratio(column, "K/D", line->kills, line->deaths);
		row_count(column, "KILLS", line->kills, 1);
		row_count(column, "ASSISTS", line->assists, 1);
		coverage(column->note, sizeof(column->note), "PLANTS AND DEFUSES", x ? x->counted_v3 : 0, base);
		medals = bomb_medals;
		break;
	case AE_RS_KIND_SLAYER:
	case AE_RS_KIND_OTHER:
	{
		long spread = line->kills - line->deaths;
		char number[24];

		/* (the kind's line counts every game of the kind: its per game always has its divisor) */
		hero_count(column, "KILLS", line->kills, 1, games, 1);
		hero_ratio(column, "K/D", line->kills, line->deaths);
		hero_count(column, "ASSISTS", line->assists, 1, games, 1);
		row_count(column, "DEATHS", line->deaths, 1);
		ae_rs_number_text(spread < 0 ? -spread : spread, number, sizeof(number));
		snprintf(text, sizeof(text), "%s%s", spread > 0 ? "+" : spread < 0 ? "-" : "", number);
		row_text(column, "KILL SPREAD", text);
		medals = kind == AE_RS_KIND_SLAYER ? slayer_medals : other_medals;
		break;
	}
	case AE_RS_KIND_RACE:
	default:
		if (counted && x->best_lap > 0)
		{
			ae_rc_clock(x->best_lap / AE_PGCR_TICKS_PER_SECOND, text, sizeof(text));
			hero(column, "BEST LAP", text, "PERSONAL BEST");
		}
		else
			hero(column, "BEST LAP", AE_RV_DASH, NULL);
		hero_count(column, "LAPS", x ? x->laps : 0, counted, games, per2);
		hero_count(column, "KILLS", line->kills, 1, games, 1);
		row_ratio(column, "K/D", line->kills, line->deaths);
		row_count(column, "ASSISTS", line->assists, 1);
		coverage(column->note, sizeof(column->note), "LAPS", x ? x->counted : 0, base);
		medals = race_medals;
		break;
	}
	snprintf(column->medals_head, sizeof(column->medals_head), "%s MEDALS", ae_sr_mode_name(kind));
	column_medals(column, in->medals, medals, AE_RM_ALL);
	if (medals[0] < 0)
		snprintf(column->medals_empty, sizeof(column->medals_empty), "%s games have no medals of their own.",
			kind == AE_RS_KIND_BOMB ? "Bomb" : "These");
}

int ae_rc_mode_columns(struct ae_rv_in const *in, struct ae_rc_column *out, int *not_played, int *not_played_count)
{
	int index, count = 0, missing = 0, best = -1;

	for (index = 0; index < AE_RC_MODE_COLUMNS; index++)
		if (in->kind[mode_kinds[index]].games > 0)
			mode_column(in, mode_kinds[index], &out[count++]);
	for (index = 0; index < AE_RC_OBJECTIVE_KINDS; index++)
		if (in->kind[objective_kinds[index]].games <= 0 && not_played)
			not_played[missing++] = objective_kinds[index];
	if (not_played_count)
		*not_played_count = missing;
	/* BEST WIN RATE: the column of the best win rate of 5 games or more (the first of equals), when two or more show */
	for (index = 0; count >= 2 && index < count; index++)
	{
		struct ae_sr_line const *line = &in->kind[out[index].kind];
		struct ae_sr_line const *was = best >= 0 ? &in->kind[out[best].kind] : NULL;

		if (line->games >= 5 && (!was || line->wins * was->games > was->wins * line->games))
			best = index;
	}
	if (best >= 0)
		snprintf(out[best].tag, sizeof(out[best].tag), "%s", "BEST WIN RATE");
	return count;
}

int ae_rc_mode_ring(struct ae_rc_column const *columns, int count, int not_played_shows, int kind, int column, int favourite)
{
	int slots = count + (not_played_shows ? 1 : 0), index;

	if (slots <= 0)
		return -1;
	if (kind == AE_RC_RING_NOT_PLAYED && not_played_shows)
		return count;
	if (kind >= 0)
		column = -1;
	for (index = 0; kind >= 0 && index < count; index++)
		if (columns[index].kind == kind)
			return index;
	if (column >= 0)
		return column < slots ? column : slots - 1;
	for (index = 0; favourite >= 0 && index < count; index++)
		if (columns[index].kind == favourite)
			return index;
	return 0;
}

int ae_rc_mode_shown(int columns, int not_played_shows)
{
	int slots = columns + (not_played_shows ? 1 : 0);

	/* (one width rule for both: the narrowest column is five across, or four beside NOT PLAYED YET) */
	if (columns <= 0 || ae_rc_column_width(columns, not_played_shows) >= ae_rc_column_width(AE_RC_RUN, 0) - 0.01f)
		return slots;
	return slots < AE_RC_RUN ? slots : AE_RC_RUN;
}

int ae_rc_mode_first(int slots, int shown, int first, int ring)
{
	int last = slots - shown;

	if (last <= 0)
		return 0;
	if (ring >= 0 && ring < first)
		first = ring;
	if (ring >= 0 && ring > first + shown - 1)
		first = ring - shown + 1;
	return first < 0 ? 0 : first > last ? last : first;
}

void ae_rc_not_played_text(char const *name, int bot, int own, char *out, size_t size)
{
	if (bot || !own)
		snprintf(out, size, "A mode gets its column after %s's first game of it on this PC.", name && name[0] ? name : "their");
	else
		snprintf(out, size, "%s", "A mode gets its column after your first game of it on this PC.");
}

float ae_rc_column_width(int columns, int not_played_shows)
{
	float room = (float)AE_RC_ROOM - (not_played_shows ? (float)(AE_RC_NOT_PLAYED_W + AE_RC_GAP) : 0.0f), w;

	if (columns <= 0)
		return 0.0f;
	w = (room - (float)AE_RC_GAP * (float)(columns - 1)) / (float)columns;
	return w < (float)AE_RC_WIDEST ? w : (float)AE_RC_WIDEST;
}

/* ---------- COMBAT */

static void column_head(struct ae_rc_column *column, char const *name)
{
	memset(column, 0, sizeof(*column));
	column->kind = -1;
	snprintf(column->name, sizeof(column->name), "%s", name);
}

int ae_rc_combat_columns(struct ae_rv_in const *in, struct ae_rc_column *out)
{
	struct ae_rs_stats const *x = in->x;
	int counted = x && ae_rs_known(x), v3 = x && ae_rs_known_v3(x), have = in->games > 0, index;
	long n = x ? x->counted : 0, n3 = x ? x->counted_v3 : 0;
	char text[40], number[24];
	struct ae_rc_column *c;

	/* ACCURACY */
	c = &out[0];
	column_head(c, "ACCURACY");
	if (counted)
	{
		ae_rs_number_text(x->fired, number, sizeof(number));
		snprintf(c->games, sizeof(c->games), "%s SHOTS", number);
		c->meter = 1;
		c->fill = x->fired > 0 ? (float)x->hit / (float)x->fired : 0.0f;
		snprintf(c->meter_word, sizeof(c->meter_word), "%ld%% LANDED", ae_rs_percent(x->hit, x->fired));
	}
	else
		snprintf(c->games, sizeof(c->games), "%s", "NO SHOTS COUNTED YET");
	if (v3 && x->kills_v3 > 0)
	{
		char share[16];

		snprintf(share, sizeof(share), "%ld%%", ae_rs_percent(x->headshots, x->kills_v3));
		ae_rs_number_text(x->headshots, number, sizeof(number));
		snprintf(text, sizeof(text), "%s KILLS", number);
		hero(c, "HEADSHOT SHARE", share, text);
	}
	else
		hero(c, "HEADSHOT SHARE", AE_RV_DASH, NULL);
	if (counted && n > 0)
	{
		tenth(x->hit, n, text, sizeof(text));
		hero(c, "SHOTS LANDED", text, "A GAME");
		tenth(x->fired, n, text, sizeof(text));
		hero(c, "SHOTS FIRED", text, "A GAME");
	}
	else
	{
		hero(c, "SHOTS LANDED", AE_RV_DASH, NULL);
		hero(c, "SHOTS FIRED", AE_RV_DASH, NULL);
	}
	row_count(c, "SHOTS LANDED", x ? x->hit : 0, counted);
	row_count(c, "SHOTS MISSED", x && x->fired > x->hit ? x->fired - x->hit : 0, counted);
	snprintf(c->medals_head, sizeof(c->medals_head), "%s", "SKILL MEDALS");
	column_medals(c, in->medals, NULL, 1 + 2);
	coverage(c->note, sizeof(c->note), "SHOTS", n, in->games);
	coverage(c->note, sizeof(c->note), "HEADSHOTS", n3, in->games);

	/* KILL KINDS */
	c = &out[1];
	column_head(c, "KILL KINDS");
	{
		static char const *const labels[AE_RV_KILL_KINDS] = { "HEADSHOTS", "POWER WEAPONS", "MELEE", "GRENADES", "ASSASSINATIONS",
			"SPLATTERS", "STICKS" };
		long counts[AE_RV_KILL_KINDS], most = 0;

		counts[0] = x ? x->headshots : 0; counts[1] = x ? x->power : 0; counts[2] = x ? x->melee : 0;
		counts[3] = x ? x->grenade : 0; counts[4] = x ? x->assassin : 0; counts[5] = x ? x->splatter : 0;
		counts[6] = x ? x->stick : 0;
		if (v3)
		{
			ae_rs_number_text(x->kills_v3, number, sizeof(number));
			snprintf(c->games, sizeof(c->games), "%s KILLS COUNTED", number);
		}
		else
			snprintf(c->games, sizeof(c->games), "%s", "NO KILLS COUNTED YET");
		for (index = 0; index < AE_RV_KILL_KINDS; index++)
			most = counts[index] > most ? counts[index] : most;
		for (index = 0; index < AE_RV_KILL_KINDS; index++)
		{
			struct ae_rc_bar *bar = &c->bar[c->bar_count++];

			snprintf(bar->label, sizeof(bar->label), "%s", labels[index]);
			if (v3)
			{
				ae_rs_number_text(counts[index], bar->count, sizeof(bar->count));
				snprintf(bar->share, sizeof(bar->share), "%ld%%", ae_rs_percent(counts[index], x->kills_v3));
				bar->length = most > 0 ? (float)counts[index] / (float)most : 0.0f;
			}
			else
			{
				snprintf(bar->count, sizeof(bar->count), "%s", "");
				snprintf(bar->share, sizeof(bar->share), "%s", AE_RV_DASH);
			}
		}
		if (v3 && n3 < in->games)
		{
			char a[24], b[24];

			ae_rs_number_text(n3, a, sizeof(a));
			ae_rs_number_text(in->games, b, sizeof(b));
			snprintf(c->note, sizeof(c->note), "SHARE OF THE KILLS FROM %s OF %s GAMES; A KILL CAN BE TWO KINDS", a, b);
		}
		else if (v3)
			snprintf(c->note, sizeof(c->note), "%s", "A KILL CAN BE TWO KINDS");
	}

	/* DAMAGE */
	c = &out[2];
	column_head(c, "DAMAGE");
	if (v3)
	{
		ae_rs_number_text(x->dealt, number, sizeof(number));
		snprintf(c->games, sizeof(c->games), "%s DEALT", number);
		c->meter = 1;
		c->fill = x->dealt + x->taken > 0 ? (float)x->dealt / (float)(x->dealt + x->taken) : 0.0f;
		ae_rs_ratio_text(ae_rs_ratio100(x->dealt, x->taken), number, sizeof(number));
		snprintf(c->meter_word, sizeof(c->meter_word), "%s RATIO", number);
	}
	else
		snprintf(c->games, sizeof(c->games), "%s", "NO DAMAGE COUNTED YET");
	if (v3 && x->seconds_v3 > 0)
	{
		tenth(x->dealt * 60, x->seconds_v3, text, sizeof(text));
		hero(c, "DEALT PER MINUTE", text, NULL);
		tenth(x->taken * 60, x->seconds_v3, text, sizeof(text));
		hero(c, "TAKEN PER MINUTE", text, NULL);
	}
	else
	{
		hero(c, "DEALT PER MINUTE", AE_RV_DASH, NULL);
		hero(c, "TAKEN PER MINUTE", AE_RV_DASH, NULL);
	}
	if (v3 && x->hit_v3 > 0)
	{
		tenth(x->dealt, x->hit_v3, text, sizeof(text));
		hero(c, "DEALT PER HIT", text, NULL);
	}
	else
		hero(c, "DEALT PER HIT", AE_RV_DASH, NULL);
	row_count(c, "DAMAGE DEALT", x ? x->dealt : 0, v3);
	row_count(c, "DAMAGE TAKEN", x ? x->taken : 0, v3);
	coverage(c->note, sizeof(c->note), "DAMAGE", n3, in->games);

	/* PER GAME */
	c = &out[3];
	column_head(c, "PER GAME");
	games_words(in->games, c->games, sizeof(c->games));
	if (have)
	{
		tenth(in->kills, in->games, text, sizeof(text));
		hero(c, "KILLS", text, NULL);
		tenth(in->deaths, in->games, text, sizeof(text));
		hero(c, "DEATHS", text, NULL);
		tenth(in->assists, in->games, text, sizeof(text));
		hero(c, "ASSISTS", text, NULL);
	}
	else
	{
		hero(c, "KILLS", AE_RV_DASH, NULL);
		hero(c, "DEATHS", AE_RV_DASH, NULL);
		hero(c, "ASSISTS", AE_RV_DASH, NULL);
	}
	row_count(c, "DAMAGE DEALT", v3 && n3 > 0 ? (x->dealt + n3 / 2) / n3 : 0, v3 && n3 > 0);
	if (counted && n > 0)
		ae_rc_clock((x->seconds + n / 2) / n, text, sizeof(text));
	else
		snprintf(text, sizeof(text), "%s", AE_RV_DASH);
	row_text(c, "TIME PLAYED", text);
	row_count(c, "BEST SPREE", in->spree_from_record ? in->best_spree : x ? x->best_spree : 0,
		in->spree_from_record ? have : counted);
	snprintf(c->medals_head, sizeof(c->medals_head), "%s", "ASSIST MEDALS");
	column_medals(c, in->medals, assist_medals, AE_RM_ALL);
	coverage(c->note, sizeof(c->note), "DAMAGE", n3, in->games);
	return AE_RC_COMBAT_COLUMNS;
}

/* ---------- WEAPONS & MAPS */

void ae_rc_weapons_column(struct ae_rv_in const *in, struct ae_rc_column *out)
{
	struct ae_rs_stats const *x = in->x;
	int count = in->weapon_count < AE_RS_WEAPONS ? in->weapon_count : AE_RS_WEAPONS, index;
	/* (a player's top weapons are counted with the new numbers: a share of all kills only while every game was counted; a
	bot's are kept from every game) */
	int of_all = in->spree_from_record || (x && x->counted > 0 && x->counted == in->games);
	long whole = 0, most = 0;

	column_head(out, "TOP WEAPONS");
	for (index = 0; index < count; index++)
	{
		whole += of_all ? 0 : in->weapons[index].kills;
		most = in->weapons[index].kills > most ? in->weapons[index].kills : most;
	}
	if (of_all)
		whole = in->kills;
	snprintf(out->games, sizeof(out->games), "%s", of_all ? "SHARE OF ALL KILLS" : "SHARE OF TOP 5");
	for (index = 0; index < count && index < AE_RC_BARS; index++)
	{
		struct ae_rc_bar *bar = &out->bar[out->bar_count++];

		upper(in->weapons[index].name, bar->label, sizeof(bar->label));
		ae_rs_number_text(in->weapons[index].kills, bar->count, sizeof(bar->count));
		snprintf(bar->share, sizeof(bar->share), "%ld%%", ae_rs_percent(in->weapons[index].kills, whole));
		bar->length = most > 0 ? (float)in->weapons[index].kills / (float)most : 0.0f;
	}
	if (!count)
		snprintf(out->medals_empty, sizeof(out->medals_empty), "%s", "No kills with a weapon counted yet.");
	if (!in->spree_from_record)
		coverage(out->note, sizeof(out->note), "WEAPONS", x ? x->counted : 0, in->games);
}

int ae_rc_maps(struct ae_rv_in const *in, struct ae_rc_map_row *out, char *head, size_t size)
{
	int order[AE_RS_MAPS], count, index;

	count = in->maps ? ae_rs_map_order(in->maps, in->map_count, order, AE_RC_MAP_ROWS) : 0;
	for (index = 0; index < count; index++)
	{
		struct ae_rs_map const *map = &in->maps[order[index]];

		upper(map->name, out[index].name, sizeof(out[index].name));
		ae_rs_number_text(map->games, out[index].games, sizeof(out[index].games));
		snprintf(out[index].win_rate, sizeof(out[index].win_rate), "%ld%%", ae_rs_percent(map->wins, map->games));
		ae_rs_ratio_text(ae_rs_ratio100(map->kills, map->deaths), out[index].kd, sizeof(out[index].kd));
		out[index].won = map->games > 0 ? (float)map->wins / (float)map->games : 0.0f;
	}
	if (head && size)
		snprintf(head, size, "%d MAP%s", in->map_count, in->map_count == 1 ? "" : "S");
	return count;
}

/* ---------- CAREER's RECORDS card */

void ae_rc_card(struct ae_rv_in const *in, struct ae_sr_last const *last, struct ae_rc_column *out)
{
	struct ae_rv_cell highlights[AE_RV_HIGHLIGHTS];
	long losses = in->games - in->wins - in->ties, medals = ae_rv_medal_total(in->medals);
	int have = in->games > 0, best = ae_rc_favourite_kind(in);
	char games[24], text[64], number[24];

	column_head(out, "");
	games_words(in->games, games, sizeof(games));
	if (in->ties > 0)
		snprintf(out->games, sizeof(out->games), "%s  %ld W  %ld L  %ld T", games, in->wins, losses > 0 ? losses : 0, in->ties);
	else
		snprintf(out->games, sizeof(out->games), "%s  %ld W  %ld L", games, in->wins, losses > 0 ? losses : 0);
	out->meter = have;
	out->fill = have ? (float)in->wins / (float)in->games : 0.0f;
	if (have)
		snprintf(out->meter_word, sizeof(out->meter_word), "%ld%% WON", ae_rs_percent(in->wins, in->games));
	if (have)
	{
		tenth(in->kills, in->games, text, sizeof(text));
		ae_rs_ratio_text(ae_rs_ratio100(in->kills, in->deaths), number, sizeof(number));
		{
			char side[32];

			snprintf(side, sizeof(side), "K/D %s", number);
			hero(out, "KILLS A GAME", text, side);
		}
		ae_rs_number_text(medals, number, sizeof(number));
		tenth(medals, in->games, text, sizeof(text));
		{
			char side[80];

			snprintf(side, sizeof(side), "%s A GAME", text);
			hero(out, "MEDALS", number, side);
		}
	}
	else
	{
		hero(out, "KILLS A GAME", AE_RV_DASH, NULL);
		hero(out, "MEDALS", AE_RV_DASH, NULL);
	}
	if (best >= 0)
	{
		games_words(in->kind[best].games, games, sizeof(games));
		hero(out, "FAVOURITE KIND", ae_sr_mode_name(best), games);
	}
	else
		hero(out, "FAVOURITE KIND", AE_RV_DASH, NULL);
	if (last && last->has)
	{
		char map[AE_RS_MAP_NAME];

		upper(last->map, map, sizeof(map));
		snprintf(text, sizeof(text), "%s, %s", last->result == 0 ? "WIN" : last->result == 2 ? "TIE" : "LOSS", map[0] ? map :
			ae_sr_mode_name(last->mode));
		row_text(out, "LAST GAME", text);
	}
	ae_rv_highlights(in, 0, highlights);
	row_text(out, "BEST MAP", highlights[2].value);
	row_text(out, "MOST KILLS WITH", highlights[0].value);
}

/* ---------- MEDALS */

int ae_rc_medal_columns(struct ae_rv_in const *in, struct ae_rc_column *out)
{
	static unsigned long const none[AE_MEDAL_COUNT];
	unsigned long const *counts = in->medals ? in->medals : none;
	int family;

	for (family = 0; family < AE_RC_MEDAL_COLUMNS; family++)
	{
		struct ae_rc_column *c = &out[family];
		int category = 1 + family, earned = ae_rm_earned(counts, category);
		unsigned long total = 0;
		int ids[AE_MEDAL_COUNT], count = ae_rm_list(counts, AE_RM_SORT_RAREST, category, ids), index;
		char number[24];

		column_head(c, ae_rm_category_name(category));
		for (index = 0; index < count; index++)
			total += counts[ids[index]];
		ae_rs_number_text((long)total, number, sizeof(number));
		snprintf(c->games, sizeof(c->games), "%d OF %d KINDS  %s EARNED", earned, ae_rm_kinds(category), number);
		c->meter = 1;
		c->fill = ae_rm_kinds(category) > 0 ? (float)earned / (float)ae_rm_kinds(category) : 0.0f;
		snprintf(c->medals_head, sizeof(c->medals_head), "%s", "RAREST FIRST");
		column_medals(c, counts, NULL, category);
	}
	return AE_RC_MEDAL_COLUMNS;
}
