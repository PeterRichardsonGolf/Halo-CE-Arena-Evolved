/*
AE_SERVER_OPS.C

The dedicated server's moderation logins and operations (ae_server_ops.h). admins.txt lives in <data>/admins.txt, one
row a line; a name row keeps its secret as an HMAC-SHA-256 only (the secret has 160 bits of its own). The web thread
reads the rows (a login) and the game's thread changes them: one lock. The webhook is posted by a thread of its own
from a small queue: the game's frames never wait on it.
*/

#include "platform.h"
#include "port_config.h"
#include "server_mode.h"
#include "ae_server_ops.h"

#if HALO_SERVER_BUILD

#include "p2p_internal.h"
#include "posix.h"
#include "posix_server.h"
#include "server_json.h"
#include "server_log.h"
#include "update.h"
#include "ae_server_admin.h"
#include "../game/ae_server_mod_rules.h"
#include "../game/ae_server_ingame_rules.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __linux__
#include <sys/statvfs.h>
#endif

enum
{
	ALERT_QUEUE = 16,
	ALERT_URL_SIZE = 260,
	ALERT_BODY_SIZE = 1024,
	ALERT_KINDS = 16,
	RESTART_LOG_KEEP = 20,
};

struct alert
{
	char url[ALERT_URL_SIZE];
	char body[ALERT_BODY_SIZE];
};

static struct
{
	SDL_Mutex *lock;
	struct ae_admin_record rows[AE_ADMIN_MAXIMUM];
	int count;
	unsigned long revision;
	int started;
	/* the alert queue */
	SDL_Mutex *alert_lock;
	SDL_Semaphore *alert_ready;
	SDL_Thread *alert_thread;
	struct alert queue[ALERT_QUEUE];
	int queue_head, queue_count;
	char kinds[ALERT_KINDS][AE_ALERT_KIND_SIZE];
	unsigned long kind_times[ALERT_KINDS];
	/* the claim */
	int claim_pending;
	char claim_token[AE_CLAIM_TOKEN_LENGTH + 1];
} ops;

static void data_path(const char *name, char *path, int size)
{
	snprintf(path, (size_t)size, "%s/%s", platform_data_root(), name);
}

/* ---------- admins.txt */

static void secret_hash(const char *secret, char *hex)
{
	static const unsigned char context[] = "ae-admin-v1";
	unsigned char digest[P2P_SHA256_SIZE];

	p2p_hmac_sha256(context, (int)sizeof(context) - 1, secret, (int)strlen(secret), digest);
	p2p_hex(digest, P2P_SHA256_SIZE, hex);
}

static void identity_text(const struct ae_admin_record *row, char *text, int size)
{
	snprintf(text, (size_t)size, "%s:%s", row->kind == 'n' ? "name" : row->kind == 'k' ? "key" : "cidr",
		row->kind == 'n' ? row->name : row->kind == 'k' ? row->key : row->cidr);
}

static int admins_save(void)
{
	char path[1100], temporary[1200];
	char *text = malloc((size_t)(AE_ADMIN_MAXIMUM + 1) * AE_ADMIN_LINE_SIZE + 256);
	size_t length;
	int index, written;

	if (!text)
		return 0;
	strcpy(text, "# Arena Evolved admins: identity, role, immunity, expires, secret hash, allow. Edit with sv_admin_add and sv_admin_remove, or by hand and sv_reload.\n");
	length = strlen(text);
	for (index = 0; index < ops.count; index++)
	{
		char line[AE_ADMIN_LINE_SIZE];

		if (!ae_admin_line_format(&ops.rows[index], line, sizeof(line)))
			continue;
		memcpy(text + length, line, strlen(line));
		length += strlen(line);
		text[length] = 0;
	}
	data_path("admins.txt", path, sizeof(path));
	ae_server_backup_file(path);
	snprintf(temporary, sizeof(temporary), "%s.tmp", path);
	written = posix_server_write_private_file(temporary, text, (unsigned long)length) && posix_server_rename(temporary, path);
	free(text);
	return written;
}

