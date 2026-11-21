/*
AE_BOTS_SEAT_RULES.C

Per-bot SHOOTING and MOVEMENT skill, as pure rules (ae_bots_seat_rules.h).
*/

#include "ae_bots_seat_rules.h"

#include <ctype.h>
#include <string.h>

static char const *const names[AE_SEAT_STEPS] = { "RECRUIT", "EASY", "NORMAL", "HEROIC", "ELITE", "LEGENDARY", "DEMON" };

void ae_seat_clear(struct ae_seat_table *table)
{
	memset(table, 0, sizeof *table);
}

void ae_seat_unset(struct ae_seat_table *table, int seat)
{
	if (seat < 0 || seat >= AE_SEAT_MOST)
		return;
	table->shooting[seat] = 0;
	table->movement[seat] = 0;
}

int ae_seat_set(struct ae_seat_table *table, int seat, int shooting_step, int movement_step)
{
	if (seat < 0 || seat >= AE_SEAT_MOST || shooting_step < 1 || shooting_step > AE_SEAT_STEPS || movement_step < 1 ||
		movement_step > AE_SEAT_STEPS)
	{
		return 0;
	}
	table->shooting[seat] = (unsigned char)shooting_step;
	table->movement[seat] = (unsigned char)movement_step;
	return 1;
}

int ae_seat_levels(struct ae_seat_table const *table, int seat, int *shooting_level, int *movement_level)
{
	if (seat < 0 || seat >= AE_SEAT_MOST || !table->shooting[seat] || !table->movement[seat])
		return 0;
	*shooting_level = table->shooting[seat] - 1;
	*movement_level = table->movement[seat] - 1;
	return 1;
}

int ae_seat_count(struct ae_seat_table const *table)
{
	int seat, count = 0;

	for (seat = 0; seat < AE_SEAT_MOST; seat++)
		count += table->shooting[seat] != 0;
	return count;
}

int ae_seat_step_of_name(char const *name)
{
	int step;

	if (!name || !name[0])
		return 0;
	if (name[0] >= '1' && name[0] <= '0' + AE_SEAT_STEPS && !name[1])
		return name[0] - '0';
	for (step = 0; step < AE_SEAT_STEPS; step++)
	{
		char const *a = name, *b = names[step];

		while (*a && *b && toupper((unsigned char)*a) == *b)
		{
			a++;
			b++;
		}
		if (!*a && !*b)
			return step + 1;
	}
	return 0;
}

char const *ae_seat_step_name(int step)
{
	return step >= 1 && step <= AE_SEAT_STEPS ? names[step - 1] : "?";
}

char const *ae_seat_level_name(int level)
{
	return ae_seat_step_name(level + 1);
}
