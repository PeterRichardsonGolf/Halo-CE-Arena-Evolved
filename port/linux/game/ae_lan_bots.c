/*
AE_LAN_BOTS.C

Bots in LAN games (ae_lan_bots.h; the rules: ae_lan_bots_rules.c). The host
keeps which of its controller slots are bots; nothing of it travels: to the
other machines a bot is the host's player at that controller.

- The lobby (ae_lan_bots_lobby, every frame of the host's server): while the
  game is set up, as many bots as bots.count asks for (at most three, one
  player slot of the game left free), on the host's free controller slots,
  their teams as bots.c's local games have them (bots.teams); they leave for
  a person of the host's machine who joins on their slot, when the setting
  asks for fewer, for a gametype bots do not play (they play Slayer, CTF,
  Oddball, King and Race: ae_bots_objective.c), and in a game hosted for
  the internet (LAN only for now: the internet's joiners would see players
  whose machine is the host's, which is fine on the wire, but the host's
  upload of their input and units is not yet measured).
- The game: a bot is no local player of the host (network_game_spawn_player
  and the client's start leave it out, AE hooks), so the host shows, hears and
  steers only its people; each tick the host's update takes the bot's input
  from the brain (ae_lan_bots_action), as it takes a client's from the wire.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/player_ui.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"

#include "bots.h"
#include "ae_lan_bots_rules.h"
#include "ae_lan_bots.h"
#include "ae_bots_objective.h"

#include <string.h>

/* port/linux/src/port_config.c, p2p.c */
void platform_log(char const *format, ...);
int p2p_invite_link(char *link, int size);
/* network_server_manager.c's */
void get_unique_random_color(struct network_game_server *server, struct network_player *player);

/* ---------- the host's bots */

static struct
{
	struct
	{
		boolean active;
		/* the host machine's controller slot, the game's player slot */
		char controller;
		char slot;
	} bots[AE_LAN_BOTS_MAXIMUM];
	/* (halo.log's lines once a reason) */
	long told_engine;
	boolean told_internet;
	/* the tick the brain's shared work was last done for */
	long last_tick;
	boolean tick_valid;
} lan = { { { FALSE, 0, 0 } }, NONE, FALSE, NONE, FALSE };

static boolean lan_hosting(void)
{
	return global_network_game_server_get() != NULL && !network_game_is_splitscreen_local();
}

static boolean lan_any(void)
{
	short index;

	for (index = 0; index < AE_LAN_BOTS_MAXIMUM; index++)
	{
		if (lan.bots[index].active)
			return TRUE;
	}
	return FALSE;
}

static void lan_forget(void)
{
	short index;

	for (index = 0; index < AE_LAN_BOTS_MAXIMUM; index++)
		lan.bots[index].active = FALSE;
}

/* the bot number on this machine's controller slot, NONE for none */
static short lan_bot_of(long machine_index, long controller_index)
{
	short local_machine = network_game_client_get_local_machine_index();
	short index;

	if (local_machine == NONE || machine_index != local_machine)
		return NONE;
	for (index = 0; index < AE_LAN_BOTS_MAXIMUM; index++)
	{
		if (lan.bots[index].active && lan.bots[index].controller == controller_index)
			return index;
	}
	return NONE;
}

static boolean lan_same_name(wchar_t const *a, wchar_t const *b)
{
	short index;

	for (index = 0; index < 12; index++)
	{
		if (a[index] != b[index])
			return FALSE;
		if (!a[index])
			return TRUE;
	}
	return TRUE;
}

/* a bot still in the game as it was put there (its slot, machine, controller and name): else forgotten (the
lobby was another, or something took the slot) */
static boolean lan_bot_still_there(struct network_game *game, short bot_index, short machine)
{
	struct network_player *player;
	struct network_player expected;

	if (!lan.bots[bot_index].active)
		return FALSE;
	if (!VALID_INDEX(lan.bots[bot_index].slot, NUMBEROF(game->players)))
		return FALSE;
	player = &game->players[(long)lan.bots[bot_index].slot];
	bots_network_player(bot_index, &expected);
	return network_player_is_valid(player) && player->machine_index == machine &&
		player->controller_index == lan.bots[bot_index].controller && lan_same_name(player->name, expected.name);
}