static int admins_load(void)
{
	char path[1100], line[AE_ADMIN_LINE_SIZE + 8];
	FILE *file;

	ops.count = 0;
	data_path("admins.txt", path, sizeof(path));
	file = fopen(path, "rb");
	if (!file)
		return 0;
	while (fgets(line, sizeof(line), file) && ops.count < AE_ADMIN_MAXIMUM)
	{
		struct ae_admin_record row;

		if (ae_admin_line_parse(line, &row))
			ops.rows[ops.count++] = row;
	}
	fclose(file);
	return ops.count;
}

static int admin_find(const char *identity)
{
	int index;
	char text[AE_KEY_ALLOW_MAXIMUM + 16];

	for (index = 0; index < ops.count; index++)
	{
		identity_text(&ops.rows[index], text, sizeof(text));
		if (!strcmp(text, identity))
			return index;
	}
	return -1;
}

int ae_ops_admin_add(const char *identity, int role, int immunity, int days, const char *allow, const char *by,
	char *secret, int secret_size, char *message, int message_size)
{
	struct ae_admin_record row;
	unsigned long now = posix_server_unix_seconds();
	char made[41];
	char lowered[72];
	int index, ok = 0;

	secret[0] = 0;
	memset(&row, 0, sizeof(row));
	allow = allow ? allow : "";
	if (!ops.lock || !identity)
	{
		snprintf(message, (size_t)message_size, "the admin store is not ready");
		return 0;
	}
	if (role < 0 || role > 2)
		snprintf(message, (size_t)message_size, "an admin row's role is viewer, moderator or admin (never owner)");
	else if (immunity < 0 || immunity > AE_IMMUNITY_MAXIMUM)
		snprintf(message, (size_t)message_size, "immunity is 0 to %d", AE_IMMUNITY_MAXIMUM);
	else if (ae_key_expiry(days, now, &row.expires) < 0)
		snprintf(message, (size_t)message_size, "days is 0 (never) or 1 to 3650");
	else if (!ae_key_allow_valid(allow))
		snprintf(message, (size_t)message_size, "allow is IPv4 addresses or a.b.c.d/n ranges, separated by commas");
	else if (!strncmp(identity, "name:", 5) && ae_key_name_valid(identity + 5))
	{
		row.kind = 'n';
		snprintf(row.name, sizeof(row.name), "%s", identity + 5);
		snprintf(row.allow, sizeof(row.allow), "%s", allow);
		ok = 1;
	}
	else if (!strncmp(identity, "cidr:", 5) && identity[5] && ae_key_allow_valid(identity + 5))
	{
		row.kind = 'c';
		snprintf(row.cidr, sizeof(row.cidr), "%s", identity + 5);
		ok = 1;
	}
	else if (!strncmp(identity, "key:", 4) && ae_ingame_key_hex(identity + 4, row.key, (int)sizeof(row.key)))
	{
		/* (0.7.7: a player's moderator key, ae_server_ingame.c; the text kept lower case, so it is found again) */
		row.kind = 'k';
		snprintf(lowered, sizeof(lowered), "key:%s", row.key);
		identity = lowered;
		ok = 1;
	}
	else
		snprintf(message, (size_t)message_size, "identity is name:<login> (a-z, 0-9, _ and -), cidr:<a.b.c.d/n[,...]> "
			"or key:<a player's moderator key, 64 hex>");
	if (!ok)
		return 0;
	ok = 0;
	row.role = role;
	row.immunity = immunity;
	if (row.kind == 'n')
	{
		unsigned char data[20];

		posix_random_bytes(data, sizeof(data));
		p2p_hex(data, (int)sizeof(data), made);
		memset(data, 0, sizeof(data));
		secret_hash(made, row.hash);
	}
	SDL_LockMutex(ops.lock);
	index = -1;
	{
		char text[AE_KEY_ALLOW_MAXIMUM + 16];

		snprintf(text, sizeof(text), "%s", identity);
		index = admin_find(text);
	}
	if (index < 0 && ops.count >= AE_ADMIN_MAXIMUM)
	{
		snprintf(message, (size_t)message_size, "%d admin rows at most", AE_ADMIN_MAXIMUM);
	}
	else
	{
		struct ae_admin_record previous;
		int was = ops.count;

		if (index >= 0)
		{
			previous = ops.rows[index];
			ops.rows[index] = row;
		}
		else
		{
			ops.rows[ops.count++] = row;
		}
		if (!admins_save())
		{
			if (index >= 0)
				ops.rows[index] = previous;
			else
				ops.count = was;
			snprintf(message, (size_t)message_size, "admins.txt cannot be written");
		}
		else
		{
			ops.revision++;
			ok = 1;
			if (row.kind == 'n')
				snprintf(secret, (size_t)secret_size, "%s", made);
			server_log(_server_log_info, "server", "admin row %s made by %s (%s, immunity %d)", identity, by,
				ae_mod_role_text(role), immunity);
		}
	}
	SDL_UnlockMutex(ops.lock);
	memset(made, 0, sizeof(made));
	return ok;
}

