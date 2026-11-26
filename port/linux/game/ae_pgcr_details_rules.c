/* The post-game report's DETAILS page (see the header). */
#include <stdio.h>
#include <string.h>

#include "ae_pgcr_details_rules.h"

#define DASH AE_RV_DASH

static int has_v3(struct ae_pgr_row const *row)
{
	return row->summary.valid && row->summary.extended;
}

/* the count of a medal in the row's summary (0 for none) */
static long medal_count(struct ae_pgr_row const *row, int medal)
{
	int index;
	long count = 0;

	for (index = 0; index < row->summary.medal_count && index < AE_PGCR_SUMMARY_MEDALS; index++)
		if (row->summary.medals[index].id == medal)
			count += row->summary.medals[index].count;
	return count;
}

int ae_pgd_this_game(struct ae_pgr_row const *row, struct ae_rv_cell *out)
{
	char text[48];
	long spread = row->kills - row->deaths;
	char magnitude[32];

	ae_rv_cell_whole(&out[0], "KILLS", row->kills);
	ae_rv_cell_whole(&out[1], "DEATHS", row->deaths);
	ae_rs_number_text(spread < 0 ? -spread : spread, magnitude, sizeof(magnitude));
	snprintf(text, sizeof(text), "%s%s", spread > 0 ? "+" : spread < 0 ? "-" : "", magnitude);
	ae_rv_cell_set(&out[2], "KILL SPREAD", text);
	ae_rv_cell_whole(&out[3], "ASSISTS", row->assists);
	ae_rv_cell_ratio(&out[4], "K/D", row->kills, row->deaths, 1);
	ae_rs_ratio_text(ae_rs_kda100(row->kills, row->assists, row->deaths), text, sizeof(text));
	ae_rv_cell_set(&out[5], "KDA", text);
	if (row->summary.valid)
	{
		ae_rv_cell_whole(&out[6], "BEST SPREE", row->summary.best_spree);
		ae_rv_cell_whole(&out[7], "MULTIKILLS", row->summary.multikills);
	}
	else
	{
		ae_rv_cell_dash(&out[6], "BEST SPREE");
		ae_rv_cell_dash(&out[7], "MULTIKILLS");
	}
	return AE_PGD_THIS_GAME;
}

int ae_pgd_accuracy_damage(struct ae_pgr_row const *row, struct ae_rv_cell *out)
{
	int shots = row->shots_fired >= 0 && row->shots_hit >= 0, v3 = has_v3(row);

	if (shots)
	{
		ae_rv_cell_whole(&out[0], "SHOTS FIRED", row->shots_fired);
		ae_rv_cell_whole(&out[1], "SHOTS LANDED", row->shots_hit);
		ae_rv_cell_whole(&out[2], "SHOTS MISSED", row->shots_fired > row->shots_hit ? row->shots_fired - row->shots_hit : 0);
	}
	else
	{
		ae_rv_cell_dash(&out[0], "SHOTS FIRED");
		ae_rv_cell_dash(&out[1], "SHOTS LANDED");
		ae_rv_cell_dash(&out[2], "SHOTS MISSED");
	}
	ae_rv_cell_percent(&out[3], "ACCURACY", row->shots_hit, row->shots_fired, shots);
	if (v3)
		ae_rv_cell_whole(&out[4], "HEADSHOT KILLS", row->summary.headshots);
	else
		ae_rv_cell_dash(&out[4], "HEADSHOT KILLS");
	if (v3)
	{
		ae_rv_cell_whole(&out[5], "DAMAGE DEALT", (long)row->summary.damage_dealt);
		ae_rv_cell_whole(&out[6], "DAMAGE TAKEN", (long)row->summary.damage_taken);
	}
	else
	{
		ae_rv_cell_dash(&out[5], "DAMAGE DEALT");
		ae_rv_cell_dash(&out[6], "DAMAGE TAKEN");
	}
	ae_rv_cell_ratio(&out[7], "DAMAGE RATIO", (long)row->summary.damage_dealt, (long)row->summary.damage_taken,
		v3 && row->summary.damage_taken > 0);
	ae_rv_cell_tenth(&out[8], "DEALT PER HIT", (long)row->summary.damage_dealt, row->shots_hit, v3 && shots);
	ae_rv_cell_tenth(&out[9], "TAKEN PER LIFE", (long)row->summary.damage_taken, row->deaths > 0 ? row->deaths : 1, v3);
	return AE_PGD_ACCURACY;
}

