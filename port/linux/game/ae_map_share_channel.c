/*
AE_MAP_SHARE_CHANNEL.C

Arena Evolved (0.7.7): map sharing's connection to the host over the game's own connection (ae_map_share_channel.h,
its rules ae_map_share_channel_rules.h; port/linux/NETCODE.md, "Over the game's connection").

A channel is one end of a stream: the guest's one, to its host, and the host's one for each machine that opened one.
What the other end sent waits in the channel's inbox until map sharing reads it; what map sharing writes goes out in
frames, only while the game's stream to that machine has nothing waiting to be written (the game's messages go first)
and the window has room (the other end said it took the bytes before). Nothing here blocks: a stream that cannot take
more now says so as a socket would (_transport_result_operation_would_block).
*/

/* ---------- headers */

#include "cseries.h"
#include "cseries/errors.h"
#include "bungie_net/network/transport.h"
#include "game/game.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_client_manager.h"
#include "networking/network_connection.h"
#include "network_distributed.h"
#include "map_share.h"
#include "ae_map_share_channel.h"
#include "ae_map_share_channel_rules.h"

#include <stdlib.h>
#include <string.h>

/* (the C library's allocator, as map_share.c's) */
#undef malloc
#undef free

/* network_game_globals.c's, network_server_message_handler.c's (AE hooks), and the game's */
boolean network_distributed_client_send_reliably(void *message, word size);
boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size);
long ae_network_client_reliable_waiting(void);
long ae_network_server_machine_reliable_waiting(long machine_index);
unsigned long network_game_server_machine_address(long machine_index);
unsigned long system_milliseconds(void);

/* ---------- constants */

enum
{
	MACHINES = HALO_PORT_MAXIMUM_NETWORK_MACHINES,
	/* the guest's channel's place, after the host's */
	GUEST_SLOT = MACHINES,
	SLOTS = MACHINES + 1,
	/* an id: its slot (+1) in the low bits, its generation above */
	ID_SLOT_BITS = 12,
	/* the envelopes' bytes before a frame: the offer message's (map_share.c's distributed_map_share_offer) and the
	request message's (distributed_map_share_request) */
	OFFER_BYTES = 68,
	OFFER_RESULT = 64,
	OFFER_FILE_COUNT = 65,
	REQUEST_BYTES = 72,
	REQUEST_WANTED = 68,
};

typedef char machines_fit_assert[SLOTS < (1 << ID_SLOT_BITS) ? 1 : -1];
typedef char frame_fits_assert[sizeof(struct distributed_message_header) + REQUEST_BYTES +
	AE_MAP_SHARE_CHANNEL_HEADER_BYTES + AE_MAP_SHARE_CHANNEL_DATA_BYTES < HALO_PORT_MAXIMUM_NETWORK_MESSAGE_SIZE ? 1 : -1];

/* the envelopes' level name: ae_map_share_all.c's frames' (no map's: it starts with a character no name has) */
static char const envelope_level_name[] = "\x7F" "AE maps";
#define ENVELOPE_MARK 0xAE
#define ENVELOPE_FILE_COUNT 0xFF

/* ---------- structures */

struct channel
{
	/* 0: not in use */
	long id;
	unsigned char session;
	/* the machine (the host's channels), and the address it had (the guest's: the host's) */
	long machine_index;
	unsigned long address;
	/* the guest's: OPEN went out with its first frame */
	boolean open_said;
	/* the other end closed it, its machine is gone, or it sent what it may not: what came is read, then it is gone */
	boolean ended;
	struct ae_map_share_channel_window window;
	struct ae_map_share_channel_inbox inbox;
	unsigned char *buffer;
	unsigned long bytes_in;
	unsigned long bytes_out;
};

struct channel_globals
{
	struct channel slots[SLOTS];
	unsigned long generation;
	unsigned char guest_session;
	boolean setting_read;
	boolean setting_off;
};

/* ---------- globals */

static struct channel_globals channel_globals;

/* ---------- the channels */

/* HALO_MAP_SHARE_CHANNEL=off: neither end takes the stream over the game's connection (a host says it does not, a
guest reaches its host on TCP 5153), as a build before 0.7.7 */
static boolean channel_off(
	void)
{
	if (!channel_globals.setting_read)
	{
		char const *value = getenv("HALO_MAP_SHARE_CHANNEL");

		channel_globals.setting_read = TRUE;
		channel_globals.setting_off = value && (!strcmp(value, "off") || !strcmp(value, "0") || !strcmp(value, "false"));
		if (channel_globals.setting_off)
			error(_error_silent, "map sharing: the stream over the game's connection is off (HALO_MAP_SHARE_CHANNEL)");
	}
	return channel_globals.setting_off;
}

