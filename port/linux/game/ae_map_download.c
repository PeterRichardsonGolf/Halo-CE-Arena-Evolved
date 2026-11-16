/*
AE_MAP_DOWNLOAD.C

Arena Evolved (0.9.3): GET MAPS (ae_map_download.h; its pure rules ae_map_download_rules.h).

THE SERVER. A machine that has not joined sends map sharing's request message with GET MAPS's hello in it: the server
answers welcome (it keeps the machine as a download peer) or refuse (the reason). A download peer's map sharing
messages are map sharing's, by its slot, as a joined machine's are; the server's list and map sharing's files go to it
(ae_map_share_all.c counts it a machine that runs this game; the server manager's AE hooks send to it, take its
address, keep it out of the joins' waiting count and the join timeout, and let it go for a joiner who finds no slot).
It never joins: no player, no seat, not in the game's lists or the browser's count.

THE GUEST. GET MAPS reads which maps AE TOURNAMENT's playlists name that this PC lacks, and the AE servers the server
browser lists (dedicated, AE's, with an address). It reaches each in turn (ae_lobby_download_host: as JOIN BY ADDRESS,
the tunnel too, but no join request), says hello, and once welcome waits for the server's list; GET ALL then fetches the
wanted maps the list has (ae_map_share_all.c: only those, ae_map_download_guest_wants). The connection is left, and the
next server asked, until nothing is missing or every server was asked.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "interface/interface.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_client_manager.h"
#include "networking/network_connection.h"
#include "network_distributed.h"
#include "map_share.h"
#include "ae_map_share.h"
#include "ae_map_share_rules.h"
#include "ae_map_download.h"
#include "ae_glue_lobby.h"
#include "ae_glue_social.h"
#include "ae_address_rules.h"
#include "../src/server_mode.h"
#ifdef HALO_GAME_BROWSER
#include "../src/browser.h"
#include "../src/p2p.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* network_game_globals.c's, network_server_message_handler.c's, network_server_manager.c's (AE hooks), the game's */
boolean network_distributed_client_send_reliably(void *message, word size);
boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size);
long ae_network_game_server_free_machine_slots(void);
unsigned long ae_network_game_server_unjoined_address(long slot);
boolean main_menu_is_active(void);
int config_boolean(const char *name);
long config_integer(const char *name);
char const *config_string(const char *name);
unsigned long system_milliseconds(void);

/* ---------- constants */

enum
{
	MACHINES = HALO_PORT_MAXIMUM_NETWORK_MACHINES,
	/* the envelopes' bytes before a frame: the offer message's and the request message's (ae_map_share_channel.c's) */
	OFFER_BYTES = 68,
	OFFER_RESULT = 64,
	OFFER_FILE_COUNT = 65,
	REQUEST_BYTES = 72,
	REQUEST_WANTED = 68,
	/* the guest: the browser's list waited for, a hello said again (the server's idle limit is longer), how long an
	outcome stays on the card */
	LISTING_WAIT_MS = 8000,
	HELLO_AGAIN_MS = 2000,
	OUTCOME_SHOWN_MS = 12000,
	/* a connection being made (the name looked up, connected): the lobby's own limits end it; this is the last word */
	CONNECT_WAIT_MS = 30000,
};

static char const envelope_level_name[] = "\x7F" "AE maps";
#define ENVELOPE_MARK 0xAE
#define ENVELOPE_FILE_COUNT 0xFF

enum
{
	_guest_idle,
	_guest_listing,
	_guest_connecting,
	_guest_answer,
	_guest_list,
	_guest_fetching,
};

/* ---------- structures */

struct peer
{
	boolean on;
	/* (a refusal is being sent: the machine takes this one frame) */
	boolean answering;
	unsigned long address;
	unsigned long since;
	unsigned long heard;
	/* (the address of the connection at this slot refused last: its hellos again are not answered) */
	unsigned long refused_address;
	unsigned long refused_at;
};

struct server
{
	char name[48];
	char host[48];
	unsigned short port;
};

struct guest
{
	int state;
	unsigned long since;
	unsigned long hello_at;
	boolean hello_sent;
	boolean connected;
	char wanted[AE_MAP_DOWNLOAD_WANTED][AE_MAP_DOWNLOAD_NAME];
	int wanted_count;
	int wanted_at_start;
	struct server servers[AE_MAP_DOWNLOAD_SERVERS];
	int server_count;
	int server_next;
	/* the server being asked (its place in servers), and what came of the servers asked */
	int current;
	int failure;
	boolean any_listed;
	boolean following;
	/* the outcome shown, since when */
	int outcome;
	unsigned long outcome_at;
	boolean outcome_shown;
	int maps_got;
	/* (debug.map_share_test "get-maps:<host>:<map>+<map>": the test's run, its one server and its maps) */
	boolean test;
	char test_maps[512];
};