int ae_pgd_kill_kinds(struct ae_pgr_row const *row, struct ae_rv_cell *out)
{
	static char const *const labels[AE_PGD_KINDS] = { "HEADSHOTS", "MELEE", "POWER WEAPONS", "GRENADES", "ASSASSINATIONS",
		"SPLATTERS", "STICKS" };
	long counts[AE_PGD_KINDS];
	int index, v3 = has_v3(row);

	counts[0] = row->summary.headshots; counts[1] = row->summary.melee; counts[2] = row->summary.power;
	counts[3] = row->summary.grenade; counts[4] = row->summary.assassination; counts[5] = row->summary.splatter;
	counts[6] = row->summary.stick;
	for (index = 0; index < AE_PGD_KINDS; index++)
	{
		if (v3)
			ae_rv_cell_whole(&out[index], labels[index], counts[index]);
		else
			ae_rv_cell_dash(&out[index], labels[index]);
	}
	return AE_PGD_KINDS;
}

static void objective(struct ae_rv_cell *cell, char const *label, long value)
{
	if (value >= 0)
		ae_rv_cell_whole(cell, label, value);
	else
		ae_rv_cell_dash(cell, label);
}

static void clock_cell(struct ae_rv_cell *cell, char const *label, long ticks)
{
	char text[32];

	if (ticks < 0)
		ae_rv_cell_dash(cell, label);
	else
	{
		ae_rs_clock_text(ticks, text, sizeof(text));
		ae_rv_cell_set(cell, label, text);
	}
}

int ae_pgd_mode_block(struct ae_pgr_report const *report, struct ae_pgr_row const *row, char *title, struct ae_rv_cell *out)
{
	switch (report->mode)
	{
	case AE_PGR_MODE_FLAG:
		snprintf(title, AE_PGD_TITLE, "FLAG");
		objective(&out[0], "CAPTURES", row->objective[2]);
		objective(&out[1], "RETURNS", row->objective[1]);
		objective(&out[2], "GRABS", row->objective[0]);
		if (has_v3(row))
			ae_rv_cell_whole(&out[3], "KILLS AS CARRIER", row->summary.kills_as_flag_carrier);
		else
			ae_rv_cell_dash(&out[3], "KILLS AS CARRIER");
		if (row->summary.valid)
			ae_rv_cell_whole(&out[4], "CARRIERS KILLED", medal_count(row, AE_MEDAL_FLAG_CARRIER_KILL));
		else
			ae_rv_cell_dash(&out[4], "CARRIERS KILLED");
		return 5;
	case AE_PGR_MODE_BALL:
		snprintf(title, AE_PGD_TITLE, "BALL");
		clock_cell(&out[0], "BALL TIME", row->score_value);
		objective(&out[1], "CARRIER KILLS", row->ball_carrier_kills);
		objective(&out[2], "KILLS AS CARRIER", row->as_ball_carrier);
		return 3;
	case AE_PGR_MODE_HILL:
		snprintf(title, AE_PGD_TITLE, "HILL");
		clock_cell(&out[0], "HILL TIME", row->score_value);
		return 1;
	case AE_PGR_MODE_BOMB:
		snprintf(title, AE_PGD_TITLE, "BOMB");
		if (has_v3(row))
		{
			ae_rv_cell_whole(&out[0], "PLANTS", row->summary.bomb_plants);
			ae_rv_cell_whole(&out[1], "DEFUSES", row->summary.bomb_defuses);
		}
		else
		{
			ae_rv_cell_dash(&out[0], "PLANTS");
			ae_rv_cell_dash(&out[1], "DEFUSES");
		}
		objective(&out[2], "PICKUPS", row->objective[0]);
		objective(&out[3], "SCORES", row->objective[2]);
		return 4;
	case AE_PGR_MODE_RACE:
		snprintf(title, AE_PGD_TITLE, "RACE");
		objective(&out[0], "LAPS", row->laps);
		clock_cell(&out[1], "BEST LAP", row->best_lap > 0 ? row->best_lap : -1);
		return 2;
	default:
		break;
	}
	/* (a game of no objective: the player's medals) */
	snprintf(title, AE_PGD_TITLE, "MEDALS");
	if (row->summary.valid)
	{
		long points = 0;
		int index;

		for (index = 0; index < row->summary.medal_count && index < AE_PGCR_SUMMARY_MEDALS; index++)
			points += (long)ae_pgr_medal_points(row->summary.medals[index].id) * row->summary.medals[index].count;
		ae_rv_cell_whole(&out[0], "EARNED", ae_pgr_medal_total(&row->summary));
		ae_rv_cell_whole(&out[1], "KINDS", row->summary.medal_count);
		ae_rv_cell_whole(&out[2], "POINTS", points);
	}
	else
	{
		ae_rv_cell_dash(&out[0], "EARNED");
		ae_rv_cell_dash(&out[1], "KINDS");
		ae_rv_cell_dash(&out[2], "POINTS");
	}
	return 3;
}

