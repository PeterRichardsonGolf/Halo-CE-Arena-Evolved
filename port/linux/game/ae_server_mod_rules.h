#ifndef AE_SERVER_MOD_RULES_H
#define AE_SERVER_MOD_RULES_H

/*
The pure rules of the dedicated server's moderation depth and abuse limits (0.7.7; docs/dedicated_server.md,
"Admins, access and the whitelist", "Abuse limits", "Alerts and hooks"): admins.txt and command_access.txt rows,
immunity, the whitelist, bans by network and by name, a chat flood and a ping watch, the event hooks' rules, the
alert webhook's text and its triggers, the first-run claim. No game, socket or file dependency: tools/test_ae_units.py
compiles ae_server_mod_rules_test.c with ae_server_admin_rules.c on its own.
*/

#include "ae_server_admin_rules.h"

enum
{
	AE_ADMIN_MAXIMUM = 64,
	AE_ADMIN_LINE_SIZE = 384,
	AE_IMMUNITY_MAXIMUM = 99,
	/* the roles, as the server's numbers (viewer 0 .. owner 3); an admin row may be at most an admin */
	AE_MOD_ROLE_OWNER = 3,
	AE_ACCESS_MAXIMUM = 64,
	AE_COMMAND_NAME_MAXIMUM = 32,
	AE_WHITELIST_MAXIMUM = 512,
	AE_WHITELIST_VALUE_SIZE = 64,
	AE_PATTERN_MAXIMUM = 31,
	AE_NAME_MAXIMUM = 15,
	AE_BAN_FIELD_SIZE = 128,
	AE_HOOK_MAXIMUM = 32,
	AE_HOOK_COMMANDS_SIZE = 400,
	AE_HOOK_COMMAND_MAXIMUM = 4,
	AE_HOOK_COMMAND_SIZE = 160,
	AE_HOOK_VALUE_SIZE = 64,
	AE_FLOOD_STAMPS = 32,
	/* the window a chat flood is counted in */
	AE_FLOOD_WINDOW_MS = 5000,
	AE_ALERT_KIND_SIZE = 24,
	AE_ALERT_TEXT_SIZE = 240,
	AE_CLAIM_TOKEN_LENGTH = 32,
	AE_CLAIM_PASSWORD_MINIMUM = 10,
	AE_CLAIM_PASSWORD_MAXIMUM = 100,
};

/* ---------- roles and immunity */

/* "viewer", "moderator", "admin" or "owner" -> 0..3; else -1 */
int ae_mod_role_parse(const char *text);
const char *ae_mod_role_text(int role);
/* whether an actor of that role and immunity may act on a target (a player, a ban, a key): the owner on anyone; else
only on a target of a lower role, or of an equal role and a lower immunity number. A target with no role (-1: no
admin row) is below every actor. Ties are refused. */
int ae_mod_may_act(int actor_role, int actor_immunity, int target_role, int target_immunity);
/* a target's strongest standing out of two rows (its address row and its key row): the higher role, then, on equal
roles, the higher immunity. A role below 0 is no row. */
void ae_mod_standing_merge(int *role, int *immunity, int other_role, int other_immunity);

/* ---------- admins.txt: identity <TAB> role <TAB> immunity <TAB> expires <TAB> hash <TAB> allow
identity "name:<login>" (a name and secret that sign in to the web page's API and the remote console; hash is the
secret's HMAC) or "cidr:<list>" (the players, and a machine joining, from those addresses hold the role and the
immunity; no secret), or "key:<64 hex>" (0.7.7 in game, ae_server_ingame.c: the player who signs in with that moderator
key, made by their game from their player key; no secret). expires 0: never. allow (a name row's): the addresses the
login works from. # begins a comment. */
struct ae_admin_record
{
	/* 'n' a login by name, 'c' a network, 'k' a moderator key */
	char kind;
	char name[AE_KEY_NAME_MAXIMUM + 1];
	char cidr[AE_KEY_ALLOW_MAXIMUM + 1];
	/* a key row's key, lower case hex */
	char key[65];
	int role;
	int immunity;
	unsigned long expires;
	char hash[65];
	char allow[AE_KEY_ALLOW_MAXIMUM + 1];
};
int ae_admin_line_format(const struct ae_admin_record *record, char *line, int size);
/* 1 if the line is a row (a comment, an empty line or a bad row: 0) */
int ae_admin_line_parse(const char *line, struct ae_admin_record *record);
/* a row that is dead at and after its expiry */
int ae_admin_expired(const struct ae_admin_record *record, unsigned long now);

