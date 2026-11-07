/*
AE_SERVER_PLAYLIST.C

See ae_server_playlist.h; the rules are ae_playlist_rules.c's.
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "networking/network_server_manager_internal.h"
#include "saved games/arena_gametypes.h"
#include "ae_joiner_notice.h"
#include "ae_lan_bots.h"
#include "ae_map_check.h"
#include "ae_map_share.h"
#include "ae_playlist_rules.h"
#include "ae_server_playlist.h"
#include "ae_silent_rules.h"
#include "server_director.h"
#include "../src/port_config.h"
#include "../src/server_log.h"
#include "../src/server_mode.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
void ae_chat_host_notice(char const *text);
void ae_chat_host_notice_to(long machine_index, char const *text);
short network_distributed_server_machines(long *machine_indices, short maximum);

static struct ae_pl_history history;
static struct ae_pl_people nominations, rock_asks, extend_asks;
static int extension_used;
static long extension_ticks;
static int replan_asked;
static unsigned char pending_machine[HALO_PORT_MAXIMUM_NETWORK_MACHINES];

/* the map check (messages 111 and 112): the maps the playlist asks machines about, and what each machine told */
enum { MACHINES = HALO_PORT_MAXIMUM_NETWORK_MACHINES, DEFER_MILLISECONDS = 10000, REPLAN_GAP_MILLISECONDS = 10000 };
static struct ae_pl_map_table check_table;
static unsigned check_serial;
static struct ae_pl_holding holdings[MACHINES];
static int asked_serial[MACHINES];
static int answered_serial[MACHINES];
/* (the last time an answer made the lobby choose again: once in REPLAN_GAP_MILLISECONDS at most) */
static unsigned long answer_replan_at;
static unsigned long deferred_at[MACHINES];
static char deferred_text[MACHINES][SERVER_ENTRY_TEXT_SIZE];
static unsigned long check_tick_at;

static int icase_contains(
	char const *text,
	char const *piece)
{
	size_t length = strlen(piece), at;

	for (; *text; text++)
	{
		for (at = 0; at < length && text[at]; at++)
			if ((text[at] | 0x20) != (piece[at] | 0x20))
				break;
		if (at == length)
			return 1;
	}
	return 0;
}

int ae_server_playlist_audience(
	struct server_entry const *entry)
{
	int needs_mcc = icase_contains(entry->level, "@mcc") || icase_contains(entry->level, "@ws");
	int needs_ae = 0;

	if (strncmp(entry->gametype, "file=", 5) && strcmp(entry->gametype, "coop"))
	{
		wchar_t wide[64];
		struct game_variant game;
		struct game_variant_options options;
		long length;

		for (length = 0; entry->gametype[length] && length < (long)NUMBEROF(wide) - 1; length++)
			wide[length] = (wchar_t)(unsigned char)(entry->gametype[length] == '_' ? ' ' : entry->gametype[length]);
		wide[length] = 0;
		csmemset(&game, 0, sizeof(game));
		needs_ae = arena_gametype_any_build(wide, &game, &options) ? 1 : 0;
	}
	return ae_pl_audience(needs_ae || needs_mcc, needs_mcc);
}

/* the entry's slot in the map check's table (-1: it needs no check) */
static int entry_slot(
	struct server_entry const *entry)
{
	if (ae_server_playlist_audience(entry) != AE_AUDIENCE_AE_MCC)
		return -1;
	return ae_pl_map_table_find(&check_table, entry->level);
}

/* whether the machine can play the entry: Arena Evolved, and for a map of MCC's what it told (nothing told: yes) */
static int machine_can_play(
	long machine_index,
	struct server_entry const *entry)
{
	int audience = ae_server_playlist_audience(entry);
	int state = -1;