static boolean is_host(
	void)
{
	return global_network_game_server_get() != NULL;
}

/* the host's address as the guest's connection has it (host byte order; 0 for none) */
static unsigned long guest_host_address(
	void)
{
	struct network_game_client *client = global_network_game_client_get();
	struct network_connection *connection = client ? network_game_client_get_connection(client) : NULL;
	struct transport_address reliable;
	struct transport_address unreliable;

	if (!connection || is_host())
		return 0;
	csmemset(&reliable, 0, sizeof(reliable));
	network_connection_get_address(connection, &reliable, &unreliable);
	return reliable.address_length == IPV4_ADDRESS_LENGTH ? reliable.address.long_words[0] : 0;
}

static struct channel *channel_of(
	long id)
{
	long slot = (id & ((1L << ID_SLOT_BITS) - 1)) - 1;

	if (!id || slot < 0 || slot >= SLOTS || channel_globals.slots[slot].id != id)
		return NULL;
	return &channel_globals.slots[slot];
}

static void channel_free(
	struct channel *channel)
{
	free(channel->buffer);
	csmemset(channel, 0, sizeof(*channel));
}

static struct channel *channel_new(
	long slot,
	unsigned char session,
	long machine_index,
	unsigned long address)
{
	struct channel *channel = &channel_globals.slots[slot];
	unsigned char *buffer = malloc(AE_MAP_SHARE_CHANNEL_WINDOW);

	if (channel->id)
		channel_free(channel);
	if (!buffer)
		return NULL;
	channel_globals.generation++;
	channel->id = (long)((slot + 1) | ((channel_globals.generation & 0x7FFFF) << ID_SLOT_BITS));
	channel->session = session;
	channel->machine_index = machine_index;
	channel->address = address;
	channel->buffer = buffer;
	ae_map_share_channel_inbox_init(&channel->inbox, buffer, AE_MAP_SHARE_CHANNEL_WINDOW);
	return channel;
}

/* what waits to be written of the game's stream to the channel's other end (NONE: no stream) */
static long channel_waiting(
	struct channel const *channel)
{
	return channel->machine_index == NONE ? ae_network_client_reliable_waiting() :
		ae_network_server_machine_reliable_waiting(channel->machine_index);
}

/* a frame sent: the guest's in the request message, the host's in the offer message */
static boolean channel_send(
	struct channel *channel,
	unsigned char flags,
	byte const *data,
	long data_bytes)
{
	struct
	{
		struct distributed_message_header header;
		byte envelope[REQUEST_BYTES];
		byte body[AE_MAP_SHARE_CHANNEL_HEADER_BYTES + AE_MAP_SHARE_CHANNEL_DATA_BYTES];
	} message;
	struct ae_map_share_channel_frame frame;
	boolean guest = channel->machine_index == NONE;
	long envelope_bytes = guest ? REQUEST_BYTES : OFFER_BYTES;
	long body_bytes;
	word size;

	csmemset(&frame, 0, sizeof(frame));
	frame.session = channel->session;
	frame.flags = flags;
	if (guest && !channel->open_said)
		frame.flags |= AE_MAP_SHARE_CHANNEL_FLAG_OPEN;
	/* (what this end took, said with every frame once it is due: the other end's window waits for it) */
	if (ae_map_share_channel_window_ack_due(&channel->window) || (flags & AE_MAP_SHARE_CHANNEL_FLAG_ACK))
	{
		frame.flags |= AE_MAP_SHARE_CHANNEL_FLAG_ACK;
		frame.taken = channel->window.took;
	}
	frame.data = data;
	frame.data_bytes = data_bytes;
	body_bytes = ae_map_share_channel_frame_encode(&frame, message.body, sizeof(message.body));
	if (!body_bytes)
		return FALSE;
	/* (the envelope where the frame's body begins: the body follows it directly) */
	csmemmove(message.envelope + envelope_bytes, message.body, (size_t)body_bytes);
	csmemset(&message.header, 0, sizeof(message.header));
	csmemset(message.envelope, 0, (size_t)envelope_bytes);
	size = (word)(sizeof(message.header) + envelope_bytes + body_bytes);
	build_message_header(&message.header.header, size, 2, 0);
	message.header.type = guest ? _distributed_message_map_share_request : _distributed_message_map_share_offer;
	message.header.count = 1;
	message.header.game_time = game_in_progress() ? game_time_get() : 0;
	csmemcpy(message.envelope, envelope_level_name, sizeof(envelope_level_name));
	if (guest)
	{
		message.envelope[REQUEST_WANTED] = ENVELOPE_MARK;
	}
	else
	{
		message.envelope[OFFER_RESULT] = ENVELOPE_MARK;
		message.envelope[OFFER_FILE_COUNT] = ENVELOPE_FILE_COUNT;
	}
	if (!(guest ? network_distributed_client_send_reliably(&message, size) :
		network_distributed_server_send_to_machine_reliably(channel->machine_index, &message, size)))
	{
		return FALSE;
	}
	if (frame.flags & AE_MAP_SHARE_CHANNEL_FLAG_ACK)
		ae_map_share_channel_window_ack_said(&channel->window);
	if (guest)
		channel->open_said = TRUE;
	return TRUE;
}