/* ---------- globals */

static struct peer peers[MACHINES];
static struct guest guest;
/* the playlist maps left out at the last read for want of Custom Edition's files (Halo PC's or MCC's) */
static struct
{
	boolean ready;
	int count;
	char seen[8][AE_MAP_DOWNLOAD_NAME];
} custom_edition_skipped;

/* ---------- the frames */

static boolean is_host(
	void)
{
	return global_network_game_server_get() != NULL;
}

/* a GET MAPS frame in map sharing's request (the guest's) or offer (the server's) message */
static boolean frame_send(
	boolean to_host,
	long slot,
	struct ae_map_download_frame const *frame)
{
	struct
	{
		struct distributed_message_header header;
		byte envelope[REQUEST_BYTES + AE_MAP_DOWNLOAD_FRAME_BYTES];
	} message;
	long envelope_bytes = to_host ? REQUEST_BYTES : OFFER_BYTES;
	long frame_bytes;
	word size;

	csmemset(&message, 0, sizeof(message));
	frame_bytes = ae_map_download_frame_encode(frame, message.envelope + envelope_bytes, AE_MAP_DOWNLOAD_FRAME_BYTES);
	if (!frame_bytes)
		return FALSE;
	size = (word)(sizeof(message.header) + envelope_bytes + frame_bytes);
	build_message_header(&message.header.header, size, 2, 0);
	message.header.type = to_host ? _distributed_message_map_share_request : _distributed_message_map_share_offer;
	message.header.count = 1;
	message.header.game_time = game_in_progress() ? game_time_get() : 0;
	message.header.checkpoint_epoch = 0;
	csmemcpy(message.envelope, envelope_level_name, sizeof(envelope_level_name));
	if (to_host)
		message.envelope[REQUEST_WANTED] = ENVELOPE_MARK;
	else
	{
		message.envelope[OFFER_RESULT] = ENVELOPE_MARK;
		message.envelope[OFFER_FILE_COUNT] = ENVELOPE_FILE_COUNT;
	}
	return to_host ? network_distributed_client_send_reliably(&message, size) :
		network_distributed_server_send_to_machine_reliably(slot, &message, size);
}

/* a GET MAPS frame of a message: from a guest (the request message), else from a server (the offer message) */
static boolean frame_of(
	word const *message,
	word size,
	boolean from_guest,
	struct ae_map_download_frame *frame)
{
	struct distributed_message_header header;
	byte const *body = (byte const *)message + sizeof(header);
	long length = (long)size - (long)sizeof(header);
	long envelope_bytes = from_guest ? REQUEST_BYTES : OFFER_BYTES;

	if (!message || size < sizeof(header))
		return FALSE;
	csmemcpy(&header, message, sizeof(header));
	if (header.type != (from_guest ? _distributed_message_map_share_request : _distributed_message_map_share_offer) ||
		length <= envelope_bytes || csmemcmp(body, envelope_level_name, sizeof(envelope_level_name)) ||
		body[envelope_bytes] != AE_MAP_DOWNLOAD_FRAME_TYPE)
	{
		return FALSE;
	}
	if (from_guest ? body[REQUEST_WANTED] != ENVELOPE_MARK :
		body[OFFER_RESULT] != ENVELOPE_MARK || body[OFFER_FILE_COUNT] != ENVELOPE_FILE_COUNT)
	{
		return FALSE;
	}
	return ae_map_download_frame_decode(body + envelope_bytes, length - envelope_bytes, frame) != 0;
}

/* ---------- the server */

static long peer_count(
	void)
{
	long slot, count = 0;

	for (slot = 0; slot < MACHINES; slot++)
		count += peers[slot].on;
	return count;
}

static int peers_cap(
	void)
{
	/* (a dedicated server's alone: a player's own game takes joiners, not download peers) */
	if (!halo_server_mode() && !ae_map_share_test_mode("download-host"))
		return 0;
	return ae_map_download_peers_setting(config_integer("server.map_download_peers"));
}

