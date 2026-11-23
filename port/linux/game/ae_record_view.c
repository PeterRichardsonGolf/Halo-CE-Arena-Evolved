/* What the SERVICE RECORD pages show (see the header). */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ae_record_focus.h"

#include "ae_record_view.h"

void ae_rv_cell_set(struct ae_rv_cell *cell, char const *label, char const *value)
{
	snprintf(cell->label, sizeof(cell->label), "%s", label);
	snprintf(cell->value, sizeof(cell->value), "%s", value);
	cell->dash = !strcmp(value, AE_RV_DASH);
}

void ae_rv_cell_dash(struct ae_rv_cell *cell, char const *label)
{
	ae_rv_cell_set(cell, label, AE_RV_DASH);
}

void ae_rv_cell_whole(struct ae_rv_cell *cell, char const *label, long value)
{
	char text[32];

	ae_rs_number_text(value, text, sizeof(text));
	ae_rv_cell_set(cell, label, text);
}

/* a over b with one decimal (b > 0): "15.3" */
static void tenth_text(long a, long b, char *out, size_t size)
{
	long tenths = b > 0 ? (a * 10 + b / 2) / b : 0;
	char whole_text[32];

	ae_rs_number_text(tenths / 10, whole_text, sizeof(whole_text));
	snprintf(out, size, "%s.%ld", whole_text, tenths % 10);
}

void ae_rv_cell_tenth(struct ae_rv_cell *cell, char const *label, long a, long b, int known)
{
	char text[32];

	if (!known || b <= 0)
		ae_rv_cell_dash(cell, label);
	else
	{
		tenth_text(a, b, text, sizeof(text));
		ae_rv_cell_set(cell, label, text);
	}
}

void ae_rv_cell_ratio(struct ae_rv_cell *cell, char const *label, long a, long b, int known)
{
	char text[32];

	if (!known)
		ae_rv_cell_dash(cell, label);
	else
	{
		ae_rs_ratio_text(ae_rs_ratio100(a, b), text, sizeof(text));
		ae_rv_cell_set(cell, label, text);
	}
}

void ae_rv_cell_percent(struct ae_rv_cell *cell, char const *label, long part, long whole_count, int known)
{
	char text[32];

	if (!known || whole_count <= 0)
		ae_rv_cell_dash(cell, label);
	else
	{
		snprintf(text, sizeof(text), "%ld%%", ae_rs_percent(part, whole_count));
		ae_rv_cell_set(cell, label, text);
	}
}

static void upper(char const *text, char *out, size_t size)
{
	size_t at = 0;

	for (; text && *text && at + 1 < size; text++)
		out[at++] = *text >= 'a' && *text <= 'z' ? (char)(*text - 32) : *text;
	if (size)
		out[at] = 0;
}

int ae_rv_medal_kinds(unsigned long const *medals)
{
	int index, kinds = 0;

	for (index = 0; medals && index < AE_MEDAL_COUNT; index++)
		kinds += medals[index] != 0;
	return kinds;
}

long ae_rv_medal_total(unsigned long const *medals)
{
	long total = 0;
	int index;

	for (index = 0; medals && index < AE_MEDAL_COUNT; index++)
		total += (long)medals[index];
	return total;
}

int ae_rv_totals(struct ae_rv_in const *in, struct ae_rv_cell *out)
{
	struct ae_rs_stats const *x = in->x;
	long losses = in->games - in->wins - in->ties;
	int have = in->games > 0, counted = x && ae_rs_known(x), v3 = x && ae_rs_known_v3(x);
	char text[32];

	if (losses < 0)
		losses = 0;
	/* first column */
	if (have) ae_rv_cell_whole(&out[0], "GAMES", in->games); else ae_rv_cell_dash(&out[0], "GAMES");
	if (have) ae_rv_cell_whole(&out[1], "WINS", in->wins); else ae_rv_cell_dash(&out[1], "WINS");
	if (have) ae_rv_cell_whole(&out[2], "LOSSES", losses); else ae_rv_cell_dash(&out[2], "LOSSES");
	if (have) ae_rv_cell_whole(&out[3], "TIES", in->ties); else ae_rv_cell_dash(&out[3], "TIES");
	ae_rv_cell_percent(&out[4], "WIN RATE", in->wins, in->games, have);
	if (in->spree_from_record ? have : counted)
		ae_rv_cell_whole(&out[5], "BEST SPREE", in->spree_from_record ? in->best_spree : x->best_spree);
	else
		ae_rv_cell_dash(&out[5], "BEST SPREE");
	if (counted)
	{
		ae_rs_time_text(x->seconds, text, sizeof(text));
		ae_rv_cell_set(&out[6], "TIME PLAYED", text);
	}
	else
		ae_rv_cell_dash(&out[6], "TIME PLAYED");
	if (have) ae_rv_cell_whole(&out[7], "MEDALS", ae_rv_medal_total(in->medals)); else ae_rv_cell_dash(&out[7], "MEDALS");
	/* second column */
	if (have) ae_rv_cell_whole(&out[8], "KILLS", in->kills); else ae_rv_cell_dash(&out[8], "KILLS");
	if (have) ae_rv_cell_whole(&out[9], "DEATHS", in->deaths); else ae_rv_cell_dash(&out[9], "DEATHS");
	if (have)
	{
		long spread = in->kills - in->deaths;

		snprintf(text, sizeof(text), "%s", "");
		ae_rs_number_text(spread < 0 ? -spread : spread, text, sizeof(text));
		{
			char signed_text[40];

			snprintf(signed_text, sizeof(signed_text), "%s%s", spread > 0 ? "+" : spread < 0 ? "-" : "", text);
			ae_rv_cell_set(&out[10], "KILL SPREAD", signed_text);
		}
	}
	else
		ae_rv_cell_dash(&out[10], "KILL SPREAD");
	if (have) ae_rv_cell_whole(&out[11], "ASSISTS", in->assists); else ae_rv_cell_dash(&out[11], "ASSISTS");
	ae_rv_cell_ratio(&out[12], "K/D", in->kills, in->deaths, have);
	if (have)
	{
		ae_rs_ratio_text(ae_rs_kda100(in->kills, in->assists, in->deaths), text, sizeof(text));
		ae_rv_cell_set(&out[13], "KDA", text);
	}
	else
		ae_rv_cell_dash(&out[13], "KDA");
	ae_rv_cell_percent(&out[14], "ACCURACY", x ? x->hit : 0, x ? x->fired : 0, counted);
	ae_rv_cell_ratio(&out[15], "DAMAGE RATIO", x ? x->dealt : 0, x ? x->taken : 0, v3 && x->taken > 0);
	return AE_RV_TOTALS;
}

