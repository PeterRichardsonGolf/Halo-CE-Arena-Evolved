/*
AE_SERVER_ADMIN.C

The dedicated server's API keys and backups (ae_server_admin.h). The keys live in <data>/api_keys.txt, one line a
key, the secret kept as an HMAC-SHA-256 only (the secret has 192 bits of its own, so a fast hash is enough). The web
thread reads them and the game's thread changes them: one lock. A secret is never logged, and is shown once, when the
key is made. The backups are the server's own copies of the files it writes (config.toml, bans.txt, api_keys.txt),
made before each write.
*/

#include "platform.h"
#include "port_config.h"
#include "server_mode.h"
#include "ae_server_admin.h"

#if HALO_SERVER_BUILD

#include "p2p_internal.h"
#include "posix.h"
#include "posix_server.h"
#include "server_json.h"
#include "server_log.h"
#include "../game/ae_server_admin_rules.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct key_entry
{
	struct ae_key_record record;
	unsigned long last_used;
	unsigned long last_address;
};

static struct
{
	SDL_Mutex *lock;
	struct key_entry keys[AE_KEY_MAXIMUM];
	int count;
	unsigned long revision;
	int started;
} admin;

static void keys_path(char *path, int size)
{
	snprintf(path, (size_t)size, "%s/api_keys.txt", platform_data_root());
}

static void key_hash(const char *secret, char *hex)
{
	static const unsigned char context[] = "ae-api-key-v1";
	unsigned char digest[P2P_SHA256_SIZE];

	p2p_hmac_sha256(context, (int)sizeof(context) - 1, secret, (int)strlen(secret), digest);
	p2p_hex(digest, P2P_SHA256_SIZE, hex);
}

static void random_hex(int bytes, char *hex)
{
	unsigned char data[32];

	posix_random_bytes(data, (posix_ulong)bytes);
	p2p_hex(data, bytes, hex);
	memset(data, 0, sizeof(data));
}

/* the keys written whole (the lock held): aside, private, then put in place */
static int keys_save(void)
{
	char path[1100], temporary[1200];
	char *text = malloc((size_t)(AE_KEY_MAXIMUM + 1) * AE_KEY_LINE_SIZE);
	size_t length = 0;
	int index, written;

	if (!text)
		return 0;
	text[0] = 0;
	for (index = 0; index < admin.count; index++)
	{
		char line[AE_KEY_LINE_SIZE];

		if (!ae_key_line_format(&admin.keys[index].record, line, sizeof(line)))
			continue;
		memcpy(text + length, line, strlen(line));
		length += strlen(line);
		text[length] = 0;
	}
	keys_path(path, sizeof(path));
	ae_server_backup_file(path);
	snprintf(temporary, sizeof(temporary), "%s.tmp", path);
	written = posix_server_write_private_file(temporary, text, (unsigned long)length) && posix_server_rename(temporary, path);
	free(text);
	return written;
}

static void keys_load(void)
{
	char path[1100], line[AE_KEY_LINE_SIZE + 8];
	FILE *file;

	admin.count = 0;
	keys_path(path, sizeof(path));
	file = fopen(path, "rb");
	if (!file)
		return;
	while (fgets(line, sizeof(line), file) && admin.count < AE_KEY_MAXIMUM)
	{
		struct ae_key_record record;

		if (ae_key_line_parse(line, &record))
		{
			memset(&admin.keys[admin.count], 0, sizeof(admin.keys[0]));
			admin.keys[admin.count++].record = record;
		}
	}
	fclose(file);
}

void ae_server_admin_start(void)
{
	char path[1100];

	if (admin.started)
		return;
	admin.started = 1;
	admin.lock = SDL_CreateMutex();
	platform_translate_path("d:\\bans.txt", path, sizeof(path));
	ae_server_backup_file(path);
	keys_path(path, sizeof(path));
	ae_server_backup_file(path);
	/* (config.toml is backed up as it is written, and here, at the start, from its place) */
	{
		const char *config = getenv("HALO_CONFIG_FILE");

		if (config && config[0])
			ae_server_backup_file(config);
		else
			ae_server_backup_file("config.toml");
	}
	SDL_LockMutex(admin.lock);
	keys_load();
	SDL_UnlockMutex(admin.lock);
	if (admin.count)
		server_log(_server_log_info, "server", "api keys: %d read from api_keys.txt", admin.count);
}