int ae_ops_admin_remove(const char *identity, const char *by)
{
	int index, found = 0;

	if (!ops.lock || !identity)
		return 0;
	SDL_LockMutex(ops.lock);
	index = admin_find(identity);
	if (index >= 0)
	{
		memmove(&ops.rows[index], &ops.rows[index + 1], sizeof(ops.rows[0]) * (size_t)(ops.count - index - 1));
		ops.count--;
		ops.revision++;
		admins_save();
		found = 1;
		server_log(_server_log_info, "server", "admin row %s taken out by %s", identity, by);
	}
	SDL_UnlockMutex(ops.lock);
	return found;
}

int ae_ops_admin_reload(void)
{
	int count;

	if (!ops.lock)
		return 0;
	SDL_LockMutex(ops.lock);
	count = admins_load();
	ops.revision++;
	SDL_UnlockMutex(ops.lock);
	return count;
}

int ae_ops_admin_text(char *text, int size)
{
	unsigned long now = posix_server_unix_seconds();
	int length = 0, index;

	text[0] = 0;
	if (ops.lock)
		SDL_LockMutex(ops.lock);
	for (index = 0; index < ops.count && length + 260 < size; index++)
	{
		const struct ae_admin_record *row = &ops.rows[index];
		char identity[AE_KEY_ALLOW_MAXIMUM + 16], expires[40] = "never";

		identity_text(row, identity, sizeof(identity));
		if (row->expires)
			snprintf(expires, sizeof(expires), ae_admin_expired(row, now) ? "EXPIRED" : "in %lu days",
				(unsigned long)((row->expires - now) / 86400UL + 1));
		length += snprintf(text + length, 260, "%s  %s  immunity %d  expires %s%s%s\n", identity, ae_mod_role_text(row->role),
			row->immunity, expires, row->allow[0] ? "  allow " : "", row->allow);
	}
	if (ops.lock)
		SDL_UnlockMutex(ops.lock);
	if (!length)
		snprintf(text, (size_t)size, "no admin rows\n");
	return length > 0;
}

void ae_ops_admins_json(struct server_json *json)
{
	unsigned long now = posix_server_unix_seconds();
	int index;

	server_json_array_begin(json, "admins");
	if (ops.lock)
		SDL_LockMutex(ops.lock);
	for (index = 0; index < ops.count; index++)
	{
		const struct ae_admin_record *row = &ops.rows[index];
		char identity[AE_KEY_ALLOW_MAXIMUM + 16];

		identity_text(row, identity, sizeof(identity));
		server_json_object_begin(json, NULL);
		server_json_string(json, "identity", identity);
		server_json_string(json, "role", ae_mod_role_text(row->role));
		server_json_integer(json, "immunity", row->immunity);
		server_json_integer(json, "expires", (long long)row->expires);
		server_json_boolean(json, "expired", ae_admin_expired(row, now));
		server_json_string(json, "allow", row->allow);
		server_json_object_end(json);
	}
	if (ops.lock)
		SDL_UnlockMutex(ops.lock);
	server_json_array_end(json);
}

