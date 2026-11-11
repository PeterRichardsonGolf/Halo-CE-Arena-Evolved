/*
AE_SERVER_BOTS_RULES.C

See ae_server_bots_rules.h.
*/

#include <string.h>

#include "ae_server_bots_rules.h"

int ae_server_bots_fill_parse(char const *name)
{
	if (!name)
		return -1;
	if (!strcmp(name, "off"))
		return AE_SERVER_BOTS_FILL_OFF;
	if (!strcmp(name, "minimum"))
		return AE_SERVER_BOTS_FILL_MINIMUM;
	if (!strcmp(name, "empty_slots"))
		return AE_SERVER_BOTS_FILL_EMPTY_SLOTS;
	if (!strcmp(name, "auto"))
		return AE_SERVER_BOTS_FILL_AUTO;
	return -1;
}

int ae_server_bots_skill_valid(char const *name)
{
	static char const *const skills[] = { "recruit", "easy", "normal", "heroic", "legendary", "demon" };
	unsigned index;

	for (index = 0; name && index < sizeof(skills) / sizeof(skills[0]); index++)
	{
		if (!strcmp(name, skills[index]))
			return 1;
	}
	return 0;
}

int ae_server_bots_fill_resolved(int fill, int entry_is_ae)
{
	if (fill == AE_SERVER_BOTS_FILL_AUTO)
		return entry_is_ae ? AE_SERVER_BOTS_FILL_MINIMUM : AE_SERVER_BOTS_FILL_OFF;
	return fill == AE_SERVER_BOTS_FILL_MINIMUM || fill == AE_SERVER_BOTS_FILL_EMPTY_SLOTS ? fill :
		AE_SERVER_BOTS_FILL_OFF;
}

int ae_server_bots_wanted(int fill, int minimum, int humans, int held, int hold_with_bot, int max_players, int alone,
	int played)
{
	int room, total, bots;

	if (humans < 0)
		humans = 0;
	if (held < 0)
		held = 0;
	if (!played || (fill != AE_SERVER_BOTS_FILL_MINIMUM && fill != AE_SERVER_BOTS_FILL_EMPTY_SLOTS))
		return 0;
	if (humans == 0 && !alone)
		return 0;
	/* (a rated game's held slots stay empty: out of the room) */
	room = max_players - (hold_with_bot ? 0 : held);
	if (room < 0)
		room = 0;
	if (fill == AE_SERVER_BOTS_FILL_EMPTY_SLOTS)
	{
		total = room;
	}
	else
	{
		total = minimum < 1 ? 1 : minimum;
		if (hold_with_bot && humans + held > total)
			total = humans + held;
		if (humans == 0 && total < 2)
			total = 2;
	}
	if (total > room)
		total = room;
	bots = total - humans;
	return bots < 0 ? 0 : bots;
}

int ae_server_bots_human_fits(int free_seats, int bots)
{
	return free_seats > 0 || bots > 0;
}

int ae_server_bots_human_needs_bot_gone(int free_seats, int bots)
{
	return free_seats <= 0 && bots > 0;
}

int ae_server_bots_surplus(int bots, int target)
{
	return bots > target && target >= 0 ? bots - target : 0;
}

int ae_server_bots_death_leaves(int surplus, int teams, int bot_team, int leaving_team)
{
	if (surplus <= 0)
		return 0;
	if (!teams)
		return 1;
	return leaving_team >= 0 && bot_team == leaving_team;
}

void ae_server_rejoin_clear(struct ae_server_rejoin_table *table)
{
	memset(table, 0, sizeof(*table));
}

int ae_server_rejoin_find(struct ae_server_rejoin_table const *table, char const *name, unsigned long address)
{
	int index;

	for (index = 0; name && index < AE_SERVER_REJOIN_MAXIMUM; index++)
	{
		if (table->entries[index].used && table->entries[index].address == address &&
			!strncmp(table->entries[index].name, name, AE_SERVER_REJOIN_NAME - 1))
		{
			return index;
		}
	}
	return -1;
}

int ae_server_rejoin_drop(struct ae_server_rejoin_table *table, char const *name, unsigned long address, int team,
	int bot_holds)
{
	int index = ae_server_rejoin_find(table, name, address);

	if (index < 0)
	{
		for (index = 0; index < AE_SERVER_REJOIN_MAXIMUM && table->entries[index].used; index++)
			;
		if (index >= AE_SERVER_REJOIN_MAXIMUM)
			return -1;
	}
	memset(&table->entries[index], 0, sizeof(table->entries[index]));
	table->entries[index].used = 1;
	strncpy(table->entries[index].name, name, AE_SERVER_REJOIN_NAME - 1);
	table->entries[index].address = address;
	table->entries[index].team = team;
	table->entries[index].bot_holds = bot_holds != 0;
	return index;
}

void ae_server_rejoin_take(struct ae_server_rejoin_table *table, int index)
{
	if (index >= 0 && index < AE_SERVER_REJOIN_MAXIMUM)
		memset(&table->entries[index], 0, sizeof(table->entries[index]));
}

int ae_server_rejoin_held(struct ae_server_rejoin_table const *table, int with_bot)
{
	int index, count = 0;

	for (index = 0; index < AE_SERVER_REJOIN_MAXIMUM; index++)
		count += table->entries[index].used && (table->entries[index].bot_holds != 0) == (with_bot != 0);
	return count;
}
