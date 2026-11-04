/*
AE_LAN_BOTS_RULES.H

The bots of a LAN game (ae_lan_bots.c) as pure rules: no engine or config
calls (port/linux/tests/ae_lan_bots_rules_test.c runs them).

A LAN game's bots are players of the HOST's machine, on the controller slots
its people leave free (a machine has four), as a second pad's player would
be: every other machine sees them as the host's players, and the network
protocol is the same. Their input is the bot brain's (bots.c), put into the
host's tick where a pad's would go; they have no view, no HUD and no sound
(they are not this machine's local players: ae_lan_bots.c).

- At most AE_LAN_BOTS_MAXIMUM (3): the host's own player and three bots fill
  the machine's four slots.
- A bot never takes a controller a person of the host plays on, nor one whose
  pad has asked to join (the Xbox lobby's START); one there gives way.
- A bot leaves one player slot of the game free, so that the game is never
  full for a machine that joins because of bots.
- Bots only in a LAN lobby being set up (not a local split screen game, which
  has bots.c's own; not a game hosted for the internet), of a gametype bots play, once a
  person of the host's machine is in it.

With debug.network_bots (on by default since 0.3.7, an off switch) a game
hosted for the internet has them too, "BOT " before their names, and up to AE_NET_BOTS_MAXIMUM: past the host's
free controller slots, the bots sit on machines of the host's own ("virtual
machines": in the game's machines, at the top machine indices, with no
connection), four to a machine, which every other machine sees as a machine
with four players. The lobby still leaves one player slot free; at the start
of a team game whose players are odd in number, one more bot takes it, so the
teams are even (ae_lan_bots_start_extra). The game in progress takes more bots
as it has room (no slot kept free there: a person joining a full game takes a
bot's place), new virtual machines too, four at a time
(ae_lan_bots_ingame_step), so a game started before its lobby had all of
bots.count's fills up a few seconds in.
*/

#ifndef __AE_LAN_BOTS_RULES_H
#define __AE_LAN_BOTS_RULES_H

/* the most bots a host's machine takes (its four slots, one the host's) */
#define AE_LAN_BOTS_MAXIMUM 3
/* a machine's controller slots (MAXIMUM_LOCAL_PLAYERS) */
#define AE_LAN_BOTS_CONTROLLERS 4
/* debug.network_bots: the most bots of a game (the session's 128 players, one a person: bots.h's BOTS_BRAINS) */
#define AE_NET_BOTS_MAXIMUM 127
/* ... and the most virtual machines they take (four bots each) */
#define AE_NET_BOTS_MACHINES ((AE_NET_BOTS_MAXIMUM + AE_LAN_BOTS_CONTROLLERS - 1) / AE_LAN_BOTS_CONTROLLERS)

/* whether the lobby has bots at all: its setting-up (pregame, the start not yet sent), not a local game, not hosted
for the internet, a gametype bots play (ae_bots_engine_played), and a person of the host's machine in it */
int ae_lan_bots_eligible(int pregame, int local_game, int internet, int played, int host_has_person);

/* the controller slots (bits 0-3) a bot may hold: none a person's nor one a pad wants to join on */
unsigned ae_lan_bots_free_controllers(unsigned people_mask, unsigned wanting_mask);

/* the most bots the seats take: the host's free controllers (free_mask), up to AE_LAN_BOTS_MAXIMUM; with
network_bots (debug.network_bots) those and four on each virtual machine the host may have (machines: its own and
the free machine slots it may take), up to AE_NET_BOTS_MAXIMUM */
int ae_lan_bots_capacity(int network_bots, unsigned free_mask, int machines);

/* how many bots the lobby keeps: bots.count asks for `wanted`, as many as the seats take (capacity), and one player
slot of the game left free (other_players: the game's players that are not bots) */
int ae_lan_bots_target(int wanted, int eligible, int other_players, int maximum_players, int capacity);

