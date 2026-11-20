/*
AE_DISC_JOIN_RULES.H

Arena Evolved: a modded disc in the join token (ae_disc_join.c is the glue;
port/linux/tests/ae_disc_join_rules_test.c runs these). Pure.

A machine's join token (network_game_generate_join_game_token) was the same
for every machine of the port, whatever maps it played: a machine on the stock
maps joined a host playing a modded disc (Halo 1: NHE, CE+ X), loaded its own
Blood Gulch where the host had the disc's, and the two games went apart. A
disc that replaces a stock map now changes the token, by its name, so only a
machine playing the same disc matches the host's and joins. With no disc, or a
disc that replaces no stock map (a disc of its own maps alone, which map
sharing sends to a joining machine), the token is the stock one: stock OpenCE
and Chupathingy machines join as before. The wire is unchanged: the token is
the join request's own field, and a refused machine is told "bad join token".
*/

#ifndef __AE_DISC_JOIN_RULES_H
#define __AE_DISC_JOIN_RULES_H

/* a disc's identity in the join token: its name, letters in any case (a disc
named NHE on one machine and nhe on another is the same disc), 64-bit FNV-1a;
never 0 for a name, 0 for none (NULL or empty) */
unsigned long long ae_disc_join_identity(char const *disc);

/* nonzero when a file of a disc's maps folder replaces a stock map that
changes a game: a ".map" (any case) named as a stock multiplayer or campaign
map. The menus' ui.map and Custom Edition's resource maps change no game */
int ae_disc_join_replaces_stock_map(char const *file_name);

/* the token (size bytes) with the identity folded in; an identity of 0 leaves
it as it is */
void ae_disc_join_fold(unsigned char *token, int size, unsigned long long identity);

#endif
