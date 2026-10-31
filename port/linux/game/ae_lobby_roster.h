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
	int bot;                         /* a LAN bot of this machine (the host's: ae_lan_bots.c), neither local nor host */
	unsigned int color;              /* the player's armour colour, RGBA (their profile's, as the game sends it) */
};

struct ae_lobby_roster
{
	short count;
	short machines;            /* the machines in the session (network bots' virtual machines too: never people) */
	short people;              /* the players who are not bots (split screen guests are people): ae_lobby_roster_order */
	int joined;                /* this machine's client has joined the session (players can be added) */
	struct ae_lobby_player players[AE_LOBBY_MAXIMUM_PLAYERS];
	char map[64];              /* the map's file name ("bloodgulch") */
	char gametype[48];         /* the gametype's name, UTF-8 */
	int team_game, in_game, postgame;
	short maximum_players;     /* (0.3.9 one lobby) the most players the session takes (the host's MAX PLAYERS) */
};

/* the roster as the game reports it, sorted host first, then by machine, then in the order it lists them (a stable
order); slot = place + 1 (1..count, always contiguous); people = the players who are not bots */
void ae_lobby_roster_order(struct ae_lobby_roster *roster);
/* (a joiner, who is told nothing of the host's bots) whether a player is a network game's bot by its names, UTF-8:
its machine is one of the host's virtual machines, named "Bots" (ae_lan_bots.c), or its name has "BOT " before it,
as the host names every network bot (ae_lan_bots_name) so that any machine tells it from a person */
int ae_lobby_bot_by_names(const char *machine_name, const char *player_name);
/* UTF-8 text into out (size bytes with its end), cut on a character's boundary when longer; returns its length */
int ae_lobby_copy_utf8(char *out, int size, const char *text);
/* count 16-bit units of UTF-16 (to the first 0) as UTF-8 into out, cut on a character's boundary; a lone
surrogate is U+FFFD; returns the length */
int ae_lobby_utf16_to_utf8(char *out, int size, const unsigned short *text, int count);

#endif