static boolean lan_remove(struct network_game *game, short bot_index, char const *why)
{
	struct network_player leaving;
	boolean removed;

	/* (only the bot as it was put there: a slot something else took since is not this bot's to empty) */
	if (!lan_bot_still_there(game, bot_index, network_game_client_get_local_machine_index()))
	{
		lan.bots[bot_index].active = FALSE;
		return FALSE;
	}
	leaving = game->players[(long)lan.bots[bot_index].slot];
	removed = network_game_remove_player(game, &leaving);
	lan.bots[bot_index].active = FALSE;
	platform_log("lan bots: bot %d left the lobby (%s)", bot_index + 1, why);
	return removed;
}

boolean ae_lan_bots_lobby(struct network_game_server *server, struct network_game *game, boolean setting_up)
{
	short machine = network_game_client_get_local_machine_index();
	unsigned people = 0, wanting = 0, bots_mask = 0, free_mask;
	long team_counts[2] = { 0, 0 };
	long people_team = NONE;
	long others = 0;
	long slot;
	short index, count = 0;
	short target;
	boolean has_person = FALSE;
	boolean internet;
	boolean played;
	boolean eligible;
	boolean changed = FALSE;
	char link[160];

	/* (the game under way, its loading and its end keep their bots) */
	if (!setting_up || !game)
		return FALSE;
	if (network_game_is_splitscreen_local() || machine == NONE)
	{
		lan_forget();
		return FALSE;
	}
	for (index = 0; index < AE_LAN_BOTS_MAXIMUM; index++)
	{
		if (lan.bots[index].active && !lan_bot_still_there(game, index, machine))
			lan.bots[index].active = FALSE;
		if (lan.bots[index].active)
			bots_mask |= 1u << lan.bots[index].controller;
	}
	for (slot = 0; slot < (long)NUMBEROF(game->players); slot++)
	{
		struct network_player *player = &game->players[slot];

		if (!network_player_is_valid(player) || lan_bot_of(player->machine_index, player->controller_index) != NONE)
			continue;
		others++;
		if (VALID_INDEX(player->team_index, 2))
		{
			team_counts[(long)player->team_index]++;
			if (people_team == NONE)
				people_team = player->team_index;
		}
		if (player->machine_index == machine && VALID_INDEX(player->controller_index, AE_LAN_BOTS_CONTROLLERS))
		{
			people |= 1u << player->controller_index;
			has_person = TRUE;
		}
	}
	/* (a pad that pressed START on the Xbox lobby, its player not yet added: ui_widget_game_data_input_functions.c
	adds none on a slot this machine already has a player on) */
	for (index = 0; index < AE_LAN_BOTS_CONTROLLERS; index++)
	{
		if (player_ui_local_player_wants_to_play_multiplayer(index) && !(people & (1u << index)))
			wanting |= 1u << index;
	}
	/* (a lobby whose gametype is not set yet, between two games: its bots stay as they are) */
	if (game->variant.game_engine_index == game_engine_none)
		return FALSE;
	internet = p2p_invite_link(link, sizeof(link)) != 0;
	played = ae_bots_engine_played(game->variant.game_engine_index);
	eligible = ae_lan_bots_eligible(TRUE, FALSE, internet, played, has_person);
	free_mask = ae_lan_bots_free_controllers(people, wanting);
	target = (short)ae_lan_bots_target(bots_wanted_count(), eligible, others, game->maximum_players, free_mask);

	/* (once a reason, while bots.count asks for some) */
	if (bots_wanted_count() > 0 && has_person)
	{
		if (internet && !lan.told_internet)
			platform_log("lan bots: none in a game hosted for the internet (LAN games only)");
		lan.told_internet = internet;
		if (!played && lan.told_engine != game->variant.game_engine_index)
		{
			platform_log("lan bots: none in this game: bots do not play its gametype (they play Slayer, CTF, Oddball, King and Race)");
		}
		lan.told_engine = played ? NONE : game->variant.game_engine_index;
	}

	/* those on a slot a person takes, then those not wanted (the last first) */
	for (index = 0; index < AE_LAN_BOTS_MAXIMUM; index++)
	{
		if (lan.bots[index].active && !(free_mask & (1u << lan.bots[index].controller)))
		{
			bots_mask &= ~(1u << lan.bots[index].controller);
			changed |= lan_remove(game, index, "a person takes its controller");
		}
		count += lan.bots[index].active ? 1 : 0;
	}
	for (index = AE_LAN_BOTS_MAXIMUM - 1; index >= 0 && count > target; index--)
	{
		if (lan.bots[index].active)
		{
			bots_mask &= ~(1u << lan.bots[index].controller);
			changed |= lan_remove(game, index, eligible ? "fewer wanted, or no room" : !has_person ?
				"no person of the host's machine in the lobby" : internet ? "a game hosted for the internet" :
				"bots do not play its gametype");
			count--;
		}
	}
	/* those wanted join, on the highest free slots */
	for (index = 0; index < AE_LAN_BOTS_MAXIMUM && count < target; index++)
	{
		struct network_player joining;
		int controller;

		if (lan.bots[index].active)
			continue;
		controller = ae_lan_bots_pick_controller(free_mask, bots_mask);
		if (controller < 0)
			break;
		bots_network_player(index, &joining);
		joining.machine_index = (char)machine;
		joining.controller_index = (char)controller;
		joining.team_index = 0;
		joining.primary_color_index = NONE;
		get_unique_random_color(server, &joining);
		joining.player_list_index = NONE;
		if (!network_game_add_player(game, &joining))
			break;
		lan.bots[index].active = TRUE;
		lan.bots[index].controller = (char)controller;
		lan.bots[index].slot = joining.player_list_index;
		bots_mask |= 1u << controller;
		count++;
		changed = TRUE;
		platform_log("lan bots: bot %d joined the lobby (the host's controller %d, slot %d)", index + 1, controller + 1,
			(int)joining.player_list_index);
	}
	/* the teams: each in turn on the smaller one, or the people's other team (as bots.c's local games) */
	for (index = 0; index < AE_LAN_BOTS_MAXIMUM; index++)
	{
		struct network_player *player;
		int team;

		if (!lan.bots[index].active)
			continue;
		player = &game->players[(long)lan.bots[index].slot];
		team = ae_lan_bots_team(bots_teams() == _bots_teams_against, (int)people_team, (int)team_counts[0],
			(int)team_counts[1]);
		if (player->team_index != team)
		{
			player->team_index = (char)team;
			changed = TRUE;
		}
		team_counts[team]++;
	}
	return changed;
}

