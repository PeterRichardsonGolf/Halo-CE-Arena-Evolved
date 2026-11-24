/*
AE_REJOIN.C

REJOIN AFTER A DROP (ae_rejoin.h; the rules: ae_rejoin_rules.c).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "main/main.h"
#include "interface/ui_widget.h"
#include "interface/player_ui.h"
#include "saved games/player_profile.h"
#include "bungie_net/network/transport.h"
#include "networking/network_client_manager.h"
#include "networking/network_connection.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "networking/network_server_manager_internal.h"
#include "../src/ae_draw.h"
#include "../src/server_log.h"
#include "../src/server_mode.h"
#include "halo_port_limits.h"
#include "ae_glue_lobby.h"
#include "ae_hooks.h"
#include "ae_rejoin.h"
#include "ae_rejoin_rules.h"
#include "ae_screen_create_game.h"
#include "ae_server_lobby.h"
#include "ae_setup_flow.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "network_distributed.h"
#include "network_replay.h"

void platform_log(char const *format, ...);
int config_boolean(char const *name);
const char *config_string(const char *name);
void ae_chat_local_notice(char const *text);
/* (network_server_manager.c's, AE hook lines) */
unsigned long ae_network_game_server_machine_silent_ms(struct network_game_server_client_machine const *machine);
boolean ae_network_game_server_drop_machine(struct network_game_server *server,
	struct network_game_server_client_machine *machine);
short ae_join_diag_machine_slot(struct network_game_server_client_machine const *machine);
/* (ae_glue_errors.c) the lost-connection error as it is shown with no rejoin */
void ae_errors_show_lost(int dedicated);
/* (port/linux/src/ae_rejoin_socket.c) a stream socket's second descriptor, -1 for none */
int ae_rejoin_socket_keep(long socket);

enum
{
	REJOIN_IDLE,
	/* the next try waits for its time (and the menus) */
	REJOIN_WAITING,
	/* a try's join goes on */
	REJOIN_TRYING,
	/* the join is made: back in the game soon (a game in progress loads; a lobby waits for the next game) */
	REJOIN_BACK,
	/* (REJOIN_BACK's limit: a join into a lobby is back too, without a game to say so in) */
	REJOIN_BACK_MS = 60000,
};

static struct
{
	/* the target of the last join this machine started */
	int kind;
	char host[128];
	unsigned short port;
	char invite[256];
	/* armed: this machine is a guest in a multiplayer game by that join */
	int armed;
	char where[64];
	int dedicated;
	/* player 1's profile as it played (a player with no saved profile keeps its name and colours: the host knows a
	rejoin by its name) */
	long profile_index;
	struct player_profile profile;
	/* the rejoin */
	int state;
	unsigned long lost_at, next_try, back_at;
	int tries;
	char body[AE_REJOIN_TEXT];
	int dialog_wanted;
	/* the test drop */
	int test_parsed, test_set, test_silent, test_done, test_hold;
	/* (the setting's text the test drop was read from: a new text arms it again, as a TEST RUN list's next test) */
	char test_text[64];
	double test_seconds;
	unsigned long test_in_game_at;
	int test_kept_socket;
} rejoin;

/* (the dialog's context: this address) */
static int rejoin_dialog_context;

/* ---------- the client */

void ae_rejoin_note_join(int kind, char const *host, unsigned short port, char const *invite)
{
	/* (a try's own join names the same target) */
	if (rejoin.state == REJOIN_TRYING || rejoin.state == REJOIN_WAITING)
		return;
	rejoin.kind = kind;
	snprintf(rejoin.host, sizeof(rejoin.host), "%s", host ? host : "");
	rejoin.port = port;
	snprintf(rejoin.invite, sizeof(rejoin.invite), "%s", invite ? invite : "");
	rejoin.armed = 0;
}

int ae_rejoin_armed(void)
{
	return rejoin.armed;
}

int ae_rejoin_running(void)
{
	return rejoin.state == REJOIN_WAITING || rejoin.state == REJOIN_TRYING;
}