	if (audience == AE_AUDIENCE_AE_MCC && machine_index >= 0 && machine_index < MACHINES)
		state = ae_pl_holding_state(&holdings[machine_index], entry_slot(entry));
	return ae_pl_can_play(audience, ae_peer_is_ae(machine_index) ? 1 : 0, state);
}

/* the people in the game (not bots): their machines, and whether one is not Arena Evolved */
static int humans_present(
	long *machines,
	int maximum,
	int *non_ae)
{
	struct network_game_server *server = global_network_game_server_get();
	struct network_game *game = server ? network_game_server_get_game(server) : NULL;
	int count = 0;
	long slot;

	*non_ae = 0;
	for (slot = 0; game && slot < (long)NUMBEROF(game->players); slot++)
	{
		struct network_player const *player = &game->players[slot];

		if (!network_player_is_valid(player) || ae_lan_bots_network_player(player))
			continue;
		if (count < maximum)
			machines[count++] = player->machine_index;
		if (!ae_peer_is_ae(player->machine_index))
			*non_ae = 1;
	}
	return count;
}

/* whether someone in the game (or about to join the lobby) cannot play the entry */
static int entry_blocked(
	struct server_entry const *entry)
{
	long machines[AE_PL_PEOPLE];
	int non_ae, count, index;

	count = humans_present(machines, AE_PL_PEOPLE, &non_ae);
	{
		/* (machines let in while the lobby chooses again for them: not players yet) */
		long pending;

		for (pending = 0; pending < MACHINES; pending++)
			if (pending_machine[pending] && !machine_can_play(pending, entry))
				return 1;
	}
	for (index = 0; index < count; index++)
		if (!machine_can_play(machines[index], entry))
			return 1;
	return 0;
}

void ae_server_playlist_game_started(
	struct server_entry const *entry)
{
	if (!halo_server_mode())
		return;
	ae_pl_history_push(&history, entry->text);
	ae_pl_people_reset(&nominations);
	ae_pl_people_reset(&rock_asks);
	ae_pl_people_reset(&extend_asks);
	extension_used = 0;
	extension_ticks = 0;
	replan_asked = 0;
	memset(pending_machine, 0, sizeof(pending_machine));
	memset(deferred_at, 0, sizeof(deferred_at));
}

long ae_server_playlist_vote_candidates(
	struct server_rotation const *rotation,
	long *candidates,
	long count)
{
	long kept[SERVER_ROTATION_MAXIMUM_ENTRIES];
	long index, left = 0;
	int depth = (int)config_integer("server.vote_no_repeat");

	if (!halo_server_mode())
		return count;
	for (index = 0; index < count; index++)
	{
		struct server_entry const *entry = &rotation->entries[candidates[index]];

		if (depth > 0 && ae_pl_history_recent(&history, entry->text, depth))
			continue;
		if (entry_blocked(entry))
			continue;
		kept[left++] = candidates[index];
	}
	if (left < 2)
	{
		/* (too few left: the repeat rule gives way before the audience does) */
		left = 0;
		for (index = 0; index < count; index++)
		{
			if (entry_blocked(&rotation->entries[candidates[index]]))
				continue;
			kept[left++] = candidates[index];
		}
		if (left < 2)
			return count;
	}
	if (left < count)
		server_log(_server_log_info, "lobby", "map vote: %ld of %ld entries left out (played in the last %d games or not "
			"for everyone here)", count - left, count, depth);
	memcpy(candidates, kept, (size_t)left * sizeof(long));
	return left;
}

static unsigned long vote_state;

