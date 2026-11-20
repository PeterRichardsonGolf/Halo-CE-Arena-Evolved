/*
AE_MAP_DOWNLOAD_RULES.H

Arena Evolved (0.9.3): GET MAPS, its pure rules (ae_map_download.c is the engine's side; port/linux/tests/
ae_map_download_rules_test.c runs them).

GET MAPS fetches the maps AE TOURNAMENT's playlists name that this PC lacks from the AE servers the server browser lists,
while those servers play their games. The guest reaches a server at its game port as a joiner does, and asks to be a
DOWNLOAD PEER instead of asking to join: a machine of the server's that never joins its game (no player, no seat, not in
the game's lists, scores or the browser's count). It takes the server's list of maps and map sharing's files over that
connection, as a joined machine of this game does in the lobby (ae_map_share_all.c, ae_map_share_channel.c), then leaves.

THE FRAME. Both ends' frames ride map sharing's messages in the envelope of AE's other frames (the level name no map
has): the guest's hello in the request message, the server's answer in the offer message. Byte 0 is
AE_MAP_DOWNLOAD_FRAME_TYPE, byte 1 the kind, then the kind's bytes:
- HELLO (guest): the frame's version (AE_MAP_DOWNLOAD_VERSION), 2 bytes, little-endian.
- WELCOME (server): nothing more.
- REFUSE (server): the reason (AE_MAP_DOWNLOAD_REFUSED_*).
- BYE (server): the reason it was let go (AE_MAP_DOWNLOAD_REFUSED_FILLED: a joiner needed its place).
A server before 0.9.3 drops a hello (it is not a join request, from a machine that has not joined): the guest hears no
answer and says the server needs 0.9.3.

ROOM. A server takes a download peer when GET MAPS is on (server.map_download_peers above 0), it holds fewer peers than
that, and the game keeps at least AE_MAP_DOWNLOAD_FREE_SLOTS other machine slots free for people who join. A joiner who
finds no slot free takes a download peer's (the peer is let go first).
*/

#ifndef __AE_MAP_DOWNLOAD_RULES_H
#define __AE_MAP_DOWNLOAD_RULES_H

#define AE_MAP_DOWNLOAD_FRAME_TYPE 6
#define AE_MAP_DOWNLOAD_VERSION 1

enum
{
	AE_MAP_DOWNLOAD_KIND_HELLO = 1,
	AE_MAP_DOWNLOAD_KIND_WELCOME = 2,
	AE_MAP_DOWNLOAD_KIND_REFUSE = 3,
	AE_MAP_DOWNLOAD_KIND_BYE = 4,

	AE_MAP_DOWNLOAD_FRAME_BYTES = 4,

	/* why a server says no (a REFUSE's or a BYE's reason) */
	AE_MAP_DOWNLOAD_REFUSED_OFF = 1,
	AE_MAP_DOWNLOAD_REFUSED_FULL = 2,
	AE_MAP_DOWNLOAD_REFUSED_FILLED = 3,
	AE_MAP_DOWNLOAD_REFUSED_VERSION = 4,

	/* the machine slots a server keeps free for joiners besides a download peer's own */
	AE_MAP_DOWNLOAD_FREE_SLOTS = 2,
	/* server.map_download_peers: its default and its most */
	AE_MAP_DOWNLOAD_PEERS_DEFAULT = 2,
	AE_MAP_DOWNLOAD_PEERS_MOST = 4,
	/* a download peer is let go after this long silent, and after this long in all */
	AE_MAP_DOWNLOAD_IDLE_MS = 20000,
	AE_MAP_DOWNLOAD_VISIT_MS = 45 * 60 * 1000,
	/* a refusal at a slot is forgotten after this long (ae_map_download_refusal_kept) */
	AE_MAP_DOWNLOAD_REFUSAL_MS = 20000,
	/* the guest: how long it waits for a server's answer to its hello, and for its list once welcome (a server hashes
	its maps once, a little each frame, before its first list goes out) */
	AE_MAP_DOWNLOAD_ANSWER_MS = 6000,
	AE_MAP_DOWNLOAD_LIST_MS = 120000,
	/* the servers GET MAPS asks in one run, at most */
	AE_MAP_DOWNLOAD_SERVERS = 16,
	/* the maps it looks for, at most */
	AE_MAP_DOWNLOAD_WANTED = 128,
	AE_MAP_DOWNLOAD_NAME = 64,
};

struct ae_map_download_frame
{
	int kind;
	int version;
	int reason;
};

/* the frame's bytes (AE_MAP_DOWNLOAD_FRAME_BYTES at most): how many, 0 for a frame that is not valid or no room */
long ae_map_download_frame_encode(struct ae_map_download_frame const *frame, unsigned char *bytes, long size);
/* a frame from its bytes: 1 if they are one (its type, a known kind, its bytes there), else 0 */
int ae_map_download_frame_decode(unsigned char const *bytes, long size, struct ae_map_download_frame *frame);

