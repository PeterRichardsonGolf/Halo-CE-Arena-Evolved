/*
AE_MODERATION.C

The host's moderation (ae_moderation.h).
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "cseries.h"
#include "networking/network_game_protocol.h"
#include "halo_port_limits.h"
#include "ae_moderation.h"
#include "ae_silent_rules.h"
#include "ae_net_flow.h"
#ifdef HALO_PORT_CHAT
#include "chat.h"
#endif

void platform_log(char const *format, ...);
/* (network_chat.c) the host's mutes, by name */
boolean network_chat_host_mute_named(char const *name, short minutes, boolean mute);
boolean network_chat_host_name_muted(wchar_t const *name);
unsigned long system_milliseconds(void);

/* (network_server_manager.c) a client machine of the host's game kicked, or banned (its line in bans.txt) */
boolean network_game_server_kick_machine_of_player(long machine_index, boolean ban);
char const *network_game_server_machine_hardware_id(long machine_index);
unsigned long network_game_server_machine_address(long machine_index);
/* (network_distributed.c) the bans */
boolean network_distributed_ban_entry(long index, char *when, long when_size, char *hardware_id, long hardware_id_size,
	char *players, long players_size, char *reason, long reason_size, unsigned long *until);
boolean network_distributed_unban(long index);
unsigned long distributed_machine_real_address(long machine_index);
void distributed_tell_removed(long machine_index, char const *why);
unsigned long distributed_real_address(unsigned long address);
void network_distributed_ban_details(char const *reason, unsigned long until);
void console_warning(char const *format, ...);

/* ---------- the 2 minutes of a kick: a few machines at a time, the oldest forgotten first */

enum { HELD_MAXIMUM = 16, HARDWARE_ID_SIZE = 40 };
static struct
{
	int used;
	unsigned long address;
	char hardware_id[HARDWARE_ID_SIZE];
	unsigned long until;     /* system_milliseconds */
} held[HELD_MAXIMUM];

static int reached(unsigned long now, unsigned long until)
{
	return (long)(now - until) >= 0;
}

static void hold(unsigned long address, char const *hardware_id)
{
	unsigned long now = system_milliseconds();
	int index, slot = 0;

	for (index = 0; index < HELD_MAXIMUM; index++)
	{
		if (!held[index].used || reached(now, held[index].until))
		{
			slot = index;
			break;
		}
		if ((long)(held[index].until - held[slot].until) < 0)
			slot = index;
	}
	held[slot].used = 1;
	held[slot].address = address;
	snprintf(held[slot].hardware_id, sizeof(held[slot].hardware_id), "%s", hardware_id ? hardware_id : "");
	held[slot].until = now + (unsigned long)AE_MODERATION_KICK_SECONDS * 1000UL;
}

long ae_moderation_wait_seconds(unsigned long address, char const *hardware_id)
{
	unsigned long now = system_milliseconds();
	unsigned long real = address ? distributed_real_address(address) : 0;
	int index;

	for (index = 0; index < HELD_MAXIMUM; index++)
	{
		if (!held[index].used || reached(now, held[index].until))
			continue;
		if ((address && held[index].address == address) || (real && held[index].address == real) ||
			(hardware_id && hardware_id[0] && held[index].hardware_id[0] && !strcmp(hardware_id, held[index].hardware_id)))
		{
			return (long)((held[index].until - now + 999UL) / 1000UL);
		}
	}
	return 0;
}

int ae_moderation_kept_out(unsigned long address, char const *hardware_id, short *refusal_code)
{
	long wait = ae_moderation_wait_seconds(address, hardware_id);

	if (wait <= 0)
		return 0;
	/* (0.7.9: the wait rides the code: the join helper says it, ae_silent_rules.h) */
	if (refusal_code)
		*refusal_code = (short)ae_held_rejection_word(wait);
	platform_log("moderation: a machine kicked by the host is held out for %ld more seconds", wait);
	return 1;
}

void ae_moderation_wait_text(long seconds, char *text, int size)
{
	if (seconds < 0)
		seconds = 0;
	snprintf(text, (size_t)size, "%ld:%02ld", seconds / 60, seconds % 60);
}

/* ---------- kick and ban */

static void say(char const *line)
{
#ifdef HALO_PORT_CHAT
	chat_server_say(line);
#endif
	platform_log("moderation: %s", line);
}

static char const *host_word(char const *host)
{
	return host && host[0] ? host : "The host";
}

/* HALO_AE_MODERATION_TEST=2: the engine's drop is left out and the kick or ban counts as done (the page's notice and
the chat line can be looked at in a headless game with bots) */
static int test_done(void)
{
	char const *test = getenv("HALO_AE_MODERATION_TEST");

	return test && test[0] == '2';
}

void ae_moderation_hold_machine(short machine, unsigned long *address, char *hardware_id, int hardware_id_size)
{
	unsigned long real = (unsigned long)distributed_machine_real_address(machine);

	if (!real)
		real = network_game_server_machine_address(machine);
	*address = real;
	snprintf(hardware_id, (size_t)hardware_id_size, "%s", network_game_server_machine_hardware_id(machine));
}

int ae_moderation_mute(char const *name, int mute)
{
	char line[160];

	if (!network_chat_host_mute_named(name, 0, mute ? TRUE : FALSE))
		return 0;
	snprintf(line, sizeof(line), "%s %s", name, mute ? "can't chat or talk in this game now" : "can chat and talk again");
	say(line);
	return 1;
}

