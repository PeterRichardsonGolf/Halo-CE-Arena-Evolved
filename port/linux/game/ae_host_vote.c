/*
AE_HOST_VOTE.C

See ae_host_vote.h.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "ae_host_vote.h"
#include "ae_host_vote_rules.h"
#include "ae_vote_loop.h"
#include "network_map_vote.h"
#include "../src/port_config.h"

unsigned long system_milliseconds(void);
void platform_log(char const *format, ...);

static struct
{
	int running;
	int count;
	int current;
	long entries[AE_HOST_VOTE_MAX];
	struct ae_playlist list;
	unsigned long state;
} host_vote;

int ae_host_vote_begin(struct ae_playlist const *list, int current, unsigned char const *playable)
{
	char map_names[NETWORK_MAP_VOTE_MAXIMUM_CHOICES][NETWORK_MAP_VOTE_MAP_SIZE];
	char gametypes[NETWORK_MAP_VOTE_MAXIMUM_CHOICES][NETWORK_MAP_VOTE_GAMETYPE_SIZE];
	int choices[AE_HOST_VOTE_MAX];
	long seconds = config_integer("server.map_vote_seconds");
	int count, index;

	if (host_vote.running || network_map_vote_running())
		return 0;
	if (!host_vote.state)
		host_vote.state = system_milliseconds() | 1;
	count = ae_host_vote_choose(list, current, playable, &host_vote.state, choices);
	if (count < 2)
	{
		platform_log("map vote (host): none to hold (%s, %d games)", list->map_vote ? "too few maps" : "MAP VOTE off",
			(int)list->count);
		return 0;
	}
	if (seconds < 5)
		seconds = 20;
	if (seconds > 120)
		seconds = 120;
	memset(map_names, 0, sizeof(map_names));
	memset(gametypes, 0, sizeof(gametypes));
	for (index = 0; index < count; index++)
	{
		snprintf(map_names[index], sizeof(map_names[index]), "%s", list->entries[choices[index]].map);
		snprintf(gametypes[index], sizeof(gametypes[index]), "%s", list->entries[choices[index]].gametype);
		host_vote.entries[index] = choices[index];
	}
	host_vote.list = *list;
	host_vote.count = count;
	host_vote.current = current;
	network_map_vote_begin_local((char const (*)[NETWORK_MAP_VOTE_MAP_SIZE])map_names,
		(char const (*)[NETWORK_MAP_VOTE_GAMETYPE_SIZE])gametypes, (short)count, (short)seconds);
	host_vote.running = network_map_vote_running();
	return host_vote.running;
}

int ae_host_vote_tick(int *entry)
{
	short winner;
	int next;

	if (!host_vote.running)
		return AE_HOST_VOTE_IDLE;
	if (!network_map_vote_running())
	{
		/* (dropped under it) */
		host_vote.running = 0;
		return AE_HOST_VOTE_IDLE;
	}
	winner = ae_vote_loop_step(host_vote.entries, (short)host_vote.count, host_vote.current);
	if (winner == NONE)
		return AE_HOST_VOTE_RUNNING;
	host_vote.running = 0;
	/* (the loop has resolved nobody voting: the winner is a choice either way) */
	next = winner >= 0 && winner < host_vote.count ? (int)host_vote.entries[winner] : -1;
	if (next < 0 || next >= host_vote.list.count)
		return AE_HOST_VOTE_IDLE;
	platform_log("map vote (host): the next game is %s on %s", host_vote.list.entries[next].gametype,
		host_vote.list.entries[next].map);
	*entry = next;
	return AE_HOST_VOTE_DECIDED;
}

int ae_host_vote_running(void)
{
	return host_vote.running;
}

void ae_host_vote_cancel(void)
{
	if (host_vote.running)
		platform_log("map vote (host): dropped");
	host_vote.running = 0;
	if (network_map_vote_running())
		network_map_vote_cancel();
}