static void server_answer(
	long slot,
	int answer,
	unsigned long address)
{
	struct ae_map_download_frame frame;
	struct peer *peer = &peers[slot];

	csmemset(&frame, 0, sizeof(frame));
	if (answer == AE_MAP_DOWNLOAD_KIND_WELCOME)
	{
		unsigned long now = system_milliseconds();

		frame.kind = AE_MAP_DOWNLOAD_KIND_WELCOME;
		csmemset(peer, 0, sizeof(*peer));
		peer->on = TRUE;
		peer->address = address;
		peer->since = peer->heard = now;
		frame_send(FALSE, slot, &frame);
		error(_error_silent, "map download: machine #%ld is a download peer (%ld of %d)", slot, peer_count(), peers_cap());
		return;
	}
	frame.kind = AE_MAP_DOWNLOAD_KIND_REFUSE;
	frame.reason = -answer;
	peer->refused_address = address;
	peer->refused_at = system_milliseconds();
	peer->answering = TRUE;
	frame_send(FALSE, slot, &frame);
	peer->answering = FALSE;
	error(_error_silent, "map download: machine #%ld refused (%s)", slot, frame.reason == AE_MAP_DOWNLOAD_REFUSED_OFF ?
		"GET MAPS is off: server.map_download_peers" : frame.reason == AE_MAP_DOWNLOAD_REFUSED_VERSION ?
		"another GET MAPS version" : "no room");
}

int ae_map_download_server_message(
	long slot,
	unsigned short const *message,
	unsigned short size)
{
	struct ae_map_download_frame frame;
	struct peer *peer;

	if (!is_host() || slot < 0 || slot >= MACHINES || !message)
		return FALSE;
	peer = &peers[slot];
	/* (a peer that is not at its slot any more is forgotten: another machine may be there) */
	if (peer->on && ae_network_game_server_unjoined_address(slot) != peer->address)
		csmemset(peer, 0, sizeof(*peer));
	if (frame_of((word const *)message, (word)size, TRUE, &frame))
	{
		unsigned long address;

		/* (debug.map_share_test "download-silent": a hello dropped as a server before 0.9.3 drops it, for the tests) */
		if (frame.kind != AE_MAP_DOWNLOAD_KIND_HELLO || ae_map_share_test_mode("download-silent"))
			return TRUE;
		address = ae_network_game_server_unjoined_address(slot);
		/* (another connection at the slot since a refusal, or the refusal old: its hello is answered) */
		if (peer->refused_address &&
			!ae_map_download_refusal_kept(peer->refused_address, address, system_milliseconds() - peer->refused_at))
		{
			peer->refused_address = 0;
		}
		/* (a refused connection's hello again: no answer, and the caller does not count it heard, so it times out) */
		if (ae_map_download_hello_action(peer->on, peer->refused_address != 0) == AE_MAP_DOWNLOAD_HELLO_IGNORE)
			return TRUE;
		/* (a hello again: the peer is still there, and is answered: a guest that has not joined drops a connection it
		hears nothing on for 5 s, network_game_client_idle_joining, as while this server first hashes its maps) */
		if (peer->on)
		{
			struct ae_map_download_frame welcome;

			peer->heard = system_milliseconds();
			csmemset(&welcome, 0, sizeof(welcome));
			welcome.kind = AE_MAP_DOWNLOAD_KIND_WELCOME;
			frame_send(FALSE, slot, &welcome);
			return TRUE;
		}
		if (!address)
			return TRUE;
		server_answer(slot, ae_map_download_room(peers_cap(), (int)peer_count(),
			(int)ae_network_game_server_free_machine_slots(), frame.version), address);
		return TRUE;
	}
	if (!peer->on || !map_share_handles_message((word const *)message, (word)size))
		return FALSE;
	peer->heard = system_milliseconds();
	map_share_handle_message(slot, (word const *)message, (word)size, TRUE);
	return TRUE;
}

int ae_map_download_peer(
	long slot)
{
	return slot >= 0 && slot < MACHINES && peers[slot].on && is_host();
}

int ae_map_download_peer_heard(
	long slot)
{
	return ae_map_download_heard(ae_map_download_peer(slot));
}

int ae_map_download_join_refused(
	long slot)
{
	if (ae_map_download_join_allowed(ae_map_download_peer(slot)))
		return FALSE;
	error(_error_silent, "map download: download peer #%ld asked to join: refused (a peer never joins)", slot);
	return TRUE;
}

int ae_map_download_peer_sendable(
	long slot)
{
	return slot >= 0 && slot < MACHINES && (peers[slot].on || peers[slot].answering) && is_host();
}

unsigned long ae_map_download_peer_address(
	long slot)
{
	return ae_map_download_peer(slot) ? peers[slot].address : 0;
}

int ae_map_download_peer_expired_now(
	long slot)
{
	unsigned long now = system_milliseconds();

	if (!ae_map_download_peer(slot))
		return FALSE;
	if (!ae_map_download_peer_expired(now - peers[slot].heard, now - peers[slot].since))
		return FALSE;
	error(_error_silent, "map download: download peer #%ld let go (%s)", slot,
		now - peers[slot].heard > AE_MAP_DOWNLOAD_IDLE_MS ? "silent" : "its visit is over");
	csmemset(&peers[slot], 0, sizeof(peers[slot]));
	return TRUE;
}