/* ---------- command_access.txt: "<command> <role>" a line (# comments); the role a command needs, in place of its own */
int ae_access_line_parse(const char *line, char *command, int size, int *role);

/* ---------- whitelist.txt: "ip:<a.b.c.d[/n]>", "hwid:<hex>", "discord:<digits>" or "name:<player name>" a line
(# begins a comment, after a space or at the start). The first three are checked at the door; a name only once the
player has one (a name is no proof: anyone can take it). */
enum { AE_WHITELIST_IP = 1, AE_WHITELIST_HWID, AE_WHITELIST_DISCORD, AE_WHITELIST_NAME };
struct ae_whitelist_entry
{
	int kind;
	char value[AE_WHITELIST_VALUE_SIZE];
};
int ae_whitelist_line_parse(const char *line, struct ae_whitelist_entry *entry);
int ae_whitelist_line_format(const struct ae_whitelist_entry *entry, char *line, int size);
/* the door: the address (host byte order), the hardware id (may be empty) or the Discord id (may be empty) is listed */
int ae_whitelist_door_ok(const struct ae_whitelist_entry *entries, int count, unsigned long address,
	const char *hardware_id, const char *discord_id);
int ae_whitelist_has_names(const struct ae_whitelist_entry *entries, int count);
int ae_whitelist_name_ok(const struct ae_whitelist_entry *entries, int count, const char *name);

/* ---------- bans by network and by name */

/* "a.b.c.d/n" (n 0..32; an address alone is /32): 1 and the text again, written plainly. /0 is refused (every address) */
int ae_cidr_normalise(const char *text, char *kept, int size);
/* a name pattern: printable ASCII, 1 to AE_PATTERN_MAXIMUM characters, * any run and ? any one character, at least one
other character; it matches the whole name, in either case */
int ae_pattern_valid(const char *pattern);
int ae_glob_match(const char *pattern, const char *text);

/* one line of bans.txt (the tab separated fields the game writes: when, ip=, hwid=, discord_username=, discord_id=,
players=, reason=, and for these bans namepat= and until=) */
struct ae_ban_record
{
	char when[24];
	char ip[40];
	char hwid[40];
	char discord_id[24];
	char players[AE_BAN_FIELD_SIZE];
	char reason[AE_BAN_FIELD_SIZE];
	char pattern[AE_PATTERN_MAXIMUM + 1];
	unsigned long until;
};
int ae_ban_line_format(const struct ae_ban_record *record, char *line, int size);
/* 1 if the line holds an ip=, hwid= or namepat= (a comment: 0) */
int ae_ban_line_parse(const char *line, struct ae_ban_record *record);
/* whether the ban is over at that second (until 0: never) */
int ae_ban_over(const struct ae_ban_record *record, unsigned long now);
/* an imported ban's fields checked and made plain: its ip an address, a /n network or "none"; hwid hex or "none"; a
Discord id digits or "none"; text printable; one of ip, hwid, discord id or pattern present. 1 if good */
int ae_ban_record_clean(struct ae_ban_record *record);
/* whether two bans name the same thing (the same address or network, hardware id, Discord id and pattern) */
int ae_ban_same(const struct ae_ban_record *first, const struct ae_ban_record *second);
/* whether the ban's ip field is a network (it has a "/") and holds the address (host byte order) */
int ae_ban_network_holds(const struct ae_ban_record *record, unsigned long address);

