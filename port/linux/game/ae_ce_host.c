/*
AE_CE_HOST.C

Arena Evolved joining a Content Evolved host that plays a modded game (ae_ce_host.h; the rules: ae_ce_host_rules.h).
*/

#include "ae_ce_host.h"
#include "ae_ce_host_rules.h"
#include "../src/port_config.h"

#include "bungie_net/common/message_header.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_manager.h"
#include "networking/network_messages.h"
#include "halo_port_limits.h"
#include "halo_skulls.h"
#include "tag_pack.h"
#include "randomizer.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* the platform layer's */
unsigned long system_milliseconds(void);
void platform_show_message(char const *title, char const *message);
/* the game's */
void display_error_text_when_main_menu_loaded(wchar_t const *text);
void network_game_abort(void);
unsigned char network_replay_playing(void);
/* ae_firefight_glue.c's */
int ae_firefight_net_widths(unsigned char *widths, int maximum);
void ae_firefight_net_apply_ce(unsigned long const *values, int count);
void ae_firefight_net_clear(void);

/* ---------- this build's record against the rules' (the rules read CE's by these offsets) */

typedef char verify_ae_ce_engine[game_engine_firefight == AE_CE_HOST_ENGINE_AE_FIREFIGHT ? 1 : -1];
typedef char verify_ae_ce_options_size[sizeof(struct game_variant_options) == AE_CE_HOST_OPTIONS_AE_SIZE ? 1 : -1];
typedef char verify_ae_ce_force_team_spawns[offsetof(struct game_variant_options, force_team_spawns) == 38 ? 1 : -1];
typedef char verify_ae_ce_jump_percent[offsetof(struct game_variant_options, jump_percent) == 39 ? 1 : -1];

#ifndef HALO_PORT_CONSOLE_LIMITS
typedef char verify_ae_ce_record_size[sizeof(struct network_game) == HALO_PORT_NETWORK_GAME_SIZE ? 1 : -1];

static struct ae_ce_host_layout const record_layout =
{
	sizeof(struct network_game),
	offsetof(struct network_game, variant_options),
	offsetof(struct network_game, variant) + offsetof(struct game_variant, game_engine_index),
	sizeof(((struct network_game *)0)->variant.game_engine_index),
	game_engine_firefight,
	offsetof(struct game_variant_options, jump_percent)
};
#endif

/* ---------- the joined host */

static int host_state = AE_CE_HOST_STATE_NOT_CE;
/* the converted options of an arrived record, applied once this game accepts the record */
static unsigned long pending_values[AE_CE_HOST_VALUES];
static int pending_values_ready;
static unsigned long capability_time;
static int record_taken_logged;

#ifndef HALO_PORT_CONSOLE_LIMITS
/* the long record as its pieces arrive */
static unsigned char ce_record[HALO_PORT_NETWORK_GAME_SIZE + AE_CE_HOST_RECORD_EXTRA];
static unsigned long ce_record_taken;
#endif

unsigned char ae_ce_host_advertised(unsigned short version, unsigned char const *reserved, unsigned char flags,
	unsigned short *stored_version)
{
	unsigned base = reserved ? (unsigned)(reserved[AE_CE_HOST_BASE_VERSION_OFFSET] |
		(reserved[AE_CE_HOST_BASE_VERSION_OFFSET + 1] << 8)) : 0;
	int host_class = ae_ce_host_classify(version, base, flags, (unsigned)HALO_PORT_NETWORK_VERSION);

	if (stored_version)
		*stored_version = ae_ce_host_class_joinable(host_class) ? (unsigned short)base : version;
	return (unsigned char)host_class;
}

short ae_ce_host_engine(unsigned char host_class, short engine)
{
	return host_class != AE_CE_HOST_NOT_CE && engine == AE_CE_HOST_ENGINE_CE_FIREFIGHT ? (short)game_engine_firefight : engine;
}

static void tell_player_or_log(boolean tell_player, char const *log_line, char const *message)
{
	network_event("not joining a Content Evolved host: %s", log_line);
	if (tell_player)
		platform_show_message("Halo: cannot join this game", message);
}