/* this machine is a guest in a multiplayer game of a remote host (not split screen, not a replay) */
static int guest_in_game(void)
{
	struct network_game_client *client = global_network_game_client_get();

	return client && !global_network_game_server_get() && game_in_progress() && !main_menu_is_active() &&
		game_engine_running() && !network_game_is_splitscreen_local() && !network_replay_playing();
}

/* the dialog: up once AE's screens are there to show it over; its words kept current */
static void dialog_picked(short choice, void *context);

static void dialog_show(void)
{
	static struct ae_flow_screen flow;
	struct ae_dialog_spec spec;

	if (!rejoin.dialog_wanted)
		return;
	if (ae_dialog_set_body(&rejoin_dialog_context, rejoin.body))
		return;
	if (!ae_menus_active() || !ae_draw_available() || ae_ui_depth() <= 0)
		return;
	memset(&flow, 0, sizeof(flow));
	ae_flow_full_density(&flow);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Reconnecting";
	spec.body = rejoin.body;
	spec.choices[0] = "Stop rejoining";
	spec.choice_count = 1;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = flow.density;
	spec.bounds = flow.frame.rect;
	spec.picked = dialog_picked;
	spec.context = &rejoin_dialog_context;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
		platform_log("rejoin: the dialog is up");
}

static void dialog_close(void)
{
	rejoin.dialog_wanted = 0;
	ae_dialog_close_context(&rejoin_dialog_context);
}

static void body_update(void)
{
	ae_rejoin_body(rejoin.body, (int)sizeof(rejoin.body), rejoin.dedicated, rejoin.where,
		rejoin.tries > 0 ? rejoin.tries : 1);
	dialog_show();
}

/* the rejoin over without getting back: the error as it would have been */
static void finish_failed(char const *why)
{
	if (rejoin.state == REJOIN_TRYING)
		ae_lobby_leave();
	platform_log("rejoin: %s after %d tr%s, %lu s: the lost-connection error shows", why, rejoin.tries,
		rejoin.tries == 1 ? "y" : "ies", (system_milliseconds() - rejoin.lost_at) / 1000);
	rejoin.state = REJOIN_IDLE;
	rejoin.armed = 0;
	dialog_close();
	ae_errors_show_lost(rejoin.dedicated);
}

static void dialog_picked(short choice, void *context)
{
	(void)choice;
	(void)context;
	rejoin.dialog_wanted = 0;
	if (ae_rejoin_running())
		finish_failed("stopped by the player");
}

int ae_rejoin_lost(int lost, int host_gave_reason)
{
	if (!ae_rejoin_wanted(config_boolean("network.rejoin_after_drop"), rejoin.armed, lost, host_gave_reason))
	{
		if (lost && rejoin.armed)
			platform_log("rejoin: not rejoining (%s)", host_gave_reason ? "the host gave a reason" :
				"network.rejoin_after_drop is off");
		rejoin.armed = 0;
		return 0;
	}
	rejoin.armed = 0;
	rejoin.state = REJOIN_WAITING;
	rejoin.lost_at = system_milliseconds();
	rejoin.tries = 0;
	rejoin.next_try = ae_rejoin_next_try(rejoin.lost_at, 0, 0);
	rejoin.dialog_wanted = 1;
	platform_log("rejoin: the connection to '%s' was lost in the game (no reason from the %s): rejoining %s:%u",
		rejoin.where, rejoin.dedicated ? "server" : "host",
		rejoin.kind == AE_REJOIN_TARGET_INVITE ? "its invite" : rejoin.host, (unsigned)rejoin.port);
	body_update();
	return 1;
}

/* a try: the same join again */
static void try_start(unsigned long now)
{
	struct ae_result result;

	rejoin.tries++;
	body_update();
	if (rejoin.kind == AE_REJOIN_TARGET_INVITE)
		result = ae_lobby_join_address(rejoin.invite);
	else
		result = ae_lobby_join_host(rejoin.host, rejoin.port);
	platform_log("rejoin: try %d of %d to %s:%u: %s", rejoin.tries, AE_REJOIN_TRIES,
		rejoin.kind == AE_REJOIN_TARGET_INVITE ? "the invite's host" : rejoin.host, (unsigned)rejoin.port,
		result.ok ? "joining" : result.reason);
	if (result.ok)
		rejoin.state = REJOIN_TRYING;
	else
		rejoin.next_try = ae_rejoin_next_try(rejoin.lost_at, rejoin.tries, now);
}

