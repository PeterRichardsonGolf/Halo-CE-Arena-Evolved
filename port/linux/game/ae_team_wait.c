/*
AE_TEAM_WAIT.C

Wait for players (ae_team_wait.h): in AE's gametypes a team game whose other team was emptied by players leaving goes
on for a minute instead of ending at once (ae_team_wait_rules.c has the rule). A host with bots fills the empty team
the way it always does (ae_lan_bots.c brings a bot back when a person leaves); without bots, the players left read a
chat line and the game ends after the minute if nobody joins the empty team. One hook, in
game_engine_should_end_game.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "saved games/arena_gametypes.h"

#include "ae_lan_bots.h"
#include "ae_team_wait.h"
#include "ae_team_wait_rules.h"
#include "../src/server_mode.h"
#ifdef HALO_PORT_CHAT
#include "chat.h"
#endif

#include <stdio.h>

void platform_log(char const *format, ...);

static long held_since = NONE; /* the game time the wait began, NONE when not waiting */
static int said_mark = 0;      /* the last chat line said: 0 none, 1 the start, 2 thirty seconds, 3 ten seconds */
static long last_seen = 0;     /* the game time of the last end check that found the teams short */

static void wait_say(char const *text)
{
	platform_log("ae team wait: %s", text);
#ifdef HALO_PORT_CHAT
	chat_server_say(text);
#endif
}

/* teams with a player still in the game (alive, dead or waiting to respawn); a player who quit counts only when the
host's bots stand in for it */
static int teams_present(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long first = NONE;
	int count = 0;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->quit_out_of_game && !ae_lan_bots_left_player(iterator.datum_index))
			continue;
		if (player->team_index == NONE)
			continue;
		if (count == 0)
		{
			first = player->team_index;
			count = 1;
		}
		else if (player->team_index != first)
			return 2;
	}
	return count;
}

boolean ae_team_wait_holds_end(void)
{
	struct game_variant *variant = game_engine_get_variant();
	int ae_rules = variant && arena_gametype_has_ae_rules(variant->human_readable_game_description);
	int teams = variant && variant->universal_variant.teams;
	long now = game_time_get();
	long held;
	long left;

	/* a wait from an earlier moment (the teams came back in between, or a new game started) does not carry over:
	the end check runs every tick, so a gap of a second means this is a new wait */
	if (held_since != NONE && (now < last_seen || now - last_seen > TICKS_PER_SECOND))
	{
		held_since = NONE;
		said_mark = 0;
	}
	last_seen = now;
	held = held_since == NONE ? 0 : now - held_since;

	if (ae_team_wait_rule(ae_team_wait_applies(ae_rules, halo_server_mode()), teams, teams_present(), held, TICKS_PER_SECOND) != _ae_team_wait_hold)
	{
		if (held_since != NONE)
			platform_log("ae team wait: ends the wait after %ld s", held / TICKS_PER_SECOND);
		held_since = NONE;
		said_mark = 0;
		return FALSE;
	}
	if (held_since == NONE)
	{
		held_since = now;
		held = 0;
	}
	left = AE_TEAM_WAIT_SECONDS - held / TICKS_PER_SECOND;
	if (said_mark < 1)
	{
		char line[128];

		snprintf(line, sizeof(line), "Waiting for players: a team is empty. The game ends in %d seconds if nobody "
			"joins it.", AE_TEAM_WAIT_SECONDS);
		wait_say(line);
		said_mark = 1;
	}
	else if (said_mark < 2 && left <= 30)
	{
		wait_say("Waiting for players: the game ends in 30 seconds.");
		said_mark = 2;
	}
	else if (said_mark < 3 && left <= 10)
	{
		wait_say("Waiting for players: the game ends in 10 seconds.");
		said_mark = 3;
	}
	return TRUE;
}

void ae_team_wait_reset(void)
{
	held_since = NONE;
	said_mark = 0;
}
