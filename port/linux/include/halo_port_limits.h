/*
HALO_PORT_LIMITS.H

Multiplayer session limits of the native builds (Windows, Linux, Android),
force-included by halo_linux_prefix.h and halo_windows_prefix.h.

The Xbox game allows 16 players on at most 4 machines (up to 4 players each
on split screen). The native builds allow 128 players on up to 128
machines; split screen stays at 4 players per machine.

128 is the largest session that fits the game's existing records: player,
machine and team indices are stored in signed chars (0..127 with NONE), and
a finishing place in 7 bits.
*/

#ifndef __HALO_PORT_LIMITS_H
#define __HALO_PORT_LIMITS_H

/* ---------- session limits */

#define HALO_PORT_MAXIMUM_NETWORK_PLAYERS 128
#define HALO_PORT_MAXIMUM_NETWORK_MACHINES 128

/* a host polls its listening socket and one socket per machine; the Xbox's
Winsock headers default to 64 (the prefix headers define FD_SETSIZE from
this before any of them is read) */
#define HALO_PORT_FD_SETSIZE 256

/* ---------- memory capacity for these limits (game state and pools) */

#include "halo_port_capacity.h"

/* ---------- struct network_game layout

The game settings record (struct network_game) is declared separately in
several networking and interface units; its layout follows from the limits.
Every copy checks its offsets against these values. The Xbox values (4
machines, 16 players) are 0x226 and 0x434. */

#define HALO_PORT_NETWORK_MACHINE_SIZE 0x44
#define HALO_PORT_NETWORK_PLAYER_SIZE 0x20
#define HALO_PORT_NETWORK_GAME_MACHINES_OFFSET 0x114
#define HALO_PORT_NETWORK_GAME_PLAYER_COUNT_OFFSET \
	(HALO_PORT_NETWORK_GAME_MACHINES_OFFSET + HALO_PORT_MAXIMUM_NETWORK_MACHINES * HALO_PORT_NETWORK_MACHINE_SIZE)
#define HALO_PORT_NETWORK_GAME_PLAYERS_OFFSET (HALO_PORT_NETWORK_GAME_PLAYER_COUNT_OFFSET + 2)
#define HALO_PORT_NETWORK_GAME_PLAYERS_END \
	(HALO_PORT_NETWORK_GAME_PLAYERS_OFFSET + HALO_PORT_MAXIMUM_NETWORK_PLAYERS * HALO_PORT_NETWORK_PLAYER_SIZE)
#define HALO_PORT_NETWORK_GAME_RANDOM_SEED_OFFSET (HALO_PORT_NETWORK_GAME_PLAYERS_END + 2)
/* (then the gametype's PC options, struct game_variant_options: 0x1C bytes) */
#define HALO_PORT_NETWORK_GAME_VARIANT_OPTIONS_OFFSET (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0xA)
#define HALO_PORT_NETWORK_GAME_LOCAL_DATA_OFFSET (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0x26)
#define HALO_PORT_NETWORK_GAME_SIZE (HALO_PORT_NETWORK_GAME_PLAYERS_END + 0x2A)

/* ---------- system link protocol

The native builds' messages differ from the Xbox game's (longer arrays, the
game settings record in fragments), so they search for games with their own
protocol version and never see the Xbox game's, or it theirs. */

#define HALO_PORT_NETWORK_GAME_MESSAGE_VERSION 2

