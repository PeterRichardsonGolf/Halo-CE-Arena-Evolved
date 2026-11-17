/*
AE_MAP_SHARE_CHANNEL.H

Arena Evolved (0.7.7): map sharing's connection to the host over the game's own connection
(ae_map_share_channel.c; port/linux/NETCODE.md, "Over the game's connection").

Map sharing (map_share.c) fetches a map over connections of its own on TCP 5153. A host reached only at its game port
(a forwarded port, a tunnel that carries the game port alone) cannot be reached there. A guest of an Arena Evolved
host that says it takes it (its hello's AE_MAP_SHARE_FLAG_CHANNEL) reaches the host over the game's connection instead:
map sharing's frames, unchanged, go as the stream's bytes in map sharing's own messages (the request message to the
host, the offer message to the guest, both named for no map). The rest is map sharing's as it was: the offer, the
pieces, their hashes, the rules of what is sent. The host still takes connections on TCP 5153 (the players of other
builds, and the swarm of the others).

A map sharing connection over the game's carries a channel's id (0: a socket's); these stand for the socket calls.
*/

#ifndef __AE_MAP_SHARE_CHANNEL_H
#define __AE_MAP_SHARE_CHANNEL_H

/* ---------- prototypes */

/* (the guest) a channel to the host for the host's map sharing connection: its id, or 0 when the host does not take
one (not Arena Evolved of this build, or HALO_MAP_SHARE_CHANNEL=off); internet play's guests too */
long ae_map_share_channel_open_host(
	void);
/* (the guest) whether to wait a moment before reaching the host on TCP 5153: it has not said yet whether it runs
this game */
boolean ae_map_share_channel_host_wait(
	void);
/* (the host) whether it says to its guests that it takes their connections over the game's */
boolean ae_map_share_channel_offered(
	void);

/* as read_endpoint and write_endpoint (transport_endpoint_winsock.c): bytes, _transport_result_operation_would_block
for none now, or 0 for a stream that is gone (closed by the other end, its machine gone, or broken by what it sent) */
long ae_map_share_channel_read(
	long id,
	byte *bytes,
	long size);
long ae_map_share_channel_write(
	long id,
	byte const *bytes,
	long size);
/* map sharing closed the connection: the other end is told */
void ae_map_share_channel_closed(
	long id);

/* a map sharing message that is a stream frame (in the AE envelope): handled (TRUE) */
boolean ae_map_share_channel_message(
	long machine_index,
	word const *message,
	word size,
	boolean from_stream);
/* every frame (ae_map_share_all_update): what is taken said, gone machines' streams closed */
void ae_map_share_channel_update(
	void);

/* (map_share.c's, an AE hook) the host takes a machine's stream as a connection: FALSE if it has no room for one, or
sends nothing now (map sharing's own rules for a connection on TCP 5153) */
boolean ae_map_share_connection_from_channel(
	long id,
	unsigned long address);
/* (ae_map_share_all.c's) the guest's host said it takes the stream over the game's connection; and whether its
hello may still come */
boolean ae_map_share_guest_host_channel(
	void);
boolean ae_map_share_guest_hello_awaited(
	void);

#endif