int ae_ops_admin_login(const char *name, const char *secret, unsigned int address, int *role, int *immunity,
	char *reason, int reason_size)
{
	unsigned long now = posix_server_unix_seconds();
	char hash[65];
	int index, match = -1;

	snprintf(reason, (size_t)reason_size, "a bad admin login");
	if (!ops.lock || !ae_key_name_valid(name) || !secret || !secret[0] || strlen(secret) > 100)
		return 0;
	secret_hash(secret, hash);
	SDL_LockMutex(ops.lock);
	for (index = 0; index < ops.count; index++)
	{
		const struct ae_admin_record *row = &ops.rows[index];

		if (row->kind == 'n' && !strcmp(row->name, name) && ae_key_equal(row->hash, hash, 64))
			match = index;
	}
	if (match >= 0)
	{
		const struct ae_admin_record *row = &ops.rows[match];

		if (ae_admin_expired(row, now))
		{
			snprintf(reason, (size_t)reason_size, "the admin %s expired", row->name);
			match = -1;
		}
		else if (!ae_key_allow_permits(row->allow, address))
		{
			snprintf(reason, (size_t)reason_size, "the admin %s signed in from an address its list does not allow", row->name);
			match = -1;
		}
		else
		{
			*role = row->role;
			*immunity = row->immunity;
		}
	}
	SDL_UnlockMutex(ops.lock);
	return match >= 0;
}

int ae_ops_admin_by_address(unsigned int address, int *role, int *immunity)
{
	unsigned long now = posix_server_unix_seconds();
	int index, found = 0;

	if (!ops.lock || !address)
		return 0;
	SDL_LockMutex(ops.lock);
	for (index = 0; index < ops.count; index++)
	{
		const struct ae_admin_record *row = &ops.rows[index];

		if (row->kind != 'c' || ae_admin_expired(row, now) || !ae_key_allow_permits(row->cidr, address))
			continue;
		if (!found || row->role > *role || (row->role == *role && row->immunity > *immunity))
		{
			*role = row->role;
			*immunity = row->immunity;
			found = 1;
		}
	}
	SDL_UnlockMutex(ops.lock);
	return found;
}

int ae_ops_admin_by_key(const char *key, int *role, int *immunity)
{
	unsigned long now = posix_server_unix_seconds();
	int index, found = 0;

	if (!ops.lock || !key)
		return 0;
	SDL_LockMutex(ops.lock);
	for (index = 0; index < ops.count; index++)
	{
		const struct ae_admin_record *row = &ops.rows[index];

		if (row->kind == 'k' && !strcmp(row->key, key) && !ae_admin_expired(row, now))
		{
			*role = row->role;
			*immunity = row->immunity;
			found = 1;
		}
	}
	SDL_UnlockMutex(ops.lock);
	return found;
}

int ae_ops_admin_key_rows(unsigned char (*keys)[32], int maximum)
{
	unsigned long now = posix_server_unix_seconds();
	int index, count = 0;

	if (!ops.lock || !keys)
		return 0;
	SDL_LockMutex(ops.lock);
	for (index = 0; index < ops.count && count < maximum; index++)
	{
		const struct ae_admin_record *row = &ops.rows[index];

		if (row->kind == 'k' && !ae_admin_expired(row, now) && ae_ingame_key_bytes(row->key, keys[count]))
			count++;
	}
	SDL_UnlockMutex(ops.lock);
	return count;
}

int ae_ops_admin_by_name(const char *name, int *role, int *immunity)
{
	unsigned long now = posix_server_unix_seconds();
	int index, found = 0;

	if (!ops.lock || !name)
		return 0;
	SDL_LockMutex(ops.lock);
	for (index = 0; index < ops.count; index++)
	{
		const struct ae_admin_record *row = &ops.rows[index];

		if (row->kind == 'n' && !strcmp(row->name, name) && !ae_admin_expired(row, now))
		{
			*role = row->role;
			*immunity = row->immunity;
			found = 1;
		}
	}
	SDL_UnlockMutex(ops.lock);
	return found;
}

unsigned long ae_ops_admin_revision(void)
{
	return ops.revision;
}

/* ---------- the alert webhook */