void ae_server_playlist_vote_order(
	struct server_rotation const *rotation,
	long *candidates,
	long count,
	long wanted)
{
	int weights[SERVER_ROTATION_MAXIMUM_ENTRIES];
	unsigned char eligible[SERVER_ROTATION_MAXIMUM_ENTRIES];
	int picked[SERVER_ROTATION_MAXIMUM_ENTRIES];
	int nominated[AE_PL_PEOPLE];
	long order[SERVER_ROTATION_MAXIMUM_ENTRIES];
	unsigned char used[SERVER_ROTATION_MAXIMUM_ENTRIES] = { 0 };
	long placed = 0, index;
	int nominated_count, at, drawn;

	if (!halo_server_mode() || count < 1 || count > SERVER_ROTATION_MAXIMUM_ENTRIES)
		return;
	if (!vote_state)
		vote_state = (unsigned long)system_milliseconds() ^ ((unsigned long)rand() << 8) ^ 0x2545F491UL;
	/* (the nominated entries first: a candidate with the same text as the nominated entry) */
	nominated_count = ae_pl_nominated(&nominations, nominated);
	for (at = 0; at < nominated_count && placed < (wanted > 1 ? wanted - 1 : 1); at++)
	{
		if (nominated[at] < 0 || nominated[at] >= rotation->count)
			continue;
		for (index = 0; index < count; index++)
			if (!used[index] && !strcmp(rotation->entries[candidates[index]].text, rotation->entries[nominated[at]].text))
			{
				used[index] = 1;
				order[placed++] = candidates[index];
				server_log(_server_log_info, "lobby", "map vote: %s on %s is a choice (nominated)",
					rotation->entries[candidates[index]].gametype, rotation->entries[candidates[index]].map_name);
				break;
			}
	}
	for (index = 0; index < count; index++)
	{
		weights[index] = ae_pl_weight(rotation->entries[candidates[index]].weight);
		eligible[index] = used[index] ? 0 : 1;
	}
	drawn = ae_pl_sample(weights, eligible, (int)count, (int)(wanted - placed), &vote_state, picked);
	for (at = 0; at < drawn; at++)
	{
		used[picked[at]] = 1;
		order[placed++] = candidates[picked[at]];
	}
	for (index = 0; index < count; index++)
		if (!used[index])
			order[placed++] = candidates[index];
	memcpy(candidates, order, (size_t)count * sizeof(long));
	ae_pl_people_reset(&nominations);
}

static void tell(
	long machine_index,
	char const *format,
	...) __attribute__((format(printf, 2, 3)));

static void tell(
	long machine_index,
	char const *format,
	...)
{
	char text[160];
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(text, sizeof(text), format, arguments);
	va_end(arguments);
	if (machine_index < 0)
		ae_chat_host_notice(text);
	else
		ae_chat_host_notice_to(machine_index, text);
}

static int word_is(
	char const *text,
	char const *word,
	char const **rest)
{
	size_t length = strlen(word);
	size_t at;

	for (at = 0; at < length; at++)
		if (!text[at] || (text[at] | 0x20) != word[at])
			return 0;
	if (text[length] && text[length] != ' ')
		return 0;
	*rest = text + length;
	while (**rest == ' ')
		(*rest)++;
	return 1;
}

static void command_nominate(
	long machine_index,
	char const *player_name,
	char const *query)
{
	struct server_rotation const *rotation = server_director_rotation();
	char const *names[SERVER_ROTATION_MAXIMUM_ENTRIES];
	char const *texts[SERVER_ROTATION_MAXIMUM_ENTRIES];
	int map[SERVER_ROTATION_MAXIMUM_ENTRIES];
	int count = 0, found, depth = (int)config_integer("server.vote_no_repeat");
	long index;

	if (!config_boolean("server.vote_nominate") || !config_boolean("server.map_vote"))
	{
		tell(machine_index, "Nominations are off on this server.");
		return;
	}
	if (!query[0])
	{
		tell(machine_index, "Say !nominate and a map's name to put it in the next vote.");
		return;
	}
	for (index = 0; rotation && index < rotation->count; index++)
	{
		if (rotation->entries[index].rejected[0])
			continue;
		names[count] = rotation->entries[index].map_name;
		texts[count] = rotation->entries[index].text;
		map[count++] = (int)index;
	}
	found = ae_pl_match_entry(names, texts, count, query);
	if (found == -1)
	{
		tell(machine_index, "No map here is called \"%.40s\".", query);
		return;
	}
	if (found == -2)
	{
		tell(machine_index, "More than one map matches \"%.40s\": say more of its name.", query);
		return;
	}
	if (depth > 0 && ae_pl_history_recent(&history, texts[found], depth))
	{
		tell(machine_index, "%s was played too recently to be in the next vote.", names[found]);
		return;
	}
	if (entry_blocked(&rotation->entries[map[found]]))
	{
		tell(machine_index, "%s needs Arena Evolved (or MCC's files) and not everyone here has it.", names[found]);
		return;
	}
	if (!ae_pl_nominate(&nominations, machine_index, map[found]))
	{
		tell(machine_index, "The nominations are full.");
		return;
	}
	tell(-1, "%.20s nominated %s (%s) for the next vote.", player_name, names[found], rotation->entries[map[found]].gametype);
}

