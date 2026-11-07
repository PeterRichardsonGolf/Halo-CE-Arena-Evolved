/*
AE_SERVER_LISTING.H

The AE master server's side of the game: its pinned key (ae_master_key.h) used only when real, the AE additions to a
dedicated server's publish (public_key, the signed "ae" block), and the browser's note of what the AE master says
of a game. Rules: ae_server_listing_rules.c.
*/

#ifndef __AE_SERVER_LISTING_H
#define __AE_SERVER_LISTING_H

struct server_json;
struct server_json_document;

/* whether a default master (url, "ed25519:<hex>") may be asked: 0 for a placeholder key, logged once */
int ae_server_listing_default_usable(
	char const *url,
	char const *key_text);

/* whether the master with that list key (32 bytes) takes the AE additions (the pinned AE master, or a key in
HALO_AE_MASTER_KEYS) */
int ae_server_listing_is_ae_master(
	unsigned char const *list_key);

/* the AE fields of a publish to that master (nothing for a master that is not the AE master's): public_key (the
publisher key, as the first publish needs), ae and ae_sig. variant is the game's variant name, or empty */
void ae_server_listing_publish_fields(
	struct server_json *json,
	unsigned char const *list_key,
	char const *server_id,
	unsigned char const *seed,
	unsigned char const *public_key,
	char const *variant);

/* the join word that fits a variant of that name: "ae" for a built-in AE gametype, else "all" */
char const *ae_server_listing_join_word(
	char const *variant);

/* the AE master's list entry (token `entry` of the parsed list) for the game whose signed listing it carried: its
"ae" block's join_now kept with the game, if there is one */
void ae_server_listing_note_join_word(
	struct server_json_document const *document,
	int entry,
	unsigned char const *listing,
	int listing_size);

/* (0.7.7) how many of the players the game's server counts are bots (below 0: none said): its listing's players are the
people, this goes in the AE block beside them */
void ae_server_listing_set_bots(
	int bots);

#endif
