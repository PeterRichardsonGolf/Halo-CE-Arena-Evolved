/*
AE_MAP_CHECK.H

(0.7.7) What an Arena Evolved machine can play, told to a dedicated server (messages 111 and 112). The server's playlist
(ae_server_playlist.c) has entries on MCC's maps (<name>@mcc, <name>@ws<item id>), which only a machine with MCC's files, and
for a Workshop map its item, can play. The server sends each Arena Evolved machine the list of those maps (112, a few
names a message, reliably, in the lobby too); the machine answers with 111: whether it has MCC's files, and a bit for each
map it holds. The server counts a machine that told nothing (an older build) as holding them.

A build that does not know 111 or 112 drops them as kinds it does not know.
*/

#ifndef AE_MAP_CHECK_H
#define AE_MAP_CHECK_H

#include "ae_playlist_rules.h"

/* the server asks a machine about the maps of the table (serial: this table's number) */
void ae_map_check_ask(
	long machine_index,
	struct ae_pl_map_table const *table,
	unsigned serial);

/* a client forgets what it was asked (a new connection) */
void ae_map_check_reset(void);

/* the distributed handler's: a message of these kinds; machine_index is the sender's on the host, NONE on a client; reliable:
it came over a stream */
int ae_map_check_handles_message(
	unsigned short const *message,
	unsigned short size);
void ae_map_check_handle_message(
	long machine_index,
	unsigned short const *message,
	unsigned short size,
	int reliable);

#endif