static int alert_thread(void *context)
{
	(void)context;
	for (;;)
	{
		struct alert alert;
		int tries;

		SDL_WaitSemaphore(ops.alert_ready);
		SDL_LockMutex(ops.alert_lock);
		if (!ops.queue_count)
		{
			SDL_UnlockMutex(ops.alert_lock);
			continue;
		}
		alert = ops.queue[ops.queue_head];
		ops.queue_head = (ops.queue_head + 1) % ALERT_QUEUE;
		ops.queue_count--;
		SDL_UnlockMutex(ops.alert_lock);
		for (tries = 0; tries < 3; tries++)
		{
			struct update_response response;
			char error[200] = "";
			int sent;

			memset(&response, 0, sizeof(response));
			sent = update_https_request("POST", alert.url, "Content-Type: application/json\r\n", alert.body,
				(unsigned long)strlen(alert.body), 4096, &response, error, sizeof(error));
			free(response.body);
			if (sent && response.status >= 200 && response.status < 300)
				break;
			if (tries == 2)
				server_log(_server_log_warning, "server", "alert webhook: not delivered (%s)", sent ? "the webhook answered an error" : error);
			else
				SDL_Delay(2000 * (Uint32)(tries + 1));
		}
	}
	return 0;
}

static void alert_thread_ensure(void)
{
	if (!ops.alert_thread)
	{
		ops.alert_thread = SDL_CreateThread(alert_thread, "alert webhook", NULL);
		if (ops.alert_thread)
			SDL_DetachThread(ops.alert_thread);
	}
}

void ae_ops_alert(const char *kind, const char *text, int min_gap)
{
	const char *url = config_string("server.alert_webhook");
	unsigned long now = posix_server_unix_seconds();
	struct alert alert;
	int index, slot = -1;

	if (!ops.alert_lock || !url[0] || !ae_webhook_url_valid(url))
		return;
	SDL_LockMutex(ops.alert_lock);
	for (index = 0; index < ALERT_KINDS; index++)
	{
		if (!strcmp(ops.kinds[index], kind))
		{
			slot = index;
			break;
		}
		if (slot < 0 && !ops.kinds[index][0])
			slot = index;
	}
	if (slot >= 0 && min_gap > 0 && !ae_alert_gap_ok(ops.kind_times[slot], now, min_gap))
	{
		SDL_UnlockMutex(ops.alert_lock);
		return;
	}
	if (slot >= 0)
	{
		snprintf(ops.kinds[slot], AE_ALERT_KIND_SIZE, "%s", kind);
		ops.kind_times[slot] = now;
	}
	if (ops.queue_count >= ALERT_QUEUE)
	{
		SDL_UnlockMutex(ops.alert_lock);
		return;
	}
	alert_thread_ensure();
	snprintf(alert.url, sizeof(alert.url), "%s", url);
	if (ae_alert_body(config_string("server.name"), kind, text, now, alert.body, sizeof(alert.body)))
	{
		ops.queue[(ops.queue_head + ops.queue_count) % ALERT_QUEUE] = alert;
		ops.queue_count++;
		SDL_SignalSemaphore(ops.alert_ready);
	}
	SDL_UnlockMutex(ops.alert_lock);
	server_log(_server_log_info, "server", "alert %s: %s", kind, text);
}

int ae_ops_disk_free_mb(void)
{
#ifdef __linux__
	struct statvfs info;

	if (statvfs(platform_data_root(), &info) != 0)
		return -1;
	return (int)((unsigned long long)info.f_bavail * info.f_frsize / (1024ULL * 1024ULL));
#else
	return -1;
#endif
}

