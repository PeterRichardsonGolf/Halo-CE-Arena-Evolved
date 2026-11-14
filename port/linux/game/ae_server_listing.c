/*
AE_SERVER_LISTING.C

See ae_server_listing.h.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "saved games/arena_gametypes.h"
#include "ae_master_key.h"
#include "ae_server_listing.h"
#include "ae_server_listing_rules.h"
#include "ae_server_playlist.h"
#include "../src/p2p.h"
#include "../src/port_config.h"
#include "../src/server_json.h"
#include "../src/server_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (from src/p2p_internal.h, which holds <pthread.h>: no <pthread.h> in a game unit, and Windows' game units are not
given the posix include folder) */
enum
{
	P2P_SIGNATURE_SIZE = 64,
};
void p2p_hex(const unsigned char *bytes, int size, char *text);
void p2p_ed25519_sign(const unsigned char *seed, const unsigned char *public_key, const void *message, int size,
	unsigned char *signature);

int ae_server_listing_default_usable(
	char const *url,
	char const *key_text)
{
	static int logged;

	if (ae_listing_key_usable(key_text, NULL))
		return 1;
	if (!logged)
	{
		logged = 1;
		server_log(_server_log_info, "master", "%s is not asked: its pinned key is a placeholder (ae_master_key.h)", url);
	}
	return 0;
}

static char const *extra_keys(void)
{
	return getenv("HALO_AE_MASTER_KEYS");
}

int ae_server_listing_is_ae_master(
	unsigned char const *list_key)
{
	return ae_listing_is_ae_master(list_key, AE_MASTER_LIST_KEY_IS_PLACEHOLDER ? NULL : AE_MASTER_LIST_KEY, extra_keys());
}

char const *ae_server_listing_join_word(
	char const *variant)
{
	struct game_variant game;
	struct game_variant_options options;
	wchar_t wide[64];
	long length;

	for (length = 0; variant && variant[length] && length < (long)NUMBEROF(wide) - 1; length++)
		wide[length] = (wchar_t)(unsigned char)(variant[length] == '_' ? ' ' : variant[length]);
	wide[length] = 0;
	csmemset(&game, 0, sizeof(game));
	return variant && variant[0] && arena_gametype_any_build(wide, &game, &options) ? "ae" : "all";
}

static int listing_bots = -1;

void ae_server_listing_set_bots(
	int bots)
{
	listing_bots = bots < 0 ? -1 : bots > 255 ? 255 : bots;
}

void ae_server_listing_publish_fields(
	struct server_json *json,
	unsigned char const *list_key,
	char const *server_id,
	unsigned char const *seed,
	unsigned char const *public_key,
	char const *variant)
{
	char canonical[AE_LISTING_EXTRAS_SIZE], text[AE_LISTING_EXTRAS_SIZE + 64], key_text[8 + 2 * 32 + 1];
	unsigned char signature[P2P_SIGNATURE_SIZE];
	char signature_text[2 * P2P_SIGNATURE_SIZE + 1];
	char const *word = ae_server_listing_join_word(variant);
	int canonical_length, text_length;
	int range_mask = 0;

	/* (the playlist's own words when this is a dedicated server with a rotation: its current entry's audience, and the
	audiences of all its entries as the range) */
	{
		char const *now_word;

		if (ae_server_playlist_listing(&now_word, &range_mask))
			word = now_word;
	}

	if (!ae_server_listing_is_ae_master(list_key))
		return;
	/* (the first publish needs the key, later ones may omit it: always sent, the master knows it then) */
	memcpy(key_text, "ed25519:", 8);
	p2p_hex(public_key, 32, key_text + 8);
	server_json_string(json, "public_key", key_text);
	canonical_length = ae_listing_extras_canonical_range(canonical, sizeof(canonical), word, range_mask, config_string("server.region"), listing_bots);
	text_length = canonical_length ? ae_listing_signed_text(text, sizeof(text), server_id, canonical) : 0;
	if (!text_length)
		return;
	p2p_ed25519_sign(seed, public_key, text, text_length, signature);
	p2p_hex(signature, P2P_SIGNATURE_SIZE, signature_text);
	/* (the block as the canonical text says: entries empty until the playlist's audiences are built) */
	server_json_object_begin(json, "ae");
	if (listing_bots >= 0)
		server_json_integer(json, "bots", listing_bots);
	server_json_array_begin(json, "entries");
	server_json_array_end(json);
	server_json_string(json, "join_now", word);
	server_json_array_begin(json, "join_range");
	if (range_mask & 7)
	{
		static char const *const words[3] = { "all", "ae", "ae+mcc" };
		int kind;

		for (kind = 0; kind < 3; kind++)
			if (range_mask & (1 << kind))
				server_json_string(json, NULL, words[kind]);
	}
	else
		server_json_string(json, NULL, word);
	server_json_array_end(json);
	{
		char region[AE_LISTING_REGION_SIZE + 1];

		ae_listing_region_clean(config_string("server.region"), region);
		server_json_string(json, "region", region);
	}
	server_json_object_end(json);
	server_json_string(json, "ae_sig", signature_text);
}

void ae_server_listing_note_join_word(
	struct server_json_document const *document,
	int entry,
	unsigned char const *listing,
	int listing_size)
{
	int block = server_json_member(document, entry, "ae");
	char word[AE_LISTING_WORD_SIZE + 8];
	char address[AE_LISTING_ADDRESS_SIZE + 8];

	/* (the address the master serves for it, if it serves one: JOIN goes straight there) */
	if (server_json_get_string(document, server_json_member(document, entry, "address"), address, sizeof(address)))
		p2p_lobby_set_master_address(listing, listing_size, address);
	if (block < 0)
		return;
	{
		double bots = 0;

		if (server_json_get_number(document, server_json_member(document, block, "bots"), &bots) && bots >= 0 && bots <= 255)
			p2p_lobby_set_master_bots(listing, listing_size, (int)bots);
	}
	{
		/* (a playlist that holds more than one kind of game: the browser's muted "mixed") */
		int range = server_json_member(document, block, "join_range");

		if (range >= 0)
			p2p_lobby_set_master_join_mixed(listing, listing_size, server_json_element(document, range, 1) >= 0);
	}
	if (server_json_get_string(document, server_json_member(document, block, "join_now"), word, sizeof(word)))
		p2p_lobby_set_master_join_word(listing, listing_size, word);
}