static void channel_end(
	struct channel *channel,
	char const *why)
{
	if (channel->ended)
		return;
	channel->ended = TRUE;
	if (why)
	{
		if (channel->machine_index == NONE)
			error(_error_silent, "map sharing: the stream to the host over the game's connection ended: %s", why);
		else
			error(_error_silent, "map sharing: machine #%ld's stream over the game's connection ended: %s",
				channel->machine_index, why);
	}
}

/* ---------- the guest */

long ae_map_share_channel_open_host(
	void)
{
	struct channel_globals *globals = &channel_globals;
	unsigned long address = guest_host_address();
	struct channel *channel;

	/* (internet play's guests too: their stream to the host goes through internet play's tunnel as the game's does, and
	the host's TCP 5153 is not reached at all where the tunnel carries the game alone) */
	if (channel_off() || is_host() || !address || !ae_map_share_guest_host_channel())
	{
		return 0;
	}
	globals->guest_session = ae_map_share_channel_next_session(globals->guest_session);
	channel = channel_new(GUEST_SLOT, globals->guest_session, NONE, address);
	if (!channel)
		return 0;
	error(_error_silent, "map sharing: reaching the host over the game's connection (stream %d)", (int)channel->session);
	return channel->id;
}

boolean ae_map_share_channel_host_wait(
	void)
{
	return !channel_off() && !is_host() && ae_map_share_guest_hello_awaited();
}

boolean ae_map_share_channel_offered(
	void)
{
	return !channel_off();
}

/* ---------- the stream */

long ae_map_share_channel_read(
	long id,
	byte *bytes,
	long size)
{
	struct channel *channel = channel_of(id);
	long taken;

	if (!channel || size <= 0)
		return 0;
	taken = ae_map_share_channel_inbox_take(&channel->inbox, bytes, size);
	if (taken)
	{
		ae_map_share_channel_window_took(&channel->window, taken);
		channel->bytes_in += (unsigned long)taken;
		return taken;
	}
	return channel->ended ? 0 : _transport_result_operation_would_block;
}

long ae_map_share_channel_write(
	long id,
	byte const *bytes,
	long size)
{
	struct channel *channel = channel_of(id);
	long sent = 0;

	if (!channel || channel->ended)
		return 0;
	while (sent < size)
	{
		long waiting = channel_waiting(channel);
		long chunk = MIN(size - sent, MIN((long)AE_MAP_SHARE_CHANNEL_DATA_BYTES,
			ae_map_share_channel_window_room(&channel->window)));

		if (waiting == NONE)
		{
			channel_end(channel, channel->machine_index == NONE ? "the game's connection is gone" : "the machine left");
			return sent ? sent : 0;
		}
		/* (the game's messages waiting first, or the window full) */
		if (waiting > 0 || chunk <= 0)
			break;
		if (!channel_send(channel, 0, bytes + sent, chunk))
		{
			channel_end(channel, "its message could not be sent");
			return sent ? sent : 0;
		}
		ae_map_share_channel_window_sent(&channel->window, chunk);
		channel->bytes_out += (unsigned long)chunk;
		sent += chunk;
	}
	return sent ? sent : _transport_result_operation_would_block;
}

void ae_map_share_channel_closed(
	long id)
{
	struct channel *channel = channel_of(id);

	if (!channel)
		return;
	/* (a guest whose OPEN never went out has nothing to close) */
	if (!channel->ended && (channel->machine_index != NONE || channel->open_said))
		channel_send(channel, AE_MAP_SHARE_CHANNEL_FLAG_CLOSE, NULL, 0);
	channel_free(channel);
}

/* ---------- the frames that come */

static void channel_take(
	struct channel *channel,
	struct ae_map_share_channel_frame const *frame)
{
	if ((frame->flags & AE_MAP_SHARE_CHANNEL_FLAG_ACK) &&
		!ae_map_share_channel_window_peer_took(&channel->window, frame->taken))
	{
		channel_end(channel, "it said it took bytes it was not sent");
		return;
	}
	if (frame->data_bytes && !ae_map_share_channel_inbox_put(&channel->inbox, frame->data, frame->data_bytes))
	{
		channel_end(channel, "it sent past its window");
		return;
	}
	if (frame->flags & AE_MAP_SHARE_CHANNEL_FLAG_CLOSE)
		channel_end(channel, NULL);
}