static void command_rtv(
	long machine_index)
{
	long machines[AE_PL_PEOPLE];
	int non_ae, humans, asks;

	if (!config_boolean("server.vote_rtv"))
	{
		tell(machine_index, "Rock the vote is off on this server.");
		return;
	}
	if (!server_director_playing())
	{
		tell(machine_index, "Rock the vote works while a game is being played.");
		return;
	}
	humans = humans_present(machines, AE_PL_PEOPLE, &non_ae);
	asks = ae_pl_rtv_ask(&rock_asks, machine_index);
	if (asks < 0)
		return;
	if (ae_pl_rtv_passed(&rock_asks, machines, humans))
	{
		tell(-1, "Enough players want to move on: the game ends now.");
		server_log(_server_log_info, "game", "rock the vote: %d of %d people asked: the game ends", asks, humans);
		ae_pl_people_reset(&rock_asks);
		server_director_end_game(_server_end_command, FALSE);
		return;
	}
	tell(-1, "%d of %d people want to move on (%d needed): say !rtv.", asks, humans, ae_pl_rtv_needed(humans));
}

static void command_extend(
	long machine_index)
{
	long machines[AE_PL_PEOPLE];
	int non_ae, humans, asks, minutes = (int)ae_pl_extend_minutes((int)config_integer("server.vote_extend_minutes"));

	if (config_integer("server.vote_extend_minutes") <= 0)
	{
		tell(machine_index, "Extending the game is off on this server.");
		return;
	}
	if (!server_director_playing() || game_variant_options_get()->time_limit <= 0)
	{
		tell(machine_index, "Extending works in a timed game that is being played.");
		return;
	}
	if (!ae_pl_extend_allowed(extension_used))
	{
		tell(machine_index, "This game was extended once already.");
		return;
	}
	humans = humans_present(machines, AE_PL_PEOPLE, &non_ae);
	asks = ae_pl_rtv_ask(&extend_asks, machine_index);
	if (asks < 0)
		return;
	if (ae_pl_rtv_passed(&extend_asks, machines, humans))
	{
		extension_used = 1;
		extension_ticks += (long)minutes * 60L * TICKS_PER_SECOND;
		ae_pl_people_reset(&extend_asks);
		tell(-1, "The game is extended by %d minute%s.", minutes, minutes == 1 ? "" : "s");
		server_log(_server_log_info, "game", "extended by %d minute%s (%d of %d people asked)", minutes,
			minutes == 1 ? "" : "s", asks, humans);
		return;
	}
	tell(-1, "%d of %d people want %d more minute%s (%d needed): say !extend.", asks, humans, minutes,
		minutes == 1 ? "" : "s", ae_pl_rtv_needed(humans));
}