int ae_pgd_assists(struct ae_pgr_row const *row, struct ae_rv_cell *out)
{
	ae_rv_cell_whole(&out[0], "ASSISTS", row->assists);
	if (row->summary.valid)
	{
		ae_rv_cell_whole(&out[1], "WINGMAN", medal_count(row, AE_MEDAL_WINGMAN));
		ae_rv_cell_whole(&out[2], "DRIVER ASSISTS", medal_count(row, AE_MEDAL_DRIVER));
		ae_rv_cell_whole(&out[3], "PROTECTOR", medal_count(row, AE_MEDAL_PROTECTOR));
	}
	else
	{
		ae_rv_cell_dash(&out[1], "WINGMAN");
		ae_rv_cell_dash(&out[2], "DRIVER ASSISTS");
		ae_rv_cell_dash(&out[3], "PROTECTOR");
	}
	return AE_PGD_ASSISTS;
}

/* a number's text, the dash when it is not known */
static void text_or_dash(long value, int known, char *out, size_t size)
{
	if (known)
		ae_rs_number_text(value, out, size);
	else
		snprintf(out, size, "%s", DASH);
}

/* the accuracy and damage of the rows of a set: the shots and the damage added over the rows that have them */
static void sums(struct ae_pgr_report const *report, int team, struct ae_pgd_line *line)
{
	long fired = 0, hit = 0, damage = 0;
	int row, shots = 0, damaged = 0;

	for (row = 0; row < report->rows; row++)
	{
		struct ae_pgr_row const *r = &report->row[row];

		if (team >= 0 && r->team != team)
			continue;
		if (r->shots_fired >= 0 && r->shots_hit >= 0)
		{
			fired += r->shots_fired;
			hit += r->shots_hit;
			shots = 1;
		}
		if (has_v3(r))
		{
			damage += (long)r->summary.damage_dealt;
			damaged = 1;
		}
	}
	if (shots && fired > 0)
		snprintf(line->acc, sizeof(line->acc), "%ld%%", ae_rs_percent(hit, fired));
	else
		snprintf(line->acc, sizeof(line->acc), "%s", DASH);
	text_or_dash(damage, damaged, line->damage, sizeof(line->damage));
}

int ae_pgd_lines(struct ae_pgr_report const *report, struct ae_pgd_line *out, char *title)
{
	int count = 0, index;

	if (report->teams)
	{
		snprintf(title, AE_PGD_TITLE, "TEAMS");
		for (index = 0; index < AE_PGR_TEAMS; index++)
		{
			struct ae_pgr_team const *team = &report->team[index];

			if (team->count <= 0 && !team->name[0])
				continue;
			memset(&out[count], 0, sizeof(out[count]));
			out[count].chip = team->color;
			snprintf(out[count].name, sizeof(out[count].name), "%s", team->name);
			ae_rs_number_text(team->kills, out[count].k, sizeof(out[count].k));
			ae_rs_number_text(team->deaths, out[count].d, sizeof(out[count].d));
			ae_rs_number_text(team->assists, out[count].a, sizeof(out[count].a));
			sums(report, index, &out[count]);
			count++;
		}
		return count;
	}
	snprintf(title, AE_PGD_TITLE, "TOP PLAYERS");
	for (index = 0; index < report->rows && count < AE_PGD_LINES; index++)
	{
		struct ae_pgr_row const *row = &report->row[index];
		struct ae_pgr_report single;
		struct ae_pgd_line *line = &out[count];

		memset(line, 0, sizeof(*line));
		line->chip = row->chip;
		line->me = row->me;
		snprintf(line->name, sizeof(line->name), "%s", row->name);
		ae_rs_number_text(row->kills, line->k, sizeof(line->k));
		ae_rs_number_text(row->deaths, line->d, sizeof(line->d));
		ae_rs_number_text(row->assists, line->a, sizeof(line->a));
		/* (the same sums over this one row) */
		memset(&single, 0, sizeof(single));
		single.rows = 1;
		single.row[0] = *row;
		single.row[0].team = -1;
		sums(&single, -1, line);
		count++;
	}
	return count;
}
