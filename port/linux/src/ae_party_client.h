/*
AE_PARTY_CLIENT.H

The game's link to the online party service (network.party_service; tools/ae_party_service): one thread that says hello with the
party key, polls for this player's party, invites and search, and makes the calls the game queues (invite, answer, leave,
QUICK PLAY, ...). Off while the setting is empty: nothing is started, nothing is sent. The wire's rules are game/
ae_party_client_rules.c; the party's are game/ae_party_net_rules.c. Log prefix "ae party:".
*/

#ifndef __AE_PARTY_CLIENT_H
#define __AE_PARTY_CLIENT_H

#include "../game/ae_party_client_rules.h"

enum { AE_PC_OFF, AE_PC_CONNECTING, AE_PC_UP, AE_PC_BACKOFF };

/* whether network.party_service names a service */
int ae_party_client_enabled(void);
/* starts the thread (once; nothing when the setting is empty or the party key cannot be made): 1 when it runs */
int ae_party_client_start(void);
/* what this PC tells the service: the name shown to others and the split-screen seats (1-4); sent with the next hello, and
at once as a seats call when the link is up */
void ae_party_client_identity(const char *name, int seats);
/* AE_PC_OFF / CONNECTING / UP / BACKOFF */
int ae_party_client_state(void);
/* the newest view (copied) and a count that rises with every change: 0 when none yet */
unsigned long ae_party_client_view(struct ae_pc_view *view);
/* this PC's own friend code ("" until the key is read); the code of the install whether or not the service is on */
int ae_party_client_code(char *code, int size);
/* whether network.party_service is a service on this machine (http://127.0.0.1 or localhost: a DEV test), which may name local servers */
int ae_party_client_service_local(void);
/* queues a call: op ("party_invite", "quick_play", ...) with key, value pairs ended by NULL; the session is added; 1 when queued */
int ae_party_client_call(const char *op, ...);
/* the last queued call's outcome as one line ("party_invite: ok", "party_invite: refused unknown_code: ..."), with a counter */
unsigned long ae_party_client_last(char *text, int size);
/* DEV MODE: while on, the first invite that arrives is answered yes at once (staging a party with no screen) */
void ae_party_client_dev_autojoin(int on);
/* ends the session politely (a bye) and stops polling until started again */
void ae_party_client_stop(void);

#endif
