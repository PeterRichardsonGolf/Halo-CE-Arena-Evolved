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
	AE_JF_ITEMS_MAXIMUM = 12,	/* items taken from one server (address and port) in a run, and in one connection */
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
	AE_JF_LIMIT_ITEMS,		/* the connection's items, or the server's for the run, are used */
	AE_JF_NOT_MULTIPLAYER,	/* not a Halo CE multiplayer item */
	AE_JF_NOT_THE_MAP,		/* the item landed but does not hold the named map */
	AE_JF_TIMEOUT			/* the game's map did not come in AE_JF_HOLD_MAXIMUM_MS */
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
	int items[AE_JF_SERVERS];
	unsigned long long item_ids[AE_JF_SERVERS][AE_JF_ITEMS_MAXIMUM];
	int next;
};
/* the distinct items queued for a server in this run, across its connections (AE_JF_ITEMS_MAXIMUM is the most) */
int ae_jf_server_items(
	struct ae_jf_servers const *servers,
	unsigned long long key);
/* an item queued for the server: 1 when it is one the server already counted (a rejoin asks for the same items again) or is
counted now; 0 (nothing added) when it is a new one and the server already has AE_JF_ITEMS_MAXIMUM */
int ae_jf_server_item_add(
	struct ae_jf_servers *servers,
	unsigned long long key,
	unsigned long long item);
unsigned long long ae_jf_server_bytes(
	struct ae_jf_servers const *servers,
	unsigned long long key);
void ae_jf_server_add(
	struct ae_jf_servers *servers,
	unsigned long long key,
	unsigned long long bytes);

/* how long a server holds a joiner's slot for a Workshop map that joiner says it is fetching from Steam, and how often the
joiner says so */
#define AE_JF_HOLD_MAXIMUM_MS 900000UL
#define AE_JF_REPORT_INTERVAL_MS 2000UL

/* a server's record of one joiner machine that says it fetches the game's Workshop map from Steam. It belongs to the
machine's address, the time it joined and the game's map (a hash of its name): a machine that left and another that took its slot do not share one. */
enum { AE_JF_HOLD_NONE, AE_JF_HOLD_FETCHING, AE_JF_HOLD_DONE, AE_JF_HOLD_LAPSED, AE_JF_HOLD_FAILED };
#define AE_JF_HOLD_FRESH_MS 20000UL
#define AE_JF_HOLD_DONE_MS 20000UL
struct ae_jf_hold
{
	unsigned long address, joined, map, since, heard, done_at;
	int state;
};
/* a progress message of the machine (0 downloading, 1 done, 2 failed). The hold's start is set once; a hold that ran its
longest, went quiet or failed is final for this machine, and a done message opens one window of AE_JF_HOLD_DONE_MS */
void ae_jf_hold_progress(
	struct ae_jf_hold *hold,
	unsigned long address,
	unsigned long joined,
	unsigned long map,
	int progress,
	unsigned long now);
/* whether the machine is held now: as one downloading the map (needs_map 0) or as one that needs the map (1) */
int ae_jf_hold_active(
	struct ae_jf_hold const *hold,
	unsigned long address,
	unsigned long joined,
	unsigned long map,
	int needs_map,
	unsigned long now);
/* 1 when the machine's fetching hold has lapsed (it went quiet or ran its longest) and is now final; stays 1 */
int ae_jf_hold_lapsed(
	struct ae_jf_hold *hold,
	unsigned long address,
	unsigned long joined,
	unsigned long map,
	unsigned long now);

/* a hash of a map's name for the record's key (never 0) */
unsigned long ae_jf_hold_map_key(
	char const *name);

/* a start time for a hold begun at now: never 0 (0 is no start). Times are compared in 32 bits, as the clock counts. */
unsigned long ae_jf_hold_clock(
	unsigned long now);

/* whether a hold begun at since (a nonzero millisecond count) has run its longest at now */
int ae_jf_hold_expired(
	unsigned long since,
	unsigned long now);

/* whether a server whose game is on identity may be told its joiner is fetching: only a Workshop map (<file>@ws<digits>) */
int ae_jf_hold_allowed(
	char const *identity);

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
