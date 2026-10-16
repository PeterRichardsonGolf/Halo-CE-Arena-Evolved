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
  has bots.c's own; not a game hosted for the internet), of Slayer, once a
  person of the host's machine is in it.
*/

#ifndef __AE_LAN_BOTS_RULES_H
#define __AE_LAN_BOTS_RULES_H

/* the most bots a host's machine takes (its four slots, one the host's) */
#define AE_LAN_BOTS_MAXIMUM 3
/* a machine's controller slots (MAXIMUM_LOCAL_PLAYERS) */
#define AE_LAN_BOTS_CONTROLLERS 4

/* whether the lobby has bots at all: its setting-up (pregame, the start not yet sent), not a local game, not hosted
for the internet, Slayer, and a person of the host's machine in it */
int ae_lan_bots_eligible(int pregame, int local_game, int internet, int slayer, int host_has_person);

/* the controller slots (bits 0-3) a bot may hold: none a person's nor one a pad wants to join on */
unsigned ae_lan_bots_free_controllers(unsigned people_mask, unsigned wanting_mask);

/* how many bots the lobby keeps: bots.count asks for `wanted`, up to AE_LAN_BOTS_MAXIMUM, as many as the free
controllers, and one player slot of the game left free (other_players: the game's players that are not bots) */
int ae_lan_bots_target(int wanted, int eligible, int other_players, int maximum_players, unsigned free_mask);

/* the controller a new bot takes: the highest free one no bot holds; -1 for none */
int ae_lan_bots_pick_controller(unsigned free_mask, unsigned bots_mask);

/* a bot's team in a team game (0 red, 1 blue): bots.teams "against" the other team of the people's (people_team,
-1 for none: blue), else the smaller team (red when even) */
int ae_lan_bots_team(int against, int people_team, int red_count, int blue_count);

#endif