/* the target from the client's connection (a join that named none) */
static void target_from_connection(void)
{
	struct network_game_client *client = global_network_game_client_get();
	struct network_connection *connection = client ? network_game_client_get_connection(client) : NULL;
	struct transport_address address;
	unsigned long ip;

	if (!connection)
		return;
	memset(&address, 0, sizeof(address));
	network_connection_get_address(connection, &address, NULL);
	ip = address.address.long_words[0];
	if (address.address_length != IPV4_ADDRESS_LENGTH || !ip || !address.port)
		return;
	rejoin.kind = AE_REJOIN_TARGET_CONNECTION;
	snprintf(rejoin.host, sizeof(rejoin.host), "%lu.%lu.%lu.%lu", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF,
		ip & 0xFF);
	rejoin.port = address.port;
}

/* armed in the game: where (the game's name, else the host's), and a dedicated server or a player's host */
static void arm(void)
{
	struct network_game const *game = network_game_get_game();

	if (rejoin.kind == AE_REJOIN_TARGET_NONE || (rejoin.kind == AE_REJOIN_TARGET_CONNECTION && !rejoin.host[0]))
		target_from_connection();
	if (rejoin.kind == AE_REJOIN_TARGET_NONE)
		return;
	rejoin.where[0] = 0;
	if (game && game->name[0])
	{
		size_t used = 0, index;

		/* (the game's name: 16-bit characters, ASCII kept) */
		for (index = 0; index < NUMBEROF(game->name) && game->name[index] && used + 1 < sizeof(rejoin.where); index++)
		{
			unsigned int c = (unsigned int)(unsigned short)game->name[index];

			if (c >= 0x20 && c < 0x7F)
				rejoin.where[used++] = (char)c;
		}
		rejoin.where[used] = 0;
	}
	if (!rejoin.where[0] && rejoin.kind != AE_REJOIN_TARGET_INVITE)
		snprintf(rejoin.where, sizeof(rejoin.where), "%s", rejoin.host);
	rejoin.dedicated = ae_server_lobby_was_dedicated();
	rejoin.profile_index = player_ui_get_active_player_profile_index(0);
	player_ui_get_active_player_profile(0, &rejoin.profile);
	rejoin.armed = 1;
	platform_log("rejoin: armed for '%s' (%s, %s)", rejoin.where, rejoin.dedicated ? "a dedicated server" : "a player's host",
		rejoin.kind == AE_REJOIN_TARGET_INVITE ? "by its invite" : rejoin.host);
}