int ae_rv_accuracy(struct ae_rv_in const *in, struct ae_rv_cell *out, char *tail)
{
	struct ae_rs_stats const *x = in->x;
	int counted = x && ae_rs_known(x), v3 = x && ae_rs_known_v3(x);

	ae_rs_from_games(x ? x->counted : 0, in->games, tail, AE_RV_TAIL);
	if (counted)
	{
		ae_rv_cell_whole(&out[0], "SHOTS FIRED", x->fired);
		ae_rv_cell_whole(&out[1], "SHOTS LANDED", x->hit);
		ae_rv_cell_whole(&out[2], "SHOTS MISSED", x->fired > x->hit ? x->fired - x->hit : 0);
	}
	else
	{
		ae_rv_cell_dash(&out[0], "SHOTS FIRED");
		ae_rv_cell_dash(&out[1], "SHOTS LANDED");
		ae_rv_cell_dash(&out[2], "SHOTS MISSED");
	}
	ae_rv_cell_percent(&out[3], "ACCURACY", x ? x->hit : 0, x ? x->fired : 0, counted);
	if (v3)
		ae_rv_cell_whole(&out[4], "HEADSHOT KILLS", x->headshots);
	else
		ae_rv_cell_dash(&out[4], "HEADSHOT KILLS");
	ae_rv_cell_percent(&out[5], "HEADSHOT SHARE", x ? x->headshots : 0, x ? x->kills_v3 : 0, v3);
	return AE_RV_ACCURACY;
}

int ae_rv_kill_kinds(struct ae_rv_in const *in, struct ae_rv_share *out, char *tail)
{
	struct ae_rs_stats const *x = in->x;
	int v3 = x && ae_rs_known_v3(x), index;
	char const *const labels[AE_RV_KILL_KINDS] = { "HEADSHOTS", "MELEE", "POWER WEAPONS", "GRENADES", "ASSASSINATIONS",
		"SPLATTERS", "STICKS" };
	long counts[AE_RV_KILL_KINDS];

	ae_rs_from_games(x ? x->counted_v3 : 0, in->games, tail, AE_RV_TAIL);
	counts[0] = x ? x->headshots : 0; counts[1] = x ? x->melee : 0; counts[2] = x ? x->power : 0;
	counts[3] = x ? x->grenade : 0; counts[4] = x ? x->assassin : 0; counts[5] = x ? x->splatter : 0;
	counts[6] = x ? x->stick : 0;
	for (index = 0; index < AE_RV_KILL_KINDS; index++)
	{
		snprintf(out[index].label, sizeof(out[index].label), "%s", labels[index]);
		if (!v3)
		{
			snprintf(out[index].count, sizeof(out[index].count), "%s", AE_RV_DASH);
			snprintf(out[index].share, sizeof(out[index].share), "%s", AE_RV_DASH);
			continue;
		}
		ae_rs_number_text(counts[index], out[index].count, sizeof(out[index].count));
		if (counts[index] > 0 && x->kills_v3 > 0)
			snprintf(out[index].share, sizeof(out[index].share), "%ld%%", ae_rs_percent(counts[index], x->kills_v3));
		else
			snprintf(out[index].share, sizeof(out[index].share), "%s", AE_RV_DASH);
	}
	return AE_RV_KILL_KINDS;
}

