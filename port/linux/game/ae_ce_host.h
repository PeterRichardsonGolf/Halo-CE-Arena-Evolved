/*
AE_CE_HOST.H

Arena Evolved joining a Content Evolved (CE) host that plays a modded game: the game's side of ae_ce_host_rules.h
(one-line AE hooks call these). Nothing here runs for a host that did not sign itself, or sends a record of any
size but CE's Firefight one.
*/

#ifndef AE_CE_HOST_H
#define AE_CE_HOST_H

#include "cseries.h"

struct network_connection;

/* (a client, an advertisement heard) the class of the host (ae_ce_host_rules.h's AE_CE_HOST_*) by its advertised version,
the advertisement's reserved bytes and flags; stored_version is the version the lists and the join go by: the host's
network version for a CE host this game plays (a plain 44 host to every rule below), else the one announced */
unsigned char ae_ce_host_advertised(unsigned short version, unsigned char const *reserved, unsigned char flags,
	unsigned short *stored_version);
/* an advertised game's engine index as this game names it (CE's Firefight, 6, is this game's 7) */
short ae_ce_host_engine(unsigned char host_class, short engine);
/* (a client, the join's test) whether a host of the class, with the flags and reserved bytes advertised, is joined:
TRUE for a host that is not CE's; else the reason is told when tell */
boolean ae_ce_host_join_check(unsigned char host_class, unsigned char flags, unsigned char const *reserved, boolean tell);
/* (a client) a join begins: the class of the advertised host, or -1 when no advertisement was heard (a join by address) */
void ae_ce_host_joining(int host_class);
/* (a client) the connection is gone, the join refused or failed, or the client reset: nothing of the host is kept
(its Firefight options go, its state is no CE host's; a refused host stays refused until the next join starts) */
void ae_ce_host_left(void);
/* (a game starts) a game this machine is not a client in (its own, or one it hosts) has none of a CE host's options */
void ae_ce_host_game_starting(void);
/* (a client) this game's verdict on a record ae_ce_host_piece made whole (AE_CE_HOST_PIECE_DONE): accepted, the host's
Firefight options are used from now; not, nothing of the host is kept */
void ae_ce_host_record_result(boolean accepted);
/* whether the joined host is a CE host (its Firefight options are kept for the game's whole time) */
boolean ae_ce_host_joined(void);
/* (a client, in the lobby and before the join) the capability message a CE host asks of every machine, once a second */
void ae_ce_host_capability(struct network_connection *connection);

/* (a client, skulls.c's mod_tags message) the host's level's shared tags against this one's: when they differ the player is
told why (AE_CE_HOST_TAGS_MESSAGE) and the game is left; a replay's playback leaves nothing */
void ae_ce_host_tags_differ(long count, unsigned long hash, long host_count, unsigned long host_hash);

/* a game settings record piece (total_size bytes in all): AE_CE_HOST_PIECE_NOT_CE when it is not one of CE's long record
(the caller goes on as before); _MORE when it is taken and the record is not whole; _DONE when it is whole and made
this game's in record (record_size bytes; its options are used once ae_ce_host_record_result says accepted); _REFUSED when the game is left (told) */
enum
{
	AE_CE_HOST_PIECE_NOT_CE = 0,
	AE_CE_HOST_PIECE_MORE,
	AE_CE_HOST_PIECE_DONE,
	AE_CE_HOST_PIECE_REFUSED
};
int ae_ce_host_piece(unsigned long total_size, unsigned long offset, unsigned long length, void const *data, void *record,
	unsigned long record_size);

#endif
