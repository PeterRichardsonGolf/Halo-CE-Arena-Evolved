/*
AE_GLUE_PARTY.H

The party record of this PC as the screens draw it (ae_party_model_rules.h): the local players, then the bots a player partied up with
(offline), the people kept on this PC (ae_party_store_rules.h, party_store.txt in the private files), and DEV MODE's staged party
(`social party ...`: made-up members, online states included, so every screen state can be looked at with no network). Log prefix
"ae party:".
*/

#ifndef __AE_GLUE_PARTY_H
#define __AE_GLUE_PARTY_H

#include "ae_party_model_rules.h"
#include "ae_party_store_rules.h"

/* the party to draw now (the staged one when DEV MODE staged it) and its source (AE_PARTY_SOURCE_*) */
struct ae_party const *ae_glue_party(int *source);
/* the people kept on this PC; the staged recent players while a party is staged */
struct ae_party_store *ae_glue_party_store(void);
/* a bot takes a party seat: 1 when it joined, 0 for a full party (the local players count toward AE_PARTY_MAX) or a bot already in. The store notes it (a favourite after games together) */
int ae_glue_party_up_bot(char const *name, int tier);
/* a bot leaves the party */
int ae_glue_party_remove(char const *id);
/* the party's bots (names and tiers) as the next plan seats them */
int ae_glue_party_bots(char names[][AE_PARTY_NAME], int *tiers, int max);
/* a party bot played a game with the player (by its name): its record together (games, wins) and the store written */
void ae_glue_party_credit(char const *name, int won);
/* a bot of a finished game that is not in the party: it goes into the recent players, its record against the player
or against (a teammate is only remembered, an opponent's game counts); won: the player's side won */
void ae_glue_party_note(char const *name, int tier, int together, int won);
/* DEV MODE `social party offline|online|toobig|full|empty|clear`: stages a party (clear: the real one again). 1 when it applied */
int ae_glue_party_dev(char const *state, char *text, int size);
/* whether a party is staged */
int ae_glue_party_staged(void);

#endif