int ae_server_playlist_chat(
	long machine_index,
	char const *player_name,
	char const *text)
{
	char const *rest;

	if (!halo_server_mode() || !text || text[0] != '!')
		return 0;
	if (word_is(text + 1, "nominate", &rest))
		command_nominate(machine_index, player_name, rest);
	else if (word_is(text + 1, "rtv", &rest))
		command_rtv(machine_index);
	else if (word_is(text + 1, "extend", &rest))
		command_extend(machine_index);
	else
		return 0;
	return 1;
}

long ae_server_playlist_time_extra(void)
{
	return extension_ticks;
}

int ae_server_playlist_blocks(
	struct server_entry const *entry)
{
	if (!halo_server_mode())
		return 0;
	return entry_blocked(entry);
}

int ae_server_playlist_join_refused(
	long machine_index,
	short *code)
{
	struct server_entry const *entry;
	struct server_rotation const *rotation;
	int audience;

	if (!halo_server_mode())
		return 0;
	/* (a machine at this index is new: what the last one told is forgotten) */
	if (machine_index >= 0 && machine_index < MACHINES)
	{
		ae_pl_holding_reset(&holdings[machine_index]);
		asked_serial[machine_index] = -1;
		answered_serial[machine_index] = -1;
		deferred_at[machine_index] = 0;
	}
	entry = server_director_current();
	rotation = server_director_rotation();
	if (!entry || !rotation)
		return 0;
	audience = ae_server_playlist_audience(entry);
	/* (0.7.7: its join request's MCC mark, when it told one: has MCC's files or not, before any map bits) */
	if (machine_index >= 0 && machine_index < MACHINES && ae_peer_is_ae(machine_index) &&
		ae_silent_machine_mcc(machine_index) >= 0)
	{
		ae_pl_holding_set_join(&holdings[machine_index], ae_silent_machine_mcc(machine_index));
		platform_log("map check: machine #%d's join request tells it %s MCC's files", (int)machine_index,
			holdings[machine_index].has_mcc ? "has" : "has no");
	}
	if (machine_can_play(machine_index, entry))
		return 0;
	/* (the lobby, its game not started: another entry that machine can play, and the lobby chooses again) */
	if (server_director_entry_can_change())
	{
		long index;

		for (index = 0; index < rotation->count; index++)
			if (!rotation->entries[index].rejected[0] && machine_can_play(machine_index, &rotation->entries[index]))
			{
				if (machine_index >= 0 && machine_index < MACHINES)
					pending_machine[machine_index] = 1;
				replan_asked = 1;
				if (machine_index >= 0 && machine_index < MACHINES)
				{
					deferred_at[machine_index] = system_milliseconds() | 1;
					snprintf(deferred_text[machine_index], sizeof(deferred_text[machine_index]), "%s", entry->text);
				}
				platform_log("playlist: machine #%d cannot play %s (%s): the lobby chooses another entry",
					(int)machine_index, entry->text, ae_pl_audience_name(audience));
				/* (now: the machine's first game settings are the new entry's, so a map it has not never reaches it) */
				server_director_replan_now();
				entry = server_director_current();
				if (entry && machine_index >= 0 && machine_index < MACHINES)
				{
					deferred_at[machine_index] = system_milliseconds() | 1;
					snprintf(deferred_text[machine_index], sizeof(deferred_text[machine_index]), "%s", entry->text);
					platform_log("playlist: the lobby now plays %s", entry->text);
				}
				return 0;
			}
	}
	platform_log("playlist: refusing machine #%d: %s needs %s", (int)machine_index, entry->text,
		ae_pl_audience_name(audience));
	ae_joiner_notice_playlist_refusal(audience);
	if (code)
		*code = ae_peer_is_ae(machine_index) && audience == AE_AUDIENCE_AE_MCC ? AE_MCC_REJECTION_CODE :
			AE_PLAYLIST_REJECTION_CODE;
	return 1;
}

int ae_server_playlist_replan_take(
	int can_change)
{
	if (!replan_asked || !can_change)
		return 0;
	replan_asked = 0;
	return 1;
}