int ae_moderation_muted(char const *name)
{
	wchar_t wide[16];
	int index;

	for (index = 0; name && name[index] && index < 15; index++)
		wide[index] = (wchar_t)(unsigned char)name[index];
	wide[index] = 0;
	return index && network_chat_host_name_muted(wide);
}

void ae_moderation_hold(unsigned long address, char const *hardware_id)
{
	if (address || (hardware_id && hardware_id[0]))
		hold(address, hardware_id);
}

int ae_moderation_kick(short machine, char const *name, char const *host, char *text, int size)
{
	unsigned long address = (unsigned long)distributed_machine_real_address(machine);
	char hardware_id[HARDWARE_ID_SIZE];
	char line[160];

	if (!address)
		address = AE_FLOW_IDENTITY(network_game_server_machine_address(machine)); /* (none for a tunnel's) */
	snprintf(hardware_id, sizeof(hardware_id), "%s", network_game_server_machine_hardware_id(machine));
	if (!test_done() && !network_game_server_kick_machine_of_player(machine, FALSE))
	{
		if (text)
			snprintf(text, (size_t)size, "the engine kept them");
		return 0;
	}
	hold(address, hardware_id);
	/* (0.7.9: the kicked player's own notice now says the wait: the later notice is the one its error shows) */
	if (!test_done())
	{
		long wait = ae_moderation_wait_seconds(address, hardware_id);
		char why[64];

		if (wait > 0)
		{
			snprintf(why, sizeof(why), "kicked by the host (rejoin in %ld:%02ld)", wait / 60, wait % 60);
			distributed_tell_removed(machine, why);
		}
	}
	snprintf(line, sizeof(line), "%s kicked %s for 2 minutes", host_word(host), name);
	say(line);
	return 1;
}

int ae_moderation_ban(short machine, char const *name, char const *host, char *text, int size)
{
	char line[160];

	network_distributed_ban_details("banned by the host", 0);
	if (!test_done() && !network_game_server_kick_machine_of_player(machine, TRUE))
	{
		network_distributed_ban_details(NULL, 0);
		if (text)
			snprintf(text, (size_t)size, "the engine kept them");
		return 0;
	}
	snprintf(line, sizeof(line), "%s banned %s", host_word(host), name);
	say(line);
	return 1;
}

/* ---------- BANNED PLAYERS */

long ae_moderation_bans(struct ae_moderation_ban *bans, long maximum)
{
	long count = 0, number;
	unsigned long now = (unsigned long)time(NULL);

	for (number = 0; count < maximum; number++)
	{
		char when[32], hardware_id[48], players[96], reason[96];
		unsigned long until = 0;
		struct ae_moderation_ban *ban;

		when[0] = hardware_id[0] = players[0] = reason[0] = 0;
		if (!network_distributed_ban_entry(number, when, sizeof(when), hardware_id, sizeof(hardware_id), players,
			sizeof(players), reason, sizeof(reason), &until))
			break;
		/* (a ban for a while that is over is not one) */
		if (until && until <= now)
			continue;
		ban = &bans[count++];
		memset(ban, 0, sizeof(*ban));
		ban->number = number;
		ban->until = until;
		snprintf(ban->name, sizeof(ban->name), "%s", players[0] ? players : "A banned machine");
		snprintf(ban->when, sizeof(ban->when), "%.16s", when);
	}
	return count;
}

int ae_moderation_unban(long number)
{
	int done = network_distributed_unban(number) ? 1 : 0;

	platform_log("moderation: unban of ban %ld: %s", number, done ? "taken out of bans.txt" : "not done");
	return done;
}

long ae_moderation_ban_age_seconds(struct ae_moderation_ban const *ban)
{
	struct tm parts;
	time_t then;

	memset(&parts, 0, sizeof(parts));
	if (sscanf(ban->when, "%d-%d-%d %d:%d", &parts.tm_year, &parts.tm_mon, &parts.tm_mday, &parts.tm_hour, &parts.tm_min) != 5)
		return -1;
	parts.tm_year -= 1900;
	parts.tm_mon -= 1;
	parts.tm_isdst = -1;
	then = mktime(&parts);
	return then == (time_t)-1 ? -1 : (long)difftime(time(NULL), then);
}

void ae_moderation_confirm_text(int ban, char const *name, char *title, int title_size, char *body, int body_size,
	char *yes, int yes_size)
{
	int at;

	if (ban)
	{
		snprintf(title, (size_t)title_size, "Ban %s?", name);
		snprintf(body, (size_t)body_size, "%s leaves now and can't join any game this PC hosts again. Undo it in SETTINGS > "
			"ONLINE > HOSTING > BANNED PLAYERS.", name);
		snprintf(yes, (size_t)yes_size, "BAN %s", name);
	}
	else
	{
		snprintf(title, (size_t)title_size, "Kick %s?", name);
		snprintf(body, (size_t)body_size, "%s leaves the game now and can't join it again for 2 minutes. Their score stays on "
			"the board.", name);
		snprintf(yes, (size_t)yes_size, "KICK %s", name);
	}
	for (at = 0; yes[at]; at++)
		if (yes[at] >= 'a' && yes[at] <= 'z')
			yes[at] = (char)(yes[at] - 'a' + 'A');
}

int ae_moderation_can_kick(int bot, int local, int host)
{
	if (local || host)
		return 0;
	if (bot)
	{
		char const *test = getenv("HALO_AE_MODERATION_TEST");

		return test && (test[0] == '1' || test[0] == '2');
	}
	return 1;
}
