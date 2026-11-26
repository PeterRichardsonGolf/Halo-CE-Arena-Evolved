/*
AE_PARTY_KEY.C

See ae_party_key.h. party.key holds the seed as 64 hex digits on its first line that is not a comment.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_party_key.h"
#include "ae_worker.h"
#include "port_config.h"
#include "p2p_internal.h"
#include "posix.h"
#ifdef HALO_GAME_BROWSER
#include "browser_http.h"
#endif

#ifndef HALO_WINDOWS
#include <fcntl.h>
#include <unistd.h>
#endif

void platform_log(const char *format, ...);

static struct
{
	struct ae_worker_lock *lock;
	int have;
	unsigned char seed[P2P_SEED_SIZE], public_key[32];
} key;

static void path_of(char *path, int size)
{
	char folder[1024];

	config_folder(folder, sizeof(folder));
	snprintf(path, (size_t)size, "%sparty.key", folder);
}

static int hex_digit(char character)
{
	if (character >= '0' && character <= '9')
		return character - '0';
	if (character >= 'a' && character <= 'f')
		return character - 'a' + 10;
	if (character >= 'A' && character <= 'F')
		return character - 'A' + 10;
	return -1;
}

/* a file written whole, its owner's alone where the system has owners: 1 */
static int write_whole(const char *path, const char *text)
{
	size_t size = strlen(text);
#ifdef HALO_WINDOWS
	FILE *file = fopen(path, "wb");
	int written;

	if (!file)
		return 0;
	written = fwrite(text, 1, size, file) == size;
	return fclose(file) == 0 && written;
#else
	size_t written = 0;
	int descriptor = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);

	if (descriptor < 0)
		return 0;
	while (written < size)
	{
		long count = (long)write(descriptor, text + written, size - written);

		if (count <= 0)
			break;
		written += (size_t)count;
	}
	return close(descriptor) == 0 && written == size;
#endif
}

static int read_seed(void)
{
	char path[1100];
	size_t size;
	char *text;
	const char *line;
	int found = 0;

	path_of(path, sizeof(path));
	text = config_file_read(path, &size);
	if (!text)
		return 0;
	for (line = text; *line && !found;)
	{
		const char *end = line + strcspn(line, "\n");
		int index;

		while (*line == ' ' || *line == '\t')
			line++;
		if (*line != '#' && end - line >= 2 * P2P_SEED_SIZE)
		{
			for (index = 0; index < P2P_SEED_SIZE; index++)
			{
				int high = hex_digit(line[2 * index]), low = high >= 0 ? hex_digit(line[2 * index + 1]) : -1;

				if (high < 0 || low < 0)
					break;
				key.seed[index] = (unsigned char)(high << 4 | low);
			}
			found = index == P2P_SEED_SIZE;
		}
		line = *end ? end + 1 : end;
	}
	memset(text, 0, size);
	free(text);
	return found;
}

static int make_seed(void)
{
	char path[1100], hex[2 * P2P_SEED_SIZE + 1], text[400];
	int written;

	path_of(path, sizeof(path));
	/* (Mbed TLS has no locks of its own: the random generator is drawn in turn with its other users) */
#ifdef HALO_GAME_BROWSER
	posix_browser_lock();
#endif
	posix_random_bytes(key.seed, sizeof(key.seed));
#ifdef HALO_GAME_BROWSER
	posix_browser_unlock();
#endif
	p2p_hex(key.seed, P2P_SEED_SIZE, hex);
	snprintf(text, sizeof(text),
		"# This machine's key for the Arena Evolved online party (its friend code). It is not sent to\n"
		"# master servers. Keep it secret: whoever has it is this friend code. Delete it for a new code.\n%s\n", hex);
	memset(hex, 0, sizeof(hex));
	written = write_whole(path, text);
	memset(text, 0, sizeof(text));
	if (!written)
	{
		platform_log("ae party: cannot write %s", path);
		memset(key.seed, 0, sizeof(key.seed));
		return 0;
	}
	platform_log("ae party: made a party key (%s)", path);
	return 1;
}

/* under the lock: the key read, or made; 1 */
static int ready(void)
{
	if (key.have)
		return 1;
	if (!read_seed() && !make_seed())
		return 0;
	p2p_ed25519_public(key.seed, key.public_key, NULL);
	key.have = 1;
	return 1;
}

static int take(void)
{
	if (!key.lock)
		key.lock = ae_worker_lock_new();
	if (!key.lock)
		return 0;
	ae_worker_lock_take(key.lock);
	return 1;
}

int ae_party_key_public(unsigned char *public_key)
{
	int ok;

	if (!take())
		return 0;
	ok = ready();
	if (ok)
		memcpy(public_key, key.public_key, 32);
	ae_worker_lock_give(key.lock);
	return ok;
}

int ae_party_key_sign(const char *text, unsigned char *signature)
{
	int ok;

	if (!text || strncmp(text, "ae-party-", 9) || !take())
		return 0;
	ok = ready();
	if (ok)
		p2p_ed25519_sign(key.seed, key.public_key, text, (int)strlen(text), signature);
	ae_worker_lock_give(key.lock);
	return ok;
}