boolean ae_ce_host_join_check(unsigned char host_class, unsigned char flags, unsigned char const *reserved, boolean tell_player)
{
	struct ae_ce_host_join_verdict verdict;
	char message[500];
	/* (the randomizer's and the skulls' copies of other levels' tags, as CE's own client asks for them; plain reads) */
	boolean randomizer_pack_missing = host_class == AE_CE_HOST_JOIN && (flags & AE_CE_HOST_FLAG_RANDOMIZER) &&
		!tag_pack_ready(RANDOMIZER_PACK);
	boolean skulls_pack_missing = host_class == AE_CE_HOST_JOIN && reserved &&
		skulls_advertised_need_pack(flags, reserved + HALO_PORT_ADVERTISED_SKULLS_OFFSET) && !tag_pack_ready(SKULLS_PACK);

	/* (a listing, tell_player 0, only classifies: the log, the player and the packs are the join's) */
	ae_ce_host_join_decide(host_class, randomizer_pack_missing, skulls_pack_missing, tell_player, &verdict);
	if (verdict.log)
	{
		switch (verdict.say)
		{
		case AE_CE_HOST_SAY_JOINABLE:
			network_event("joining a Content Evolved host (mod version %d)", AE_CE_HOST_MOD_VERSION);
			break;
		case AE_CE_HOST_SAY_PACKS_MISSING:
			snprintf(message, sizeof(message),
				"This game uses data (%s%s%s) that is not ready on this machine.\n\n"
				"It is written now, at the main menu: join again once it is done.",
				randomizer_pack_missing ? "maps/" RANDOMIZER_PACK ".pack" : "",
				randomizer_pack_missing && skulls_pack_missing ? " and " : "",
				skulls_pack_missing ? "maps/" SKULLS_PACK ".pack" : "");
			tell_player_or_log(verdict.tell_player, "its data packs are not written", message);
			break;
		case AE_CE_HOST_SAY_MOD_VERSION_OLDER:
		case AE_CE_HOST_SAY_MOD_VERSION_NEWER:
			snprintf(message, sizeof(message),
				"The host plays Content Evolved's game of another mod version than this game.\n\n"
				"This game plays mod version %d. %s", AE_CE_HOST_MOD_VERSION,
				verdict.say == AE_CE_HOST_SAY_MOD_VERSION_NEWER ? "Update this game to join this host." : "Ask the host to update the game.");
			tell_player_or_log(verdict.tell_player, "another mod version", message);
			break;
		case AE_CE_HOST_SAY_TAG_SHARE:
			tell_player_or_log(verdict.tell_player, "its game shares Tag Injector tags",
				"This Content Evolved game shares tags from its host's Tag Injector profile, which this game cannot "
				"download yet.\n\nJoin a game without a shared profile.");
			break;
		default:
			tell_player_or_log(verdict.tell_player, "another network version",
				"The host plays Content Evolved's game of another network version than this game.\n\n"
				"Update the game, or ask the host to, to join this host.");
			break;
		}
	}
	if (verdict.request_packs & AE_CE_HOST_PACK_RANDOMIZER)
		tag_pack_request(RANDOMIZER_PACK);
	if (verdict.request_packs & AE_CE_HOST_PACK_SKULLS)
		tag_pack_request(SKULLS_PACK);
	return verdict.joinable ? TRUE : FALSE;
}

/* the one place the CE state changes: the rule says the next state and whether the options kept from the host go */
static int state_event(int event)
{
	int clear_options = 0;

	host_state = ae_ce_host_state_next(host_state, event, &clear_options);
	if (clear_options)
		ae_firefight_net_clear();
	return host_state;
}

void ae_ce_host_joining(int host_class)
{
	capability_time = 0;
	record_taken_logged = 0;
	pending_values_ready = 0;
#ifndef HALO_PORT_CONSOLE_LIMITS
	ce_record_taken = 0;
#endif
	state_event(host_class < 0 ? AE_CE_HOST_EVENT_JOIN_STARTED_UNKNOWN : host_class == AE_CE_HOST_JOIN ?
		AE_CE_HOST_EVENT_JOIN_STARTED_CE : AE_CE_HOST_EVENT_JOIN_STARTED_OTHER);
}

void ae_ce_host_left(void)
{
	/* (the host's Firefight options were kept for the game's whole time: they go with it) */
	pending_values_ready = 0;
#ifndef HALO_PORT_CONSOLE_LIMITS
	ce_record_taken = 0;
#endif
	state_event(AE_CE_HOST_EVENT_LEFT);
}

void ae_ce_host_game_starting(void)
{
	int connection = game_connection();

	/* (a game this machine is not a client in has none of a host's) */
	if (connection != _game_connection_network_client)
		state_event(connection == _game_connection_network_server ? AE_CE_HOST_EVENT_HOSTING : AE_CE_HOST_EVENT_SOLO_START);
}

void ae_ce_host_record_result(boolean accepted)
{
	if (!pending_values_ready)
		return;
	pending_values_ready = 0;
	if (!accepted)
	{
		state_event(AE_CE_HOST_EVENT_REJECTED);
		return;
	}
	/* (this game took the record: the host's options are used now, and no sooner) */
	if (state_event(AE_CE_HOST_EVENT_RECORD_ACCEPTED) == AE_CE_HOST_STATE_CE)
	{
		ae_firefight_net_apply_ce(pending_values, AE_CE_HOST_VALUES);
		if (!record_taken_logged++)
			network_event("cross-play: the host's settings record is Content Evolved's Firefight: made this game's, its %d options kept",
				AE_CE_HOST_VALUES);
	}
}

boolean ae_ce_host_joined(void)
{
	return host_state == AE_CE_HOST_STATE_CE;
}