int ae_rv_assists(struct ae_rv_in const *in, struct ae_rv_cell *out)
{
	int have = in->games > 0;

	if (have) ae_rv_cell_whole(&out[0], "ASSISTS", in->assists); else ae_rv_cell_dash(&out[0], "ASSISTS");
	ae_rv_cell_tenth(&out[1], "ASSISTS PER GAME", in->assists, in->games, have);
	ae_rv_cell_whole(&out[2], "WINGMAN", in->medals ? (long)in->medals[AE_MEDAL_WINGMAN] : 0);
	ae_rv_cell_whole(&out[3], "DRIVER ASSISTS", in->medals ? (long)in->medals[AE_MEDAL_DRIVER] : 0);
	ae_rv_cell_whole(&out[4], "PROTECTOR", in->medals ? (long)in->medals[AE_MEDAL_PROTECTOR] : 0);
	return AE_RV_ASSISTS;
}

int ae_rv_damage(struct ae_rv_in const *in, struct ae_rv_cell *out, char *tail)
{
	struct ae_rs_stats const *x = in->x;
	int v3 = x && ae_rs_known_v3(x);

	ae_rs_from_games(x ? x->counted_v3 : 0, in->games, tail, AE_RV_TAIL);
	if (v3) ae_rv_cell_whole(&out[0], "DAMAGE DEALT", x->dealt); else ae_rv_cell_dash(&out[0], "DAMAGE DEALT");
	if (v3) ae_rv_cell_whole(&out[1], "DAMAGE TAKEN", x->taken); else ae_rv_cell_dash(&out[1], "DAMAGE TAKEN");
	ae_rv_cell_ratio(&out[2], "DAMAGE RATIO", x ? x->dealt : 0, x ? x->taken : 0, v3 && x->taken > 0);
	ae_rv_cell_tenth(&out[3], "DEALT PER MINUTE", x ? x->dealt * 60 : 0, x ? x->seconds_v3 : 0, v3);
	ae_rv_cell_tenth(&out[4], "TAKEN PER MINUTE", x ? x->taken * 60 : 0, x ? x->seconds_v3 : 0, v3);
	ae_rv_cell_tenth(&out[5], "DEALT PER HIT", x ? x->dealt : 0, x ? x->hit_v3 : 0, v3);
	return AE_RV_DAMAGE;
}

int ae_rv_per_game(struct ae_rv_in const *in, struct ae_rv_cell *out, char *tail)
{
	struct ae_rs_stats const *x = in->x;
	int have = in->games > 0, v3 = x && ae_rs_known_v3(x);

	ae_rs_from_games(x ? x->counted_v3 : 0, in->games, tail, AE_RV_TAIL);
	ae_rv_cell_tenth(&out[0], "KILLS", in->kills, in->games, have);
	ae_rv_cell_tenth(&out[1], "DEATHS", in->deaths, in->games, have);
	ae_rv_cell_tenth(&out[2], "ASSISTS", in->assists, in->games, have);
	if (v3)
		ae_rv_cell_whole(&out[3], "DAMAGE DEALT", (x->dealt + x->counted_v3 / 2) / x->counted_v3);
	else
		ae_rv_cell_dash(&out[3], "DAMAGE DEALT");
	return AE_RV_PER_GAME;
}

int ae_rv_objectives(struct ae_rv_in const *in, struct ae_rv_cell *out, char *tail)
{
	struct ae_rs_stats const *x = in->x;
	int counted = x && ae_rs_known(x), v3 = x && ae_rs_known_v3(x);
	int flag = in->kind[AE_RS_KIND_FLAG].games > 0, hill = in->kind[AE_RS_KIND_HILL].games > 0,
		ball = in->kind[AE_RS_KIND_BALL].games > 0, bomb = in->kind[AE_RS_KIND_BOMB].games > 0,
		race = in->kind[AE_RS_KIND_RACE].games > 0;
	char text[32];

	ae_rs_from_games(x ? x->counted : 0, in->games, tail, AE_RV_TAIL);
	if (flag && counted) ae_rv_cell_whole(&out[0], "FLAG CAPTURES", x->flag_caps); else ae_rv_cell_dash(&out[0], "FLAG CAPTURES");
	if (flag && counted) ae_rv_cell_whole(&out[1], "FLAG RETURNS", x->flag_returns); else ae_rv_cell_dash(&out[1], "FLAG RETURNS");
	if (flag && counted) ae_rv_cell_whole(&out[2], "FLAG GRABS", x->flag_grabs); else ae_rv_cell_dash(&out[2], "FLAG GRABS");
	if (flag && v3) ae_rv_cell_whole(&out[3], "KILLS AS FLAG CARRIER", x->as_carrier); else ae_rv_cell_dash(&out[3], "KILLS AS FLAG CARRIER");
	if (flag && in->medals)
		ae_rv_cell_whole(&out[4], "FLAG CARRIERS KILLED", (long)in->medals[AE_MEDAL_FLAG_CARRIER_KILL]);
	else
		ae_rv_cell_dash(&out[4], "FLAG CARRIERS KILLED");
	if (hill && counted)
	{
		ae_rs_clock_text(x->hill_ticks, text, sizeof(text));
		ae_rv_cell_set(&out[5], "HILL TIME", text);
	}
	else
		ae_rv_cell_dash(&out[5], "HILL TIME");
	if (ball && counted)
	{
		ae_rs_clock_text(x->ball_ticks, text, sizeof(text));
		ae_rv_cell_set(&out[6], "BALL TIME", text);
	}
	else
		ae_rv_cell_dash(&out[6], "BALL TIME");
	if (bomb && v3) ae_rv_cell_whole(&out[7], "BOMB PLANTS", x->bomb_plants); else ae_rv_cell_dash(&out[7], "BOMB PLANTS");
	if (bomb && v3) ae_rv_cell_whole(&out[8], "BOMB DEFUSES", x->bomb_defuses); else ae_rv_cell_dash(&out[8], "BOMB DEFUSES");
	if (race && counted) ae_rv_cell_whole(&out[9], "RACE LAPS", x->laps); else ae_rv_cell_dash(&out[9], "RACE LAPS");
	return AE_RV_OBJECTIVES;
}