int ae_map_download_peer_let_go(
	long slot)
{
	struct ae_map_download_frame frame;

	if (!ae_map_download_peer(slot))
		return FALSE;
	csmemset(&frame, 0, sizeof(frame));
	frame.kind = AE_MAP_DOWNLOAD_KIND_BYE;
	frame.reason = AE_MAP_DOWNLOAD_REFUSED_FILLED;
	frame_send(FALSE, slot, &frame);
	error(_error_silent, "map download: download peer #%ld let go for a joiner (no machine slot free)", slot);
	csmemset(&peers[slot], 0, sizeof(peers[slot]));
	return TRUE;
}

static void server_update(
	void)
{
	long slot;

	for (slot = 0; slot < MACHINES; slot++)
	{
		if (peers[slot].on && ae_network_game_server_unjoined_address(slot) != peers[slot].address)
		{
			error(_error_silent, "map download: download peer #%ld left", slot);
			csmemset(&peers[slot], 0, sizeof(peers[slot]));
		}
		/* (review 0.9.3e: a refused connection that left its slot leaves no refusal behind: the same guest back later
		from the same address is answered) */
		if (!peers[slot].on && peers[slot].refused_address && !ae_map_download_refusal_kept(peers[slot].refused_address,
			ae_network_game_server_unjoined_address(slot), system_milliseconds() - peers[slot].refused_at))
		{
			peers[slot].refused_address = 0;
		}
	}
}

/* ---------- the guest: the maps wanted */

static int stock_name(
	void *context,
	char const *name)
{
	char plain[AE_MAP_DOWNLOAD_NAME];
	char *at;

	(void)context;
	/* (a Custom Edition map, <name>@ce, on a PC without Custom Edition's resource maps: never fetched, it could not
	be played; counted for the card's note) */
	if (ae_map_download_needs_custom_edition(name) && !custom_edition_skipped.ready)
	{
		if (!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])custom_edition_skipped.seen,
			MIN(custom_edition_skipped.count, (int)NUMBEROF(custom_edition_skipped.seen)), name))
		{
			if (custom_edition_skipped.count < (int)NUMBEROF(custom_edition_skipped.seen))
				snprintf(custom_edition_skipped.seen[custom_edition_skipped.count], AE_MAP_DOWNLOAD_NAME, "%s", name);
			custom_edition_skipped.count++;
		}
		return TRUE;
	}
	snprintf(plain, sizeof(plain), "%s", name);
	at = strchr(plain, '@');
	/* (a pack's map is never a stock one: <file>@<pack>) */
	if (at)
		return FALSE;
	return ae_map_share_stock_name(plain) != 0;
}

static int map_here(
	void *context,
	char const *name)
{
	return ae_social_map_here(context, name);
}

static int wanted_read(
	char (*out)[AE_MAP_DOWNLOAD_NAME],
	int maximum)
{
	static char const *names[AE_SOCIAL_PLAYLISTS * AE_SOCIAL_ENTRIES];
	static int fallbacks[AE_SOCIAL_PLAYLISTS * AE_SOCIAL_ENTRIES];
	int lists = ae_social_playlist_count(), list, count = 0;

	csmemset(&custom_edition_skipped, 0, sizeof(custom_edition_skipped));
	custom_edition_skipped.ready = ae_map_share_ce_resources_ready();
	/* (the test's maps, '+' between them, in place of the playlists') */
	if (guest.test)
	{
		static char copy[sizeof(guest.test_maps)];
		char *name = copy;

		snprintf(copy, sizeof(copy), "%s", guest.test_maps);
		while (name && *name && count < (int)NUMBEROF(names))
		{
			char *plus = strchr(name, '+');

			if (plus)
				*plus++ = 0;
			names[count] = name;
			fallbacks[count++] = 0;
			name = plus;
		}
		return ae_map_download_wanted(names, fallbacks, count, stock_name, map_here, NULL, out, maximum);
	}
	for (list = 0; list < lists; list++)
	{
		struct ae_social_playlist const *playlist = ae_social_playlist_at(list);
		int entry;

		for (entry = 0; playlist && entry < playlist->count && count < (int)NUMBEROF(names); entry++)
		{
			names[count] = playlist->entry[entry].map;
			fallbacks[count] = playlist->entry[entry].fallback;
			count++;
		}
	}
	return ae_map_download_wanted(names, fallbacks, count, stock_name, map_here, NULL, out, maximum);
}