void ae_rejoin_tick(unsigned long now)
{
	if (guest_in_game())
	{
		if (!rejoin.armed)
			arm();
		if (rejoin.state == REJOIN_BACK)
		{
			platform_log("rejoin: back in the game %lu s after the drop (try %d)", (now - rejoin.lost_at) / 1000,
				rejoin.tries);
			ae_chat_local_notice("Rejoined the game after the connection dropped.");
			rejoin.state = REJOIN_IDLE;
		}
		return;
	}
	switch (rejoin.state)
	{
	case REJOIN_WAITING:
		dialog_show();
		if (ae_rejoin_give_up(now, rejoin.lost_at, rejoin.tries, 0))
			finish_failed("gave up");
		else if (ae_rejoin_try_due(now, rejoin.lost_at, rejoin.next_try) && main_menu_is_active())
			try_start(now);
		break;
	case REJOIN_TRYING:
	{
		struct ae_result failure;
		int state = ae_lobby_join_state(&failure);

		dialog_show();
		if (state == 0)
		{
			if (ae_rejoin_give_up(now, rejoin.lost_at, rejoin.tries, 1))
				finish_failed("gave up (the last try took too long)");
			break;
		}
		if (state < 0)
		{
			platform_log("rejoin: try %d failed: %s", rejoin.tries, failure.reason[0] ? failure.reason : "no answer");
			if (global_network_game_client_get())
				ae_lobby_leave();
			rejoin.state = REJOIN_WAITING;
			rejoin.next_try = ae_rejoin_next_try(rejoin.lost_at, rejoin.tries, now);
			if (ae_rejoin_give_up(now, rejoin.lost_at, rejoin.tries, 0))
				finish_failed("gave up");
			break;
		}
		{
			static struct ae_lobby_roster roster;

			/* (joined: this PC's player added once its client is in the session, as JOIN BY ADDRESS does) */
			if (!ae_lobby_roster(&roster).ok || !roster.joined)
				break;
			/* (player 1 as it played, unless the player picked another profile meanwhile) */
			if (player_ui_get_active_player_profile_index(0) == rejoin.profile_index)
				player_ui_set_active_player_profile(0, rejoin.profile_index, &rejoin.profile);
			if (!ae_lobby_add_local_player(0).ok)
			{
				platform_log("rejoin: try %d joined, but the game took no player", rejoin.tries);
				ae_lobby_leave();
				rejoin.state = REJOIN_WAITING;
				rejoin.next_try = ae_rejoin_next_try(rejoin.lost_at, rejoin.tries, now);
				break;
			}
			platform_log("rejoin: try %d joined %s", rejoin.tries, rejoin.where);
			rejoin.state = REJOIN_BACK;
			rejoin.back_at = now;
			dialog_close();
			if (ae_menus_active() && ae_ui_depth() > 0)
				ae_screen_create_game_lobby_joined("REJOINED");
		}
		break;
	}
	case REJOIN_BACK:
		if (now - rejoin.back_at >= REJOIN_BACK_MS)
		{
			platform_log("rejoin: joined, no game started yet (a lobby): done");
			rejoin.state = REJOIN_IDLE;
		}
		break;
	default:
		break;
	}
}

/* ---------- the host */

/* a machine's hardware id as the host keeps it, "" for none */
static char const *hardware_id_of(long machine_index)
{
	char const *id = network_game_server_machine_hardware_id(machine_index);

	return id ? id : "";
}

/* a client machine's real address (0 when not known) */
static unsigned long real_address_of(struct network_game_server_client_machine *machine)
{
	struct network_connection *connection = network_game_server_get_client_connection(machine);
	struct transport_address address;

	if (!connection)
		return 0;
	memset(&address, 0, sizeof(address));
	network_connection_get_address(connection, &address, NULL);
	return address.address.long_words[0] ? distributed_real_address(address.address.long_words[0]) : 0;
}

/* a 16-bit name in ASCII for the log */
static void name_text(wchar_t const *name, int length, char *out, size_t size)
{
	size_t used = 0;
	int index;

	for (index = 0; index < length && name[index] && used + 1 < size; index++)
	{
		unsigned int c = (unsigned int)(unsigned short)name[index];

		out[used++] = c >= 0x20 && c < 0x7F ? (char)c : '?';
	}
	out[used] = 0;
}

