/*
AE_MOTD_FETCH.H

Arena Evolved's MESSAGE OF THE DAY from the masters (menus phase 1; docs/master_protocol.md "Other endpoints"): a worker
asks each master the player's machine asks (master_list_client_masters) for GET /v1/motd and /v1/motd.sig, and checks
the body with that master's pinned key (ae_motd_rules.h). The menus wait at most 4 s for it; no answer, a refused one
(signature, expired) or motd_offline: none (the saved message, else the release's notes).
Logs "ae motd: fetched rev N", "ae motd: rejected (signature)" / "(expired)" / "(malformed)", "ae motd: no answer".
*/

#ifndef __AE_MOTD_FETCH_H
#define __AE_MOTD_FETCH_H

struct ae_motd;

/* starts the worker once (later calls: nothing) */
void ae_motd_fetch_start(void);
/* 1 done with a message (out filled, with its body and signature for the cache), 0 done with none, -1 still asking */
int ae_motd_fetch_result(struct ae_motd *out, char *body, int body_size, char *signature, int signature_size);
/* a saved body and signature checked again with the masters' keys (a cache is trusted only as signed): AE_MOTD_OK
and out, else why not */
int ae_motd_fetch_check_saved(const char *body, const char *signature, struct ae_motd *out);
/* (DEV MODE's motd_offline) asks nothing until the game restarts */
void ae_motd_fetch_offline(void);

#endif