/* (debug.network_bots) the bots added at the start: 1 in a team game (not bots.teams "against") whose players
(other_players and bots) are odd, when the free slot and a seat are there and no machine that joined is still to add
its player (playerless_machines: the free slot is that player's), else 0 */
int ae_lan_bots_start_extra(int teams, int against, int bots, int other_players, int maximum_players, int capacity,
	int playerless_machines);

/* (debug.network_bots) the bots the game in progress keeps: bots.count's `wanted`, as many as the seats take and
the game has player slots (none kept free: a person joining a full game takes a bot's place,
ae_lan_bots_leaving_team), one more in a team game whose players are odd (as at the start) */
int ae_lan_bots_ingame_target(int wanted, int eligible, int other_players, int maximum_players, int capacity, int teams,
	int against);

/* (debug.network_bots) the game in progress's seats: the bots there are (bots), the free seats of the host's
controllers and of its virtual machines (free_seats), and four on each virtual machine it may yet add (new_machines: a
virtual machine joins the game in progress as a late-joining machine does), up to AE_NET_BOTS_MAXIMUM */
int ae_lan_bots_ingame_capacity(int bots, int free_seats, int new_machines);

/* (debug.network_bots) the game in progress's next step towards its target: the bots that join (> 0) or leave (< 0)
at once, at most a machine's four (AE_LAN_BOTS_CONTROLLERS); 0 for none */
int ae_lan_bots_ingame_step(int bots, int target);

/* (debug.network_bots) the ticks the game in progress waits before a step (`next`: ae_lan_bots_ingame_step's) since
its players last changed: AE_LAN_BOTS_STEP_TICKS when the last change was the bots' own step the same way (`last`:
> 0 joined, < 0 left; a bots.count of 31 fills a game started at once in a few seconds), else
AE_LAN_BOTS_SETTLE_TICKS (a person joined or left, the game started, or the bots turn from leaving to joining: a
seat let go is taken again only once its player has quit on every machine) */
#define AE_LAN_BOTS_SETTLE_TICKS 60
#define AE_LAN_BOTS_STEP_TICKS 3
int ae_lan_bots_ingame_wait(int last, int next);

/* (debug.network_bots) the team whose bot leaves (a person takes its place, or fewer are wanted): in a team game the
team with more players that has a bot (red when even), else the other's (0 red, 1 blue); -1 in a game without teams
(the last bot) or when no team has a bot */
int ae_lan_bots_leaving_team(int teams, int red_bots, int blue_bots, int red_players, int blue_players);

/* the virtual machine a new bot sits on: the first of `count` (their bots in counts) with a free seat; `count` for
a new one when fewer than AE_NET_BOTS_MACHINES; -1 for none */
int ae_lan_bots_pick_machine(int const *counts, int count);

/* the number after a bot's name (bots.c names fifteen): 0 for the first fifteen, then 2, 3, ... */
int ae_lan_bots_name_number(int bot_index, int names);

/* a network game's bot's name (ASCII, at most AE_LAN_BOTS_NAME_LENGTH characters): with tagged (debug.network_bots)
"BOT " before it, so that every machine, a stock one too, tells it from a person; the base name cut so the number
(when not 0) still fits after it */
#define AE_LAN_BOTS_NAME_LENGTH 11
void ae_lan_bots_name(char const *base, int number, int tagged, char *name, int size);

/* the controller a new bot takes: the highest free one no bot holds; -1 for none */
int ae_lan_bots_pick_controller(unsigned free_mask, unsigned bots_mask);

/* a bot's team in a team game (0 red, 1 blue): bots.teams "against" the other team of the people's (people_team,
-1 for none: blue), else the smaller team (red when even) */
int ae_lan_bots_team(int against, int people_team, int red_count, int blue_count);

/* the lobby's bots' teams (0 red, 1 blue) for count bots in their order: one the host chose (chosen[i] 0 or 1; -1 for
none, or chosen NULL) keeps it and is counted first, the others take ae_lan_bots_team in turn on the counts so far
(red_count and blue_count: the people's). A chosen team never moves by the others joining or leaving. */
void ae_lan_bots_teams(int count, int const *chosen, int against, int people_team, int red_count, int blue_count,
	int *teams);

#endif