/* ---------- a chat flood: more than limit lines in AE_FLOOD_WINDOW_MS */
struct ae_flood
{
	unsigned long stamps[AE_FLOOD_STAMPS];
	int count;
	int head;
};
/* a line at now_ms: 1 if it is the line past the limit (the count is then cleared); limit 0: never */
int ae_flood_note(struct ae_flood *flood, unsigned long now_ms, int limit);

/* ---------- a ping watch: over limit_ms for seconds, without a break */
struct ae_ping_watch
{
	unsigned long over_since;
	int over;
};
/* a ping (milliseconds, negative if unknown) seen at now_ms: 1 when it has been over limit_ms for seconds; limit_ms 0
or an unknown ping clears it */
int ae_ping_watch_note(struct ae_ping_watch *watch, long ping_ms, long limit_ms, long seconds, unsigned long now_ms);

/* ---------- event hooks: "<event>[field=value] -> command; command" a line */
struct ae_hook_rule
{
	char event[AE_KEY_NAME_MAXIMUM + 1];
	char field[24];
	char value[AE_HOOK_VALUE_SIZE];
	char commands[AE_HOOK_COMMANDS_SIZE];
};
int ae_hook_line_parse(const char *line, struct ae_hook_rule *rule);
/* the rule's commands split at ";" (at most AE_HOOK_COMMAND_MAXIMUM) into the buffers: how many */
int ae_hook_split(const struct ae_hook_rule *rule, char commands[][AE_HOOK_COMMAND_SIZE]);
/* a field's text for {field} (empty if the event has none); 1 if it has one */
typedef int (*ae_hook_lookup)(void *context, const char *field, char *value, int size);
/* the command with {field} put in from the event: the value kept to printable ASCII with no quote, ";", "{", "}" or
backslash (each becomes "_"), so that a player's name cannot add a command or end a quoted word; an unknown field is
empty. 1 if it fits */
int ae_hook_expand(const char *command, ae_hook_lookup lookup, void *context, char *out, int size);
/* whether the rule fires for the event: the event's name, and its field if the rule names one */
int ae_hook_fires(const struct ae_hook_rule *rule, const char *event, ae_hook_lookup lookup, void *context);
/* the hooks run at most this many times in a window: 1 if one may run (and is counted) */
int ae_hook_allow(unsigned long *window_start, int *used, unsigned long now_ms, int limit, unsigned long window_ms);

/* ---------- the alert webhook */

/* https://host[:port]/path, or http:// to 127.0.0.1 or localhost (a test's), no spaces or control characters, 256 at most */
int ae_webhook_url_valid(const char *url);
/* text escaped for a JSON string (control characters become spaces); 1 if it fits */
int ae_json_escape(const char *text, char *out, int size);
/* {"source":"arena-evolved-server","server":..,"event":..,"text":..,"content":..,"time":n} */
int ae_alert_body(const char *server, const char *kind, const char *text, unsigned long now, char *out, int size);
/* the starts (unix seconds) in the last window_s seconds before now, counting now's own: a restart loop if >= count */
int ae_restart_loop(const unsigned long *starts, int number, unsigned long now, long window_s, int count);
/* whether an alert of a kind may be sent again: last 0 or min_gap seconds since */
int ae_alert_gap_ok(unsigned long last, unsigned long now, long min_gap);
/* the free space is low: free_bytes under threshold_mb (0: never low) */
int ae_disk_low(unsigned long long free_bytes, long threshold_mb);
/* nobody for hours (0: off) and not alerted yet in this empty spell (alerted 0/1) */
int ae_empty_alert_due(unsigned long empty_since, unsigned long now, long hours, int alerted);

/* ---------- the first-run claim */

/* the token is lowercase hex, AE_CLAIM_TOKEN_LENGTH long */
int ae_claim_token_valid(const char *token);
/* a password the claim sets: AE_CLAIM_PASSWORD_MINIMUM to AE_CLAIM_PASSWORD_MAXIMUM printable ASCII characters */
int ae_claim_password_valid(const char *password);
/* http://host:port/claim, a bind of 0.0.0.0, empty or :: shown as the machine's own name "localhost"; 1 if it fits */
int ae_claim_link(const char *bind, long port, char *out, int size);

#endif