/* The native builds' network code has a version of its own (an unsigned
16-bit number): machines of different versions cannot play together, and a
client does not join a host of another version, but tells the player which
is newer (network_client_manager.c). A host advertises it, with its netcode,
in its game's advertisement's reserved bytes (network_server_message_handler.c),
which hosts built before there was a version send as zeros: version 0.
Raise it with any change to what the machines send each other. */
#define HALO_PORT_NETWORK_VERSION 25
/* ... the versions whose hosts a client joins: its own, and those that differ
from it only in what the other machines leave out (a message a machine of
the other version does not know it drops). Which are which is delta.h's
table (DELTA_LEGACY_VERSIONS): the minimum is its newest breaking version,
the maximum and the version above its newest (tools/test_delta.py checks
them; they stay numbers here, which the command repository reads). A host
never checks a client's version: the client does (network_client_manager.c),
so the range is the client's. */
#define HALO_PORT_NETWORK_VERSION_MINIMUM 11
#define HALO_PORT_NETWORK_VERSION_MAXIMUM 25
/* ... the numbers in use (port/linux/src/delta.c): the three above, until a
legacy table (docs/delta.md) widens them; code asks these, not the numbers */
int delta_legacy_announce(void);
int delta_legacy_minimum(void);
int delta_legacy_maximum(void);
/* ... the advertisement's reserved bytes: the version (a little-endian word),
then flags */
#define HALO_PORT_ADVERTISED_VERSION_OFFSET 0
#define HALO_PORT_ADVERTISED_FLAGS_OFFSET 2
/* ... the host plays the distributed netcode (always, since the lockstep
netcode was removed; hosts of version 4 built before then may not) */
#define HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG 0x01
/* ... whether a client joins a host that advertises this version and these flags: at least this build's own
network version (no older host, whatever the wider range above: the range is what the other machines' messages
leave out, the floor is what this build refuses), inside minimum..maximum (delta_legacy_minimum/maximum), and on the
distributed netcode. The one test: the join (network_client_manager.c), AE's lobby (ae_glue_lobby.c) and the game
lists' rows (halo_port_advertised_join_state, below) all ask it,
and port/linux/tests/ae_lobby_compat_test.c checks it. */
static inline int halo_port_advertised_joinable(unsigned int theirs, unsigned int flags, unsigned int minimum,
	unsigned int maximum)
{
	return theirs >= (unsigned int)HALO_PORT_NETWORK_VERSION && theirs >= minimum && theirs <= maximum &&
		(flags & HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG) != 0;
}
/* AE: what the game lists say of a host that advertises this version and these flags, which they keep listed, dimmed,
when it is not joined (the Online Games screen browser_screen.c, the PC menus' lists menu_functions.c): joined (as the
test above says, and only then), or why not. A host under this build's version must update (whatever the range); one
over the maximum is newer than this game, which must; one inside both and off the distributed netcode plays the
lockstep netcode this game no longer has. port/linux/tests/ae_lobby_compat_test.c checks it. */
enum
{
	HALO_PORT_JOIN_OK,
	HALO_PORT_JOIN_HOST_OLDER,
	HALO_PORT_JOIN_HOST_NEWER,
	HALO_PORT_JOIN_HOST_LOCKSTEP,
};
static inline int halo_port_advertised_join_state(unsigned int theirs, unsigned int flags, unsigned int minimum,
	unsigned int maximum)
{
	if (halo_port_advertised_joinable(theirs, flags, minimum, maximum))
		return HALO_PORT_JOIN_OK;
	if (theirs > maximum)
		return HALO_PORT_JOIN_HOST_NEWER;
	if (theirs < (unsigned int)HALO_PORT_NETWORK_VERSION || theirs < minimum)
		return HALO_PORT_JOIN_HOST_OLDER;
	return HALO_PORT_JOIN_HOST_LOCKSTEP;
}
/* ... and the reason a list gives on the row's line, a format taking the host's version (NULL for a host that is
joined). A host that advertises no version (0: built before there was one) has the reason without a number. */
static inline char const *halo_port_join_reason_format(int state, unsigned int version)
{
	switch (state)
	{
	case HALO_PORT_JOIN_HOST_OLDER: return version ? "HOST NEEDS TO UPDATE (VERSION %u)" : "HOST NEEDS TO UPDATE";
	case HALO_PORT_JOIN_HOST_NEWER: return "UPDATE THIS GAME TO JOIN (HOST VERSION %u)";
	case HALO_PORT_JOIN_HOST_LOCKSTEP: return "HOST NEEDS TO UPDATE (OLD NETCODE, VERSION %u)";
	default: return (char const *)0;
	}
}
/* AE: a game list's version field (text, as the list server sends it) as a network version: digits only (spaces
around them allowed), 0 to 65535; -1 for anything else (a sign, letters, a larger number), which is no version:
never narrowed into one (browser.c drops the line) */
static inline long halo_port_listed_version(char const *text)
{
	long version = 0;
	int digits = 0;

	while (*text == ' ' || *text == '\t')
		text++;
	for (; *text >= '0' && *text <= '9'; text++, digits++)
	{
		version = version * 10 + (*text - '0');
		if (version > 0xFFFF)
			return -1;
	}
	while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
		text++;
	return digits && !*text ? version : -1;
}
/* AE: where a game list's next game goes in an array of maximum games, count of them kept (unjoinable[]: whether
each kept one's host is not joined): the next place while there is room; in a full array a game whose host is joined
takes the place of the last kept one whose host is not (hosts this build does not join never push out those it
does); else -1, the game left out */
static inline int halo_port_listed_place(unsigned char const *unjoinable, int count, int maximum, int joinable)
{
	int index;

	if (count < maximum)
		return count;
	for (index = joinable ? count - 1 : -1; index >= 0; index--)
	{
		if (unjoinable[index])
			return index;
	}
	return -1;
}
/* ... the game is under way (loading, playing or over), not in its lobby:
the menus show it before joining it (hosts built before then never set it) */
#define HALO_PORT_ADVERTISED_IN_PROGRESS_FLAG 0x02

/* a message header's 12-bit length allows messages of up to 0xFFF bytes,
header included; the per-tick update of 128 players is 3,857 */
#define HALO_PORT_MAXIMUM_NETWORK_MESSAGE_SIZE 0x1000

/* the packet codec's limit on a decoded or encoded packet; the per-tick
update of 128 players decodes to 0x1010 bytes */
#define HALO_PORT_NETWORK_PACKET_SIZE 0x1100

/* the game settings record (HALO_PORT_NETWORK_GAME_SIZE, 13,120 bytes at 128
machines and players) does not fit one message; it is sent in pieces of
this many bytes (4 pieces), each 3,594 bytes on the wire */
#define HALO_PORT_NETWORK_GAME_SETTINGS_FRAGMENT_SIZE 0xE00

#endif /* __HALO_PORT_LIMITS_H */
