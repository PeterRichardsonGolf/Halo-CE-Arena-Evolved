/*
AE_NET_EXTENSIONS.C

See ae_net_extensions.h. The bytes and their reading are ae_net_extensions_rules.c's; this file keeps each machine's
record and sends.
*/

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "cseries.h"
#include "bungie_net/common/message_header.h"
#include "ae_net_extensions.h"
#include "ae_net_extensions_rules.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
boolean network_distributed_client_send_reliably(void *message, word size);
boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size);
boolean network_game_server_kick_machine_of_player(long machine_index, boolean ban);
void display_error_text_when_main_menu_loaded(wchar_t const *text);
void network_game_abort(void);
void distributed_tell_removed(long machine_index, char const *why);
int ae_silent_machine_marked(long machine_index);
boolean game_in_progress(void);
int ae_firefight_net_rules(unsigned long *values, unsigned char *widths, int maximum);
int ae_firefight_net_widths(unsigned char *widths, int maximum);
void ae_firefight_net_apply(unsigned long const *values, int count);
void ae_firefight_net_clear(void);
void ae_ce_host_left(void);

enum
{
	MACHINES = 16,
	/* how long the host waits for a machine's answer when its game requires a capability */
	ANSWER_MILLISECONDS = 5000,
	/* how long the notice is up before the machine is removed */
	NOTICE_MILLISECONDS = 3000,
	/* the type of a game packet in the transport's header */
	MESSAGE_TYPE_PACKET = 3
};

/* what the host's game requires of each capability of the table (NULL: nothing does) */
struct build_capability
{
	struct ae_netext_capability capability;
	boolean (*required)(void);
};

static struct build_capability const build_table[] =
{
	{ { AE_NETEXT_AE_ARENA_EVOLVED, 1, "You need Arena Evolved to play this game." }, NULL },
};
#define TABLE_COUNT ((long)(sizeof(build_table) / sizeof(build_table[0])))

static struct ae_netext_capability table[TABLE_COUNT];
static boolean table_ready;

static struct
{
	boolean present;
	boolean announced;
	boolean answered;
	boolean removed;
	boolean kicked;
	boolean malformed_logged;
	boolean mark_logged;
	unsigned long announced_required;
	unsigned long announced_time;
	unsigned long removed_time;
	struct ae_netext_peer peer;
	/* the Firefight rules last sent to it, and whether a game was running when they were */
	boolean ff_sent;
	boolean ff_was_running;
	int ff_count;
	unsigned long ff_values[AE_NETEXT_FIREFIGHT_MAXIMUM_RULES];
} host_machines[MACHINES];

static struct
{
	boolean announced;
	boolean left;
	boolean malformed_logged;
	struct ae_netext_peer peer;
} client;

static void table_make(void)
{
	long index;

	if (table_ready)
		return;
	for (index = 0; index < TABLE_COUNT; index++)
		table[index] = build_table[index].capability;
	table_ready = TRUE;
}

static unsigned long required_now(void)
{
	unsigned long required = 0;
	long index;

	for (index = 0; index < TABLE_COUNT && index < 32; index++)
	{
		if (build_table[index].required && build_table[index].required())
			required |= 1UL << index;
	}

	return required;
}

/* a built message to the machine (the host) or the host (NONE) */
static boolean send_bytes(unsigned char const *data, long size, long machine_index)
{
	unsigned char buffer[AE_NETEXT_MAXIMUM_BYTES + 16];
	void *message = create_message(MESSAGE_TYPE_PACKET, (void *)data, (unsigned long)size, buffer, (word)sizeof(buffer));
	word message_size;

	if (!message)
		return FALSE;
	message_size = (word)GET_MESSAGE_SIZE(*(word *)message);

	return machine_index == NONE ?
		network_distributed_client_send_reliably(message, message_size) :
		network_distributed_server_send_to_machine_reliably(machine_index, message, message_size);
}

