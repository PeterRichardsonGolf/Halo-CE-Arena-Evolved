/*
AE_JOIN_FETCH_RULES.H

Getting a dedicated server's Workshop maps from Steam for a player who joins without them: the items' numbers from the
server's names, what stops a fetch, the one budget, and the words that tell the player.
Pure: string and stdlib only (unit test: port/linux/tests/ae_join_fetch_rules_test.c). The server's words are data: only
a name of the form <file>@ws<digits> is ever turned into an item number, and nothing else of them reaches Steam or the
disk.
*/

#ifndef AE_JOIN_FETCH_RULES_H
#define AE_JOIN_FETCH_RULES_H

#include "ae_playlist_rules.h"

enum
{
	AE_JF_ITEMS_MAXIMUM = 12,	/* items taken in one connection */
	AE_JF_SERVERS = 16			/* servers whose Steam bytes are kept for the session */
};

/* what one item may weigh, by Steam's own figures, and what one server's (address and port) items may weigh together, kept for the run */
#define AE_JF_ITEM_BYTES_MAXIMUM ((unsigned long long)256 << 20)
#define AE_JF_SERVER_BYTES_MAXIMUM ((unsigned long long)1 << 30)

/* why a map is not fetched (or what is waited for) */
enum
{
	AE_JF_OK,
	AE_JF_WAIT,				/* Steam's sign-in is not settled yet: look again */
	AE_JF_OFF,				/* network.map_download or network.map_download_steam is off */
	AE_JF_NOT_BUILT,		/* this build has no Steam client */
	AE_JF_NO_MCC,			/* no MCC files on this PC */
	AE_JF_NOT_SIGNED_IN,
	AE_JF_REFUSED,			/* Steam would not give the item (not found, no files, a failure) */
	AE_JF_NO_ROOM,
	AE_JF_TOO_LARGE,		/* the item is over the item ceiling */
	AE_JF_LIMIT_BYTES,		/* the connection's bytes are used */
	AE_JF_LIMIT_ITEMS,		/* the connection's items are used */
	AE_JF_NOT_MULTIPLAYER,	/* not a Halo CE multiplayer item */
	AE_JF_NOT_THE_MAP		/* the item landed but does not hold the named map */
};

/* the client's session states, as ae_steam_rules.h has them */
enum { AE_JF_SESSION_OUT = 0, AE_JF_SESSION_IN = 4, AE_JF_SESSION_UNAVAILABLE = 5 };

/* the item number of a name <file>@ws<digits> (1, *id set); 0 for any other name, an item number of zero, or one that
does not fit 64 bits */
int ae_jf_item_of(
	char const *identity,
	unsigned long long *id);

/* whether a fetch can start now: AE_JF_OK, AE_JF_WAIT or a reason */
int ae_jf_gate(
	int setting_on,
	int built,
	int has_mcc,
	int session,
	int checking);

/* what is left of the one budget: DOWNLOAD LIMIT (megabytes) less what the host sent and Steam gave this connection
(connection_taken), and AE_JF_SERVER_BYTES_MAXIMUM less what Steam gave this server in this run (server_taken): the
smaller */
unsigned long long ae_jf_budget_left(
	long limit_megabytes,
	unsigned long long connection_taken,
	unsigned long long server_taken);

/* the Steam bytes of each server, by its address and port (a key, 0 for none), kept for the run (oldest forgotten when full) */
struct ae_jf_servers
{
	unsigned long long key[AE_JF_SERVERS];
	unsigned long long bytes[AE_JF_SERVERS];
	int next;
};
unsigned long long ae_jf_server_bytes(
	struct ae_jf_servers const *servers,
	unsigned long long key);
void ae_jf_server_add(
	struct ae_jf_servers *servers,
	unsigned long long key,
	unsigned long long bytes);

/* the plain sentence for a reason (empty for OK, WAIT and OFF) */
void ae_jf_why_text(
	int why,
	char *text,
	int size);

/* "Getting "<title>" from Steam." / "Got "<title>" from Steam (2 of 7)." / the lobby card's line "52 of 120 MB, from Steam." */
void ae_jf_start_text(
	char const *title,
	int number,
	int of,
	char *text,
	int size);
void ae_jf_done_text(
	char const *title,
	int number,
	int of,
	char *text,
	int size);
void ae_jf_progress_text(
	unsigned long long done,
	unsigned long long total,
	char *text,
	int size);

#endif
