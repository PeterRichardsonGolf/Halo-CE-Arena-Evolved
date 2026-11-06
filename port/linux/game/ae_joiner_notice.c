/*
AE_JOINER_NOTICE.C

A host's chat line for a machine that joins without Arena Evolved
(ae_joiner_notice.h). An AE machine marks its join request (ae_peer_is_ae);
any other machine is named by what Delta Peer knows of it: the build its
HELLO names, or OpenCE's when it never spoke Delta. The line goes through
chat_server_say, so it reaches only a host on Delta (its OpenCE machines by
the chat bridge); a host on OpenCE's protocol alone logs it.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "halo_port_limits.h"
#include "halo_product.h"
#include "../src/delta_peer.h"
#include "../src/delta_wire.h"

#include "ae_joiner_notice.h"
#include "ae_joiner_notice_rules.h"
#include "ae_lan_bots.h"
#include "ae_lobby_roster.h"
#include "ae_map_share.h"
#include "ae_silent.h"
#include "ae_silent_rules.h"
#include "chat.h"

void platform_log(char const *format, ...);
const char *updater_version(void);
unsigned long system_milliseconds(void);

/* (a build without text chat, the web's, has no chat line to say it with) */
static int notice_say(char const *text)
{
#ifdef HALO_PORT_CHAT
	return chat_server_say(text);
#else
	(void)text;
	return 0;
#endif
}

#define NOTICE_MACHINES 128
/* (a machine not seen this long was not in the server's list: its slot is another machine's when seen again) */
#define NOTICE_GONE_MS 2000
/* (a machine whose build is still not known by then: said as another build) */
#define NOTICE_WAIT_MS 10000

static struct
{
	unsigned long seen[NOTICE_MACHINES];
	unsigned long joined[NOTICE_MACHINES];
	boolean done[NOTICE_MACHINES];
} notice;

/* the machine's first player's name (UTF-8), FALSE while it has none in the game */
static boolean machine_player_name(long machine_index, char *name, int size)
{
	struct network_game *game = network_game_get_game();
	short index;

	if (!game)
		return FALSE;
	for (index = 0; index < (short)NUMBEROF(game->players); index++)
	{
		struct network_player const *player = &game->players[index];

		if (!network_player_is_valid((struct network_player *)player) || player->machine_index != machine_index)
			continue;
		if (ae_lobby_utf16_to_utf8(name, size, (const unsigned short *)player->name, NUMBEROF(player->name)) <= 0)
			name[0] = 0;
		return TRUE;
	}
	return FALSE;
}

