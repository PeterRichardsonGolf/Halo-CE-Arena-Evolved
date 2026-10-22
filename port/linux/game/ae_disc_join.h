/*
AE_DISC_JOIN.H

Arena Evolved: a modded disc in the join token, so a machine joins only a host
playing the same maps (ae_disc_join_rules.h says why and how).
*/

#ifndef __AE_DISC_JOIN_H
#define __AE_DISC_JOIN_H

/* (network_server_manager.c network_game_generate_join_game_token) folds the
disc the game started with into the token when it replaces a stock map; the
stock token otherwise */
void ae_disc_join_token(unsigned char *token, long size);

/* (network_client_manager.c network_game_client_rejected_by_game) a host
refused this machine's join token: tells the player the discs differ */
void ae_disc_join_rejected(unsigned short rejection_code);

#endif