/* a message of `kind` with the table's capabilities, those of `required` flagged, to the machine (the host) or the
host (NONE) */
static boolean send_message(unsigned char kind, unsigned long required, long machine_index)
{
	unsigned char data[AE_NETEXT_MAXIMUM_BYTES + 1];
	unsigned char buffer[AE_NETEXT_MAXIMUM_BYTES + 16];
	long size;
	void *message;
	word message_size;

	table_make();
	size = ae_netext_build(kind, table, TABLE_COUNT, required, data, (long)sizeof(data));
	if (size < 0)
		return FALSE;
	message = create_message(MESSAGE_TYPE_PACKET, data, (unsigned long)size, buffer, (word)sizeof(buffer));
	if (!message)
		return FALSE;
	message_size = (word)GET_MESSAGE_SIZE(*(word *)message);

	return machine_index == NONE ?
		network_distributed_client_send_reliably(message, message_size) :
		network_distributed_server_send_to_machine_reliably(machine_index, message, message_size);
}

static void names_text(struct ae_netext_peer const *peer, char *text, long size)
{
	long used = 0;
	int index;

	text[0] = 0;
	for (index = 0; index < peer->count && used < size - 1; index++)
	{
		int wrote = snprintf(text + used, (size_t)(size - used), "%s%s=%u", index ? " " : "", peer->names[index],
			(unsigned int)peer->versions[index]);

		if (wrote < 0 || wrote >= (int)(size - used))
			break;
		used += wrote;
	}
}

/* ---------- the host */

void ae_net_extensions_host_machine(long machine_index, boolean remote_joined)
{
	unsigned long required, now;

	if (machine_index < 0 || machine_index >= MACHINES)
		return;
	if (!remote_joined)
	{
		if (host_machines[machine_index].present)
			memset(&host_machines[machine_index], 0, sizeof(host_machines[machine_index]));
		return;
	}
	table_make();
	now = system_milliseconds();
	required = required_now();
	if (!host_machines[machine_index].present)
	{
		memset(&host_machines[machine_index], 0, sizeof(host_machines[machine_index]));
		host_machines[machine_index].present = TRUE;
		host_machines[machine_index].announced_time = now;
	}
	/* (announced when it joined, and again when what the game requires changes: its answer stands, an answer
	listing all it has) */
	if (!host_machines[machine_index].announced || host_machines[machine_index].announced_required != required)
	{
		if (send_message(AE_NETEXT_KIND_ANNOUNCEMENT, required, machine_index))
		{
			if (!host_machines[machine_index].announced)
				platform_log("net extensions: announced %ld capabilities to machine #%ld", (long)TABLE_COUNT, machine_index);
			host_machines[machine_index].announced = TRUE;
			host_machines[machine_index].announced_required = required;
			if (!host_machines[machine_index].answered)
				host_machines[machine_index].announced_time = now;
		}
		return;
	}
	if (host_machines[machine_index].removed)
	{
		if (!host_machines[machine_index].kicked && now - host_machines[machine_index].removed_time >= NOTICE_MILLISECONDS)
		{
			host_machines[machine_index].kicked = TRUE;
			network_game_server_kick_machine_of_player(machine_index, FALSE);
		}
		return;
	}
	if (required)
	{
		long missing = ae_netext_first_missing(&host_machines[machine_index].peer, table, TABLE_COUNT, required);

		if (missing >= 0 && (host_machines[machine_index].answered ||
			now - host_machines[machine_index].announced_time >= ANSWER_MILLISECONDS))
		{
			host_machines[machine_index].removed = TRUE;
			host_machines[machine_index].removed_time = now;
			platform_log("net extensions: machine #%ld %s %s: removed", machine_index,
				host_machines[machine_index].answered ? "has not got" : "did not answer for", table[missing].key);
			/* (the notice every other removal gives) */
			distributed_tell_removed(machine_index, table[missing].missing_notice);
		}
	}
}