void ae_joiner_notice_host_machine(long machine_index, boolean remote_joined)
{
	unsigned long now = system_milliseconds();
	char name[64], build[32], text[DELTA_WIRE_CHAT_TEXT_SIZE + 1];
	int kind;

	if (machine_index < 0 || machine_index >= NOTICE_MACHINES)
		return;
	if (!notice.seen[machine_index] || now - notice.seen[machine_index] > NOTICE_GONE_MS || !remote_joined)
	{
		notice.joined[machine_index] = 0;
		notice.done[machine_index] = FALSE;
	}
	notice.seen[machine_index] = now ? now : 1;
	/* (the wait counts from the frame it is first seen joined: a machine is listed while it connects) */
	if (remote_joined && !notice.joined[machine_index])
		notice.joined[machine_index] = now ? now : 1;
	if (!remote_joined || notice.done[machine_index] || ae_lan_bots_machine_held(machine_index) ||
		!machine_player_name(machine_index, name, (int)sizeof(name)))
	{
		return;
	}
	/* (0.7.5: an Arena Evolved machine on another version is named, with both versions, so the players know why an
	extra misbehaves; the same version: nothing) */
	if (ae_peer_is_ae(machine_index))
	{
		char own[48];
		int known = delta_peer_game_machine_build((int)machine_index, build, (int)sizeof(build)) > 0 && build[0];

		/* (its build named once Delta knows it: waited for as long as for another build's) */
		if (!known && now - notice.joined[machine_index] < NOTICE_WAIT_MS)
			return;
		notice.done[machine_index] = TRUE;
		snprintf(own, sizeof(own), HALO_PRODUCT_SHORT_NAME " %s", updater_version());
		if (known && ae_joiner_notice_version_text(text, (int)sizeof(text), name, build, own) && notice_say(text))
			platform_log("joiner notice: machine #%ld is on %s, this host on %s: said in chat", machine_index, build, own);
		return;
	}
	kind = delta_peer_game_machine_build((int)machine_index, build, (int)sizeof(build));
	if (kind < 0 && now - notice.joined[machine_index] < NOTICE_WAIT_MS)
		return;
	notice.done[machine_index] = TRUE;
	if (!ae_joiner_notice_text(text, (int)sizeof(text), name, kind, build))
		return;
	/* (a name chat's text rules refuse: the line again without it) */
	if (notice_say(text) ||
		(ae_joiner_notice_text(text, (int)sizeof(text), NULL, kind, build) && notice_say(text)))
	{
		platform_log("joiner notice: machine #%ld is not on Arena Evolved (%s): said in chat", machine_index,
			kind == 0 ? "OpenCE" : kind > 0 ? (build[0] ? build : "Delta, no build name") : "not known");
	}
	else
	{
		platform_log("joiner notice: machine #%ld is not on Arena Evolved (%s): no chat line (this host is not on "
			"Delta)", machine_index, kind == 0 ? "OpenCE" : kind > 0 ? (build[0] ? build : "Delta") : "not known");
	}
}

void ae_joiner_notice_pack_refusal(void)
{
	static unsigned long said_at = 0;
	static int said = 0;
	unsigned long now = system_milliseconds();

	if (said && now - said_at < 60000UL)
		return;
	said = 1;
	said_at = now;
	notice_say("A player tried to join without the weapon packs this gametype uses. They need Arena Evolved with "
		"its packs, or pick a gametype without packs to let them in.");
}

void ae_joiner_notice_cache_refusal(int cache_case)
{
	static unsigned long said_at = 0;
	static int said = 0;
	unsigned long now = system_milliseconds();

	/* (at most once a minute, as the pack refusal: a refused player tries again) */
	if (said && now - said_at < 60000)
		return;
	said = 1;
	said_at = now;
	if (cache_case == AE_CACHE_JOIN_NO_BLOCK)
	{
		notice_say("A player tried to join without the weapon cache this gametype uses. Arena Evolved 0.7.6 or later (the "
			"newest) is needed, or pick a gametype that does not use it.");
		return;
	}
	notice_say("A player tried to join with another weapon cache than this game's. They need the same Arena Evolved "
		"version as the host.");
}

void ae_joiner_notice_territories_refusal(void)
{
	static unsigned long said_at = 0;
	static int said = 0;
	unsigned long now = system_milliseconds();

	if (said && now - said_at < 60000UL)
		return;
	said = 1;
	said_at = now;
	notice_say("A player tried to join without Arena Evolved. Territories is played with Arena Evolved only, or pick "
		"another gametype to let them in.");
}

void ae_joiner_notice_rounds_full(void)
{
	static unsigned long said_at = 0;
	static int said = 0;
	unsigned long now = system_milliseconds();

	if (said && now - said_at < 60000UL)
		return;
	said = 1;
	said_at = now;
	notice_say("A player tried to join, but Elimination holds 16 players and the game is full.");
}

void ae_joiner_notice_rounds_refusal(void)
{
	static unsigned long said_at = 0;
	static int said = 0;
	unsigned long now = system_milliseconds();

	if (said && now - said_at < 60000UL)
		return;
	said = 1;
	said_at = now;
	notice_say("A player tried to join without Arena Evolved 0.7.6 or later. Elimination is played with it only, or "
		"pick another gametype to let them in.");
}
