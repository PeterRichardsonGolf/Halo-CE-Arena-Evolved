/*
AE_SERVER_PLAYLIST.H

(0.7.7) A dedicated server's playlist rules, the engine side (the rules: ae_playlist_rules.h): the audience of each
rotation entry (ANYONE, AE, AE + MCC), the vote's no-repeat history, weighted choices and nominations, the chat commands
!nominate, !rtv and !extend, the one extension of a game, entries skipped when someone present cannot play them, the
refusal of a joiner who cannot play the game, and the join words the listing tells. Everything is nothing outside a
dedicated server. server_director.c, network_chat.c, ae_silent.c, ae_server_listing.c and ae_dev_cmd.c call these
(one hook line each).

Settings: server.vote_no_repeat (how many of the last games a vote keeps out, default 2), server.vote_nominate,
server.vote_rtv, server.vote_extend_minutes (the minutes one !extend gives; 0, the default, is off).
*/

#ifndef AE_SERVER_PLAYLIST_H
#define AE_SERVER_PLAYLIST_H

#include "../src/server_rotation.h"

/* the audience of a rotation entry (ae_playlist_rules.h's AE_AUDIENCE_*) */
int ae_server_playlist_audience(
	struct server_entry const *entry);

/* a game starts (the director's "loading"): the entry joins the history; nominations, rock the vote and the extension
are cleared */
void ae_server_playlist_game_started(
	struct server_entry const *entry);

/* the map vote's candidates (entries of the rotation, one per text): without the last games played and without entries
someone present cannot play, when two or more are left. Returns how many are left */
long ae_server_playlist_vote_candidates(
	struct server_rotation const *rotation,
	long *candidates,
	long count);

/* the candidates in the order the vote takes its first `wanted` from: the nominated entries first (at most wanted - 1),
then entries drawn with a chance in proportion to their weights */
void ae_server_playlist_vote_order(
	struct server_rotation const *rotation,
	long *candidates,
	long count,
	long wanted);

/* a chat line from a machine, at the host: "!nominate <map>", "!rtv", "!extend". Returns 1 if it was one (the line is not
sent on) */
int ae_server_playlist_chat(
	long machine_index,
	char const *player_name,
	char const *text);

/* the ticks the game's time limit is extended by (!extend) */
long ae_server_playlist_time_extra(void);

/* whether someone in the game (or about to join the lobby) cannot play the entry */
int ae_server_playlist_blocks(
	struct server_entry const *entry);

/* a machine asks to join: 1 if it cannot play the game and must be refused (*code is the reason and the host's chat says
why); 0 if it may (when the lobby's entry is one it cannot play and another is, the lobby is set up again for it) */
int ae_server_playlist_join_refused(
	long machine_index,
	short *code);

/* (the lobby's tick) whether the entry must be chosen again for a joiner who could not play it, and the flag cleared */
int ae_server_playlist_replan_take(
	int can_change);

/* the join words the listing tells: the current entry's audience word and a mask of the audiences of all playable entries
(ae_playlist_rules.h). 0 if this is no dedicated server or it has no rotation */
int ae_server_playlist_listing(
	char const **now,
	int *range_mask);

/* a machine's join was let through while the lobby's entry is chosen again for it (the packs' and the cache's own gates
would refuse it once for the old entry): 1 until the lobby's new entry is applied (at most ten seconds) */
int ae_server_playlist_join_deferred(
	long machine_index);

/* about once a second: the map check's list rebuilt when the rotation changed, and each Arena Evolved machine asked about
the maps the playlist needs MCC for (message 112) */
void ae_server_playlist_tick(void);

/* a machine's answer (message 111): it has MCC's files, and bits for the asked maps it holds. The lobby chooses its entry
again when this machine cannot play it; the machine is told what it lacks */
void ae_server_playlist_map_answer(
	long machine_index,
	unsigned serial,
	int has_mcc,
	unsigned char const *bits,
	int count);

#endif