int ae_map_download_missing(
	void)
{
	static char wanted[AE_MAP_DOWNLOAD_WANTED][AE_MAP_DOWNLOAD_NAME];
	static unsigned long read_at;
	static int count;
	unsigned long now = system_milliseconds();

	/* (read again every second at most: a map that came counts at once while the card is up) */
	if (!read_at || now - read_at >= 1000)
	{
		read_at = now ? now : 1;
		count = wanted_read(wanted, AE_MAP_DOWNLOAD_WANTED);
	}
	return count;
}

/* ---------- the guest: the servers */

static int servers_read(
	void)
{
	int count = 0;
#ifdef HALO_GAME_BROWSER
	static struct p2p_listing listings[96];
	int read = p2p_lobby_games(listings, (int)NUMBEROF(listings)), index;

	for (index = 0; index < read && count < AE_MAP_DOWNLOAD_SERVERS; index++)
	{
		struct p2p_listing const *listing = &listings[index];
		struct ae_address address;
		int other;

		/* (an AE dedicated server the AE master lists by its address: no player's own game) */
		if (!listing->dedicated || !listing->ae_address[0] || !(listing->ae_build || listing->ae_join_now[0]) ||
			ae_address_parse(listing->ae_address, ae_lobby_host_port(), &address) != AE_ADDRESS_OK)
		{
			continue;
		}
		for (other = 0; other < count; other++)
		{
			if (!strcmp(guest.servers[other].host, address.host) && guest.servers[other].port == address.port)
				break;
		}
		if (other < count)
			continue;
		snprintf(guest.servers[count].name, sizeof(guest.servers[count].name), "%s", listing->name);
		snprintf(guest.servers[count].host, sizeof(guest.servers[count].host), "%s", address.host);
		guest.servers[count].port = address.port;
		count++;
	}
#endif
	return count;
}

static void follow_listings(
	boolean on)
{
#ifdef HALO_GAME_BROWSER
	if (guest.following != on)
		browser_follow_listings(on ? 1 : 0);
#endif
	guest.following = on;
}

/* ---------- the guest: the run */

static void outcome_set(
	int outcome)
{
	guest.outcome = outcome;
	guest.outcome_at = system_milliseconds();
	guest.outcome_shown = outcome != AE_MAP_DOWNLOAD_OUTCOME_NONE;
}

static void run_end(
	int outcome)
{
	char text[160];

	if (guest.state >= _guest_connecting)
		ae_map_share_all_stop();
	if (ae_lobby_downloading() || (guest.state >= _guest_connecting && global_network_game_client_get()))
		ae_lobby_leave();
	follow_listings(FALSE);
	guest.state = _guest_idle;
	guest.maps_got = guest.wanted_at_start - wanted_read(guest.wanted, AE_MAP_DOWNLOAD_WANTED);
	if (guest.maps_got < 0)
		guest.maps_got = 0;
	outcome_set(outcome);
	ae_map_download_outcome_text(outcome, guest.maps_got, guest.current >= 0 && guest.current < guest.server_count ?
		guest.servers[guest.current].name : "", text, sizeof(text));
	error(_error_silent, "map download: done: %s", text);
}

/* the next server, or the end */
static void server_next(
	void)
{
	if (ae_lobby_downloading() || global_network_game_client_get())
		ae_lobby_leave();
	guest.wanted_count = wanted_read(guest.wanted, AE_MAP_DOWNLOAD_WANTED);
	if (!guest.wanted_count)
	{
		run_end(AE_MAP_DOWNLOAD_OUTCOME_DONE);
		return;
	}
	if (guest.server_next >= guest.server_count)
	{
		int got = guest.wanted_at_start - guest.wanted_count;

		run_end(got > 0 ? AE_MAP_DOWNLOAD_OUTCOME_SOME : guest.failure != AE_MAP_DOWNLOAD_OUTCOME_NONE &&
			!guest.any_listed ? guest.failure : AE_MAP_DOWNLOAD_OUTCOME_NONE_HAVE);
		return;
	}
	guest.current = guest.server_next++;
	guest.hello_sent = FALSE;
	guest.connected = FALSE;
	guest.since = system_milliseconds();
	if (!ae_lobby_download_host(guest.servers[guest.current].host, guest.servers[guest.current].port).ok)
	{
		guest.failure = AE_MAP_DOWNLOAD_OUTCOME_OFFLINE;
		server_next();
		return;
	}
	guest.state = _guest_connecting;
	error(_error_silent, "map download: asking %s (%s:%u), %d maps still wanted", guest.servers[guest.current].name,
		guest.servers[guest.current].host, (unsigned)guest.servers[guest.current].port, guest.wanted_count);
}