static int key_find_name(const char *name)
{
	int index;

	for (index = 0; index < admin.count; index++)
	{
		if (!strcmp(admin.keys[index].record.name, name))
			return index;
	}
	return -1;
}

int ae_server_key_create(const char *name, int role, long days, const char *allow, int minter_role, const char *by,
	char *token, int token_size, char *message, int message_size)
{
	struct key_entry entry;
	char secret[AE_KEY_SECRET_LENGTH + 1];
	unsigned long now = posix_server_unix_seconds();
	int index, ok = 0;

	if (!admin.lock)
	{
		snprintf(message, (size_t)message_size, "the key store is not ready");
		return 0;
	}
	memset(&entry, 0, sizeof(entry));
	allow = allow ? allow : "";
	if (!ae_key_name_valid(name))
		snprintf(message, (size_t)message_size, "a key's name is 1 to 32 characters of a-z, 0-9, _ and -");
	else if (role < 0 || role > AE_KEY_ROLE_MAXIMUM)
		snprintf(message, (size_t)message_size, "a key's role is viewer, moderator or admin (never owner)");
	else if (!ae_key_role_may_mint(minter_role, role))
		snprintf(message, (size_t)message_size, "a key cannot be made with a role above the caller's");
	else if (ae_key_expiry(days, now, &entry.record.expires) < 0)
		snprintf(message, (size_t)message_size, "days is 0 (never) or 1 to 3650");
	else if (!ae_key_allow_valid(allow))
		snprintf(message, (size_t)message_size, "allow is IPv4 addresses or a.b.c.d/n ranges, separated by commas (96 characters at most)");
	else
	{
		SDL_LockMutex(admin.lock);
		if (key_find_name(name) >= 0)
		{
			snprintf(message, (size_t)message_size, "a key of that name exists: revoke it first");
		}
		else if (admin.count >= AE_KEY_MAXIMUM)
		{
			snprintf(message, (size_t)message_size, "64 keys at most: revoke one first");
		}
		else
		{
			do
			{
				random_hex(AE_KEY_ID_LENGTH / 2, entry.record.id);
				for (index = 0; index < admin.count && strcmp(admin.keys[index].record.id, entry.record.id); index++)
				{
				}
			} while (index < admin.count);
			random_hex(AE_KEY_SECRET_LENGTH / 2, secret);
			snprintf(entry.record.name, sizeof(entry.record.name), "%s", name);
			entry.record.role = role;
			entry.record.created = now;
			snprintf(entry.record.allow, sizeof(entry.record.allow), "%s", allow);
			key_hash(secret, entry.record.hash);
			admin.keys[admin.count++] = entry;
			if (!keys_save())
			{
				admin.count--;
				snprintf(message, (size_t)message_size, "api_keys.txt cannot be written");
			}
			else
			{
				snprintf(token, (size_t)token_size, "aek_%s_%s", entry.record.id, secret);
				admin.revision++;
				ok = 1;
				server_log(_server_log_info, "server", "api key %s made by %s (%s%s%s)", name, by,
					ae_key_role_text(role), entry.record.expires ? ", expires" : "", allow[0] ? ", allow-listed" : "");
			}
		}
		SDL_UnlockMutex(admin.lock);
	}
	memset(secret, 0, sizeof(secret));
	return ok;
}