void ae_net_extensions_host_firefight(long machine_index, boolean remote_joined, boolean firefight_game)
{
	unsigned long values[AE_NETEXT_FIREFIGHT_MAXIMUM_RULES];
	unsigned char widths[AE_NETEXT_FIREFIGHT_MAXIMUM_RULES];
	unsigned char data[AE_NETEXT_MAXIMUM_BYTES + 1];
	boolean running;
	int count, changed;
	long size;

	if (machine_index < 0 || machine_index >= MACHINES || !remote_joined || !host_machines[machine_index].present)
		return;
	/* (a game just ended: the lobby is back, and the client's copy went with its game: sent again) */
	running = game_in_progress() != 0;
	if (host_machines[machine_index].ff_was_running && !running)
		host_machines[machine_index].ff_sent = FALSE;
	host_machines[machine_index].ff_was_running = running;
	if (!firefight_game)
		return;
	count = ae_firefight_net_rules(values, widths, AE_NETEXT_FIREFIGHT_MAXIMUM_RULES);
	if (count < 1)
		return;
	changed = !host_machines[machine_index].ff_sent || host_machines[machine_index].ff_count != count ||
		memcmp(host_machines[machine_index].ff_values, values, (size_t)count * sizeof(values[0]));
	if (!ae_netext_firefight_send(1, ae_net_extensions_agreed(machine_index, AE_NETEXT_AE_ARENA_EVOLVED, 1), changed))
		return;
	size = ae_netext_firefight_build(values, widths, count, data, (long)sizeof(data));
	if (size < 0 || !send_bytes(data, size, machine_index))
		return;
	host_machines[machine_index].ff_sent = TRUE;
	host_machines[machine_index].ff_count = count;
	memcpy(host_machines[machine_index].ff_values, values, (size_t)count * sizeof(values[0]));
	platform_log("net extensions: sent %d Firefight rules to machine #%ld", count, machine_index);
}

void ae_net_extensions_host_handle(long machine_index, void const *data, long size)
{
	struct ae_netext_peer read;
	int result;
	char text[AE_NET_EXTENSIONS_STATUS_LINE];

	if (machine_index < 0 || machine_index >= MACHINES || !host_machines[machine_index].present || size < 1)
		return;
	table_make();
	result = ae_netext_parse(data, size, AE_NETEXT_KIND_ANSWER, table, TABLE_COUNT, &read);
	if (result == AE_NETEXT_PARSE_MALFORMED)
	{
		if (!host_machines[machine_index].malformed_logged)
			platform_log("net extensions: machine #%ld sent a malformed packet (%ld bytes): dropped", machine_index, size);
		host_machines[machine_index].malformed_logged = TRUE;
		return;
	}
	if (result != AE_NETEXT_PARSE_OK)
		return;
	names_text(&read, text, (long)sizeof(text));
	/* (the first answer is logged, and one that differs from the last; a repeat of the same is not) */
	{
		char before[AE_NET_EXTENSIONS_STATUS_LINE];

		before[0] = 0;
		if (host_machines[machine_index].answered)
			names_text(&host_machines[machine_index].peer, before, (long)sizeof(before));
		if (!host_machines[machine_index].answered || strcmp(before, text))
			platform_log("net extensions: machine #%ld answered with %d capabilities: %s", machine_index, read.count, text);
	}
	host_machines[machine_index].peer = read;
	host_machines[machine_index].answered = TRUE;
	/* (the join's own gate is the AE mark in the machine's name: told twice, the two must agree) */
	if (!host_machines[machine_index].mark_logged)
	{
		host_machines[machine_index].mark_logged = TRUE;
		if ((ae_netext_peer_version(&read, AE_NETEXT_AE_ARENA_EVOLVED) != 0) != (ae_silent_machine_marked(machine_index) != 0))
			platform_log("net extensions: machine #%ld: the AE mark in its name and %s disagree (mark %d)", machine_index,
				AE_NETEXT_AE_ARENA_EVOLVED, ae_silent_machine_marked(machine_index));
	}
}

boolean ae_net_extensions_agreed(long machine_index, char const *key, unsigned short version)
{
	unsigned short told;

	if (machine_index < 0 || machine_index >= MACHINES || !host_machines[machine_index].answered)
		return FALSE;
	told = ae_netext_peer_version(&host_machines[machine_index].peer, key);

	return told != 0 && told >= version;
}

/* ---------- the client */

void ae_net_extensions_client_reset(void)
{
	memset(&client, 0, sizeof(client));
	ae_firefight_net_clear();
	ae_ce_host_left();
}