/* (the host) a machine's frame */
static void host_frame(
	long machine_index,
	struct ae_map_share_channel_frame const *frame)
{
	struct channel *channel = &channel_globals.slots[machine_index];
	unsigned long address = network_game_server_machine_address(machine_index);

	if (frame->flags & AE_MAP_SHARE_CHANNEL_FLAG_OPEN)
	{
		struct channel refused;

		/* (a new stream: one before it is the machine's no more, and its connection reads its end) */
		if (channel_off() || !address)
			return;
		if (channel->id && channel->session == frame->session && !channel->ended)
		{
			channel_take(channel, frame);
			return;
		}
		channel = channel_new(machine_index, frame->session, machine_index, address);
		if (channel && ae_map_share_connection_from_channel(channel->id, address))
		{
			error(_error_silent, "map sharing: machine #%ld reaches this host over the game's connection (stream %d)",
				machine_index, (int)frame->session);
			channel_take(channel, frame);
			return;
		}
		if (channel)
			channel_free(channel);
		/* (no room: the guest is told, and tries again later) */
		csmemset(&refused, 0, sizeof(refused));
		refused.session = frame->session;
		refused.machine_index = machine_index;
		channel_send(&refused, AE_MAP_SHARE_CHANNEL_FLAG_CLOSE, NULL, 0);
		error(_error_silent, "map sharing: machine #%ld's stream over the game's connection not taken (no room)",
			machine_index);
		return;
	}
	if (!channel->id || channel->session != frame->session || channel->ended)
		return;
	channel_take(channel, frame);
}

boolean ae_map_share_channel_message(
	long machine_index,
	word const *message,
	word size,
	boolean from_stream)
{
	struct distributed_message_header header;
	byte const *body = (byte const *)message + sizeof(header);
	long length = (long)size - (long)sizeof(header);
	struct ae_map_share_channel_frame frame;
	boolean request;
	long envelope_bytes;

	if (size < sizeof(header))
		return FALSE;
	csmemcpy(&header, message, sizeof(header));
	request = header.type == _distributed_message_map_share_request;
	if (!request && header.type != _distributed_message_map_share_offer)
		return FALSE;
	envelope_bytes = request ? REQUEST_BYTES : OFFER_BYTES;
	if (length <= envelope_bytes || csmemcmp(body, envelope_level_name, sizeof(envelope_level_name)) ||
		body[envelope_bytes] != AE_MAP_SHARE_CHANNEL_FRAME_TYPE)
	{
		return FALSE;
	}
	if (request ? body[REQUEST_WANTED] != ENVELOPE_MARK :
		body[OFFER_RESULT] != ENVELOPE_MARK || body[OFFER_FILE_COUNT] != ENVELOPE_FILE_COUNT)
	{
		return FALSE;
	}
	/* (from here a stream frame, whatever becomes of it: over the sender's stream alone, a guest's to its host, the
	host's to a guest) */
	if (!from_stream || !ae_map_share_channel_frame_decode(body + envelope_bytes, length - envelope_bytes, &frame))
		return TRUE;
	if (request)
	{
		if (machine_index >= 0 && machine_index < MACHINES && is_host())
			host_frame(machine_index, &frame);
	}
	else if (machine_index == NONE && !is_host())
	{
		struct channel *channel = &channel_globals.slots[GUEST_SLOT];

		if (channel->id && channel->session == frame.session && !channel->ended && channel->open_said)
			channel_take(channel, &frame);
	}
	return TRUE;
}

/* ---------- every frame */

void ae_map_share_channel_update(
	void)
{
	long slot;

	for (slot = 0; slot < SLOTS; slot++)
	{
		struct channel *channel = &channel_globals.slots[slot];

		if (!channel->id || channel->ended)
			continue;
		/* (its machine gone, or another in its place; the guest's host gone or another) */
		if (channel->machine_index != NONE ?
			!is_host() || network_game_server_machine_address(channel->machine_index) != channel->address :
			is_host() || guest_host_address() != channel->address)
		{
			channel_end(channel, "its game connection is gone");
			continue;
		}
		/* (what this end took, said when it is due and nothing else carried it) */
		if (ae_map_share_channel_window_ack_due(&channel->window) &&
			(channel->machine_index != NONE || channel->open_said) && channel_waiting(channel) == 0)
		{
			channel_send(channel, AE_MAP_SHARE_CHANNEL_FLAG_ACK, NULL, 0);
		}
	}
}