void ae_ce_host_capability(struct network_connection *connection)
{
	unsigned long now = system_milliseconds();
	byte capable = AE_CE_HOST_MOD_VERSION;
	word *message;

	if (host_state != AE_CE_HOST_STATE_CE || !connection || (capability_time && now - capability_time < 1000))
		return;
	capability_time = now ? now : 1;
	message = (word *)create_network_game_message(_message_client_randomizer_capability, &capable, sizeof(capable));
	if (message)
		network_game_client_write(connection, message, GET_MESSAGE_SIZE(*message), NULL, 1);
}

int ae_ce_host_piece(unsigned long total_size, unsigned long offset, unsigned long length, void const *data, void *record,
	unsigned long record_size)
{
#ifdef HALO_PORT_CONSOLE_LIMITS
	(void)total_size; (void)offset; (void)length; (void)data; (void)record; (void)record_size;
	return AE_CE_HOST_PIECE_NOT_CE;
#else
	unsigned long values[AE_CE_HOST_VALUES];
	unsigned char widths[AE_CE_HOST_VALUES];
	int result;

	/* (only CE's long record, from a host that is CE's or whose kind is not known; this game's own size, and every
	other, goes the old way) */
	if (total_size != ae_ce_host_record_size(&record_layout) || host_state == AE_CE_HOST_STATE_NOT_CE || !data || !record ||
		record_size != sizeof(struct network_game) || length > 0xE00 || offset + length > total_size)
		return AE_CE_HOST_PIECE_NOT_CE;
	/* (a host whose record was refused, and told: the rest of its record goes unread, and unsaid) */
	if (host_state == AE_CE_HOST_STATE_REFUSED)
		return AE_CE_HOST_PIECE_MORE;
	if (offset == 0)
		ce_record_taken = 0;
	if (offset != ce_record_taken)
	{
		network_event("got a Content Evolved game settings piece out of order; waiting for the next record");
		ce_record_taken = 0;
		return AE_CE_HOST_PIECE_MORE;
	}
	memcpy(ce_record + offset, data, length);
	ce_record_taken += length;
	if (ce_record_taken != total_size)
		return AE_CE_HOST_PIECE_MORE;
	ce_record_taken = 0;
	/* (this game's Firefight options come over its own table: the widths must be CE's) */
	if (ae_firefight_net_widths(widths, AE_CE_HOST_VALUES) != AE_CE_HOST_VALUES ||
		memcmp(widths, ae_ce_host_value_widths, sizeof(widths)))
	{
		tell_player_or_log(TRUE, "Firefight's options are not the ones this game's table reads",
			"This game cannot read this Content Evolved game's Firefight options.");
		state_event(AE_CE_HOST_EVENT_RECORD_REFUSED);
		return AE_CE_HOST_PIECE_REFUSED;
	}
	result = ae_ce_host_record_convert(ce_record, total_size, (unsigned char *)record, &record_layout, values);
	if (result == AE_CE_HOST_RECORD_TAG_PROFILE)
	{
		tell_player_or_log(TRUE, "its Firefight plays a Tag Injector profile",
			"This Content Evolved Firefight plays a Tag Injector profile whose tags the host shares, which this game cannot "
			"download yet.\n\nJoin a game without a shared profile.");
		state_event(AE_CE_HOST_EVENT_RECORD_REFUSED);
		return AE_CE_HOST_PIECE_REFUSED;
	}
	if (result != AE_CE_HOST_RECORD_OK)
	{
		tell_player_or_log(TRUE, "its game is not a Firefight game (a co-op or another game of its own)",
			"This game can join a Content Evolved host's Firefight games only, not its co-op games.");
		state_event(AE_CE_HOST_EVENT_RECORD_REFUSED);
		return AE_CE_HOST_PIECE_REFUSED;
	}
	/* (the options wait for this game to accept the record: ae_ce_host_record_result) */
	memcpy(pending_values, values, sizeof(pending_values));
	pending_values_ready = 1;
	return AE_CE_HOST_PIECE_DONE;
#endif
}

void ae_ce_host_tags_differ(long count, unsigned long hash, long host_count, unsigned long host_hash)
{
	static char const message[] = AE_CE_HOST_TAGS_MESSAGE;
	wchar_t text[sizeof(message)];
	long index;

	/* (network.leave_on_tag_mismatch off: the warning lines only, the game goes on) */
	if (!config_boolean("network.leave_on_tag_mismatch") ||
		!ae_ce_host_tags_leave(count, hash, host_count, host_hash, network_replay_playing() != 0))
		return;
	network_event("left the game: this level's shared tags (%ld, %08lx) are not the host's (%ld, %08lx)", count, hash,
		host_count, host_hash);
	for (index = 0; message[index] && index < (long)(sizeof(text) / sizeof(text[0])) - 1; index++)
		text[index] = (wchar_t)(unsigned char)message[index];
	text[index] = 0;
	display_error_text_when_main_menu_loaded(text);
	network_game_abort();
}
