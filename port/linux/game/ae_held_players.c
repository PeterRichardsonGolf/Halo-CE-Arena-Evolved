/*
AE_HELD_PLAYERS.C

A joining client's player additions that map sharing held (network_client_manager.c network_game_client_add_player:
the host's settings not come yet, or the map being downloaded: map_share_holds_players). Upstream's pregame widget
asks again each frame, so a held ask is only late there; AE's menus ask once (ae_screen_browser.c, ae_screen_join_invite.c,
ae_glue_lobby.c ae_lobby_add_local_player), and a held ask was lost: the game started with no player for that
machine, its screen black (seen in 0.5.0's test round, a second PC joining a lobby). Each held controller is remembered
and asked for from the client's idle once the hold is over, in the pregame or in a game in progress the client has
loaded, unless the host's settings have the player already (another ask went through meanwhile). Cleared by a
removal ask and the client's reset.

(0.7.9 join race) Each ask is also remembered as WANTED until the host's settings have the player: one they still lack
2 seconds after the ask is asked for again, at most ASK_AGAIN_MOST times. A machine that joined a game in progress and
downloaded its map was seen sitting in the pregame with no ask ever reaching the host (a community dedicated server,
0.7.7 and 0.7.8: "add requests 0" in the server's timeout line), until the server's playerless timeout dropped it.
*/

#include "cseries.h"
#include "cseries_windows.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_manager.h"
#include "networking/network_messages.h"

#include "ae_glue_lobby.h"
#include "ae_screen_main_menu.h"
#include "ae_hooks.h"
#include "map_share.h"

/* (network_client_manager.c enum network_game_client_state) */
enum { AE_CLIENT_STATE_INGAME = 3 };

#define ASK_AGAIN_MS 2000UL
#define ASK_AGAIN_MOST 10

