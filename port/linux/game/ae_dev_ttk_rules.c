/*
AE_DEV_TTK_RULES.C

DEV MODE's time-to-kill readout: the table and the line (ae_dev_ttk_rules.h). No engine includes.
*/

#include <stdio.h>
#include <string.h>

#include "ae_dev_ttk_rules.h"

static int in_range(int slot)
{
	return slot >= 0 && slot < AE_TTK_PLAYERS;
}

static void row_clear(struct ae_ttk_row *row, long life)
{
	int killer;

	row->life = life;
	for (killer = 0; killer < AE_TTK_PLAYERS; killer++)
	{
		row->killer[killer].first_tick = -1;
		row->killer[killer].hits = 0;
		row->killer[killer].head = 0;
	}
}

void ae_ttk_clear(struct ae_ttk_table *table)
{
	int victim;

	for (victim = 0; victim < AE_TTK_PLAYERS; victim++)
		row_clear(&table->victim[victim], -1);
}

void ae_ttk_clear_victim(struct ae_ttk_table *table, int victim)
{
	if (in_range(victim))
		row_clear(&table->victim[victim], -1);
}

int ae_ttk_hit(struct ae_ttk_table *table, int killer, int victim, long life, long tick, int head)
{
	struct ae_ttk_row *row;
	struct ae_ttk_hit *hit;

	if (!in_range(killer) || !in_range(victim) || killer == victim)
		return 0;
	row = &table->victim[victim];
	if (row->life != life)
		row_clear(row, life);
	hit = &row->killer[killer];
	if (hit->first_tick < 0)
		hit->first_tick = tick;
	if (hit->hits < 32767)
		hit->hits++;
	if (head && hit->head < 32767)
		hit->head++;
	return 1;
}

int ae_ttk_kill(struct ae_ttk_table *table, int killer, int victim, long life, long tick, struct ae_ttk_result *out)
{
	struct ae_ttk_row *row;
	int found = 0;

	memset(out, 0, sizeof *out);
	if (!in_range(victim))
		return 0;
	row = &table->victim[victim];
	if (in_range(killer) && killer != victim && row->life == life && row->killer[killer].first_tick >= 0)
	{
		out->ticks = tick - row->killer[killer].first_tick;
		if (out->ticks < 0)
			out->ticks = 0;
		out->hits = row->killer[killer].hits;
		out->head = row->killer[killer].head;
		found = 1;
	}
	row_clear(row, -1);
	return found;
}

void ae_ttk_format(char *text, int size, char const *killer, char const *victim, char const *weapon,
	struct ae_ttk_result const *result)
{
	snprintf(text, (size_t)size, "ttk: %s -> %s %s %.2fs, %d hits (%d head)", killer, victim, weapon,
		(double)result->ticks / (double)AE_TTK_TICKS_PER_SECOND, result->hits, result->head);
}

void ae_ttk_short_format(char *text, int size, char const *weapon, struct ae_ttk_result const *result)
{
	char hits[32], head[24] = "";

	snprintf(hits, sizeof hits, result->hits == 1 ? "%d hit" : "%d hits", result->hits);
	if (result->head > 0)
		snprintf(head, sizeof head, " (%d head)", result->head);
	snprintf(text, (size_t)size, "TTK %.2fs - %s%s - %.16s", (double)result->ticks / (double)AE_TTK_TICKS_PER_SECOND, hits,
		head, weapon ? weapon : "?");
}
