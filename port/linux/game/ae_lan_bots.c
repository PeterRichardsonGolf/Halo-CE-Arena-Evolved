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
  Oddball, King, Race and Bomb: ae_bots_objective.c), and in a game hosted for
  the internet (LAN only for now: the internet's joiners would see players
  whose machine is the host's, which is fine on the wire, but the host's
  upload of their input and units is not yet measured).
- The game: a bot is no local player of the host (network_game_spawn_player
  and the client's start leave it out, AE hooks), so the host shows, hears and
  steers only its people; each tick the host's update takes the bot's input
  from the brain (ae_lan_bots_action), as it takes a client's from the wire.

Network games' bots (debug.network_bots, on by default since 0.3.7; off it
is 0.3.6's LAN bots above): the internet's games have them too, "BOT "
before their names (a stock machine shows it too), and up to
AE_NET_BOTS_MAXIMUM: past the host's free controllers,
the bots sit on VIRTUAL MACHINES, machines of the host's own in the game's
machines (at the top machine indices, named "Bots"), four to a machine, with
no connection: the server's client machine slot stays empty, so none of the
server's per-connection work (loading, keep-alives, timeouts, map sharing,
kicks, the distributed netcode's per-machine sends) ever sees one, and every
other machine sees a machine with up to four players. A new connection
never takes a virtual machine's slot (ae_lan_bots_machine_held, an AE hook
in network_game_server_add_new_client). At the start of a team game whose
players are odd in number, one more bot takes the lobby's free slot
(ae_lan_bots_start), so the teams are even. The game in progress takes bots
up to bots.count as it has room (ae_lan_bots_ingame), on new virtual machines
too: such a machine joins the host's game as a late-joining machine does (the
other machines know it by its players, added in game). A bot's input asked
again for a tick it has one for (an update built twice in a tick) is the same
input.
*/

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/player_ui.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_messages.h"
#include "networking/network_server_message_handler.h"

#include "bots.h"
#include "ae_lan_bots_rules.h"
#include "ae_lan_bots.h"
#include "ae_bots_objective.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* port/linux/src/port_config.c, p2p.c */
void platform_log(char const *format, ...);
int config_boolean(const char *name);
int config_integer(const char *name);
int p2p_invite_link(char *link, int size);
/* network_server_manager.c's */
void get_unique_random_color(struct network_game_server *server, struct network_player *player);

typedef char ae_lan_bots_brains_assert[BOTS_BRAINS >= AE_NET_BOTS_MAXIMUM ? 1 : -1];

#define LAN_BOTS AE_NET_BOTS_MAXIMUM
#define LAN_MACHINES AE_NET_BOTS_MACHINES
#define LAN_MACHINE_SLOTS HALO_PORT_MAXIMUM_NETWORK_MACHINES

/* the brain's time over a tick that the log counts (milliseconds) */
static double const lan_tick_marks[] = { 5.0, 16.0, 33.0 };

/* ---------- the host's bots */

static struct
{
	boolean initialized;
	struct
	{
		boolean active;
		/* the machine (the host's, or a virtual machine), its controller slot, the game's player slot */
		short machine;
		char controller;
		char slot;
		/* the team the host chose for it in the lobby (the PC lobby's team arrows), 0 red or 1 blue; NONE: dealt by
		bots.teams */
		signed char team;
		/* the game tick its input was last made for, and that input (an update built twice in a tick) */
		long driven_tick;
		struct player_action action;
	} bots[LAN_BOTS];
	/* the virtual machines' machine indices (NONE: none there) */
	short machines[LAN_MACHINES];
	/* the bot on each machine's controller slot (NONE) */
	short seats[LAN_MACHINE_SLOTS][AE_LAN_BOTS_CONTROLLERS];
	/* (ae_lan_bots_start: the lobby's last look before the start) */
	boolean starting;
	/* bots.teams as the lobby last saw it (NONE: not yet): a change lets go of the teams the host chose */
	short told_teams;
	/* (halo.log's lines once a reason) */
	long told_engine;
	boolean told_internet;
	/* the tick the brain's shared work was last done for */
	long last_tick;
	boolean tick_valid;
	/* (debug.network_bots) the brain's time on the host, per bot's input and per tick (all its bots) */
	double brain_seconds;
	double brain_input_max;
	long brain_inputs;
	long brain_repeats;
	double tick_seconds;
	double tick_seconds_max;
	double tick_seconds_total;
	long ticks;
	long ticks_over[NUMBEROF(lan_tick_marks)];
	/* the game tick and the time of the last line (NONE: none yet) */
	long report_tick;
	double report_time;
	/* the game in progress: its players as last counted (people and bots) and the tick they last changed, so that a
	bot comes back only once the game has settled (NONE: not yet in a game) */
	long ingame_signature;
	long ingame_settled_tick;
	/* the bots' own last step in it (ae_lan_bots_ingame_step: > 0 joined, < 0 left; 0 none since its players last
	changed otherwise) */
	int ingame_last_step;
	/* the host's local players (its views) as last told in halo.log: a bot is none of them (NONE: not yet) */
	short ingame_views;
	/* the player datums of the bots that left the game in progress, to the game's end (NONE: none): they hold their
	teams in it (ae_lan_bots_left_player), and the next ones overwrite the oldest */
	long left_players[LAN_BOTS];
	int left_next;
} lan;

enum
{
	/* network_server_manager.c's NETWORK_GAME_PLAYER_QUIT_DELAY: the ticks from a quit message to the quit */
	LAN_QUIT_DELAY = 33,
};

