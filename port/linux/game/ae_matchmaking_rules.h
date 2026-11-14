/*
AE_MATCHMAKING_RULES.H

Arena Evolved's MATCHMAKING (menus phase 1, owner 2026-11-10): the playlists file beside the game (matchmaking.txt, from
port/assets/network/matchmaking.txt: one playlist a line, `id|LABEL|description|address [address ...]`, `#` comments),
and the server a party is sent to: the playlist's first server, in the file's order, not tried yet this search, that
answers and has room for the whole party (one not heard from yet is tried). Pure
(port/linux/tests/ae_matchmaking_rules_test.c); the screen is ae_screen_matchmaking.c.
*/

#ifndef __AE_MATCHMAKING_RULES_H
#define __AE_MATCHMAKING_RULES_H

enum { AE_MM_PLAYLIST_ADDRESSES = 4 };

struct ae_mm_playlist
{
	char id[16], label[24], description[96];
	char addresses[AE_MM_PLAYLIST_ADDRESSES][64];
	int address_count;
};

/* a server as the browser's query last heard it */
struct ae_mm_server_seen
{
	char address[64];
	int players, max_players, reachable;
};

/* the file's playlists into out (at most maximum); malformed lines (fewer than 4 fields, an empty id or label, no
address) and comments skipped. Returns how many */
int ae_mm_playlists_parse(const char *text, struct ae_mm_playlist *out, int maximum);

/* the index of the playlist's server to try (file order) not in tried_mask, reachable with players + party <= max_players
(an address seen has no entry: tried, as reachable with room); -1 when none */
int ae_mm_playlist_pick(struct ae_mm_playlist const *p, struct ae_mm_server_seen const *seen, int seen_count, int tried_mask,
	int party);

#endif