/* the server said no, or did not answer: the next one */
static void server_failed(
	int outcome,
	char const *why)
{
	error(_error_silent, "map download: %s: %s", guest.servers[guest.current].name, why);
	guest.failure = outcome;
	server_next();
}

int ae_map_download_start(
	void)
{
	if (guest.state != _guest_idle)
		return TRUE;
	csmemset(&guest, 0, sizeof(guest));
	guest.current = -1;
	if (!config_boolean("network.online"))
	{
		outcome_set(AE_MAP_DOWNLOAD_OUTCOME_ONLINE_OFF);
		return FALSE;
	}
	if (!config_boolean("network.map_download"))
	{
		outcome_set(AE_MAP_DOWNLOAD_OUTCOME_DOWNLOADS_OFF);
		return FALSE;
	}
	if (!main_menu_is_active() || global_network_game_client_get() || global_network_game_server_get())
	{
		outcome_set(AE_MAP_DOWNLOAD_OUTCOME_BUSY);
		return FALSE;
	}
	guest.wanted_count = guest.wanted_at_start = wanted_read(guest.wanted, AE_MAP_DOWNLOAD_WANTED);
	if (!guest.wanted_count)
	{
		outcome_set(AE_MAP_DOWNLOAD_OUTCOME_NOTHING_NEEDED);
		return FALSE;
	}
	follow_listings(TRUE);
	guest.state = _guest_listing;
	guest.since = system_milliseconds();
	error(_error_silent, "map download: GET MAPS: %d playlist maps are missing; reading the server list",
		guest.wanted_count);
	return TRUE;
}

void ae_map_download_cancel(
	void)
{
	if (guest.state == _guest_idle)
		return;
	error(_error_silent, "map download: cancelled");
	run_end(AE_MAP_DOWNLOAD_OUTCOME_CANCELLED);
}

int ae_map_download_guest_active(
	void)
{
	return guest.state >= _guest_connecting && !is_host();
}

int ae_map_download_guest_holds_join(
	void)
{
	return ae_map_download_guest_active() && ae_lobby_downloading();
}

int ae_map_download_guest_wants(
	char const *name)
{
	return ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])guest.wanted, guest.wanted_count, name);
}

int ae_map_download_guest_message(
	long machine_index,
	unsigned short const *message,
	unsigned short size,
	int from_stream)
{
	struct ae_map_download_frame frame;

	if (!frame_of((word const *)message, (word)size, FALSE, &frame))
		return FALSE;
	/* (the server's alone, over its connection, to a guest of GET MAPS) */
	if (machine_index != NONE || !from_stream || is_host() || !ae_map_download_guest_active() ||
		guest.current < 0)
	{
		return TRUE;
	}
	switch (frame.kind)
	{
	case AE_MAP_DOWNLOAD_KIND_WELCOME:
		if (guest.state == _guest_answer)
		{
			guest.state = _guest_list;
			guest.since = system_milliseconds();
			error(_error_silent, "map download: %s takes this machine as a download peer",
				guest.servers[guest.current].name);
		}
		break;
	case AE_MAP_DOWNLOAD_KIND_REFUSE:
	case AE_MAP_DOWNLOAD_KIND_BYE:
		/* (said in the update: not from inside the message handling, which the leave would pull from under it) */
		guest.failure = ae_map_download_outcome_of_refusal(frame.reason);
		guest.state = _guest_answer;
		guest.hello_at = 0;
		guest.since = 0;
		break;
	}
	return TRUE;
}