/* (the game in progress waits AE_LAN_BOTS_SETTLE_TICKS after its players change, two seconds: a seat let go is free
on every machine by then) */
typedef char ae_lan_bots_settle_assert[AE_LAN_BOTS_SETTLE_TICKS == 2 * TICKS_PER_SECOND &&
	AE_LAN_BOTS_SETTLE_TICKS > LAN_QUIT_DELAY ? 1 : -1];

/* _message_server_remove_player_ingame's record (network_server_manager.c's message_server_remove_player_ingame):
the player and its quit time */
struct lan_remove_message
{
	struct network_player player;
	long quit_time;
};

static void lan_initialize(void)
{
	long index, controller;

	if (lan.initialized)
		return;
	csmemset(&lan, 0, sizeof(lan));
	for (index = 0; index < LAN_BOTS; index++)
	{
		lan.bots[index].driven_tick = NONE;
		lan.bots[index].team = (signed char)NONE;
	}
	for (index = 0; index < LAN_MACHINES; index++)
		lan.machines[index] = NONE;
	for (index = 0; index < LAN_MACHINE_SLOTS; index++)
	{
		for (controller = 0; controller < AE_LAN_BOTS_CONTROLLERS; controller++)
			lan.seats[index][controller] = NONE;
	}
	lan.told_engine = NONE;
	lan.told_teams = NONE;
	lan.last_tick = NONE;
	lan.report_tick = NONE;
	lan.ingame_signature = NONE;
	lan.ingame_settled_tick = NONE;
	lan.ingame_last_step = 0;
	lan.ingame_views = NONE;
	for (index = 0; index < LAN_BOTS; index++)
		lan.left_players[index] = NONE;
	lan.initialized = TRUE;
}

/* debug.network_bots (HALO_NETWORK_BOTS, on by default; an off switch): the host's bots play in a game it hosts for the
internet too, more than three of them (virtual machines), and halo.log has the brain's time a minute. Internet
play's tunnel (p2p.c) carries the system link game's own traffic, so the bots reach an internet client as they reach
a LAN one: the host's players. */
static boolean lan_network_bots(void)
{
	return config_boolean("debug.network_bots") != 0;
}

/* test hook (the environment only, no setting): HALO_NETWORK_BOTS_FAKE_INTERNET=1 has the lobby take the game for one hosted for the internet
(the box's tests run with network.online off: no tunnel, no invite) */
static boolean lan_internet(void)
{
	char link[160];
	char const *fake = getenv("HALO_NETWORK_BOTS_FAKE_INTERNET");

	if (fake && fake[0] == '1')
		return TRUE;
	return p2p_invite_link(link, sizeof(link)) != 0;
}

/* the bots bots.count asks for: up to 15 (bots.c's local games), with debug.network_bots up to 127 */
static long lan_wanted(void)
{
	long count;

	if (!lan_network_bots())
		return bots_wanted_count();
	count = config_integer("bots.count");
	return count < 0 ? 0 : count > AE_NET_BOTS_MAXIMUM ? AE_NET_BOTS_MAXIMUM : count;
}

static double lan_now(void)
{
	LARGE_INTEGER now, frequency;

	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&frequency);
	return (double)now.QuadPart / (double)frequency.QuadPart;
}

static boolean lan_hosting(void)
{
	return global_network_game_server_get() != NULL && !network_game_is_splitscreen_local();
}

static boolean lan_any(void)
{
	short index;

	if (!lan.initialized)
		return FALSE;
	for (index = 0; index < LAN_BOTS; index++)
	{
		if (lan.bots[index].active)
			return TRUE;
	}
	return FALSE;
}

static void lan_unseat(short bot_index)
{
	long machine = lan.bots[bot_index].machine;
	long controller = lan.bots[bot_index].controller;

	if (VALID_INDEX(machine, LAN_MACHINE_SLOTS) && VALID_INDEX(controller, AE_LAN_BOTS_CONTROLLERS) &&
		lan.seats[machine][controller] == bot_index)
	{
		lan.seats[machine][controller] = NONE;
	}
	lan.bots[bot_index].active = FALSE;
	lan.bots[bot_index].driven_tick = NONE;
	lan.bots[bot_index].team = (signed char)NONE;
}

static void lan_forget(void)
{
	short index;

	lan_initialize();
	for (index = 0; index < LAN_BOTS; index++)
	{
		if (lan.bots[index].active)
			lan_unseat(index);
	}
	for (index = 0; index < LAN_MACHINES; index++)
		lan.machines[index] = NONE;
	/* (a seat left naming a bot that is not active, or a machine slot this call never saw, would hand a later game
	a bot that is not there: every seat is let go, whatever the bots say) */
	for (index = 0; index < LAN_MACHINE_SLOTS; index++)
	{
		short controller;

		for (controller = 0; controller < AE_LAN_BOTS_CONTROLLERS; controller++)
			lan.seats[index][controller] = NONE;
	}
}

/* the bot on a machine's controller slot (the host's or a virtual machine), NONE for none */
static short lan_bot_of(long machine_index, long controller_index)
{
	if (!lan.initialized || !VALID_INDEX(machine_index, LAN_MACHINE_SLOTS) ||
		!VALID_INDEX(controller_index, AE_LAN_BOTS_CONTROLLERS))
	{
		return NONE;
	}
	return lan.seats[machine_index][controller_index];
}

