/*
AE_PARTY_TEAMS.C

See ae_party_teams.h.
*/

#include "cseries.h"
#include "memory/data.h"
#include "game/players.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"

#include "ae_party_teams.h"
#include "ae_party_teams_rules.h"
#include "ae_team_shuffle.h"
#include "ae_variant_ext.h"
#include "ae_lan_bots.h"
#include "bots.h"
#include "../src/port_config.h"

/* a bot's party number, past every machine's */
#define BOT_PARTY_BASE 1000

int ae_party_teams_on(void)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();

	if (!config_boolean("game.keep_parties_together"))
		return 0;
	return !ext || !(ext->match_flags & AE_EXT_MATCH_SPLIT_PARTIES);
}

int ae_party_teams_player_is_bot(struct network_game const *game, struct network_player const *player)
{
	if (bots_machine_is_bot(player->machine_index) &&
		game->machines[player->machine_index].machine_index != player->machine_index)
	{
		return TRUE;
	}
	return ae_lan_bots_network_player(player);
}

/* the lobby's players with a team of the game's, in slot order; slots[i] is where players[i] is */
static int lobby_players(struct network_game const *game, struct ae_party_player *players, long *slots)
{
	int count = 0;
	long slot;

	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS && count < AE_PARTY_PLAYERS_MAX; slot++)
	{
		struct network_player const *player = &game->players[slot];

		if (!network_player_is_valid((struct network_player *)player) || !VALID_INDEX(player->team_index, 2))
			continue;
		players[count].bot = ae_party_teams_player_is_bot(game, player) ? 1 : 0;
		players[count].party = players[count].bot ? BOT_PARTY_BASE + (int)slot : (int)player->machine_index;
		players[count].team = player->team_index;
		if (slots)
			slots[count] = slot;
		count++;
	}
	return count;
}

int ae_party_teams_join(struct network_game const *game, struct network_player const *player, int old_team)
{
	struct ae_party_player players[AE_PARTY_PLAYERS_MAX];
	int count, size = 1, cap, team;
	long slot;

	if (!game || !player || !game->variant.universal_variant.teams || !ae_party_teams_on() ||
		ae_party_teams_player_is_bot(game, player))
	{
		return old_team;
	}
	count = lobby_players(game, players, NULL);
	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		struct network_player const *other = &game->players[slot];

		if (network_player_is_valid((struct network_player *)other) && other->machine_index == player->machine_index &&
			!ae_party_teams_player_is_bot(game, other))
		{
			size++;
		}
	}
	/* (a party arrives a player at a time: those after the first take its team, and the first's room counts the
	players its machine has in the game now, one more) */
	cap = game->maximum_players ? (game->maximum_players + 1) / 2 : HALO_PORT_MAXIMUM_NETWORK_PLAYERS;
	team = ae_party_join_team(players, count, (int)player->machine_index, size, cap);
	return team < 0 ? old_team : team;
}

int ae_party_teams_balance_lobby(struct network_game *game, int *changed)
{
	struct ae_party_player players[AE_PARTY_PLAYERS_MAX];
	long slots[AE_PARTY_PLAYERS_MAX];
	int count, index;

	*changed = 0;
	if (!game || !ae_party_teams_on())
		return 0;
	count = lobby_players(game, players, slots);
	if (ae_party_balance(players, count) <= 0)
		return 1;
	for (index = 0; index < count; index++)
	{
		if (game->players[slots[index]].team_index != players[index].team)
		{
			game->players[slots[index]].team_index = (char)players[index].team;
			*changed = 1;
		}
	}
	return 1;
}

/* the slot and team the sender's player had before the host took its settings (ae_party_teams_note_player) */
static struct { long slot; int team; } noted = { NONE, NONE };

void ae_party_teams_note_player(struct network_game const *game, struct network_player const *player)
{
	noted.slot = NONE;
	noted.team = NONE;
	if (game && player && VALID_INDEX(player->player_list_index, HALO_PORT_MAXIMUM_NETWORK_PLAYERS) &&
		network_player_is_valid((struct network_player *)&game->players[player->player_list_index]))
	{
		noted.slot = player->player_list_index;
		noted.team = game->players[player->player_list_index].team_index;
	}
}

