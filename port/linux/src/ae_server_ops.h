#ifndef AE_SERVER_OPS_H
#define AE_SERVER_OPS_H

/*
AE_SERVER_OPS.H

The dedicated server's moderation logins and operations (0.7.7; docs/dedicated_server.md, "Admins, access and the
whitelist", "Alerts and hooks"): admins.txt (name logins and networks), the alert webhook, the first-run claim and the
disk. Rules: game/ae_server_mod_rules.c. Plain types only across this boundary (the game and the platform layer do not
share a heap).
*/

struct server_json;

/* at the server's start: admins.txt read, the start logged for the restart-loop alert, the webhook ready */
void ae_ops_start(void);

/* ---------- admins.txt */

/* a row made: identity "name:<login>" (a secret made and shown once in secret) or "cidr:<list>"; days 0: never expires.
1, else 0 and why. A name already there is replaced (a new secret). */
int ae_ops_admin_add(const char *identity, int role, int immunity, int days, const char *allow, const char *by,
	char *secret, int secret_size, char *message, int message_size);
/* a row taken out: 1, or 0 if there is none */
int ae_ops_admin_remove(const char *identity, const char *by);
/* admins.txt read again (sv_reload, a hand edit): the rows read */
int ae_ops_admin_reload(void);
/* the rows, without their secrets, one a line; 1 if there are some */
int ae_ops_admin_text(char *text, int size);
void ae_ops_admins_json(struct server_json *json);
/* a login by name and secret from an address (host byte order): 1 with the role and immunity; else 0 and the reason (for the
log, never for the caller) */
int ae_ops_admin_login(const char *name, const char *secret, unsigned int address, int *role, int *immunity,
	char *reason, int reason_size);
/* the role and immunity a network row gives an address (the highest of the rows that hold it): 1, or 0 if none */
int ae_ops_admin_by_address(unsigned int address, int *role, int *immunity);
/* (0.7.7) the role and immunity of a moderator key's row (its 64 lower case hex characters): 1, or 0 */
int ae_ops_admin_by_key(const char *key, int *role, int *immunity);
/* (0.7.7) the moderator keys of the rows that have not expired (32 bytes each), at most maximum: their count */
int ae_ops_admin_key_rows(unsigned char (*keys)[32], int maximum);
/* the role and immunity of a login's row by name: 1, or 0 */
int ae_ops_admin_by_name(const char *name, int *role, int *immunity);
unsigned long ae_ops_admin_revision(void);

/* ---------- the alert webhook (server.alert_webhook) */

/* an alert queued to be posted (never blocks): kind a short word (disk_low), text for people. Dropped without a webhook set,
and a kind not sent again within min_gap seconds (0: every one is sent) */
void ae_ops_alert(const char *kind, const char *text, int min_gap);
/* the free space of the data folder's disk in megabytes; -1 if unknown */
int ae_ops_disk_free_mb(void);

/* ---------- the first-run claim (server.claim) */

/* with no web password: a token made, written to <data>/claim-token (mode 0600), the link shown; 1 if a claim is waiting */
int ae_ops_claim_begin(const char *bind, int port);
int ae_ops_claim_pending(void);
/* the token given is the waiting one's (compared in constant time) */
int ae_ops_claim_check(const char *token);
/* the claim done: its token file taken away */
void ae_ops_claim_done(void);

#endif