static void guest_update(
	void)
{
	unsigned long now = system_milliseconds();
	struct ae_map_share_all_state state;
	struct ae_result failure;

	if (guest.outcome_shown && now - guest.outcome_at >= OUTCOME_SHOWN_MS)
		guest.outcome_shown = FALSE;
	if (guest.state == _guest_idle)
		return;
	/* (the menus left for a game, or a game hosted or joined another way: GET MAPS ends) */
	if (!main_menu_is_active() || is_host())
	{
		run_end(AE_MAP_DOWNLOAD_OUTCOME_CANCELLED);
		return;
	}
	switch (guest.state)
	{
	case _guest_listing:
		guest.server_count = servers_read();
		/* (the AE master's list is read in a few seconds; its games come in as they are heard) */
		if (now - guest.since < LISTING_WAIT_MS &&
#ifdef HALO_GAME_BROWSER
			browser_list_status() != BROWSER_LIST_FAILED &&
#endif
			(guest.server_count == 0 || now - guest.since < 2000))
		{
			return;
		}
		if (!guest.server_count)
		{
			run_end(AE_MAP_DOWNLOAD_OUTCOME_NO_SERVERS);
			return;
		}
		error(_error_silent, "map download: %d AE servers listed", guest.server_count);
		guest.failure = AE_MAP_DOWNLOAD_OUTCOME_NONE;
		server_next();
		return;
	case _guest_connecting:
		if (ae_lobby_join_state(&failure) == -1 || !ae_lobby_downloading())
		{
			server_failed(AE_MAP_DOWNLOAD_OUTCOME_OFFLINE, "no connection");
			return;
		}
		if (ae_lobby_download_connected())
		{
			struct ae_map_download_frame frame;

			csmemset(&frame, 0, sizeof(frame));
			frame.kind = AE_MAP_DOWNLOAD_KIND_HELLO;
			frame.version = AE_MAP_DOWNLOAD_VERSION;
			guest.connected = TRUE;
			if (frame_send(TRUE, NONE, &frame))
			{
				guest.hello_sent = TRUE;
				guest.hello_at = now;
				guest.state = _guest_answer;
				guest.since = now;
			}
			return;
		}
		if (now - guest.since >= CONNECT_WAIT_MS)
			server_failed(AE_MAP_DOWNLOAD_OUTCOME_OFFLINE, "no connection in time");
		return;
	case _guest_answer:
		/* (a refusal or a bye came: guest_message left since at 0) */
		if (!guest.since)
		{
			server_failed(guest.failure, "refused");
			return;
		}
		/* (the hello said and no answer before the connection ended: a server before 0.9.3 drops the hello, and the
		engine ends a join that hears nothing for 5 s) */
		if (!ae_lobby_download_connected())
		{
			server_failed(guest.hello_sent ? AE_MAP_DOWNLOAD_OUTCOME_OLD : AE_MAP_DOWNLOAD_OUTCOME_OFFLINE,
				"the connection ended before an answer");
			return;
		}
		/* (no answer: a server before 0.9.3 drops the hello) */
		if (now - guest.since >= AE_MAP_DOWNLOAD_ANSWER_MS)
			server_failed(AE_MAP_DOWNLOAD_OUTCOME_OLD, "no answer to the hello");
		return;
	default:
		break;
	}
	/* (welcome: the list, then GET ALL's queue of the wanted maps it has) */
	if (!ae_lobby_download_connected())
	{
		server_failed(guest.failure != AE_MAP_DOWNLOAD_OUTCOME_NONE ? guest.failure : AE_MAP_DOWNLOAD_OUTCOME_OFFLINE,
			"the connection ended");
		return;
	}
	if (now - guest.hello_at >= HELLO_AGAIN_MS)
	{
		struct ae_map_download_frame frame;

		csmemset(&frame, 0, sizeof(frame));
		frame.kind = AE_MAP_DOWNLOAD_KIND_HELLO;
		frame.version = AE_MAP_DOWNLOAD_VERSION;
		if (frame_send(TRUE, NONE, &frame))
			guest.hello_at = now;
	}
	ae_map_share_all_state(&state);
	if (guest.state == _guest_list)
	{
		/* (its hello said it does not share with this machine: no list comes) */
		if (state.host_is_ae && !state.host_shares)
		{
			server_failed(AE_MAP_DOWNLOAD_OUTCOME_OFF, "it does not share its maps");
			return;
		}
		if (!state.catalogue_complete || !state.begun)
		{
			if (now - guest.since >= AE_MAP_DOWNLOAD_LIST_MS)
				server_failed(AE_MAP_DOWNLOAD_OUTCOME_NONE_HAVE, "its list did not come in time");
			return;
		}
		error(_error_silent, "map download: %s lists %ld maps; %ld of them are wanted here",
			guest.servers[guest.current].name, state.entries, state.queue_count);
		if (!state.active || !state.queue_count)
		{
			server_next();
			return;
		}
		guest.any_listed = TRUE;
		guest.state = _guest_fetching;
		guest.since = now;
		return;
	}
	if (!state.active)
		server_next();
}