int ae_server_playlist_join_deferred(
	long machine_index)
{
	struct server_entry const *entry;

	if (!halo_server_mode() || machine_index < 0 || machine_index >= MACHINES || !deferred_at[machine_index])
		return 0;
	/* (until the lobby's new entry is applied, or ten seconds; an entry chosen again to the same one gives no more time) */
	entry = server_director_current();
	if (!entry || strcmp(entry->text, deferred_text[machine_index]) ||
		system_milliseconds() - deferred_at[machine_index] > DEFER_MILLISECONDS)
	{
		deferred_at[machine_index] = 0;
		return 0;
	}
	return 1;
}

/* ---------- the map check (messages 111 and 112) */

static void check_table_rebuild(void)
{
	struct server_rotation const *rotation = server_director_rotation();
	struct ae_pl_map_table table;
	long index;

	memset(&table, 0, sizeof(table));
	for (index = 0; rotation && index < rotation->count; index++)
	{
		struct server_entry const *entry = &rotation->entries[index];

		if (!entry->rejected[0] && ae_server_playlist_audience(entry) == AE_AUDIENCE_AE_MCC)
			ae_pl_map_table_slot(&table, entry->level);
	}
	if (ae_pl_map_table_same(&table, &check_table))
		return;
	check_table = table;
	check_serial++;
	{
		int machine;

		/* (what a join request told stays; the map bits are asked again) */
		for (machine = 0; machine < MACHINES; machine++)
		{
			holdings[machine].bits_told = 0;
			memset(holdings[machine].bits, 0, sizeof(holdings[machine].bits));
		}

		for (machine = 0; machine < MACHINES; machine++)
		{
			asked_serial[machine] = -1;
			answered_serial[machine] = -1;
		}
	}
	platform_log("map check: the playlist asks about %d MCC maps (list %u)", check_table.count, check_serial & 0xFFFF);
}

void ae_server_playlist_tick(void)
{
	long machines[MACHINES];
	unsigned char present[MACHINES] = { 0 };
	unsigned long now;
	short count, index;
	int machine;
	struct network_game_client *local_client = global_network_game_client_get();
	long local_machine = local_client ? network_game_client_get_machine_index(local_client) : NONE;

	if (!halo_server_mode())
		return;
	now = system_milliseconds();
	if (check_tick_at && now - check_tick_at < 1000)
		return;
	check_tick_at = now ? now : 1;
	check_table_rebuild();
	count = network_distributed_server_machines(machines, MACHINES);
	for (index = 0; index < count; index++)
	{
		long machine_index = machines[index];

		if (machine_index < 0 || machine_index >= MACHINES || machine_index == local_machine)
			continue;
		present[machine_index] = 1;
		if (check_table.count > 0 && ae_peer_is_ae(machine_index) && asked_serial[machine_index] != (int)(check_serial & 0xFFFF))
		{
			asked_serial[machine_index] = (int)(check_serial & 0xFFFF);
			ae_map_check_ask(machine_index, &check_table, check_serial & 0xFFFF);
			platform_log("map check: asked machine #%d about %d MCC maps", (int)machine_index, check_table.count);
		}
	}
	for (machine = 0; machine < MACHINES; machine++)
	{
		/* (a machine the lobby chose again for, gone before it joined (refused, or left) and its time to join over: not
		pending any more, or it would keep every MCC entry out of the votes until the next game) */
		if (!present[machine] && pending_machine[machine] &&
			(!deferred_at[machine] || now - deferred_at[machine] > DEFER_MILLISECONDS))
		{
			pending_machine[machine] = 0;
			deferred_at[machine] = 0;
			platform_log("playlist: machine #%d, which the lobby chose again for, is gone: its wants are forgotten",
				(int)machine);
		}
		if (!present[machine] && !pending_machine[machine] && (holdings[machine].told || asked_serial[machine] != -1))
		{
			ae_pl_holding_reset(&holdings[machine]);
			asked_serial[machine] = -1;
			answered_serial[machine] = -1;
		}
	}
}