int ae_rv_kinds(struct ae_rv_in const *in, struct ae_rv_kind_row *out)
{
	int kind, count = 0;

	for (kind = 0; kind < AE_RS_KINDS; kind++)
	{
		struct ae_sr_line const *line = &in->kind[kind];
		struct ae_rv_kind_row *row = &out[count];

		if (!line->games)
			continue;
		snprintf(row->name, sizeof(row->name), "%s", ae_sr_mode_name(kind));
		ae_rs_number_text(line->games, row->games, sizeof(row->games));
		ae_rs_number_text(line->wins, row->wins, sizeof(row->wins));
		ae_rs_number_text(line->kills, row->kills, sizeof(row->kills));
		ae_rs_number_text(line->deaths, row->deaths, sizeof(row->deaths));
		ae_rs_number_text(line->assists, row->assists, sizeof(row->assists));
		ae_rs_ratio_text(ae_rs_ratio100(line->kills, line->deaths), row->kd, sizeof(row->kd));
		count++;
	}
	return count;
}

int ae_rv_maps(struct ae_rv_in const *in, struct ae_rv_map_row *out, int *more, char *tail)
{
	int order[AE_RS_MAPS], count, index;

	ae_rs_from_games(in->x ? in->x->counted : 0, in->games, tail, AE_RV_TAIL);
	count = in->maps ? ae_rs_map_order(in->maps, in->map_count, order, AE_RV_MAPS) : 0;
	for (index = 0; index < count; index++)
	{
		struct ae_rs_map const *map = &in->maps[order[index]];

		upper(map->name, out[index].name, sizeof(out[index].name));
		ae_rs_number_text(map->games, out[index].games, sizeof(out[index].games));
		ae_rs_number_text(map->wins, out[index].wins, sizeof(out[index].wins));
		ae_rs_ratio_text(ae_rs_ratio100(map->kills, map->deaths), out[index].kd, sizeof(out[index].kd));
	}
	if (more)
		*more = in->map_count > count ? in->map_count - count : 0;
	return count;
}

int ae_rv_weapons(struct ae_rv_in const *in, struct ae_rv_cell *out, char *tail)
{
	int index, count = in->weapon_count < AE_RS_WEAPONS ? in->weapon_count : AE_RS_WEAPONS;
	char name[AE_RS_WEAPON_NAME];

	ae_rs_from_games(in->x ? in->x->counted : 0, in->games, tail, AE_RV_TAIL);
	for (index = 0; index < count; index++)
	{
		upper(in->weapons[index].name, name, sizeof(name));
		ae_rv_cell_whole(&out[index], name, in->weapons[index].kills);
	}
	return count;
}

int ae_rv_highlights(struct ae_rv_in const *in, int rivals, struct ae_rv_cell *out)
{
	char text[AE_RS_MAP_NAME];
	int kind, best = -1, index, map = -1;

	if (in->weapon_count > 0 && in->weapons[0].name[0])
	{
		upper(in->weapons[0].name, text, sizeof(text));
		ae_rv_cell_set(&out[0], "MOST KILLS WITH", text);
	}
	else
		ae_rv_cell_dash(&out[0], "MOST KILLS WITH");
	for (kind = 0; kind < AE_RS_KINDS; kind++)
	{
		struct ae_sr_line const *line = &in->kind[kind];

		if (line->games > 0 && (best < 0 || line->games > in->kind[best].games ||
			(line->games == in->kind[best].games && line->kills > in->kind[best].kills)))
			best = kind;
	}
	if (best >= 0)
		ae_rv_cell_set(&out[1], "FAVOURITE KIND", ae_sr_mode_name(best));
	else
		ae_rv_cell_dash(&out[1], "FAVOURITE KIND");
	for (index = 0; index < in->map_count && in->maps; index++)
		if (in->maps[index].wins > 0 && (map < 0 || in->maps[index].wins > in->maps[map].wins ||
			(in->maps[index].wins == in->maps[map].wins && in->maps[index].games > in->maps[map].games)))
			map = index;
	if (map >= 0)
	{
		upper(in->maps[map].name, text, sizeof(text));
		ae_rv_cell_set(&out[2], "BEST MAP", text);
	}
	else
		ae_rv_cell_dash(&out[2], "BEST MAP");
	ae_rv_cell_whole(&out[3], "MEDAL KINDS", ae_rv_medal_kinds(in->medals));
	ae_rv_cell_whole(&out[4], "RIVALS SET", rivals);
	return AE_RV_HIGHLIGHTS;
}