/* the tests: debug.map_share_test "get-maps:<host>:<map>+<map>" (an automated network test alone, debug.network_test
set): GET MAPS a few seconds into the main menu, from that host alone, for those maps (no server list, no playlists,
internet play off as the tests have it) */
static void test_update(
	void)
{
	static boolean started;
	static unsigned long seen_at;
	char const *setting = config_string("debug.map_share_test");
	char const *network_test = config_string("debug.network_test");
	char const *colon;

	if (started || !setting || strncmp(setting, "get-maps:", 9) || !network_test || !network_test[0] ||
		guest.state != _guest_idle || !main_menu_is_active() || is_host() || global_network_game_client_get())
	{
		return;
	}
	if (!seen_at)
	{
		seen_at = system_milliseconds() | 1;
		return;
	}
	if (system_milliseconds() - seen_at < 4000)
		return;
	started = TRUE;
	setting += 9;
	colon = strchr(setting, ':');
	if (!colon || colon == setting || colon - setting >= (long)sizeof(guest.servers[0].host))
		return;
	csmemset(&guest, 0, sizeof(guest));
	guest.current = -1;
	guest.test = TRUE;
	snprintf(guest.test_maps, sizeof(guest.test_maps), "%s", colon + 1);
	snprintf(guest.servers[0].name, sizeof(guest.servers[0].name), "Test host");
	snprintf(guest.servers[0].host, sizeof(guest.servers[0].host), "%.*s", (int)(colon - setting), setting);
	guest.servers[0].port = ae_lobby_host_port();
	guest.server_count = 1;
	guest.wanted_count = guest.wanted_at_start = wanted_read(guest.wanted, AE_MAP_DOWNLOAD_WANTED);
	error(_error_silent, "map download: the test's GET MAPS: %d maps wanted from %s", guest.wanted_count,
		guest.servers[0].host);
	if (!guest.wanted_count)
	{
		outcome_set(AE_MAP_DOWNLOAD_OUTCOME_NOTHING_NEEDED);
		return;
	}
	guest.failure = AE_MAP_DOWNLOAD_OUTCOME_NONE;
	server_next();
}

void ae_map_download_update(
	void)
{
	if (is_host())
		server_update();
	test_update();
	guest_update();
}

int ae_map_download_status(
	struct ae_map_download_status *status)
{
	struct ae_map_share_all_state state;

	if (!status)
		return FALSE;
	csmemset(status, 0, sizeof(*status));
	status->outcome = guest.outcome_shown ? guest.outcome : AE_MAP_DOWNLOAD_OUTCOME_NONE;
	status->maps_got = guest.maps_got;
	if (status->outcome != AE_MAP_DOWNLOAD_OUTCOME_NONE)
		ae_map_download_outcome_text(guest.outcome, guest.maps_got, guest.current >= 0 &&
			guest.current < guest.server_count ? guest.servers[guest.current].name : "", status->outcome_text,
			(int)sizeof(status->outcome_text));
	/* (the playlist maps a PC without Custom Edition's files never gets, said after the outcome: "Every playlist map
	is already here" alone would not be so) */
	if (custom_edition_skipped.count && (status->outcome == AE_MAP_DOWNLOAD_OUTCOME_DONE ||
		status->outcome == AE_MAP_DOWNLOAD_OUTCOME_SOME || status->outcome == AE_MAP_DOWNLOAD_OUTCOME_NONE_HAVE ||
		status->outcome == AE_MAP_DOWNLOAD_OUTCOME_NOTHING_NEEDED))
	{
		char note[96];
		size_t length;

		ae_map_download_custom_edition_note(custom_edition_skipped.count, custom_edition_skipped.seen[0], note,
			(int)sizeof(note));
		if (status->outcome == AE_MAP_DOWNLOAD_OUTCOME_NOTHING_NEEDED)
			status->outcome_text[0] = 0;
		else if (status->outcome == AE_MAP_DOWNLOAD_OUTCOME_DONE)
			snprintf(status->outcome_text, sizeof(status->outcome_text), status->maps_got == 1 ? "Got 1 map." :
				"Got %d maps.", status->maps_got);
		length = strlen(status->outcome_text);
		snprintf(status->outcome_text + length, sizeof(status->outcome_text) - length, "%s%s", length ? " " : "",
			note);
	}
	if (guest.state == _guest_idle)
		return status->outcome != AE_MAP_DOWNLOAD_OUTCOME_NONE;
	status->running = TRUE;
	status->server_count = guest.server_count;
	if (guest.current >= 0 && guest.current < guest.server_count)
	{
		snprintf(status->server, sizeof(status->server), "%s", guest.servers[guest.current].name);
		status->server_index = guest.current + 1;
	}
	if (guest.state == _guest_fetching)
	{
		ae_map_share_all_state(&state);
		status->map_count = (int)state.queue_count;
		status->map_index = (int)MIN(state.queue_next + 1, state.queue_count);
		status->total = state.plan_bytes;
		status->bytes = state.queued_done_bytes + state.active_done;
		if (status->bytes > status->total)
			status->bytes = status->total;
	}
	ae_map_download_progress_text(status->server, status->map_index, status->map_count, status->bytes, status->total,
		status->line, (int)sizeof(status->line));
	return TRUE;
}

void ae_map_download_outcome_seen(
	void)
{
	guest.outcome_shown = FALSE;
}
