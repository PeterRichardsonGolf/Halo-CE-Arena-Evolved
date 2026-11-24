#ifndef AE_SERVER_ADMIN_H
#define AE_SERVER_ADMIN_H

/*
AE_SERVER_ADMIN.H

The dedicated server's API keys and backups (0.7.7; docs/dedicated_server.md, "API keys" and "Backups"; rules in
game/ae_server_admin_rules.c).
*/

struct server_json;

/* at the server's start: backs config.toml, bans.txt and api_keys.txt up, then reads the keys */
void ae_server_admin_start(void);

/* ---------- keys */

/* a key made: its token in token (AE_KEY_TOKEN_LENGTH + 1) shown once; 1 and the token, else 0 and message */
int ae_server_key_create(const char *name, int role, long days, const char *allow, int minter_role, const char *by,
	char *token, int token_size, char *message, int message_size);
/* a key revoked at once: 1, or 0 if there is none of that name */
int ae_server_key_remove(const char *name, const char *by);
/* a token checked for the address (host byte order): 1 with the key's name, role and id; 0 wrong, expired or from an
address its list does not allow (the reason for the log, never for the caller) */
int ae_server_key_check(const char *token, unsigned long address, char *name, int name_size, int *role, char *id,
	char *reason, int reason_size);
/* whether the key of that id is still valid (not revoked, not expired) */
int ae_server_key_alive(const char *id);
/* changes with every key made or revoked */
unsigned long ae_server_key_revision(void);
/* the keys, without their secrets: "keys" array in json */
void ae_server_keys_json(struct server_json *json);
/* the keys, as text lines for the console (into the caller's buffer: the game and the platform layer do not share
a heap); 1 if there are some */
int ae_server_keys_text(char *text, int size);

/* ---------- backups */

/* the file (a host path) copied to <data>/backups/<name>.<YYYYMMDD-HHMMSS> (UTC) unless the newest backup of it holds
the same bytes; the older ones past server.backups_keep dropped. Never fails the caller. */
void ae_server_backup_file(const char *host_path);
/* the same for a path of the game's ("d:\\bans.txt") */
void ae_server_backup_game_file(const char *game_path);
/* the backups for the web page: "keep" and a "backups" array of {file, name, time (unix seconds, UTC), size (bytes)}, each
file's newest first (GET /api/backups) */
void ae_server_backups_json(struct server_json *json);
/* the backups, newest first, one line each, into the caller's buffer; 1 if there are some */
int ae_server_backups_text(char *text, int size);

#endif