int ae_rv_last_game(struct ae_sr_last const *last, struct ae_rv_cell *out, char *map, char *kind, size_t size)
{
	static char const *const labels[AE_RV_LAST] = { "RESULT", "KILLS", "DEATHS", "ASSISTS", "K/D", "BEST SPREE",
		"MOST KILLS WITH", "MEDALS" };
	char text[AE_RS_MAP_NAME];
	int index;

	if (size)
		map[0] = kind[0] = 0;
	if (!last || !last->has)
	{
		for (index = 0; index < AE_RV_LAST; index++)
			ae_rv_cell_dash(&out[index], labels[index]);
		return AE_RV_LAST;
	}
	ae_rv_cell_set(&out[0], labels[0], last->result == 0 ? "WIN" : last->result == 2 ? "TIE" : "LOSS");
	ae_rv_cell_whole(&out[1], labels[1], last->kills);
	ae_rv_cell_whole(&out[2], labels[2], last->deaths);
	ae_rv_cell_whole(&out[3], labels[3], last->assists);
	ae_rs_ratio_text(ae_rs_ratio100(last->kills, last->deaths), text, sizeof(text));
	ae_rv_cell_set(&out[4], labels[4], text);
	ae_rv_cell_whole(&out[5], labels[5], last->spree);
	if (last->weapon[0])
	{
		upper(last->weapon, text, sizeof(text));
		ae_rv_cell_set(&out[6], labels[6], text);
	}
	else
		ae_rv_cell_dash(&out[6], labels[6]);
	ae_rv_cell_whole(&out[7], labels[7], last->medals);
	upper(last->map, map, size);
	snprintf(kind, size, "%s", ae_sr_mode_name(last->mode));
	return AE_RV_LAST;
}

void ae_rv_from_player(struct ae_sr_player const *player, struct ae_rv_in *in)
{
	static struct ae_rs_stats const none;
	static unsigned long const no_medals[AE_MEDAL_COUNT];
	int kind;

	memset(in, 0, sizeof(*in));
	in->x = &none;
	in->medals = no_medals;
	if (!player)
		return;
	in->games = player->total.games;
	in->wins = player->total.wins;
	in->kills = player->total.kills;
	in->deaths = player->total.deaths;
	in->assists = player->total.assists;
	in->ties = player->x.ties;
	in->x = &player->x;
	in->medals = player->medals;
	for (kind = 0; kind < AE_RS_KINDS && kind < AE_SR_MODES; kind++)
		in->kind[kind] = player->mode[kind];
	in->maps = player->map;
	in->map_count = player->map_count;
	in->weapons = player->weapons;
	in->weapon_count = player->weapon_count;
}

/* ---------- the DETAILS tables */

void ae_rv_table_row(struct ae_rv_table *table, char const *name, char const *first, ...)
{
	va_list arguments;
	char const *value = first;
	int column = 0;

	if (table->rows >= AE_RV_TABLE_ROWS)
		return;
	snprintf(table->row[table->rows].name, sizeof(table->row[0].name), "%s", name);
	va_start(arguments, first);
	while (value && column < AE_RV_TABLE_COLUMNS)
	{
		snprintf(table->row[table->rows].value[column++], sizeof(table->row[0].value[0]), "%s", value);
		value = va_arg(arguments, char const *);
	}
	va_end(arguments);
	for (; column < AE_RV_TABLE_COLUMNS; column++)
		snprintf(table->row[table->rows].value[column], sizeof(table->row[0].value[0]), "%s", AE_RV_DASH);
	table->rows++;
}

static void heads(struct ae_rv_table *table, char const *a, char const *b, char const *c, char const *d, char const *e, char const *f)
{
	char const *list[AE_RV_TABLE_COLUMNS] = { a, b, c, d, e, f };
	int index;

	for (index = 0; index < AE_RV_TABLE_COLUMNS && list[index]; index++)
	{
		snprintf(table->head[index], sizeof(table->head[0]), "%s", list[index]);
		table->columns = index + 1;
	}
}

/* a number's text: whole with separators, or the dash when not known */
static void num(char *out, long value, int known)
{
	if (known)
		ae_rs_number_text(value, out, 20);
	else
		snprintf(out, 20, "%s", AE_RV_DASH);
}