int ae_server_key_remove(const char *name, const char *by)
{
	int index, found = 0;

	if (!admin.lock)
		return 0;
	SDL_LockMutex(admin.lock);
	index = key_find_name(name);
	if (index >= 0)
	{
		memmove(&admin.keys[index], &admin.keys[index + 1], sizeof(admin.keys[0]) * (size_t)(admin.count - index - 1));
		admin.count--;
		admin.revision++;
		keys_save();
		found = 1;
		server_log(_server_log_info, "server", "api key %s revoked by %s", name, by);
	}
	SDL_UnlockMutex(admin.lock);
	return found;
}

int ae_server_key_check(const char *token, unsigned long address, char *name, int name_size, int *role, char *id,
	char *reason, int reason_size)
{
	char token_id[AE_KEY_ID_LENGTH + 1], secret[AE_KEY_SECRET_LENGTH + 1], hash[65];
	unsigned long now = posix_server_unix_seconds();
	int index, match = -1;

	snprintf(reason, (size_t)reason_size, "a bad key");
	if (!admin.lock || !ae_key_token_parse(token, token_id, secret))
		return 0;
	key_hash(secret, hash);
	memset(secret, 0, sizeof(secret));
	SDL_LockMutex(admin.lock);
	/* (every key is compared, so the time does not tell which id exists) */
	for (index = 0; index < admin.count; index++)
	{
		int same_id = ae_key_equal(admin.keys[index].record.id, token_id, AE_KEY_ID_LENGTH);
		int same_hash = ae_key_equal(admin.keys[index].record.hash, hash, 64);

		if (same_id && same_hash)
			match = index;
	}
	if (match >= 0)
	{
		struct key_entry *entry = &admin.keys[match];

		if (ae_key_expired(entry->record.expires, now))
		{
			snprintf(reason, (size_t)reason_size, "the key %s expired", entry->record.name);
			match = -1;
		}
		else if (!ae_key_allow_permits(entry->record.allow, address))
		{
			snprintf(reason, (size_t)reason_size, "the key %s used from an address its list does not allow",
				entry->record.name);
			match = -1;
		}
		else
		{
			entry->last_used = now;
			entry->last_address = address;
			snprintf(name, (size_t)name_size, "%s", entry->record.name);
			snprintf(id, AE_KEY_ID_LENGTH + 1, "%s", entry->record.id);
			*role = entry->record.role;
		}
	}
	SDL_UnlockMutex(admin.lock);
	return match >= 0;
}

int ae_server_key_alive(const char *id)
{
	unsigned long now = posix_server_unix_seconds();
	int index, alive = 0;

	if (!admin.lock)
		return 0;
	SDL_LockMutex(admin.lock);
	for (index = 0; index < admin.count; index++)
	{
		if (!strcmp(admin.keys[index].record.id, id) && !ae_key_expired(admin.keys[index].record.expires, now))
			alive = 1;
	}
	SDL_UnlockMutex(admin.lock);
	return alive;
}

unsigned long ae_server_key_revision(void)
{
	return admin.revision;
}

void ae_server_keys_json(struct server_json *json)
{
	unsigned long now = posix_server_unix_seconds();
	int index;

	server_json_array_begin(json, "keys");
	if (admin.lock)
		SDL_LockMutex(admin.lock);
	for (index = 0; index < admin.count; index++)
	{
		struct key_entry *entry = &admin.keys[index];

		server_json_object_begin(json, NULL);
		server_json_string(json, "name", entry->record.name);
		server_json_string(json, "id", entry->record.id);
		server_json_string(json, "role", ae_key_role_text(entry->record.role));
		server_json_integer(json, "created", (long long)entry->record.created);
		server_json_integer(json, "expires", (long long)entry->record.expires);
		server_json_boolean(json, "expired", ae_key_expired(entry->record.expires, now));
		server_json_string(json, "allow", entry->record.allow);
		server_json_integer(json, "last_used", (long long)entry->last_used);
		server_json_object_end(json);
	}
	if (admin.lock)
		SDL_UnlockMutex(admin.lock);
	server_json_array_end(json);
}