/* the virtual machine number of a machine index, NONE for none of them */
static short lan_machine_number(long machine_index)
{
	short index;

	if (!lan.initialized || machine_index == NONE)
		return NONE;
	for (index = 0; index < LAN_MACHINES; index++)
	{
		if (lan.machines[index] == machine_index)
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

/* a bot's network player: bots.c's fifteen names, then the same with a number; in a network game (debug.network_bots)
"BOT " before it, so that every machine, a stock one too, tells a bot from a person */
static void lan_bot_named(short bot_index, boolean tagged, struct network_player *player)
{
	char base[16], name[AE_LAN_BOTS_NAME_LENGTH + 1];
	long index;

	bots_network_player(bot_index % BOTS_MAXIMUM, player);
	for (index = 0; index < (long)sizeof(base) - 1 && index < (long)NUMBEROF(player->name) && player->name[index]; index++)
		base[index] = (char)player->name[index];
	base[index] = 0;
	ae_lan_bots_name(base, ae_lan_bots_name_number(bot_index, BOTS_MAXIMUM), tagged, name, sizeof(name));
	for (index = 0; index < (long)NUMBEROF(player->name) - 1 && name[index]; index++)
		player->name[index] = (wchar_t)name[index];
	player->name[index] = 0;
}

static void lan_bot_player(short bot_index, struct network_player *player)
{
	lan_bot_named(bot_index, lan_network_bots(), player);
}

/* a bot still in the game as it was put there (its slot, machine, controller and name): else forgotten (the
lobby was another, or something took the slot) */
static boolean lan_bot_still_there(struct network_game *game, short bot_index)
{
	struct network_player *player;
	struct network_player expected;

	if (!lan.bots[bot_index].active)
		return FALSE;
	if (!VALID_INDEX(lan.bots[bot_index].slot, NUMBEROF(game->players)))
		return FALSE;
	player = &game->players[(long)lan.bots[bot_index].slot];
	if (!network_player_is_valid(player) || player->machine_index != lan.bots[bot_index].machine ||
		player->controller_index != lan.bots[bot_index].controller)
	{
		return FALSE;
	}
	/* (its name with the tag or without: the switch may have changed since it joined) */
	lan_bot_named(bot_index, TRUE, &expected);
	if (lan_same_name(player->name, expected.name))
		return TRUE;
	lan_bot_named(bot_index, FALSE, &expected);
	return lan_same_name(player->name, expected.name);
}

static boolean lan_remove(struct network_game *game, short bot_index, char const *why)
{
	struct network_player leaving;
	boolean removed;

	/* (only the bot as it was put there: a slot something else took since is not this bot's to empty) */
	if (!lan_bot_still_there(game, bot_index))
	{
		lan_unseat(bot_index);
		return FALSE;
	}
	leaving = game->players[(long)lan.bots[bot_index].slot];
	removed = network_game_remove_player(game, &leaving);
	lan_unseat(bot_index);
	platform_log("lan bots: bot %d left the lobby (%s)", bot_index + 1, why);
	return removed;
}

/* a free machine slot for a virtual machine below `below`, from the top: none a connection holds or the game has */
static long lan_machine_slot(struct network_game_server *server, long below)
{
	long machine_index;

	for (machine_index = below - 1; machine_index > 0; machine_index--)
	{
		if (ae_lan_bots_server_slot_free(server, machine_index))
			return machine_index;
	}
	return NONE;
}

/* the virtual machines there are and may yet be had (free slots from the top) */
static int lan_machines_available(struct network_game_server *server)
{
	long machine_index = LAN_MACHINE_SLOTS;
	int count = 0;
	short index;

	for (index = 0; index < LAN_MACHINES; index++)
		count += lan.machines[index] != NONE ? 1 : 0;
	while (count < LAN_MACHINES && (machine_index = lan_machine_slot(server, machine_index)) != NONE)
		count++;
	return count;
}

/* a new virtual machine in the game, its number in lan.machines; NONE for none */
static short lan_machine_new(struct network_game_server *server, struct network_game *game)
{
	struct network_machine machine;
	long machine_index;
	short number;

	for (number = 0; number < LAN_MACHINES && lan.machines[number] != NONE; number++)
		;
	if (number >= LAN_MACHINES)
		return NONE;
	machine_index = lan_machine_slot(server, LAN_MACHINE_SLOTS);
	if (machine_index == NONE)
		return NONE;
	csmemset(&machine, 0, sizeof(machine));
	machine.name[0] = L'B';
	machine.name[1] = L'o';
	machine.name[2] = L't';
	machine.name[3] = L's';
	machine.machine_index = (char)machine_index;
	if (!network_game_add_machine(game, &machine))
		return NONE;
	lan.machines[number] = (short)machine_index;
	platform_log("net bots: virtual machine #%ld joined the %s (bots' machine %d)", machine_index,
		game_engine_running() ? "game in progress" : "lobby", number + 1);
	return number;
}

/* virtual machines with no bot leave the game (not with forget_only); those the game no longer has are forgotten */
static boolean lan_machines_tidy(struct network_game *game, boolean forget_only)
{
	boolean changed = FALSE;
	short number;

	for (number = 0; number < LAN_MACHINES; number++)
	{
		long machine_index = lan.machines[number];
		long controller;
		boolean seated = FALSE;

		if (machine_index == NONE)
			continue;
		if (game->machines[machine_index].machine_index != machine_index)
		{
			lan.machines[number] = NONE;
			continue;
		}
		if (forget_only)
			continue;
		for (controller = 0; controller < AE_LAN_BOTS_CONTROLLERS; controller++)
			seated |= lan.seats[machine_index][controller] != NONE;
		if (seated)
			continue;
		network_game_remove_machine(game, &game->machines[machine_index]);
		lan.machines[number] = NONE;
		changed = TRUE;
		platform_log("net bots: virtual machine #%ld left the lobby", machine_index);
	}
	return changed;
}

/* a seat for a new bot: the host's highest free controller no bot holds, else (debug.network_bots) a virtual
machine's free controller, a new machine's when those are full; FALSE for none */
static boolean lan_seat(struct network_game_server *server, struct network_game *game, short local_machine,
	unsigned free_mask, boolean network_bots, boolean new_machine, short *machine_index, char *controller_index)
{
	unsigned host_bots = 0;
	int counts[LAN_MACHINES];
	short numbers[LAN_MACHINES];
	int count = 0, pick;
	long controller;
	short number;

	for (controller = 0; controller < AE_LAN_BOTS_CONTROLLERS; controller++)
		host_bots |= lan.seats[local_machine][controller] != NONE ? 1u << controller : 0u;
	pick = ae_lan_bots_pick_controller(free_mask, host_bots);
	if (pick >= 0)
	{
		*machine_index = local_machine;
		*controller_index = (char)pick;
		return TRUE;
	}
	if (!network_bots)
		return FALSE;
	for (number = 0; number < LAN_MACHINES; number++)
	{
		if (lan.machines[number] == NONE)
			continue;
		counts[count] = 0;
		for (controller = 0; controller < AE_LAN_BOTS_CONTROLLERS; controller++)
			counts[count] += lan.seats[lan.machines[number]][controller] != NONE ? 1 : 0;
		numbers[count++] = number;
	}
	pick = ae_lan_bots_pick_machine(counts, count);
	if (pick < 0 || (pick >= count && !new_machine))
		return FALSE;
	number = pick < count ? numbers[pick] : lan_machine_new(server, game);
	if (number == NONE)
		return FALSE;
	for (controller = 0; controller < AE_LAN_BOTS_CONTROLLERS; controller++)
	{
		if (lan.seats[lan.machines[number]][controller] == NONE)
		{
			*machine_index = lan.machines[number];
			*controller_index = (char)controller;
			return TRUE;
		}
	}
	return FALSE;
}

boolean ae_lan_bots_lobby(struct network_game_server *server, struct network_game *game, boolean setting_up)
{
	short machine = network_game_client_get_local_machine_index();
	unsigned people = 0, wanting = 0, free_mask;
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
	boolean network_bots;
	boolean changed = FALSE;
	int capacity;
	long wanted;

	/* (the game under way, its loading and its end keep their bots) */
	if (!setting_up || !game)
		return FALSE;
	lan_initialize();
	if (network_game_is_splitscreen_local() || machine == NONE || !VALID_INDEX(machine, LAN_MACHINE_SLOTS))
	{
		lan_forget();
		return FALSE;
	}
	/* (the virtual machines the game still has, then the bots it still has) */
	lan_machines_tidy(game, TRUE);
	for (index = 0; index < LAN_BOTS; index++)
	{
		if (lan.bots[index].active && !lan_bot_still_there(game, index))
			lan_unseat(index);
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
	network_bots = lan_network_bots();
	/* test hook (the environment only, no setting): HALO_NETWORK_BOTS_MAXIMUM_PLAYERS sets the game's most players, as Server Setup's MAXIMUM
	PLAYERS does (the box's BTB tests host without the menus) */
	{
		char const *maximum = getenv("HALO_NETWORK_BOTS_MAXIMUM_PLAYERS");
		long players = maximum ? atol(maximum) : 0;

		if (network_bots && players >= 2 && players <= HALO_PORT_MAXIMUM_NETWORK_PLAYERS &&
			game->maximum_players != players)
		{
			game->maximum_players = (byte)players;
			changed = TRUE;
		}
	}
	internet = lan_internet();
	/* test hook (the environment only, no setting): HALO_NETWORK_BOTS_INVITE_FILE, a file the host's invite link is
	written to once it has one (the box's real internet test hosts in a window no one sees: the link is on the
	clipboard and only its start in the log) */
	{
		static boolean written;
		char const *path = getenv("HALO_NETWORK_BOTS_INVITE_FILE");
		char link[256];

		if (path && path[0] && !written && p2p_invite_link(link, sizeof(link)) && link[0])
		{
			FILE *file = fopen(path, "w");

			if (file)
			{
				fprintf(file, "%s\n", link);
				fclose(file);
				written = TRUE;
			}
		}
	}
	played = ae_bots_engine_played(game->variant.game_engine_index);
	/* (debug.network_bots lets the internet's games have them) */
	eligible = ae_lan_bots_eligible(TRUE, FALSE, internet && !network_bots, played, has_person);
	free_mask = ae_lan_bots_free_controllers(people, wanting);
	capacity = ae_lan_bots_capacity(network_bots, free_mask, network_bots ? lan_machines_available(server) : 0);
	wanted = lan_wanted();
	target = (short)ae_lan_bots_target((int)wanted, eligible, (int)others, game->maximum_players, capacity);
	/* (the start of a team game: one more, for even teams) */
	if (lan.starting && network_bots && eligible)
	{
		short extra = (short)ae_lan_bots_start_extra(game->variant.universal_variant.teams != 0,
			bots_teams() == _bots_teams_against, target, (int)others, game->maximum_players, capacity,
			(int)ae_lan_bots_server_playerless_machines(server));

		if (extra)
			platform_log("net bots: one more bot at the start, for even teams (%ld players and %d bots)", others, target);
		target += extra;
	}

	/* (once a reason, while bots.count asks for some) */
	if (wanted > 0 && has_person)
	{
		if (internet && !lan.told_internet)
			platform_log(network_bots ? "lan bots: a game hosted for the internet, bots in it (debug.network_bots)" :
				"lan bots: none in a game hosted for the internet (LAN games only)");
		lan.told_internet = internet;
		if (!played && lan.told_engine != game->variant.game_engine_index)
		{
			platform_log("lan bots: none in this game: bots do not play its gametype (they play Slayer, CTF, Oddball, King, Race and Bomb)");
		}
		lan.told_engine = played ? NONE : game->variant.game_engine_index;
	}

	/* those on a slot of the host's a person takes, then those not wanted (the last first) */
	for (index = 0; index < LAN_BOTS; index++)
	{
		if (lan.bots[index].active && lan.bots[index].machine == machine &&
			!(free_mask & (1u << lan.bots[index].controller)))
		{
			changed |= lan_remove(game, index, "a person takes its controller");
		}
		count += lan.bots[index].active ? 1 : 0;
	}
	for (index = LAN_BOTS - 1; index >= 0 && count > target; index--)
	{
		if (lan.bots[index].active)
		{
			changed |= lan_remove(game, index, eligible ? "fewer wanted, or no room" : !has_person ?
				"no person of the host's machine in the lobby" : internet ? "a game hosted for the internet" :
				"bots do not play its gametype");
			count--;
		}
	}
	/* those wanted join: the host's highest free slots, then the virtual machines' */
	for (index = 0; index < LAN_BOTS && count < target; index++)
	{
		struct network_player joining;
		short seat_machine;
		char controller;

		if (lan.bots[index].active)
			continue;
		if (!lan_seat(server, game, machine, free_mask, network_bots, TRUE, &seat_machine, &controller))
			break;
		lan_bot_player(index, &joining);
		joining.machine_index = (char)seat_machine;
		joining.controller_index = controller;
		joining.team_index = 0;
		joining.primary_color_index = NONE;
		get_unique_random_color(server, &joining);
		joining.player_list_index = NONE;
		if (!network_game_add_player(game, &joining))
			break;
		lan.bots[index].active = TRUE;
		lan.bots[index].machine = seat_machine;
		lan.bots[index].controller = controller;
		lan.bots[index].slot = joining.player_list_index;
		lan.bots[index].driven_tick = NONE;
		lan.bots[index].team = (signed char)NONE;
		lan.seats[seat_machine][(long)controller] = index;
		count++;
		changed = TRUE;
		platform_log("lan bots: bot %d joined the lobby (machine #%d%s, controller %d, slot %d)", index + 1,
			(int)seat_machine, seat_machine == machine ? " the host's" : "", controller + 1, (int)joining.player_list_index);
	}
	/* (virtual machines left with no bot leave) */
	changed |= lan_machines_tidy(game, FALSE);
	/* the teams: each in turn on the smaller one, or the people's other team (as bots.c's local games); one the host
	chose with the lobby's team arrows keeps its team (BOT TEAMS changed: those are let go) */
	if (lan.told_teams != bots_teams())
	{
		if (lan.told_teams != NONE)
			ae_lan_bots_clear_teams();
		lan.told_teams = bots_teams();
	}
	{
		int chosen[LAN_BOTS], teams[LAN_BOTS], seated[LAN_BOTS], seats = 0;

		for (index = 0; index < LAN_BOTS; index++)
		{
			if (!lan.bots[index].active)
				continue;
			chosen[seats] = lan.bots[index].team;
			seated[seats++] = index;
		}
		ae_lan_bots_teams(seats, chosen, bots_teams() == _bots_teams_against, (int)people_team, (int)team_counts[0],
			(int)team_counts[1], teams);
		for (index = 0; index < seats; index++)
		{
			struct network_player *player = &game->players[(long)lan.bots[seated[index]].slot];

			if (player->team_index != teams[index])
			{
				player->team_index = (char)teams[index];
				changed = TRUE;
			}
		}
	}
	return changed;
}

void ae_lan_bots_start(struct network_game_server *server, struct network_game *game)
{
	if (!server || !game || !lan_network_bots() || !lan_hosting())
		return;
	lan.starting = TRUE;
	ae_lan_bots_lobby(server, game, TRUE);
	lan.starting = FALSE;
}

/* ---------- the game in progress (debug.network_bots): a person takes a bot's place, a bot comes back */

/* the game's players as the bots see them */
struct lan_scan
{
	unsigned people;
	long others;
	long people_team;
	long team_players[2];
	long team_bots[2];
	long bots;
	boolean has_person;
};

static void lan_scan_players(struct network_game *game, short machine, struct lan_scan *scan)
{
	long slot;

	csmemset(scan, 0, sizeof(*scan));
	scan->people_team = NONE;
	for (slot = 0; slot < (long)NUMBEROF(game->players); slot++)
	{
		struct network_player *player = &game->players[slot];
		boolean bot;

		if (!network_player_is_valid(player))
			continue;
		bot = lan_bot_of(player->machine_index, player->controller_index) != NONE;
		if (VALID_INDEX(player->team_index, 2))
		{
			scan->team_players[(long)player->team_index]++;
			if (bot)
				scan->team_bots[(long)player->team_index]++;
			else if (scan->people_team == NONE)
				scan->people_team = player->team_index;
		}
		if (bot)
		{
			scan->bots++;
			continue;
		}
		scan->others++;
		if (player->machine_index == machine && VALID_INDEX(player->controller_index, AE_LAN_BOTS_CONTROLLERS))
		{
			scan->people |= 1u << player->controller_index;
			scan->has_person = TRUE;
		}
	}
}

static long lan_player_at(long absolute_index);

/* a bot leaves the game in progress: out of the game's players, and every machine told it quits (as a machine's
player that leaves: network_game_server_send_player_quit_messages_ingame), at once for the host's settings, at its
quit time on each machine; its slot is free for the person who takes its place */
static boolean lan_remove_ingame(struct network_game_server *server, struct network_game *game, short bot_index,
	char const *why)
{
	struct lan_remove_message removal;
	void *message;

	if (!lan_bot_still_there(game, bot_index))
	{
		lan_unseat(bot_index);
		return FALSE;
	}
	removal.player = game->players[(long)lan.bots[bot_index].slot];
	/* (its player, kept to the game's end as a player that quit, still holds its team) */
	lan.left_players[lan.left_next] = lan_player_at((long)lan.bots[bot_index].slot);
	lan.left_next = (lan.left_next + 1) % LAN_BOTS;
	if (!network_game_remove_player(game, &removal.player))
	{
		platform_log("net bots: bot %d could not leave the game in progress", bot_index + 1);
		return FALSE;
	}
	lan_unseat(bot_index);
	removal.quit_time = game_time_get() + LAN_QUIT_DELAY;
	message = create_network_game_message(_message_server_remove_player_ingame, &removal, sizeof(removal));
	if (!message || !network_game_server_send_message_to_all_machines(server, message))
		platform_log("net bots: the machines missed bot %d leaving the game in progress", bot_index + 1);
	platform_log("net bots: bot %d left the game in progress (%s, tick %ld)", bot_index + 1, why, game_time_get());
	return TRUE;
}

/* the bot that leaves the game in progress: the last of the team ae_lan_bots_leaving_team picks (any team's in a
game without teams); NONE for none */
static short lan_leaving_bot(struct network_game *game, struct lan_scan const *scan)
{
	int team = ae_lan_bots_leaving_team(game->variant.universal_variant.teams != 0, (int)scan->team_bots[0],
		(int)scan->team_bots[1], (int)scan->team_players[0], (int)scan->team_players[1]);
	short index;

	for (index = LAN_BOTS - 1; index >= 0; index--)
	{
		struct network_player *player;

		if (!lan.bots[index].active || !VALID_INDEX(lan.bots[index].slot, NUMBEROF(game->players)))
			continue;
		player = &game->players[(long)lan.bots[index].slot];
		if (team < 0 || player->team_index == team)
			return index;
	}
	return NONE;
}

boolean ae_lan_bots_room_ingame(struct network_game *game)
{
	struct lan_scan scan;
	short machine = network_game_client_get_local_machine_index();

	if (!game || !lan_any() || !lan_network_bots() || !lan_hosting() || machine == NONE)
		return FALSE;
	lan_scan_players(game, machine, &scan);
	return scan.bots > 0;
}

void ae_lan_bots_make_room(struct network_game_server *server, struct network_game *game,
	struct network_player const *joining)
{
	struct lan_scan scan;
	short machine = network_game_client_get_local_machine_index();
	short bot_index;

	if (!server || !game || !joining || !lan_any() || !lan_network_bots() || !lan_hosting() || machine == NONE)
		return;
	/* (a pad of the host's added in game on a bot's controller: that bot gives way) */
	bot_index = lan_bot_of(joining->machine_index, joining->controller_index);
	if (bot_index != NONE)
	{
		if (lan_remove_ingame(server, game, bot_index, "a person of the host's machine takes its controller"))
			lan.ingame_settled_tick = game_time_get();
		return;
	}
	if (network_game_has_free_player_slot(game))
		return;
	lan_scan_players(game, machine, &scan);
	bot_index = lan_leaving_bot(game, &scan);
	if (bot_index != NONE && lan_remove_ingame(server, game, bot_index, "a person joins the full game"))
		lan.ingame_settled_tick = game_time_get();
}

/* a bot joins the game in progress on the seat lan_seat gives it (a new virtual machine's when the others are full:
it joins the game as a late-joining machine does, in the host's game alone: the other machines know a machine by its
players), on the team the bots take, and every machine is told as for a player added in game; FALSE for none */
static boolean lan_add_ingame(struct network_game_server *server, struct network_game *game, short machine,
	unsigned free_mask, struct lan_scan *scan)
{
	struct network_player joining;
	short index, seat_machine;
	char controller;
	int team;

	for (index = 0; index < LAN_BOTS && lan.bots[index].active; index++)
		;
	if (index >= LAN_BOTS || !network_game_has_free_player_slot(game) ||
		!lan_seat(server, game, machine, free_mask, TRUE, TRUE, &seat_machine, &controller))
	{
		return FALSE;
	}
	team = ae_lan_bots_team(bots_teams() == _bots_teams_against, (int)scan->people_team, (int)scan->team_players[0],
		(int)scan->team_players[1]);
	lan_bot_player(index, &joining);
	joining.machine_index = (char)seat_machine;
	joining.controller_index = controller;
	joining.team_index = (char)team;
	joining.primary_color_index = NONE;
	get_unique_random_color(server, &joining);
	joining.player_list_index = NONE;
	/* (a virtual machine just added for it stays, with no bot, for the next: the lobby lets it go) */
	if (!network_game_add_player(game, &joining))
		return FALSE;
	/* (seated before the machines are told: the host's own spawns it as no local player) */
	lan.bots[index].active = TRUE;
	lan.bots[index].machine = seat_machine;
	lan.bots[index].controller = controller;
	lan.bots[index].slot = joining.player_list_index;
	lan.bots[index].driven_tick = NONE;
	lan.bots[index].team = (signed char)NONE;
	lan.seats[seat_machine][(long)controller] = index;
	if (!network_game_server_send_player_joined_info_ingame(server, &joining))
		platform_log("net bots: the machines missed bot %d joining the game in progress", index + 1);
	platform_log("net bots: bot %d joined the game in progress (machine #%d, controller %d, slot %d, team %d, tick %ld)",
		index + 1, (int)seat_machine, controller + 1, (int)joining.player_list_index, team, game_time_get());
	scan->bots++;
	scan->team_players[team]++;
	scan->team_bots[team]++;
	return TRUE;
}

void ae_lan_bots_ingame(struct network_game_server *server, struct network_game *game)
{
	struct lan_scan scan;
	short machine = network_game_client_get_local_machine_index();
	long now, signature, wanted;
	int capacity, target, step, done;
	unsigned free_mask;
	boolean played;

	if (!server || !game || !lan.initialized || !lan_network_bots() || !lan_hosting() || machine == NONE ||
		!VALID_INDEX(machine, LAN_MACHINE_SLOTS) || !game_engine_running() ||
		game->variant.game_engine_index == game_engine_none)
	{
		lan.ingame_signature = NONE;
		lan.ingame_views = NONE;
		/* (between games: the next game's left bots are its own) */
		if (lan.initialized && !game_engine_running())
		{
			short index;

			for (index = 0; index < LAN_BOTS; index++)
				lan.left_players[index] = NONE;
		}
		return;
	}
	now = game_time_get();
	lan_scan_players(game, machine, &scan);
	if (local_player_count() != lan.ingame_views)
	{
		lan.ingame_views = local_player_count();
		platform_log("net bots: the host has %d local players (views) with %d bots in the game (tick %ld)",
			(int)lan.ingame_views, (int)scan.bots, now);
	}
	/* (changes wait for the game to settle: its players unchanged for a while, no machine still to add its
	player; the bots' own steps the same way follow each other sooner) */
	signature = scan.others * 1000 + scan.bots;
	if (signature != lan.ingame_signature || lan.ingame_settled_tick == NONE || now < lan.ingame_settled_tick)
	{
		lan.ingame_signature = signature;
		lan.ingame_settled_tick = now;
		lan.ingame_last_step = 0;
		return;
	}
	if (ae_lan_bots_server_playerless_machines(server) > 0)
		return;
	played = ae_bots_engine_played(game->variant.game_engine_index);
	free_mask = ae_lan_bots_free_controllers(scan.people, 0);
	/* (the seats: the bots there are, the host's free controllers, the virtual machines' free seats, and the virtual
	machines the game may yet take, four seats each) */
	{
		short number, controller;
		int free_seats = 0, machines = 0;

		for (controller = 0; controller < AE_LAN_BOTS_CONTROLLERS; controller++)
			free_seats += ((free_mask >> controller) & 1u) && lan.seats[machine][controller] == NONE ? 1 : 0;
		for (number = 0; number < LAN_MACHINES; number++)
		{
			if (lan.machines[number] == NONE)
				continue;
			machines++;
			for (controller = 0; controller < AE_LAN_BOTS_CONTROLLERS; controller++)
				free_seats += lan.seats[lan.machines[number]][controller] == NONE ? 1 : 0;
		}
		capacity = ae_lan_bots_ingame_capacity((int)scan.bots, free_seats, lan_machines_available(server) - machines);
	}
	wanted = lan_wanted();
	target = ae_lan_bots_ingame_target((int)wanted, played && scan.has_person, (int)scan.others, game->maximum_players,
		capacity, game->variant.universal_variant.teams != 0, bots_teams() == _bots_teams_against);
	step = ae_lan_bots_ingame_step((int)scan.bots, target);
	if (!step || now - lan.ingame_settled_tick < ae_lan_bots_ingame_wait(lan.ingame_last_step, step))
		return;
	for (done = 0; step < 0 && done < -step; done++)
	{
		short bot_index = lan_leaving_bot(game, &scan);

		if (bot_index == NONE || !lan_remove_ingame(server, game, bot_index, "fewer wanted"))
			break;
		lan_scan_players(game, machine, &scan);
	}
	for (; step > 0 && done < step; done++)
	{
		if (!lan_add_ingame(server, game, machine, free_mask, &scan))
			break;
	}
	/* (the game's players as the bots left them: the next step of the same way follows soon) */
	if (done > 0)
	{
		lan.ingame_signature = scan.others * 1000 + scan.bots;
		lan.ingame_last_step = step;
	}
	lan.ingame_settled_tick = now;
}

boolean ae_lan_bots_machine_held(long machine_index)
{
	return lan_hosting() && lan_machine_number(machine_index) != NONE;
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

boolean ae_lan_bots_flip_team(short machine_index, short controller_index)
{
	struct network_game *game = network_game_get_game();
	short bot = lan_hosting() ? lan_bot_of(machine_index, controller_index) : NONE;
	short index;
	long team;

	if (!game || bot == NONE || !lan_bot_still_there(game, bot) || game->variant.game_engine_index == game_engine_none ||
		!game->variant.universal_variant.teams)
		return FALSE;
	/* (every bot holds the team it is on now, so that only this one moves: the others would be dealt again around it) */
	for (index = 0; index < LAN_BOTS; index++)
	{
		if (lan.bots[index].active && lan_bot_still_there(game, index))
			lan.bots[index].team = game->players[(long)lan.bots[index].slot].team_index == 1 ? 1 : 0;
	}
	team = lan.bots[bot].team == 1 ? 0 : 1;
	lan.bots[bot].team = (signed char)team;
	platform_log("lan bots: bot %d chosen for the %s team", bot + 1, team ? "blue" : "red");
	return TRUE;
}

void ae_lan_bots_clear_teams(void)
{
	short index;

	if (!lan.initialized)
		return;
	for (index = 0; index < LAN_BOTS; index++)
		lan.bots[index].team = (signed char)NONE;
}

/* the player datum at an absolute index, NONE for none */
static long lan_player_at(long absolute_index)
{
	struct datum_header *header;

	if (!player_data || !player_data->valid || absolute_index < 0 || absolute_index >= player_data->maximum_count)
		return NONE;
	header = (struct datum_header *)((byte *)xbox_pointer(player_data->data) + player_data->size * absolute_index);
	if (!header->identifier)
		return NONE;
	return ((long)(word)header->identifier << 16) | absolute_index;
}

/* the player at an absolute index that is a LAN bot (no local player here), its datum index; NONE for none */
static long lan_bot_player_index(long absolute_player_index, short *bot_index)
{
	long player_index = lan_player_at(absolute_player_index);
	struct player_datum *player;

	if (player_index == NONE)
		return NONE;
	player = (struct player_datum *)datum_get(player_data, player_index);
	if (player->local_player_index != NONE)
		return NONE;
	*bot_index = lan_bot_of(player->network_player_data.machine_index, player->network_player_data.controller_index);
	return *bot_index != NONE ? player_index : NONE;
}

/* (debug.network_bots) the brain's time of the tick that ended (all its bots'), counted */
static void lan_tick_ended(void)
{
	double milliseconds = lan.tick_seconds * 1000.0;
	short mark;

	if (lan.tick_seconds <= 0.0)
		return;
	lan.ticks++;
	lan.tick_seconds_total += lan.tick_seconds;
	if (lan.tick_seconds > lan.tick_seconds_max)
		lan.tick_seconds_max = lan.tick_seconds;
	for (mark = 0; mark < (short)NUMBEROF(lan_tick_marks); mark++)
		lan.ticks_over[mark] += milliseconds > lan_tick_marks[mark] ? 1 : 0;
	lan.tick_seconds = 0.0;
}

void ae_lan_bots_action(long absolute_player_index, struct player_action *action)
{
	long player_index;
	short bot_index = NONE;
	long now;
	double started;
	boolean network_bots;

	if (!lan_any() || !player_data || !action || game_connection() != _game_connection_network_server ||
		!lan_hosting() || !game_engine_running())
	{
		return;
	}
	player_index = lan_bot_player_index(absolute_player_index, &bot_index);
	if (player_index == NONE)
		return;
	network_bots = lan_network_bots();
	now = game_time_get();
	/* (debug.network_bots) an update built again in a tick (network_server_manager.c's bursts): the world has not moved, the
	bot's input is the one it had (as a client's held input) */
	if (network_bots && lan.bots[bot_index].driven_tick == now)
	{
		*action = lan.bots[bot_index].action;
		lan.brain_repeats++;
		return;
	}
	started = lan_now();
	/* (once a tick, before its first bot: a new game's time starts again) */
	if (!lan.tick_valid || now != lan.last_tick)
	{
		lan_tick_ended();
		bots_ae_lan_tick();
		lan.last_tick = now;
		lan.tick_valid = TRUE;
	}
	if (lan.bots[bot_index].driven_tick == now)
		lan.brain_repeats++;
	lan.bots[bot_index].driven_tick = now;
	bots_ae_lan_drive(bot_index, player_index, action);
	lan.bots[bot_index].action = *action;
	/* (debug.network_bots) the brain's time per bot's input (the tick's shared work counted in its first bot's) and per tick, a
	line a minute of game time, with the host's ticks a second of the clock */
	started = lan_now() - started;
	lan.brain_seconds += started;
	lan.tick_seconds += started;
	if (started > lan.brain_input_max)
		lan.brain_input_max = started;
	lan.brain_inputs++;
	if (lan.report_tick == NONE || now < lan.report_tick)
	{
		lan.report_tick = now;
		lan.report_time = lan_now();
	}
	if (network_bots && now - lan.report_tick >= TICKS_PER_SECOND * 60 && lan.ticks > 0)
	{
		long bots = 0;
		short index;

		for (index = 0; index < LAN_BOTS; index++)
			bots += lan.bots[index].active ? 1 : 0;
		platform_log("net bots: %ld bots, brain %.3f ms a tick (max %.3f ms, ticks over 5/16/33 ms: %ld/%ld/%ld of %ld), "
			"%.1f us a bot's input (max %.1f us) over %ld inputs, %ld repeats reused, game tick %ld "
			"(%.1f ticks a second)",
			bots,
			lan.tick_seconds_total * 1e3 / lan.ticks, lan.tick_seconds_max * 1e3, lan.ticks_over[0], lan.ticks_over[1],
			lan.ticks_over[2], lan.ticks, lan.brain_seconds * 1e6 / (lan.brain_inputs ? lan.brain_inputs : 1),
			lan.brain_input_max * 1e6, lan.brain_inputs, lan.brain_repeats, now,
			(double)(now - lan.report_tick) / (lan_now() - lan.report_time > 0.0 ? lan_now() - lan.report_time : 1.0));
		lan.report_tick = now;
		lan.report_time = lan_now();
		lan.brain_seconds = lan.brain_input_max = 0.0;
		lan.brain_inputs = lan.brain_repeats = 0;
		lan.tick_seconds_total = lan.tick_seconds_max = 0.0;
		lan.ticks = 0;
		csmemset(lan.ticks_over, 0, sizeof(lan.ticks_over));
	}
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

/* a bot that left the game in progress (BOTS lowered, or a person took its place) is still in it for the one team
left rule (game_engine.c's multiple_teams_alive and team_has_players): the host left alone by its bots plays on, as
in a game it started alone, to the time or score limit or until it ends the game; a person who leaves ends it as
before */
boolean ae_lan_bots_left_player(long player_index)
{
	short index;

	if (!lan.initialized || player_index == NONE || !lan_hosting())
		return FALSE;
	for (index = 0; index < LAN_BOTS; index++)
	{
		if (lan.left_players[index] == player_index)
			return TRUE;
	}
	return FALSE;
}
