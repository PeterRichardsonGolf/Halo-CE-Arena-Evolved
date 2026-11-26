/*
AE_GLUE_STEAM_FRIENDS.H

STEAM FRIENDS glued to the party service (ae_steam_friends_rules.h). The friend list comes from a provider: the Steam client's friend
list when it gives one (not built yet) or DEV MODE's made-up one. Nothing is sent unless network.steam_friends is on and the party
service is started; turning it off (or `party online steam off`) withdraws the registration. Log prefix "ae steam friends:".
*/

#ifndef __AE_GLUE_STEAM_FRIENDS_H
#define __AE_GLUE_STEAM_FRIENDS_H

#include "ae_steam_friends_rules.h"

/* the provider's answer: this account's Steam id and its friends (copied; up to AE_STEAM_FRIENDS_MAX) */
void ae_steam_friends_provide(unsigned long long own_id, const struct ae_steam_friend *friends, int count);
/* registers this account's hash and asks about the friends' (queued calls); 1 when sent, 0 with the reason in text */
int ae_steam_friends_send(char *text, int size);
/* withdraws the registration */
int ae_steam_friends_withdraw(char *text, int size);
/* the rows for the screen / DEV: how many, and a one-line summary */
int ae_steam_friends_rows(struct ae_steam_row *rows, int maximum, int *others);
void ae_steam_friends_summary(char *text, int size);

#endif