int ae_server_keys_text(char *text, int size)
{
	unsigned long now = posix_server_unix_seconds();
	int length = 0, index;

	text[0] = 0;
	if (admin.lock)
		SDL_LockMutex(admin.lock);
	for (index = 0; index < admin.count && length + 220 < size; index++)
	{
		struct key_entry *entry = &admin.keys[index];
		char expires[48] = "never";

		if (entry->record.expires)
		{
			if (ae_key_expired(entry->record.expires, now))
				snprintf(expires, sizeof(expires), "EXPIRED");
			else
				snprintf(expires, sizeof(expires), "in %lu days", (unsigned long)((entry->record.expires - now) / 86400UL + 1));
		}
		length += snprintf(text + length, 220, "%s  id %s  %s  expires %s%s%s\n", entry->record.name, entry->record.id,
			ae_key_role_text(entry->record.role), expires, entry->record.allow[0] ? "  allow " : "", entry->record.allow);
	}
	if (admin.lock)
		SDL_UnlockMutex(admin.lock);
	if (!length)
		snprintf(text, (size_t)size, "no keys\n");
	return length > 0;
}

/* ---------- backups */

static void backups_folder(char *path, int size)
{
	snprintf(path, (size_t)size, "%s/backups", platform_data_root());
}

static const char *base_name(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

/* a whole file (up to 16 MB), or NULL */
static unsigned char *read_whole(const char *path, long *size)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data;
	long length;

	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (length < 0 || length > 16L * 1024 * 1024)
	{
		fclose(file);
		return NULL;
	}
	data = malloc((size_t)length + 1);
	if (data && fread(data, 1, (size_t)length, file) != (size_t)length)
	{
		free(data);
		data = NULL;
	}
	fclose(file);
	*size = length;
	return data;
}

static int compare_names(const void *first, const void *second)
{
	return strcmp(*(char *const *)second, *(char *const *)first);
}

/* the file's backups, newest first (the caller frees each and the array) */
static int backups_of(const char *name, char ***entries)
{
	char folder[1100], entry[300];
	void *directory;
	int count = 0, capacity = 0;

	*entries = NULL;
	backups_folder(folder, sizeof(folder));
	directory = posix_directory_open(folder);
	if (!directory)
		return 0;
	while (posix_directory_next(directory, entry, sizeof(entry)))
	{
		if (ae_backup_matches(name, entry))
		{
			if (count == capacity)
			{
				capacity = capacity ? capacity * 2 : 32;
				*entries = realloc(*entries, sizeof(char *) * (size_t)capacity);
			}
			(*entries)[count++] = strdup(entry);
		}
	}
	posix_directory_close(directory);
	if (count)
		qsort(*entries, (size_t)count, sizeof(char *), compare_names);
	return count;
}

