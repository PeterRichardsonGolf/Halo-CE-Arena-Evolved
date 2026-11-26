/* The post-game report's counting on the host (see the header). */
#include <string.h>
#include "cseries.h"
#include "memory/data.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"

#include "ae_pgcr_stats.h"
#include "ae_pgcr_data.h"
#include "ae_glue_pgcr.h"
#include "network_distributed.h"

void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

/* message 107: one player's summary, from the host to every client, reliably (the bytes ae_pgcr_encode makes, in a block
of the longest size; a build that does not know the kind drops it) */
struct ae_pgcr_message
{
	struct distributed_message_header header;
	unsigned char length;
	unsigned char reserved;
	unsigned char data[AE_PGCR_WIRE_MAXIMUM];
};

typedef char ae_pgcr_message_even[sizeof(struct ae_pgcr_message) % 2 == 0 ? 1 : -1];

/* (rc2) the block of the versions before the wire's version 3 (a build before it takes this size alone): the host sends it first
for each player (a version 2 form of the summary), then the version 3 block, which a build that knows it keeps in its place; a
build before it drops the second for its size. A client takes either size; a version 2 block never replaces a version 3 one */
struct ae_pgcr_message_v2
{
	struct distributed_message_header header;
	unsigned char length;
	unsigned char reserved;
	unsigned char data[AE_PGCR_V2_MAXIMUM];
};

static struct ae_pgcr_game counts;
static boolean ended;

/* the slot's player, NULL for none */
static struct player_datum *player_of(int slot)
{
	return slot >= 0 && slot < AE_PGCR_PLAYERS ? distributed_player((short)slot) : NULL;
}

/* a game is being played: the screen's kept report (the lobby reopens it) goes. A map loading is not that (the lobby's own scene
loads one on the way back) */
static void game_goes_on(void)
{
	if (!ended && ae_glue_pgcr_kept())
	{
		platform_log("pgcr: the next game is under way: the kept report goes");
		ae_glue_pgcr_forget();
	}
}

void ae_pgcr_stats_new_map(void)
{
	ae_pgcr_reset(&counts);
	ae_pgcr_data_reset();
	ended = FALSE;
}

void ae_pgcr_stats_shot(int slot)
{
	struct player_datum *player = player_of(slot);

	game_goes_on();
	if (!player)
		return;
	player->statistics.shots_fired = ae_pgcr_shot(&counts, slot);
}

void ae_pgcr_stats_damage(int attacker, int victim, int category)
{
	struct player_datum *player = player_of(attacker);

	game_goes_on();
	if (player && ae_pgcr_hit(&counts, attacker, victim, category, game_time_get()))
		player->statistics.shots_hit = counts.shots_hit[attacker];
}

void ae_pgcr_stats_damage_share(int attacker, int victim, float share, int friendly)
{
	game_goes_on();
	ae_pgcr_damage(&counts, attacker, victim, share, friendly);
}

void ae_pgcr_stats_kill_kinds(struct ae_medal_kill const *kill)
{
	game_goes_on();
	ae_pgcr_kill_kinds(&counts, kill);
}

void ae_pgcr_stats_carrier_tick(int slot)
{
	ae_pgcr_carrier_tick(&counts, slot);
}

void ae_pgcr_stats_bomb(long player_index, int what)
{
	long slot;

	if (player_index == NONE || game_connection() == _game_connection_network_client)
		return;
	slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	game_goes_on();
	ae_pgcr_bomb(&counts, (int)slot, what);
}

void ae_pgcr_stats_medal(int slot, int medal)
{
	game_goes_on();
	ae_pgcr_medal(&counts, slot, medal);
}

void ae_pgcr_stats_death(int victim)
{
	game_goes_on();
	ae_pgcr_death(&counts, victim);
}

void ae_pgcr_stats_kill(int killer, int victim, int headshot, long damage_tag)
{
	game_goes_on();
	ae_pgcr_kill(&counts, killer, victim, headshot, damage_tag, game_time_get());
}

