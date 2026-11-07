/*
AE_SERVER_SHUFFLE.C

See ae_server_shuffle.h; the order's rules are ae_rotation_shuffle.c's.
*/

#include "cseries.h"

#include "ae_rotation_shuffle.h"
#include "ae_server_shuffle.h"
#include "../src/server_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int config_boolean(const char *name);
unsigned long system_milliseconds(void);

static unsigned long passes;

static void shuffle(
	struct server_rotation *rotation,
	struct server_entry const *previous)
{
	static struct server_rotation copy;
	char const *map[AE_ROTATION_SHUFFLE_MAXIMUM];
	char const *gametype[AE_ROTATION_SHUFFLE_MAXIMUM];
	int weights[AE_ROTATION_SHUFFLE_MAXIMUM];
	int order[AE_ROTATION_SHUFFLE_MAXIMUM];
	int index;
	int fine;
	char line[900];
	int used = 0;

	if (rotation->count < 2 || rotation->count > AE_ROTATION_SHUFFLE_MAXIMUM)
		return;
	for (index = 0; index < rotation->count; index++)
	{
		map[index] = rotation->entries[index].map;
		gametype[index] = rotation->entries[index].gametype;
		weights[index] = rotation->entries[index].weight;
	}
	fine = ae_rotation_shuffle_order_weighted(rotation->count, map, gametype, weights, previous ? previous->map : NULL,
		previous ? previous->gametype : NULL, (system_milliseconds() * 2654435761UL) ^ (++passes * 40503UL) ^
		(unsigned long)rand(), order);
	copy = *rotation;
	for (index = 0; index < rotation->count; index++)
		rotation->entries[index] = copy.entries[order[index]];
	line[0] = 0;
	for (index = 0; index < rotation->count && used < (int)sizeof(line) - 80; index++)
		used += snprintf(line + used, sizeof(line) - (size_t)used, "%s%s", index ? ", " : "", rotation->entries[index].text);
	server_log(_server_log_info, "server", "rotation shuffled%s: %s", fine ? "" : " (the same map twice in a row cannot be avoided)",
		line);
}

void ae_server_shuffle_start(
	struct server_rotation *rotation)
{
	if (config_boolean("server.rotation_shuffle"))
		shuffle(rotation, NULL);
}

void ae_server_shuffle_pass_end(
	struct server_rotation *rotation,
	long *index,
	struct server_entry const *played,
	long *current_index)
{
	long after;

	if (!config_boolean("server.rotation_shuffle") || rotation->count < 2 || *index < 0 || *index >= rotation->count)
		return;
	for (after = *index + 1; after < rotation->count; after++)
		if (!rotation->entries[after].rejected[0])
			return;
	shuffle(rotation, played);
	*index = rotation->count - 1;
	if (current_index)
	{
		long found;

		*current_index = NONE;
		for (found = 0; found < rotation->count; found++)
			if (!strcmp(rotation->entries[found].text, played->text))
			{
				*current_index = found;
				break;
			}
	}
}