void ae_server_playlist_map_answer(
	long machine_index,
	unsigned serial,
	int has_mcc,
	unsigned char const *bits,
	int count)
{
	struct server_entry const *entry;
	int held = 0, slot;

	if (!halo_server_mode() || machine_index < 0 || machine_index >= MACHINES || !ae_peer_is_ae(machine_index))
		return;
	if (serial != (check_serial & 0xFFFF) || count != check_table.count)
	{
		platform_log("map check: machine #%d answered for another list (%u of %d maps; this is %u of %d): ignored",
			(int)machine_index, serial, count, check_serial & 0xFFFF, check_table.count);
		return;
	}
	/* (one answer a machine for each list: a second one for the same list could make the lobby choose again at will) */
	if (answered_serial[machine_index] == (int)serial)
	{
		platform_log("map check: machine #%d answered again for the same list: ignored", (int)machine_index);
		return;
	}
	answered_serial[machine_index] = (int)serial;
	ae_pl_holding_set(&holdings[machine_index], has_mcc, bits, count);
	for (slot = 0; slot < count; slot++)
		held += ae_pl_holding_state(&holdings[machine_index], slot) == 1;
	platform_log("map check: machine #%d tells it %s MCC's files and holds %d of %d maps", (int)machine_index,
		has_mcc ? "has" : "has no", held, count);
	entry = server_director_current();
	if (entry && !machine_can_play(machine_index, entry))
	{
		char text[160];
		int missing = ae_pl_missing(&holdings[machine_index], entry_slot(entry));

		ae_pl_missing_text(missing, entry->level, text, sizeof(text));
		tell(machine_index, "%s", text);
		if (server_director_entry_can_change())
		{
			unsigned long now = system_milliseconds();

			/* (the lobby chooses again for answers once in ten seconds at most) */
			if (answer_replan_at && now - answer_replan_at < REPLAN_GAP_MILLISECONDS)
				platform_log("map check: machine #%d cannot play %s (%s): the lobby chose again %lu ms ago, not again yet",
					(int)machine_index, entry->text, missing == AE_MISSING_MCC ? "no MCC" : "map missing",
					now - answer_replan_at);
			else
			{
				answer_replan_at = now | 1;
				replan_asked = 1;
				platform_log("map check: machine #%d cannot play %s (%s): the lobby chooses another entry",
					(int)machine_index, entry->text, missing == AE_MISSING_MCC ? "no MCC" : "map missing");
			}
		}
		else
			platform_log("map check: machine #%d cannot play %s (%s): the game is running", (int)machine_index,
				entry->text, missing == AE_MISSING_MCC ? "no MCC" : "map missing");
	}
}

int ae_server_playlist_listing(
	char const **now,
	int *range_mask)
{
	struct server_rotation const *rotation;
	struct server_entry const *entry;
	int audiences[SERVER_ROTATION_MAXIMUM_ENTRIES];
	unsigned char playable[SERVER_ROTATION_MAXIMUM_ENTRIES];
	long index;
	int first = -1;

	if (!halo_server_mode())
		return 0;
	rotation = server_director_rotation();
	if (!rotation || rotation->count < 1 || rotation->count > SERVER_ROTATION_MAXIMUM_ENTRIES)
		return 0;
	for (index = 0; index < rotation->count; index++)
	{
		audiences[index] = ae_server_playlist_audience(&rotation->entries[index]);
		playable[index] = rotation->entries[index].rejected[0] ? 0 : 1;
		if (playable[index] && first < 0)
			first = (int)index;
	}
	if (first < 0)
		return 0;
	entry = server_director_current();
	*now = ae_pl_audience_word(entry ? ae_server_playlist_audience(entry) : audiences[first]);
	*range_mask = ae_pl_range_mask(audiences, playable, (int)rotation->count);
	return 1;
}
