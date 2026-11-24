#ifndef AE_SERVER_ADMIN_RULES_H
#define AE_SERVER_ADMIN_RULES_H

/*
The pure rules of the dedicated server's admin features (0.7.7): API keys, the restart warnings and the daily
restart, and the backups' names. No game, socket or file dependency: tools/test_ae_units.py compiles
ae_server_admin_rules_test.c on its own.
*/

enum
{
	AE_KEY_ID_LENGTH = 8,
	AE_KEY_SECRET_LENGTH = 48,
	AE_KEY_NAME_MAXIMUM = 32,
	AE_KEY_MAXIMUM = 64,
	AE_KEY_ALLOW_MAXIMUM = 96,
	/* "aek_" + id + "_" + secret */
	AE_KEY_TOKEN_LENGTH = 4 + AE_KEY_ID_LENGTH + 1 + AE_KEY_SECRET_LENGTH,
	AE_KEY_LINE_SIZE = 384,
	/* the roles a key may have, as the server's role numbers (viewer 0, moderator 1, admin 2; never owner 3) */
	AE_KEY_ROLE_MAXIMUM = 2,
	AE_WARNING_STEPS = 4,
	AE_BACKUPS_KEEP_MAXIMUM = 1000,
};

/* a-z0-9_- , 1 to AE_KEY_NAME_MAXIMUM characters */
int ae_key_name_valid(const char *name);
/* "viewer", "moderator" or "admin" -> 0..2; owner and anything else: -1 */
int ae_key_role_parse(const char *text);
const char *ae_key_role_text(int role);
/* a token "aek_<8 hex>_<48 hex>": its id and secret; 0 if it is not one */
int ae_key_token_parse(const char *token, char *id, char *secret);
/* lowercase hex only */
int ae_key_is_hex(const char *text, int length);
/* expires 0: never; else the key is dead at and after that second */
int ae_key_expired(unsigned long expires, unsigned long now);
/* the time of expiry from "days" given now (0 days: never); 0 days or a number 1 to 3650 is valid, else -1 */
long ae_key_expiry(long days, unsigned long now, unsigned long *expires);
/* equal in time that does not depend on where they differ */
int ae_key_equal(const char *first, const char *second, int length);
/* a role of a request may mint a key of at most its own role (the owner's console, role 3, any key) */
int ae_key_role_may_mint(int minter_role, int role);
/* an allow-list "a.b.c.d/n[,a.b.c.d/n...]" (an address alone is /32); empty: any address */
int ae_key_allow_valid(const char *text);
/* whether the IPv4 address (host byte order) is in the list; an empty list allows all */
int ae_key_allow_permits(const char *text, unsigned long address);

/* one key's line of api_keys.txt:
id <TAB> name <TAB> role <TAB> created <TAB> expires <TAB> hash(64 hex) <TAB> allow */
struct ae_key_record
{
	char id[AE_KEY_ID_LENGTH + 1];
	char name[AE_KEY_NAME_MAXIMUM + 1];
	int role;
	unsigned long created;
	unsigned long expires;
	char hash[65];
	char allow[AE_KEY_ALLOW_MAXIMUM + 1];
};
int ae_key_line_format(const struct ae_key_record *record, char *line, int size);
int ae_key_line_parse(const char *line, struct ae_key_record *record);

/* the warnings before a stop or restart: the seconds left at which the next notice is due (60, 30, 10, 5), given the
seconds the countdown began with, the seconds left now, and the last notice sent (the seconds it announced; 0 for
none yet). Returns the number of seconds to announce, or 0 for none due. The first notice is for the whole delay. */
int ae_warning_due(long total, long left, long last_announced);

/* "HH:MM" (24 h) -> minutes since midnight; -1 for a bad or empty text */
int ae_daily_parse(const char *text);
/* seconds from the second of the day to the next time of day (1 to 86400) */
long ae_daily_seconds_until(int minute_of_day, long second_of_day);

/* a backup's file name: <name>.<YYYYMMDD-HHMMSS> from a broken-down time */
int ae_backup_name(const char *name, int year, int month, int day, int hour, int minute, int second, char *text,
	int size);
/* whether a directory entry is a backup of the file (its name, then a dot and a stamp) */
int ae_backup_matches(const char *file, const char *entry);
/* a backup's time from its stamp (YYYYMMDD-HHMMSS, UTC; the entry or the stamp after its last dot): seconds since 1970,
or -1 for no stamp */
long long ae_backup_stamp_time(const char *entry);
/* of count entries (names of one file's backups), which of them are dropped to keep the last keep? Fills drop[i]
with 1 for the older ones; returns how many are dropped. Names sort by their stamp. */
int ae_backup_prune(const char *const *entries, int count, int keep, int *drop);

#endif