void ae_server_backup_file(const char *host_path)
{
	const char *name = base_name(host_path);
	char folder[1100], target[1500], stamp[320];
	unsigned char *old, *last;
	long old_size = 0, last_size = 0;
	char **entries;
	int count, index, keep;
	time_t now = (time_t)posix_server_unix_seconds();
	struct tm moment;

	if (!name[0] || !halo_server_mode())
		return;
	old = read_whole(host_path, &old_size);
	if (!old)
		return;
	backups_folder(folder, sizeof(folder));
	posix_make_directory(folder);
	count = backups_of(name, &entries);
	/* (nothing changed since the newest: no copy) */
	if (count)
	{
		snprintf(target, sizeof(target), "%s/%s", folder, entries[0]);
		last = read_whole(target, &last_size);
		if (last && last_size == old_size && !memcmp(last, old, (size_t)old_size))
		{
			free(last);
			free(old);
			for (index = 0; index < count; index++)
				free(entries[index]);
			free(entries);
			return;
		}
		free(last);
	}
#ifdef _WIN32
	gmtime_s(&moment, &now);
#else
	gmtime_r(&now, &moment);
#endif
	if (ae_backup_name(name, moment.tm_year + 1900, moment.tm_mon + 1, moment.tm_mday, moment.tm_hour, moment.tm_min,
		moment.tm_sec, stamp, sizeof(stamp)))
	{
		snprintf(target, sizeof(target), "%s/%s", folder, stamp);
		if (posix_server_write_file(target, (const char *)old, (unsigned long)old_size))
		{
			posix_server_private_file(target);
			server_log(_server_log_info, "server", "backup: %s kept as backups/%s", name, stamp);
		}
	}
	free(old);
	for (index = 0; index < count; index++)
		free(entries[index]);
	free(entries);
	/* the older ones past the keep dropped: the server's own backups, inside backups/ alone */
	keep = (int)ae_config_integer_held("server.backups_keep"); /* (config_write holds the settings' lock while it backs config.toml up) */
	/* (0 or unread: the default; this runs while the settings load, too) */
	if (keep < 1)
		keep = 20;
	if (keep > AE_BACKUPS_KEEP_MAXIMUM)
		keep = AE_BACKUPS_KEEP_MAXIMUM;
	count = backups_of(name, &entries);
	if (count > keep)
	{
		int *drop = calloc((size_t)count, sizeof(int));

		if (drop)
		{
			ae_backup_prune((const char *const *)entries, count, keep, drop);
			for (index = 0; index < count; index++)
			{
				if (drop[index])
				{
					snprintf(target, sizeof(target), "%s/%s", folder, entries[index]);
					posix_server_remove(target);
				}
			}
			free(drop);
		}
	}
	for (index = 0; index < count; index++)
		free(entries[index]);
	free(entries);
}

void ae_server_backup_game_file(const char *game_path)
{
	char path[1100];

	platform_translate_path(game_path, path, sizeof(path));
	ae_server_backup_file(path);
}

/* the files the server backs up (docs/dedicated_server.md, "Backups") */
static const char *const backup_files[] = { "config.toml", "bans.txt", "api_keys.txt", "admins.txt", "command_access.txt",
	"whitelist.txt" };

/* a file's size in bytes, -1 if it cannot be read */
static long file_size(const char *path)
{
	FILE *file = fopen(path, "rb");
	long length;

	if (!file)
		return -1;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fclose(file);
	return length;
}

void ae_server_backups_json(struct server_json *json)
{
	char folder[1100], path[1500];
	unsigned long file;
	long keep = (long)config_integer("server.backups_keep");

	backups_folder(folder, sizeof(folder));
	server_json_integer(json, "keep", keep < 1 ? 20 : keep);
	server_json_array_begin(json, "backups");
	for (file = 0; file < sizeof(backup_files) / sizeof(backup_files[0]); file++)
	{
		char **entries;
		int count = backups_of(backup_files[file], &entries), index;

		for (index = 0; index < count; index++)
		{
			snprintf(path, sizeof(path), "%s/%s", folder, entries[index]);
			server_json_object_begin(json, NULL);
			server_json_string(json, "file", backup_files[file]);
			server_json_string(json, "name", entries[index]);
			server_json_integer(json, "time", ae_backup_stamp_time(entries[index]));
			server_json_integer(json, "size", file_size(path));
			server_json_object_end(json);
			free(entries[index]);
		}
		free(entries);
	}
	server_json_array_end(json);
}

int ae_server_backups_text(char *text, int size)
{
	const char *const *files = backup_files;
	int length = 0;
	unsigned long file;

	text[0] = 0;
	for (file = 0; file < sizeof(backup_files) / sizeof(backup_files[0]); file++)
	{
		char **entries;
		int count = backups_of(files[file], &entries), index;

		for (index = 0; index < count; index++)
		{
			if (length + 128 < size)
				length += snprintf(text + length, 128, "backups/%s\n", entries[index]);
			free(entries[index]);
		}
		free(entries);
	}
	return length > 0;
}

#else

void ae_server_admin_start(void)
{
}

void ae_server_backup_file(const char *host_path)
{
	(void)host_path;
}

void ae_server_backup_game_file(const char *game_path)
{
	(void)game_path;
}

#endif