/* a network host: the summary to every client */
static void send_summary(struct ae_pgcr_summary const *summary)
{
	struct ae_pgcr_message message;
	struct ae_pgcr_message_v2 older;
	unsigned char bytes[AE_PGCR_WIRE_MAXIMUM];
	int length;

	if (game_connection() != _game_connection_network_server)
		return;
	/* (the version 2 block first, for the builds that know no version 3) */
	memset(&older, 0, sizeof(older));
	length = ae_pgcr_encode_version(summary, bytes, sizeof(bytes), 2);
	if (length > 0 && length <= AE_PGCR_V2_MAXIMUM)
	{
		memcpy(older.data, bytes, (size_t)length);
		older.length = (unsigned char)length;
		distributed_send(&older, _distributed_message_ae_pgcr, 1, sizeof(older), _distributed_to_clients_reliably);
	}
	memset(&message, 0, sizeof(message));
	length = ae_pgcr_encode(summary, message.data, sizeof(message.data));
	if (length <= 0)
		return;
	message.length = (unsigned char)length;
	distributed_send(&message, _distributed_message_ae_pgcr, 1, sizeof(message), _distributed_to_clients_reliably);
}

void ae_pgcr_stats_game_end(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (ended)
		return;
	ended = TRUE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		struct ae_pgcr_summary summary;

		if (slot < 0 || slot >= AE_PGCR_PLAYERS || player->quit_out_of_game)
			continue;
		ae_pgcr_summarize(&counts, (int)slot, &summary);
		platform_log("pgcr: player %ld damage %lu/%lu kinds m%d p%d g%d a%d s%d t%d carrier k%d %ds bomb %d/%d", slot,
			summary.damage_dealt, summary.damage_taken, summary.melee, summary.power, summary.grenade,
			summary.assassination, summary.splatter, summary.stick, summary.kills_as_flag_carrier,
			summary.flag_carrier_seconds, summary.bomb_plants, summary.bomb_defuses);
		platform_log("pgcr: player %ld shots %ld/%ld headshots %d spree %d multikills %d medals %d weapons %d "
			"killed_most %d x%d killed_by %d x%d", slot, (long)player->statistics.shots_hit,
			(long)player->statistics.shots_fired, summary.headshots, summary.best_spree, summary.multikills,
			summary.medal_count, summary.weapon_count, summary.killed_most == AE_PGCR_NO_PLAYER ? -1 : summary.killed_most,
			summary.killed_most_count, summary.killed_by_most == AE_PGCR_NO_PLAYER ? -1 : summary.killed_by_most,
			summary.killed_by_most_count);
		ae_pgcr_data_set(&summary);
		send_summary(&summary);
	}
}

int ae_pgcr_stats_kills(int killer, int victim)
{
	if (killer < 0 || killer >= AE_PGCR_PLAYERS || victim < 0 || victim >= AE_PGCR_PLAYERS)
		return 0;
	return counts.kills[killer][victim];
}

int ae_pgcr_stats_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return 0;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_ae_pgcr;
}

/* (the host's alone, over its stream, whole) */
void ae_pgcr_stats_handle_message(long machine_index, unsigned short const *data, unsigned short size)
{
	struct ae_pgcr_message message;
	struct ae_pgcr_message_v2 older;
	struct ae_pgcr_summary summary;
	struct ae_pgcr_summary const *kept;
	struct distributed_message_header const *header;
	unsigned char const *bytes;
	unsigned char length, limit;

	if (machine_index != NONE || game_connection() != _game_connection_network_client ||
		network_connection_last_read_was_unreliable())
		return;
	if (size == sizeof(message))
	{
		memcpy(&message, data, sizeof(message));
		header = &message.header;
		bytes = message.data;
		length = message.length;
		limit = AE_PGCR_WIRE_MAXIMUM;
	}
	else if (size == sizeof(older))
	{
		memcpy(&older, data, sizeof(older));
		header = &older.header;
		bytes = older.data;
		length = older.length;
		limit = AE_PGCR_V2_MAXIMUM;
	}
	else
		return;
	if (GET_MESSAGE_TYPE(header->header) != 2 || header->type != _distributed_message_ae_pgcr || header->count != 1 ||
		length > limit)
		return;
	if (!ae_pgcr_decode(bytes, length, &summary))
		return;
	/* (a version 2 block, the host's first, never replaces a version 3 one) */
	kept = ae_pgcr_data_get(summary.slot);
	if (kept && kept->extended && !summary.extended)
		return;
	if (ae_pgcr_data_set(&summary))
		platform_log("pgcr: summary for player %d from the host (%d medals, %d weapons%s)", summary.slot,
			summary.medal_count, summary.weapon_count, summary.extended ? "" : ", an older form");
}