int ae_rejoin_server_replace(struct network_game_server *server, struct network_game_server_client_machine *joining,
	wchar_t const *name, int length)
{
	struct network_game *game;
	short joining_index;
	char const *joining_id;
	unsigned long joining_address;
	long index;

	if (!server || !joining || !name || length <= 0)
		return 0;
	game = network_game_server_get_game(server);
	joining_index = ae_join_diag_machine_slot(joining);
	if (!game || !VALID_INDEX(joining_index, HALO_PORT_MAXIMUM_NETWORK_MACHINES))
		return 0;
	joining_id = hardware_id_of(joining_index);
	joining_address = real_address_of(joining);
	for (index = 0; index < HALO_PORT_MAXIMUM_NETWORK_MACHINES; index++)
	{
		struct network_game_server_client_machine *old = network_game_server_get_client_machine_at_index(server, index);
		short old_index = ae_join_diag_machine_slot(old);
		unsigned long silent, old_address;
		char const *old_id;
		int same_name;

		if (!old || old == joining || !VALID_INDEX(old_index, (short)NUMBEROF(game->machines)) || old_index == joining_index ||
			!network_game_server_client_machine_is_joined_to_game(server, old) ||
			network_game_server_client_machine_is_local(server, old))
			continue;
		same_name = ae_rejoin_same_name((unsigned short const *)name, (unsigned short const *)game->machines[old_index].name,
			length < (int)NUMBEROF(game->machines[old_index].name) ? length : (int)NUMBEROF(game->machines[old_index].name));
		if (!same_name)
			continue;
		silent = ae_network_game_server_machine_silent_ms(old);
		old_id = hardware_id_of(old_index);
		old_address = real_address_of(old);
		{
			char text[40];
			int replaces = ae_rejoin_replaces(same_name, joining_id, old_id, joining_address, old_address, silent);

			name_text(name, length, text, sizeof(text));
			platform_log("rejoin: machine %d '%s' joining (hardware id %s, address %08lx); machine %d of that name silent "
				"%lu ms (hardware id %s, address %08lx): %s", (int)joining_index, text, joining_id[0] ? joining_id : "none",
				joining_address, (int)old_index, silent, old_id[0] ? old_id : "none", old_address,
				replaces ? "replaced by its rejoin" : silent < AE_REJOIN_STALE_MS ? "kept (heard lately)" :
				"kept (another machine)");
			if (!replaces)
				continue;
			if (halo_server_mode())
				server_log(_server_log_info, "player", "%s: machine %d replaced by its rejoin as machine %d (silent %lu ms)",
					text, (int)old_index, (int)joining_index, silent);
			if (!ae_network_game_server_drop_machine(server, old))
				platform_log("rejoin: machine %d could not be dropped", (int)old_index);
			return 1;
		}
	}
	return 0;
}

/* ---------- the test drop */

void ae_rejoin_test_drop(struct network_connection *connection)
{
	unsigned long now;

	{
		char const *text = config_string("debug.network_test_drop");

		if (!text)
			text = "";
		if (!rejoin.test_parsed || strncmp(text, rejoin.test_text, sizeof(rejoin.test_text) - 1))
		{
			rejoin.test_parsed = 1;
			snprintf(rejoin.test_text, sizeof(rejoin.test_text), "%s", text);
			rejoin.test_set = ae_rejoin_test_drop_parse(text, &rejoin.test_seconds, &rejoin.test_silent);
			rejoin.test_done = !rejoin.test_set;
			rejoin.test_in_game_at = 0;
			if (text[0] && !rejoin.test_set)
				platform_log("rejoin test: debug.network_test_drop '%s' is not <seconds>:reset or <seconds>:silent", text);
		}
	}
	if (rejoin.test_done || !rejoin.test_set || !connection)
		return;
	now = system_milliseconds();
	if (!rejoin.test_in_game_at)
		rejoin.test_in_game_at = now ? now : 1;
	if ((double)(now - rejoin.test_in_game_at) < rejoin.test_seconds * 1000.0)
		return;
	rejoin.test_done = 1;
	platform_log("rejoin test: dropping the connection (%s) %.1f s into the game", rejoin.test_silent ? "silent" : "reset",
		rejoin.test_seconds);
	rejoin.test_hold = rejoin.test_silent;
	network_connection_disconnect(connection);
	rejoin.test_hold = 0;
}

void ae_rejoin_test_socket_closing(long socket)
{
	int kept;

	if (!rejoin.test_hold)
		return;
	/* (a second descriptor keeps a stream open: closing the first sends nothing; a datagram socket is let go) */
	kept = ae_rejoin_socket_keep(socket);
	if (kept < 0)
		return;
	rejoin.test_hold = 0;
	rejoin.test_kept_socket = kept;
	platform_log("rejoin test: the stream kept open (descriptor %d): the host hears nothing", kept);
}