/* the start logged (when a webhook is set), and a restart loop alerted */
static void restart_check(void)
{
	char path[1100], text[RESTART_LOG_KEEP * 16 + 16];
	unsigned long starts[RESTART_LOG_KEEP + 1];
	unsigned long now = posix_server_unix_seconds();
	int count = 0, index;
	FILE *file;
	long limit = config_integer("server.alert_restart_loop");

	if (!config_string("server.alert_webhook")[0])
		return;
	data_path("server-starts.log", path, sizeof(path));
	file = fopen(path, "rb");
	if (file)
	{
		char line[32];

		while (fgets(line, sizeof(line), file))
		{
			unsigned long value = strtoul(line, NULL, 10);

			if (value && count < RESTART_LOG_KEEP)
				starts[count++] = value;
		}
		fclose(file);
	}
	if (count == RESTART_LOG_KEEP)
	{
		memmove(starts, starts + 1, sizeof(starts[0]) * (RESTART_LOG_KEEP - 1));
		count--;
	}
	starts[count++] = now;
	text[0] = 0;
	for (index = 0; index < count; index++)
		snprintf(text + strlen(text), sizeof(text) - strlen(text), "%lu\n", starts[index]);
	posix_server_write_file(path, text, (unsigned long)strlen(text));
	if (limit > 0 && ae_restart_loop(starts, count, now, 600, (int)limit))
	{
		char message[AE_ALERT_TEXT_SIZE];

		snprintf(message, sizeof(message), "the server has started %ld times or more in 10 minutes", limit);
		ae_ops_alert("restart_loop", message, 0);
	}
}

/* ---------- the first-run claim */

static void claim_path(char *path, int size)
{
	data_path("claim-token", path, size);
}

int ae_ops_claim_begin(const char *bind, int port)
{
	unsigned char data[16];
	char path[1100], link[160];

	posix_random_bytes(data, sizeof(data));
	p2p_hex(data, (int)sizeof(data), ops.claim_token);
	memset(data, 0, sizeof(data));
	claim_path(path, sizeof(path));
	if (!posix_server_write_private_file(path, ops.claim_token, (unsigned long)strlen(ops.claim_token)))
	{
		server_log(_server_log_warning, "server", "claim: %s cannot be written: no claim is waiting (set server.web_password)", path);
		memset(ops.claim_token, 0, sizeof(ops.claim_token));
		return 0;
	}
	ops.claim_pending = 1;
	ae_claim_link(bind, port, link, sizeof(link));
	if (posix_server_stdout_is_terminal())
		server_log_private(_server_log_info, "server", "this server is not claimed yet. Open %s and give this token to set the "
			"admin password: %s", link, ops.claim_token);
	else
		server_log(_server_log_info, "server", "this server is not claimed yet. Open %s and give the token in %s (mode 0600) to "
			"set the admin password", link, path);
	return 1;
}

int ae_ops_claim_pending(void)
{
	return ops.claim_pending;
}

int ae_ops_claim_check(const char *token)
{
	if (!ops.claim_pending || !ae_claim_token_valid(token))
		return 0;
	return ae_key_equal(token, ops.claim_token, AE_CLAIM_TOKEN_LENGTH);
}

void ae_ops_claim_done(void)
{
	char path[1100];

	claim_path(path, sizeof(path));
	remove(path);
	memset(ops.claim_token, 0, sizeof(ops.claim_token));
	ops.claim_pending = 0;
	server_log(_server_log_info, "server", "the server was claimed: the admin password is set");
}

/* ---------- start */

void ae_ops_start(void)
{
	char path[1100];

	if (ops.started)
		return;
	ops.started = 1;
	ops.lock = SDL_CreateMutex();
	ops.alert_lock = SDL_CreateMutex();
	ops.alert_ready = SDL_CreateSemaphore(0);
	data_path("admins.txt", path, sizeof(path));
	ae_server_backup_file(path);
	data_path("command_access.txt", path, sizeof(path));
	ae_server_backup_file(path);
	data_path("whitelist.txt", path, sizeof(path));
	ae_server_backup_file(path);
	SDL_LockMutex(ops.lock);
	admins_load();
	SDL_UnlockMutex(ops.lock);
	if (ops.count)
		server_log(_server_log_info, "server", "admins: %d rows read from admins.txt", ops.count);
	if (config_string("server.alert_webhook")[0])
	{
		if (!ae_webhook_url_valid(config_string("server.alert_webhook")))
		{
			server_log(_server_log_warning, "server", "server.alert_webhook is not an https:// address (or http:// to this machine): no alerts are sent");
		}
		else
		{
			restart_check();
		}
	}
}

#else

void ae_ops_start(void) {}

#endif
