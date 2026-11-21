/*
AE_JOIN_FETCH.H

A player who joins a server whose games are on Steam Workshop maps (<file>@ws<item id>) gets the ones the server's table
(message 112, ae_map_check.h) lists and this PC lacks, the game's map first when it is listed, then the table's order, at most
12 items from one server (address and port) for the run, however often the player rejoins. The Steam client of STEAM ACCOUNT (ae_steam_client.h) is asked for the items by their numbers, one at a time, and
the server is told again (an update of the answer, message 111) as each map is here. The setting is
network.map_download_steam, and the host's map sharing policy applies (network.map_download, network.map_download_internet);
DOWNLOAD LIMIT counts what the host sent and what Steam gave together, and each server's Steam bytes are kept to 1 GB for
the run. The player is told in the chat's notice line and, for the map being loaded, on a dedicated server's lobby download
card. The rules and the words are ae_join_fetch_rules.c's.

Nothing is fetched for a name that is not <file>@ws<digits>; the size, title, tags, app and files are Steam's, never the
server's; an item already in mcc_maps is not fetched again; the queue is dropped when the connection ends. The Steam client
is not started by a join.
*/

#ifndef AE_JOIN_FETCH_H
#define AE_JOIN_FETCH_H

#include "ae_playlist_rules.h"

/* the machine answered a server's question (ae_map_check.c): the names asked are kept to answer again as maps arrive */
void ae_join_fetch_offer(
	unsigned serial,
	char const names[][AE_PL_MAP_NAME],
	int count,
	unsigned char const *held_bits,
	int has_mcc);

/* each frame (ae_hooks.c) */
void ae_join_fetch_tick(unsigned long now);

/* the item being fetched now: 1 with its title, the bytes had and in all (total 0 while Steam is asked), else 0 */
int ae_join_fetch_progress(
	char *title,
	int size,
	unsigned long long *done,
	unsigned long long *total);

/* the game's map is a Workshop map this PC lacks (the check that would refuse the join, cache_files.c): 1 when it is being
fetched and the join is held for it (also held on the server: map_share.c), 0 when this PC cannot or may not get it */
int ae_join_fetch_current_map(
	char const *level);
/* whether that map is the one held for (NULL: any) */
int ae_join_fetch_current_holds(
	char const *level);

/* (a server) a machine's progress message while the game is on a Workshop map: 1 taken (0 downloading, 1 done, 2 failed),
0 not for this server to take; and whether the machine is held as one downloading the game's map (needs_map: asked for the players' hold, not the
timeout's). A hold belongs to the machine's address and join time, is set once, and is final once it lapsed, ran its longest
(AE_JF_HOLD_MAXIMUM_MS) or failed; a done message holds the machine as downloading once, for AE_JF_HOLD_DONE_MS */
int ae_join_fetch_host_progress(
	long machine_index,
	int state);
int ae_join_fetch_host_holds(
	long machine_index,
	int needs_map);
/* whether the machine's hold lapsed (it went quiet or ran its longest): map_share.c counts it as a stalled download */
int ae_join_fetch_host_lapsed(
	long machine_index);

#endif