/* the host's game requires a capability this build has not: the player told why, and the game left */
static void client_cannot_play(struct ae_netext_peer const *read)
{
	char message[AE_NETEXT_MAXIMUM_TEXT + AE_NETEXT_MAXIMUM_KEY + 96];
	wchar_t text[sizeof(message)];
	long index;

	if (client.left)
		return;
	client.left = TRUE;
	if (read->missing_text[0])
		snprintf(message, sizeof(message), "%s", read->missing_text);
	else
		snprintf(message, sizeof(message), "This game needs a capability your game does not have (%s).", read->missing_key);
	platform_log("net extensions: the host's game requires %s, which this build has not: left", read->missing_key);
	for (index = 0; message[index] && index < (long)(sizeof(text) / sizeof(text[0])) - 1; index++)
		text[index] = (wchar_t)(unsigned char)message[index];
	text[index] = 0;
	display_error_text_when_main_menu_loaded(text);
	network_game_abort();
}

void ae_net_extensions_client_handle(void const *data, long size)
{
	struct ae_netext_peer read;
	int result;
	char text[AE_NET_EXTENSIONS_STATUS_LINE];

	if (size < 1)
		return;
	/* (the host's Firefight rules: kept for this game; a message that does not fit this build's rules is dropped, its
	game on this machine's own) */
	{
		unsigned long values[AE_NETEXT_FIREFIGHT_MAXIMUM_RULES];
		unsigned char widths[AE_NETEXT_FIREFIGHT_MAXIMUM_RULES];
		int count = ae_firefight_net_widths(widths, AE_NETEXT_FIREFIGHT_MAXIMUM_RULES);
		int rules = count > 0 ? ae_netext_firefight_parse(data, size, widths, count, values) : AE_NETEXT_PARSE_OTHER;

		if (rules == AE_NETEXT_PARSE_OK)
		{
			ae_firefight_net_apply(values, count);
			platform_log("net extensions: the host's %d Firefight rules are in use for this game", count);
			return;
		}
		if (rules == AE_NETEXT_PARSE_MALFORMED)
		{
			platform_log("net extensions: the host's Firefight rules did not fit (%ld bytes): dropped", size);
			return;
		}
	}
	table_make();
	result = ae_netext_parse(data, size, AE_NETEXT_KIND_ANNOUNCEMENT, table, TABLE_COUNT, &read);
	if (result == AE_NETEXT_PARSE_MALFORMED)
	{
		if (!client.malformed_logged)
			platform_log("net extensions: the host sent a malformed packet (%ld bytes): dropped", size);
		client.malformed_logged = TRUE;
		return;
	}
	if (result != AE_NETEXT_PARSE_OK)
		return;
	client.peer = read;
	client.announced = TRUE;
	names_text(&read, text, (long)sizeof(text));
	platform_log("net extensions: the host announced %d capabilities: %s", read.count, text);
	send_message(AE_NETEXT_KIND_ANSWER, 0, NONE);
	if (read.missing_required)
		client_cannot_play(&read);
}

boolean ae_net_extensions_host_has(char const *key, unsigned short version)
{
	unsigned short told;

	if (!client.announced)
		return FALSE;
	told = ae_netext_peer_version(&client.peer, key);

	return told != 0 && told >= version;
}

/* ---------- status */

long ae_net_extensions_status(char (*lines)[AE_NET_EXTENSIONS_STATUS_LINE], long maximum)
{
	long count = 0, index;
	char text[AE_NET_EXTENSIONS_STATUS_LINE];

	table_make();
	if (maximum < 1)
		return 0;
	text[0] = 0;
	for (index = 0; index < TABLE_COUNT; index++)
	{
		size_t used = strlen(text);

		snprintf(text + used, sizeof(text) - used, "%s%s=%u", index ? " " : "", table[index].key,
			(unsigned int)table[index].version);
	}
	snprintf(lines[count++], AE_NET_EXTENSIONS_STATUS_LINE, "netext: this machine: %s", text);
	if (count < maximum && client.announced)
	{
		names_text(&client.peer, text, (long)sizeof(text));
		snprintf(lines[count++], AE_NET_EXTENSIONS_STATUS_LINE, "netext: the host told: %s", text);
	}
	for (index = 0; index < MACHINES && count < maximum; index++)
	{
		if (!host_machines[index].present)
			continue;
		if (host_machines[index].answered)
			names_text(&host_machines[index].peer, text, (long)sizeof(text));
		else
			snprintf(text, sizeof(text), "(no answer yet)");
		snprintf(lines[count++], AE_NET_EXTENSIONS_STATUS_LINE, "netext: machine #%ld told: %s", index, text);
	}

	return count;
}
