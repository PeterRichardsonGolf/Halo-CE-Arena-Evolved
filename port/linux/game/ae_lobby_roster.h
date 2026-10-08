/*
AE_LOBBY_ROSTER.H

(M2, preflight P18) The lobby's roster as AE's screens show it, in plain types
(ae_glue_lobby.c fills it from the game's network game): its players in the
order AE lists them, and names in UTF-8. Pure, no engine includes (unit test:
port/linux/tests/ae_lobby_roster_test.c).
*/

#ifndef __AE_LOBBY_ROSTER_H
#define __AE_LOBBY_ROSTER_H

enum { AE_LOBBY_MAXIMUM_PLAYERS = 128 };   /* = HALO_PORT_MAXIMUM_NETWORK_PLAYERS (checked in ae_glue_lobby.c) */
enum { AE_LOBBY_NAME_SIZE = 48 };

struct ae_lobby_player
{
	char name[AE_LOBBY_NAME_SIZE];   /* UTF-8 of the game's 16-bit name */
	short team, machine, controller, slot;
	int local, host;
};

struct ae_lobby_roster
{
	short count;
	short machines;            /* the machines in the session */
	int joined;                /* this machine's client has joined the session (players can be added) */
	struct ae_lobby_player players[AE_LOBBY_MAXIMUM_PLAYERS];
	char map[64];              /* the map's file name ("bloodgulch") */
	char gametype[48];         /* the gametype's name, UTF-8 */
	int team_game, in_game, postgame;
};

/* the roster as the game reports it, sorted host first, then by machine, then in the order it lists them (a stable
order); slot = place + 1 (1..count, always contiguous) */
void ae_lobby_roster_order(struct ae_lobby_roster *roster);
/* UTF-8 text into out (size bytes with its end), cut on a character's boundary when longer; returns its length */
int ae_lobby_copy_utf8(char *out, int size, const char *text);
/* count 16-bit units of UTF-16 (to the first 0) as UTF-8 into out, cut on a character's boundary; a lone
surrogate is U+FFFD; returns the length */
int ae_lobby_utf16_to_utf8(char *out, int size, const unsigned short *text, int count);

#endif
