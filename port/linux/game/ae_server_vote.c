/*
AE_SERVER_VOTE.C

SAME AGAIN in a dedicated server's map vote (ae_server_vote.h).
*/

#include <stdio.h>

#include "cseries.h"
#include "ae_server_vote.h"

short ae_server_vote_same_again(struct server_rotation const *rotation, long current, long *entries, short count,
	char (*map_names)[NETWORK_MAP_VOTE_MAP_SIZE], char (*gametypes)[NETWORK_MAP_VOTE_GAMETYPE_SIZE],
	struct server_json *json)
{
	struct server_entry const *entry;
	short at;

	if (current == NONE || current < 0 || current >= rotation->count || count >= NETWORK_MAP_VOTE_MAXIMUM_CHOICES)
		return count;
	entry = &rotation->entries[current];
	if (entry->rejected[0])
		return count;
	for (at = 0; at < count; at++)
		if (entries[at] == current)
			return count;
	entries[count] = current;
	snprintf(map_names[count], NETWORK_MAP_VOTE_MAP_SIZE, "%s", entry->map_name);
	server_rotation_vote_gametype(entry, gametypes[count], NETWORK_MAP_VOTE_GAMETYPE_SIZE);
	server_json_object_begin(json, NULL);
	server_json_string(json, "map", entry->map_name);
	server_json_string(json, "gametype", entry->gametype);
	server_json_integer(json, "index", current);
	server_json_boolean(json, "same_again", TRUE);
	server_json_object_end(json);
	return (short)(count + 1);
}