void ae_lan_bots_give_way(struct network_game *game, struct network_player const *joining, boolean setting_up)
{
	short bot_index;

	if (!setting_up || !game || !joining || !lan_hosting())
		return;
	bot_index = lan_bot_of(joining->machine_index, joining->controller_index);
	if (bot_index != NONE)
		lan_remove(game, bot_index, "a person joins on its controller");
}

boolean ae_lan_bots_network_player(struct network_player const *player)
{
	return player && lan_hosting() && lan_bot_of(player->machine_index, player->controller_index) != NONE;
}

/* the player at an absolute index that is a LAN bot (no local player here), its datum index; NONE for none */
static long lan_bot_player(long absolute_player_index, short *bot_index)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) != absolute_player_index)
			continue;
		if (player->local_player_index != NONE)
			return NONE;
		*bot_index = lan_bot_of(player->network_player_data.machine_index, player->network_player_data.controller_index);
		return *bot_index != NONE ? iterator.datum_index : NONE;
	}
	return NONE;
}

void ae_lan_bots_action(long absolute_player_index, struct player_action *action)
{
	long player_index;
	short bot_index = NONE;
	long now;

	if (!lan_any() || !player_data || !action || game_connection() != _game_connection_network_server ||
		!lan_hosting() || !game_engine_running())
	{
		return;
	}
	player_index = lan_bot_player(absolute_player_index, &bot_index);
	if (player_index == NONE)
		return;
	/* (once a tick, before its first bot: a new game's time starts again) */
	now = game_time_get();
	if (!lan.tick_valid || now != lan.last_tick)
	{
		bots_ae_lan_tick();
		lan.last_tick = now;
		lan.tick_valid = TRUE;
	}
	bots_ae_lan_drive(bot_index, player_index, action);
}

boolean ae_lan_bots_in_game(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (!lan_any() || !player_data || !lan_hosting())
		return FALSE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->local_player_index == NONE &&
			lan_bot_of(player->network_player_data.machine_index, player->network_player_data.controller_index) != NONE)
		{
			return TRUE;
		}
	}
	return FALSE;
}