static boolean held[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
/* (0.7.9) asked for and not yet in the host's settings; when last asked; how many times asked again */
static boolean wanted[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
static unsigned long asked_at[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
static short asked_again[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

/* (0.7.9 split screen join) when the client joined a game in progress */
static unsigned long ingame_asked_at[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
static short ingame_asked[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

boolean ae_held_player_note(short local_player_index, boolean is_held)
{
	if (local_player_index >= 0 && local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
	{
		held[local_player_index] = is_held;
		wanted[local_player_index] = TRUE;
		asked_at[local_player_index] = system_milliseconds();
	}
	return TRUE;
}

void ae_held_player_removed(short local_player_index)
{
	if (local_player_index >= 0 && local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
	{
		held[local_player_index] = FALSE;
		wanted[local_player_index] = FALSE;
		asked_again[local_player_index] = 0;
		/* (a player taken out stays out: no more asks for its pad) */
		ingame_asked[local_player_index] = ASK_AGAIN_MOST;
	}
}

void ae_held_players_clear(void)
{
	csmemset(ingame_asked_at, 0, sizeof(ingame_asked_at));
	csmemset(ingame_asked, 0, sizeof(ingame_asked));
	csmemset(held, 0, sizeof(held));
	csmemset(wanted, 0, sizeof(wanted));
	csmemset(asked_again, 0, sizeof(asked_again));
}

/* this machine's player of the controller is in the host's settings */
static boolean in_game_of(struct network_game const *game, long machine_index, short controller)
{
	long index;

	for (index = 0; index < (long)NUMBEROF(game->players); index++)
	{
		struct network_player const *player = &game->players[index];

		if (network_player_is_valid((struct network_player *)player) && player->machine_index == machine_index &&
			player->controller_index == controller)
			return TRUE;
	}
	return FALSE;
}

/* (0.7.9 join race) an ask the host's settings never answered, asked again */
static void ask_again_unanswered(struct network_game_client *client, long machine_index, struct network_game const *game)
{
	unsigned long now = system_milliseconds();
	short controller;

	for (controller = 0; controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; controller++)
	{
		if (!wanted[controller] || held[controller])
			continue;
		if (in_game_of(game, machine_index, controller))
		{
			if (asked_again[controller])
				network_event("join-race: controller index #%d is in the host's settings after %d asks again",
					controller, (int)asked_again[controller]);
			wanted[controller] = FALSE;
			asked_again[controller] = 0;
			continue;
		}
		if (now - asked_at[controller] < ASK_AGAIN_MS)
			continue;
		if (asked_again[controller] >= ASK_AGAIN_MOST)
		{
			network_event("join-race: controller index #%d still has no player in the host's settings after %d asks "
				"again: no more", controller, (int)asked_again[controller]);
			wanted[controller] = FALSE;
			continue;
		}
		asked_again[controller]++;
		network_event("join-race: controller index #%d has no player in the host's settings %d ms after its ask: asking "
			"again (%d of %d)", controller, (int)(now - asked_at[controller]), (int)asked_again[controller], ASK_AGAIN_MOST);
		network_game_client_add_player(client, controller);
	}
}

/* (0.7.9 split screen join) A machine with 2-3 local players that joins a game in progress: the browser asks for
controller 0 alone, and the other pads' players (signed in on the main menu) joined from the create-game lobby page's
update or the server lobby page, both gone once the game runs: they never asked, the server (which makes a bot give way
for each person who asks, ae_lan_bots_make_room) never heard of them, and their pads had no player. Each pad signed in
that has no player yet joins now, once this machine's first player is in. */
static void ask_signed_in_ingame(struct network_game_client *client, long machine_index, struct network_game const *game)
{
	unsigned long now = system_milliseconds();
	short controller = ae_screen_main_menu_signed_in_waiting();
	struct ae_result joined;
	boolean any_in = FALSE;
	short index;

	(void)client;
	if (controller == NONE || controller < 1 || controller >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return;
	for (index = 0; index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; index++)
		any_in = any_in || in_game_of(game, machine_index, index);
	if (!any_in || wanted[controller] || held[controller] || in_game_of(game, machine_index, controller) ||
		ingame_asked[controller] >= ASK_AGAIN_MOST ||
		(ingame_asked_at[controller] && now - ingame_asked_at[controller] < ASK_AGAIN_MS))
	{
		return;
	}
	ingame_asked_at[controller] = now + (now == 0);
	ingame_asked[controller]++;
	network_event("split screen join: controller index #%d is signed in with no player in the game in progress: joining "
		"(%d of %d)", controller, (int)ingame_asked[controller], ASK_AGAIN_MOST);
	joined = ae_lobby_join_controller(controller);
	if (joined.ok)
		ae_screen_main_menu_signed_in_joined(controller);
}

void ae_held_players_send(struct network_game_client *client, boolean ready, long machine_index,
	struct network_game const *game)
{
	short controller;

	if (!client || !game)
		return;
	if (!ready || map_share_holds_players())
	{
		/* (join-race debug: a held ask still waiting, told every 5 seconds: what it waits for) */
		static unsigned long told_at;
		boolean any = FALSE;

		for (controller = 0; controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; controller++)
			any = any || held[controller];
		if (any && system_milliseconds() - told_at > 5000)
		{
			told_at = system_milliseconds();
			network_event("a held player addition waits (client ready %d, map sharing holds players %d)",
				(int)ready, (int)map_share_holds_players());
		}
		return;
	}
	for (controller = 0; controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; controller++)
	{
		if (!held[controller])
			continue;
		held[controller] = FALSE;
		if (in_game_of(game, machine_index, controller))
			continue;
		network_event("the player addition held for the host's settings or the map goes now (controller index #%d)",
			controller);
		network_game_client_add_player(client, controller);
	}
	/* (not the dedicated server's own client, which adds no player: network_game_client_add_player) */
	if (machine_index >= 0)
	{
		ask_again_unanswered(client, machine_index, game);
		if (network_game_client_get_state(client, NULL) == AE_CLIENT_STATE_INGAME)
			ask_signed_in_ingame(client, machine_index, game);
	}
}
