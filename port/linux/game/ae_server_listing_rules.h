/*
AE_SERVER_LISTING_RULES.H

Pure rules of the AE master listing (port/linux/tests/ae_server_listing_rules_test.c): whether a pinned key may be
used, the AE extras block of a publish in the master's canonical form and the text its signature covers
(~/ae-master internal/aeextras), and how the browser merges what several masters say of one game.
*/

#ifndef AE_SERVER_LISTING_RULES_H
#define AE_SERVER_LISTING_RULES_H

enum
{
	AE_LISTING_WORD_SIZE = 25,
	AE_LISTING_REGION_SIZE = 17,
	AE_LISTING_EXTRAS_SIZE = 512,
	AE_LISTING_KEY_BYTES = 32
};

/* "ed25519:" and 64 hex digits that are not a placeholder (all one digit): 1; the bytes in key_bytes if not NULL */
int ae_listing_key_usable(
	char const *text,
	unsigned char *key_bytes);

/* whether key is the pinned AE master's (pinned_text, if usable) or one of the comma separated "ed25519:<hex>" keys
in extra_text (a private AE master: HALO_AE_MASTER_KEYS; may be NULL) */
int ae_listing_is_ae_master(
	unsigned char const *key,
	char const *pinned_text,
	char const *extra_text);

/* the words a join_now takes: "all", "ae", "ae+opence", each with an optional "+mcc" / "+workshop": 1 if word is one
(a-z and +, 1 to 24 characters) */
int ae_listing_word_valid(
	char const *word);

/* region text the master takes (lowercase a-z 0-9 -, up to 16; clean needs AE_LISTING_REGION_SIZE + 1): anything else, empty */
void ae_listing_region_clean(
	char const *region,
	char *clean);

/* the block as the master canonicalises it: {"entries":[],"join_now":w,"join_range":[w],"region":r}; region is
cut to what the master takes (lowercase a-z 0-9 -, 16) or empty. Returns its length, 0 if it does not fit or the
word is not valid */
int ae_listing_extras_canonical(
	char *out,
	int size,
	char const *join_now,
	char const *region);

/* "AE-Extras-v1\n" + server id + "\n" + canonical: the text the publisher key signs; returns length, 0 if too small */
int ae_listing_signed_text(
	char *out,
	int size,
	char const *server_id,
	char const *canonical);

/* the join word shown for a master's join_now (the browser's badge): "ANYONE", "AE", "AE+MCC", "AE+WORKSHOP", else "AE" */
char const *ae_browser_badge_word(
	char const *join_now);

/* which of two masters' words for one game the browser keeps: the richer (non-empty over empty, the AE master's over
none); equal richness keeps current. Returns 1 if incoming replaces current */
int ae_browser_merge_word(
	char const *current,
	char const *incoming);

/* the longest address text kept with a listed game ("host:port") */
enum { AE_LISTING_ADDRESS_SIZE = 47 };

/* whether text is a master's "address" a client may connect to: host and a port after one colon, the host an IPv4
address in four numbers or a name (letters, digits, '.', '-'), the port 1 to 65535, the whole at most
AE_LISTING_ADDRESS_SIZE characters. 1 and the parts in host (host_size bytes) and port if so; 0 else (outputs untouched) */
int ae_listing_address_split(
	char const *text,
	char *host,
	int host_size,
	unsigned short *port);

/* the way a listed game is joined: 1 straight to its address (the master gave a good one), 0 through its invite
(the brokers' way: no address, or one that is no address) */
int ae_browser_joins_direct(
	char const *address);

/* the same with the server's bot count (0 to 255; below 0: not said, none in the block): "bots" is the first key (the
keys sort) */
int ae_listing_extras_canonical_bots(
	char *out,
	int size,
	char const *join_now,
	char const *region,
	int bots);

/* the same with the playlist's range: range_mask has a bit per kind of entry it holds (bit 0 "all", bit 1 "ae", bit 2
"ae+mcc"; none set: join_range is join_now alone), the words in that order. bots below 0: no bots key */
int ae_listing_extras_canonical_range(
	char *out,
	int size,
	char const *join_now,
	int range_mask,
	char const *region,
	int bots);

/* a listed server's seats as the browser reads them. players are the PEOPLE a server lists (a bots-aware server, the
AE master's "bots" said, leaves its bots out); bots below 0: the server did not say, its players are all it has (an old
server: it may count bots in them, as before). full: the people fill the seats (a bot's seat is free: a joining person
replaces it); empty: no person (a bot-only server is empty). Returns the sum of the flags 1 full, 2 empty */
int ae_browser_seat_flags(
	int players,
	int maximum,
	int bots);

/* the count text of a row: "X / Y" and, with bots said and above 0, " \xC2\xB7 N bots" ("1 bot") */
void ae_browser_players_text(
	char *out,
	int size,
	int players,
	int maximum,
	int bots);

#endif