/* a over b with one decimal, or the dash */
static void per(char *out, long a, long b, int known)
{
	if (known && b > 0)
		tenth_text(a, b, out, 20);
	else
		snprintf(out, 20, "%s", AE_RV_DASH);
}

static void pct(char *out, long a, long b, int known)
{
	if (known && b > 0)
		snprintf(out, 20, "%ld%%", ae_rs_percent(a, b));
	else
		snprintf(out, 20, "%s", AE_RV_DASH);
}

int ae_rv_has_details(int box, struct ae_rv_in const *in, struct ae_sr_last const *last)
{
	int kind, any = 0;

	switch (box)
	{
	case AE_RF_LAST: return last && last->has;
	case AE_RF_MAPS: return in->map_count > 0;
	case AE_RF_WEAPONS: return in->weapon_count > 0;
	case AE_RF_MEDALS: return ae_rv_medal_kinds(in->medals) > 0;
	case AE_RF_KINDS:
		for (kind = 0; kind < AE_RS_KINDS; kind++)
			any |= in->kind[kind].games > 0;
		return any;
	case AE_RF_VERSUS: return 0;
	default: return in->games > 0;
	}
}

int ae_rv_details(int box, struct ae_rv_in const *in, struct ae_sr_last const *last, struct ae_rv_table *table)
{
	struct ae_rs_stats const *x = in->x;
	int counted = x && ae_rs_known(x), v3 = x && ae_rs_known_v3(x), kind, index;
	long n = x ? x->counted : 0, n3 = x ? x->counted_v3 : 0;
	char a[20], b[20], c[20], d[20], e[20], f[20];

	memset(table, 0, sizeof(*table));
	if (!ae_rv_has_details(box, in, last))
		return 0;
	snprintf(table->title, sizeof(table->title), "%s", ae_rf_name(box));
	switch (box)
	{
	case AE_RF_ACCURACY:
		ae_rs_from_games(n, in->games, table->tail, AE_RV_TAIL);
		heads(table, "TOTAL", "PER GAME", NULL, NULL, NULL, NULL);
		num(a, x ? x->fired : 0, counted); per(b, x ? x->fired : 0, n, counted);
		ae_rv_table_row(table, "SHOTS FIRED", a, b, NULL);
		num(a, x ? x->hit : 0, counted); per(b, x ? x->hit : 0, n, counted);
		ae_rv_table_row(table, "SHOTS LANDED", a, b, NULL);
		num(a, x && x->fired > x->hit ? x->fired - x->hit : 0, counted); per(b, x && x->fired > x->hit ? x->fired - x->hit : 0, n, counted);
		ae_rv_table_row(table, "SHOTS MISSED", a, b, NULL);
		pct(a, x ? x->hit : 0, x ? x->fired : 0, counted);
		ae_rv_table_row(table, "ACCURACY", a, AE_RV_DASH, NULL);
		num(a, x ? x->headshots : 0, v3); per(b, x ? x->headshots : 0, n3, v3);
		ae_rv_table_row(table, "HEADSHOT KILLS", a, b, NULL);
		pct(a, x ? x->headshots : 0, x ? x->kills_v3 : 0, v3);
		ae_rv_table_row(table, "HEADSHOT SHARE", a, AE_RV_DASH, NULL);
		break;
	case AE_RF_KILL_KINDS:
	{
		struct ae_rv_share rows[AE_RV_KILL_KINDS];
		long counts[AE_RV_KILL_KINDS];

		ae_rv_kill_kinds(in, rows, table->tail);
		heads(table, "KILLS", "SHARE", "PER GAME", NULL, NULL, NULL);
		counts[0] = x ? x->headshots : 0; counts[1] = x ? x->melee : 0; counts[2] = x ? x->power : 0; counts[3] = x ? x->grenade : 0;
		counts[4] = x ? x->assassin : 0; counts[5] = x ? x->splatter : 0; counts[6] = x ? x->stick : 0;
		for (index = 0; index < AE_RV_KILL_KINDS; index++)
		{
			per(c, counts[index], n3, v3);
			ae_rv_table_row(table, rows[index].label, rows[index].count, rows[index].share, c, NULL);
		}
		break;
	}
	case AE_RF_ASSISTS:
		heads(table, "TOTAL", "PER GAME", NULL, NULL, NULL, NULL);
		num(a, in->assists, 1); per(b, in->assists, in->games, 1);
		ae_rv_table_row(table, "ASSISTS", a, b, NULL);
		if (in->medals)
		{
			static int const ids[3] = { AE_MEDAL_WINGMAN, AE_MEDAL_DRIVER, AE_MEDAL_PROTECTOR };
			static char const *const names[3] = { "WINGMAN", "DRIVER ASSISTS", "PROTECTOR" };

			for (index = 0; index < 3; index++)
			{
				num(a, (long)in->medals[ids[index]], 1); per(b, (long)in->medals[ids[index]], in->games, 1);
				ae_rv_table_row(table, names[index], a, b, NULL);
			}
		}
		break;
	case AE_RF_DAMAGE:
		ae_rs_from_games(n3, in->games, table->tail, AE_RV_TAIL);
		heads(table, "TOTAL", "PER GAME", "PER MINUTE", NULL, NULL, NULL);
		num(a, x ? x->dealt : 0, v3); per(b, x ? x->dealt : 0, n3, v3); per(c, x ? x->dealt * 60 : 0, x ? x->seconds_v3 : 0, v3);
		ae_rv_table_row(table, "DAMAGE DEALT", a, b, c, NULL);
		num(a, x ? x->taken : 0, v3); per(b, x ? x->taken : 0, n3, v3); per(c, x ? x->taken * 60 : 0, x ? x->seconds_v3 : 0, v3);
		ae_rv_table_row(table, "DAMAGE TAKEN", a, b, c, NULL);
		if (v3 && x->taken > 0)
			ae_rs_ratio_text(ae_rs_ratio100(x->dealt, x->taken), a, 20);
		else
			snprintf(a, 20, "%s", AE_RV_DASH);
		ae_rv_table_row(table, "DAMAGE RATIO", a, NULL);
		per(a, x ? x->dealt : 0, x ? x->hit_v3 : 0, v3);
		ae_rv_table_row(table, "DEALT PER HIT", a, NULL);
		break;
	case AE_RF_PER_GAME:
		ae_rs_from_games(n3, in->games, table->tail, AE_RV_TAIL);
		heads(table, "TOTAL", "PER GAME", NULL, NULL, NULL, NULL);
		num(a, in->kills, 1); per(b, in->kills, in->games, 1);
		ae_rv_table_row(table, "KILLS", a, b, NULL);
		num(a, in->deaths, 1); per(b, in->deaths, in->games, 1);
		ae_rv_table_row(table, "DEATHS", a, b, NULL);
		num(a, in->assists, 1); per(b, in->assists, in->games, 1);
		ae_rv_table_row(table, "ASSISTS", a, b, NULL);
		num(a, x ? x->dealt : 0, v3); per(b, x ? x->dealt : 0, n3, v3);
		ae_rv_table_row(table, "DAMAGE DEALT", a, b, NULL);
		if (counted)
		{
			ae_rs_time_text(x->seconds, a, 20);
			ae_rs_time_text(n > 0 ? x->seconds / n : 0, b, 20);
		}
		else
			snprintf(a, 20, "%s", AE_RV_DASH), snprintf(b, 20, "%s", AE_RV_DASH);
		ae_rv_table_row(table, "TIME PLAYED", a, b, NULL);
		ae_rv_table_row(table, "BEST SPREE", in->spree_from_record ? (num(a, in->best_spree, 1), a) : (num(a, x ? x->best_spree : 0, counted), a), NULL);
		break;
	case AE_RF_OBJECTIVES:
	{
		struct ae_rv_cell cells[AE_RV_OBJECTIVES];
		long gk[AE_RV_OBJECTIVES] = { in->kind[AE_RS_KIND_FLAG].games, in->kind[AE_RS_KIND_FLAG].games, in->kind[AE_RS_KIND_FLAG].games,
			in->kind[AE_RS_KIND_FLAG].games, in->kind[AE_RS_KIND_FLAG].games, in->kind[AE_RS_KIND_HILL].games,
			in->kind[AE_RS_KIND_BALL].games, in->kind[AE_RS_KIND_BOMB].games, in->kind[AE_RS_KIND_BOMB].games,
			in->kind[AE_RS_KIND_RACE].games };
		long totals[AE_RV_OBJECTIVES];

		ae_rv_objectives(in, cells, table->tail);
		heads(table, "TOTAL", "PER GAME", NULL, NULL, NULL, NULL);
		totals[0] = x ? x->flag_caps : 0; totals[1] = x ? x->flag_returns : 0; totals[2] = x ? x->flag_grabs : 0;
		totals[3] = x ? x->as_carrier : 0; totals[4] = in->medals ? (long)in->medals[AE_MEDAL_FLAG_CARRIER_KILL] : 0;
		totals[5] = 0; totals[6] = 0; totals[7] = x ? x->bomb_plants : 0; totals[8] = x ? x->bomb_defuses : 0; totals[9] = x ? x->laps : 0;
		for (index = 0; index < AE_RV_OBJECTIVES; index++)
		{
			if (index == 5 || index == 6)
				snprintf(b, 20, "%s", AE_RV_DASH);
			else
				per(b, totals[index], gk[index], !cells[index].dash);
			ae_rv_table_row(table, cells[index].label, cells[index].value, b, NULL);
		}
		/* (kept in the file, shown here: time with the flag, the ball's carrier numbers and the best lap) */
		num(a, x ? x->carrier_seconds : 0, v3 && in->kind[AE_RS_KIND_FLAG].games > 0);
		ae_rv_table_row(table, "SECONDS AS FLAG CARRIER", a, AE_RV_DASH, NULL);
		num(a, x ? x->ball_carrier_kills : 0, counted && in->kind[AE_RS_KIND_BALL].games > 0);
		ae_rv_table_row(table, "BALL CARRIER KILLS", a, AE_RV_DASH, NULL);
		num(a, x ? x->as_ball_carrier : 0, counted && in->kind[AE_RS_KIND_BALL].games > 0);
		ae_rv_table_row(table, "KILLS AS BALL CARRIER", a, AE_RV_DASH, NULL);
		if (counted && x->best_lap > 0)
			ae_rs_clock_text(x->best_lap, a, 20);
		else
			snprintf(a, 20, "%s", AE_RV_DASH);
		ae_rv_table_row(table, "BEST LAP", a, AE_RV_DASH, NULL);
		break;
	}
	case AE_RF_KINDS:
	{
		struct ae_rv_kind_row rows[AE_RS_KINDS];
		int count = ae_rv_kinds(in, rows);

		if (in->spree_from_record)
			ae_rs_from_games(n, in->games, table->tail, AE_RV_TAIL);
		heads(table, "GAMES", "WINS", "WIN RATE", "KILLS", "DEATHS", "K/D");
		for (kind = 0, index = 0; kind < AE_RS_KINDS && index < count; kind++)
			if (in->kind[kind].games > 0)
			{
				pct(a, in->kind[kind].wins, in->kind[kind].games, 1);
				ae_rv_table_row(table, rows[index].name, rows[index].games, rows[index].wins, a, rows[index].kills,
					rows[index].deaths, rows[index].kd, NULL);
				index++;
			}
		break;
	}
	case AE_RF_MAPS:
	{
		int order[AE_RS_MAPS], count = ae_rs_map_order(in->maps, in->map_count, order, AE_RS_MAPS);

		ae_rs_from_games(n, in->games, table->tail, AE_RV_TAIL);
		heads(table, "GAMES", "WINS", "WIN RATE", "KILLS", "DEATHS", "K/D");
		for (index = 0; index < count; index++)
		{
			struct ae_rs_map const *map = &in->maps[order[index]];
			char name[AE_RS_MAP_NAME];
			int at;

			for (at = 0; map->name[at] && at + 1 < (int)sizeof(name); at++)
				name[at] = map->name[at] >= 'a' && map->name[at] <= 'z' ? (char)(map->name[at] - 32) : map->name[at];
			name[at] = 0;
			num(a, map->games, 1); num(b, map->wins, 1); pct(c, map->wins, map->games, 1); num(d, map->kills, 1); num(e, map->deaths, 1);
			ae_rs_ratio_text(ae_rs_ratio100(map->kills, map->deaths), f, 20);
			ae_rv_table_row(table, name, a, b, c, d, e, f, NULL);
		}
		break;
	}
	case AE_RF_WEAPONS:
	{
		long total = 0;
		/* (the share is of all the record's kills while the weapons cover every game: a bot's always, a player's once every game
		was counted; else of the kept weapons, and the head says so) */
		int of_all = in->spree_from_record || (x && x->counted > 0 && x->counted == in->games);

		ae_rs_from_games(n, in->games, table->tail, AE_RV_TAIL);
		heads(table, "KILLS", of_all ? "SHARE" : "OF TOP 5", NULL, NULL, NULL, NULL);
		for (index = 0; index < in->weapon_count && !of_all; index++)
			total += in->weapons[index].kills;
		if (of_all)
			total = in->kills;
		for (index = 0; index < in->weapon_count; index++)
		{
			char name[AE_RS_WEAPON_NAME];
			int at;

			for (at = 0; in->weapons[index].name[at] && at + 1 < (int)sizeof(name); at++)
				name[at] = in->weapons[index].name[at] >= 'a' && in->weapons[index].name[at] <= 'z' ?
					(char)(in->weapons[index].name[at] - 32) : in->weapons[index].name[at];
			name[at] = 0;
			num(a, in->weapons[index].kills, 1);
			pct(b, in->weapons[index].kills, total, 1);
			ae_rv_table_row(table, name, a, b, NULL);
		}
		break;
	}
	case AE_RF_LAST:
	{
		struct ae_rv_cell cells[AE_RV_LAST];
		char map[AE_RS_MAP_NAME], kind_word[16];

		ae_rv_last_game(last, cells, map, kind_word, sizeof(map));
		heads(table, "THIS GAME", NULL, NULL, NULL, NULL, NULL);
		ae_rv_table_row(table, "MAP", map[0] ? map : AE_RV_DASH, NULL);
		ae_rv_table_row(table, "GAME KIND", kind_word[0] ? kind_word : AE_RV_DASH, NULL);
		for (index = 0; index < AE_RV_LAST; index++)
			ae_rv_table_row(table, cells[index].label, cells[index].value, NULL);
		break;
	}
	default:
		return 0;
	}
	return 1;
}
