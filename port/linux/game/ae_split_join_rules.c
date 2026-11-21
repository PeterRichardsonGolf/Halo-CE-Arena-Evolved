#include "ae_split_join_rules.h"

int ae_split_join_check(struct ae_split_join_facts const *facts)
{
	if (!facts->multiplayer)
		return AE_SPLIT_JOIN_NOT_MULTIPLAYER;
	if (facts->replay)
		return AE_SPLIT_JOIN_REPLAY;
	if (facts->postgame)
		return AE_SPLIT_JOIN_POSTGAME;
	if (!facts->option_on)
		return AE_SPLIT_JOIN_OFF;
	if (facts->tournament)
		return AE_SPLIT_JOIN_TOURNAMENT;
	if (facts->vote)
		return AE_SPLIT_JOIN_VOTE;
	if (facts->controller_plays)
		return AE_SPLIT_JOIN_IN_GAME;
	if (!facts->pad)
		return AE_SPLIT_JOIN_NO_PAD;
	if (facts->local_players >= 4)
		return AE_SPLIT_JOIN_MAX_LOCAL;
	if (facts->slots_free <= 0)
		return AE_SPLIT_JOIN_FULL;
	return AE_SPLIT_JOIN_OK;
}

int ae_split_join_host_allows(int option_on, int tournament, int machine_players, long since_first_ms)
{
	if (machine_players <= 0)
		return 1;
	if (since_first_ms >= 0 && since_first_ms < AE_SPLIT_JOIN_TOGETHER_MS)
		return 1;
	return option_on && !tournament;
}

const char *ae_split_join_reason_text(int reason)
{
	switch (reason)
	{
	case AE_SPLIT_JOIN_NOT_MULTIPLAYER: return "ONLY A MULTIPLAYER GAME TAKES A PLAYER IN PROGRESS";
	case AE_SPLIT_JOIN_REPLAY: return "A REPLAY HAS NO PLAYERS TO ADD";
	case AE_SPLIT_JOIN_POSTGAME: return "THE GAME IS OVER";
	case AE_SPLIT_JOIN_OFF: return "THE HOST HAS TURNED OFF JOINING IN PROGRESS";
	case AE_SPLIT_JOIN_TOURNAMENT: return "A TOURNAMENT MATCH KEEPS ITS PLAYERS";
	case AE_SPLIT_JOIN_VOTE: return "THE MAP VOTE IS UNDER WAY";
	case AE_SPLIT_JOIN_NO_PAD: return "NO FREE CONTROLLER ON THIS PC";
	case AE_SPLIT_JOIN_IN_GAME: return "THIS CONTROLLER IS PLAYING ALREADY";
	case AE_SPLIT_JOIN_MAX_LOCAL: return "FOUR PLAYERS ARE ON THIS PC ALREADY";
	case AE_SPLIT_JOIN_FULL: return "THE GAME IS FULL";
	}
	return "";
}

int ae_split_leave_allowed(int controller_plays, int local_players)
{
	return controller_plays && local_players > 1;
}

int ae_split_join_controller(unsigned plays, unsigned pads, int hint)
{
	int controller;

	if (hint >= 0 && hint < 4 && !(plays & (1u << hint)) && (pads & (1u << hint)))
		return hint;
	for (controller = 0; controller < 4; controller++)
	{
		if (!(plays & (1u << controller)) && (pads & (1u << controller)))
			return controller;
	}
	return -1;
}

int ae_split_join_views_after(int local_players, int joining)
{
	int count = local_players + (joining ? 1 : -1);

	return count < 1 ? 1 : count > 4 ? 4 : count;
}