/* a hello's answer: AE_MAP_DOWNLOAD_KIND_WELCOME, else the REFUSE's reason as a negative number. cap is
server.map_download_peers (0 or less: off), peers those it holds now, free_slots the machine slots free besides the
asker's own, version the hello's */
int ae_map_download_room(int cap, int peers, int free_slots, int version);
/* what a server does with a hello (review 0.9.3: a refused connection that says hello again must not hold its machine
slot): ANSWER it (welcome or refuse), AGAIN for a download peer's (answered, the peer heard), IGNORE a refused
connection's next ones (no answer: it is never counted heard and times out as any machine that never joins) */
enum
{
	AE_MAP_DOWNLOAD_HELLO_IGNORE,
	AE_MAP_DOWNLOAD_HELLO_ANSWER,
	AE_MAP_DOWNLOAD_HELLO_AGAIN
};
int ae_map_download_hello_action(int peer_on, int refused_before);
/* whether a machine that has not joined is counted heard for a GET MAPS message: a download peer's alone (a refused
hello never keeps a connection open) */
int ae_map_download_heard(int peer_on);
/* whether a machine's join request is taken: never a download peer's (review 0.9.3: it would join past the
per-address waiting cap and the reserved slots, from a slot held as a peer's) */
int ae_map_download_join_allowed(int peer_on);
/* whether a refusal at a machine slot still holds (review 0.9.3e: it outlived its connection, so the same guest back
later from the same address into the same slot was ignored for good): the refused connection's address, the address of
the connection at the slot now (0 for none), and the refusal's age. It holds while the same address is there, for
AE_MAP_DOWNLOAD_REFUSAL_MS at most (a refused connection is never counted heard, so it still times out) */
int ae_map_download_refusal_kept(unsigned long refused_address, unsigned long slot_address, unsigned long age_ms);
/* server.map_download_peers kept to 0 .. AE_MAP_DOWNLOAD_PEERS_MOST */
int ae_map_download_peers_setting(long value);
/* whether a download peer is let go: silent for idle_ms, or there for visit_ms */
int ae_map_download_peer_expired(unsigned long idle_ms, unsigned long visit_ms);

/* THE MAPS WANTED: the playlists' maps (names as the playlist file has them), each with whether it is a stock
fallback; is_stock and present the game's answers. The distinct maps that are not fallbacks nor stock ones and that
this PC lacks go into out (up to maximum, in the order first named); how many */
typedef int (*ae_map_download_name_fn)(void *context, char const *name);
int ae_map_download_wanted(char const *const *names, int const *fallbacks, int count, ae_map_download_name_fn is_stock,
	ae_map_download_name_fn present, void *context, char (*out)[AE_MAP_DOWNLOAD_NAME], int maximum);
/* whether name (a list entry's: <file>@<pack>, a plain file, or a Custom Edition map's <file>_<8 hex digits of its
   hash>, which a playlist names <file>@ce or <file>) is one of the wanted (any case) */
int ae_map_download_is_wanted(char const (*wanted)[AE_MAP_DOWNLOAD_NAME], int count, char const *name);

/* whether a playlist's map is a Custom Edition map (<name>@ce): it wants Custom Edition's resource maps, Halo PC's or
MCC's, and is not fetched for a PC without them */
int ae_map_download_needs_custom_edition(char const *name);
/* the card's note on the playlist maps left out for want of Custom Edition's files: "h2_warlock needs MCC or Halo PC."
(one: its name before the @) or "3 maps need MCC or Halo PC." ("" for none) */
void ae_map_download_custom_edition_note(int count, char const *first, char *out, int size);

/* THE CARD's words */
enum
{
	AE_MAP_DOWNLOAD_OUTCOME_NONE,
	AE_MAP_DOWNLOAD_OUTCOME_DONE,           /* every wanted map is here now */
	AE_MAP_DOWNLOAD_OUTCOME_SOME,           /* some came, the rest no server had */
	AE_MAP_DOWNLOAD_OUTCOME_NOTHING_NEEDED, /* nothing was missing */
	AE_MAP_DOWNLOAD_OUTCOME_NO_SERVERS,     /* the browser lists no AE server */
	AE_MAP_DOWNLOAD_OUTCOME_NONE_HAVE,      /* no server listed a missing map (or none answered) */
	AE_MAP_DOWNLOAD_OUTCOME_OFFLINE,        /* the last server asked did not answer */
	AE_MAP_DOWNLOAD_OUTCOME_OLD,            /* it needs 0.9.3 */
	AE_MAP_DOWNLOAD_OUTCOME_FULL,           /* it is busy with other downloads, or nearly full */
	AE_MAP_DOWNLOAD_OUTCOME_OFF,            /* it does not share maps */
	AE_MAP_DOWNLOAD_OUTCOME_FILLED,         /* it filled up with players: let go */
	AE_MAP_DOWNLOAD_OUTCOME_DOWNLOADS_OFF,  /* this PC's map downloads are off */
	AE_MAP_DOWNLOAD_OUTCOME_ONLINE_OFF,     /* internet play is off */
	AE_MAP_DOWNLOAD_OUTCOME_CANCELLED,
	AE_MAP_DOWNLOAD_OUTCOME_BUSY,           /* a game is joined or hosted */
	AE_MAP_DOWNLOAD_OUTCOMES
};

/* the outcome of a server's REFUSE or BYE reason */
int ae_map_download_outcome_of_refusal(int reason);
/* the card's end line for an outcome: done counts the maps that came, server the last server's name ("" for none) */
void ae_map_download_outcome_text(int outcome, int done, char const *server, char *out, int size);
/* whether an outcome is a failure (drawn as a warning) */
int ae_map_download_outcome_failed(int outcome);
/* the card's progress line: "GETTING MAPS FROM <SERVER>  3 OF 12  48 / 310 MB" (bytes in MB, rounded down; a total
not known yet leaves the MB out) */
void ae_map_download_progress_text(char const *server, int index, int count, unsigned long long bytes,
	unsigned long long total, char *out, int size);

#endif