void ae_party_teams_follow(struct network_game *game, struct network_player const *player)
{
	struct ae_party_player players[AE_PARTY_PLAYERS_MAX];
	long slots[AE_PARTY_PLAYERS_MAX];
	int count, index, sender = -1;

	/* (only a team change moves anyone: a name or a colour change moves nobody; the siblings who were on the sender's old
	team follow it) */
	if (!game || !player || noted.slot != player->player_list_index || !game->variant.universal_variant.teams ||
		!VALID_INDEX(player->team_index, 2) || !VALID_INDEX(noted.team, 2) || player->team_index == noted.team ||
		!ae_party_teams_on() || ae_party_teams_player_is_bot(game, player))
	{
		return;
	}
	count = lobby_players(game, players, slots);
	for (index = 0; index < count; index++)
	{
		if (slots[index] == noted.slot)
			sender = index;
	}
	if (sender < 0)
		return;
	/* (the settings are in the game's record now: the sender is on its new team, and its siblings were on the old) */
	if (ae_party_follow(players, count, sender, noted.team, player->team_index) <= 0)
		return;
	for (index = 0; index < count; index++)
		game->players[slots[index]].team_index = (char)players[index].team;
	noted.slot = NONE;
}

int ae_party_teams_group_size(struct network_game const *game, long slot)
{
	struct ae_party_player players[AE_PARTY_PLAYERS_MAX];
	long slots[AE_PARTY_PLAYERS_MAX];
	int count, index, size;

	if (!game || slot < 0 || slot >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS || !ae_party_teams_on())
		return 1;
	/* (the people of the slot's machine on its team: those ae_party_teams_follow moves with it) */
	count = lobby_players(game, players, slots);
	for (index = 0; index < count; index++)
	{
		if (slots[index] == slot)
		{
			size = players[index].bot ? 1 : ae_party_group_size(players, count, players[index].party, players[index].team);
			return size < 1 ? 1 : size;
		}
	}
	return 1;
}

int ae_party_teams_death_may_move(long player_index)
{
	struct data_iterator iterator;
	struct player_datum *player, *dead = player_try_and_get(player_index);
	struct ae_party_player people[AE_PARTY_PLAYERS_MAX];
	short count[2] = { 0, 0 };
	int size, people_count = 0, party;

	if (!dead || !ae_party_teams_on() || !VALID_INDEX(dead->team_index, 2))
		return 1;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->quit_out_of_game || !VALID_INDEX(player->team_index, 2))
			continue;
		count[player->team_index]++;
		if (people_count < AE_PARTY_PLAYERS_MAX)
		{
			/* (a bot, whose machine is its own, and a LAN bot, which shares its host's machine, are not people) */
			people[people_count].bot = (player->local_player_index == NONE &&
				bots_machine_is_bot(player->network_player_data.machine_index)) ||
				ae_lan_bots_network_player(&player->network_player_data);
			people[people_count].party = (int)player->network_player_data.machine_index;
			people[people_count].team = player->team_index;
			people_count++;
		}
	}
	/* (a bot is alone: its party is itself) */
	party = (int)dead->network_player_data.machine_index;
	if ((dead->local_player_index == NONE && bots_machine_is_bot(dead->network_player_data.machine_index)) ||
		ae_lan_bots_network_player(&dead->network_player_data))
		size = 1;
	else
		size = ae_party_group_size(people, people_count, party, dead->team_index);
	return ae_party_death_may_move(size, count[dead->team_index], count[1 - dead->team_index]);
}

void ae_party_teams_deal(int const *machine, int count, unsigned long seed, signed char *teams)
{
	if (ae_party_teams_on())
		ae_party_deal(machine, count, seed, teams);
	else
		ae_team_shuffle_deal(count, seed, teams);
}
