/*
AE_MAP_DOWNLOAD.H

Arena Evolved (0.9.3): GET MAPS (its rules and the frame: ae_map_download_rules.h). AE TOURNAMENT's GET MAPS fetches
the maps its playlists name that this PC lacks from the AE servers the server browser lists, while they play: the
guest reaches each server at its game port as a joiner does (ae_glue_lobby.c's download join), says hello instead of
asking to join, and the server, if it has room, keeps it as a DOWNLOAD PEER: a machine that never joins its game. The
server's list of maps and map sharing's files come over that connection as they do for a joined machine of this game
in the lobby (ae_map_share_all.c, ae_map_share_channel.c); only the wanted maps are fetched, from the servers whose
list has them, until none is missing.

Log: "map download: ..." (both ends).
*/

#ifndef __AE_MAP_DOWNLOAD_H
#define __AE_MAP_DOWNLOAD_H

#include "ae_map_download_rules.h"

/* ---------- the server's side (network_server_manager.c, network_server_message_handler.c: AE hooks) */

/* a map sharing message from machine slot (one that has not joined the game): a hello taken (answered: welcome or
refuse), or a download peer's message handed to map sharing. FALSE: not GET MAPS's (the caller's rules go on) */
int ae_map_download_server_message(long slot, unsigned short const *message, unsigned short size);
/* whether the machine at slot is a download peer (and, sendable, one being answered: a refusal goes to it too) */
int ae_map_download_peer(long slot);
/* whether the server counts a machine that has not joined heard for its GET MAPS message: a download peer's alone */
int ae_map_download_peer_heard(
	long slot);
/* whether the server refuses this machine's join request: a download peer's (logged) */
int ae_map_download_join_refused(
	long slot);
int ae_map_download_peer_sendable(long slot);
/* a download peer's address (its connection's, host order), 0 for none */
unsigned long ae_map_download_peer_address(long slot);
/* whether the server lets the download peer at slot go now (silent too long, or there too long) */
int ae_map_download_peer_expired_now(long slot);
/* a joiner found no free slot: the peer at slot is told it is let go (it is dropped then); FALSE for no peer there */
int ae_map_download_peer_let_go(long slot);

/* ---------- the guest's side */

struct ae_map_download_status
{
	int running;
	/* the server being asked (its browser name), and which of how many */
	char server[48];
	int server_index, server_count;
	/* the maps of this server being fetched: the one coming (1 ..) of how many, its bytes (both 0: not known yet) */
	int map_index, map_count;
	unsigned long long bytes, total;
	/* the progress line (ae_map_download_progress_text) */
	char line[160];
	/* the last run's outcome (AE_MAP_DOWNLOAD_OUTCOME_*) and its words, while they are shown */
	int outcome;
	char outcome_text[160];
	int maps_got;
};

/* starts GET MAPS (in the menus, no game joined or hosted): 1 if it runs; else the outcome says why not (nothing missing,
downloads off, ...) */
int ae_map_download_start(void);
/* B: the run stops (the map coming is dropped; those that came stay) */
void ae_map_download_cancel(void);
/* every frame (ae_map_share_all_update) */
void ae_map_download_update(void);
/* the card's state; whether there is anything to show (a run, or its outcome for a while) */
int ae_map_download_status(struct ae_map_download_status *status);
/* the outcome shown no more (the card's line went away) */
void ae_map_download_outcome_seen(void);
/* the maps of AE TOURNAMENT's playlists this PC lacks (ae_map_download_wanted): how many */
int ae_map_download_missing(void);

/* (ae_glue_lobby.c, ae_map_share_all.c, network_client_manager.c's AE hook) a GET MAPS connection is under way: no join
request goes out, no lobby is taken, no other map of the server is fetched */
int ae_map_download_guest_active(void);
int ae_map_download_guest_holds_join(void);
/* whether a server's list entry is one GET MAPS wants (any case) */
int ae_map_download_guest_wants(char const *name);
/* (ae_map_share_all.c) a frame of GET MAPS from the server: taken (TRUE), else not GET MAPS's */
int ae_map_download_guest_message(long machine_index, unsigned short const *message, unsigned short size, int from_stream);

#endif
