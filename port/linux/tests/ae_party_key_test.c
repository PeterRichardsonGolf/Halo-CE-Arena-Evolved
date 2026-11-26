/* ae_party_key.c: the party key is its own file (party.key), made once, read back, and signs only "ae-party-" texts */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ae_party_key.h"
#include "../game/ae_friend_code.h"

static char folder[256];
static int signed_calls;

void platform_log(const char *format, ...) { (void)format; }
void config_folder(char *path, size_t size) { snprintf(path, size, "%s", folder); }
char *config_file_read(const char *path, size_t *size)
{
	FILE *file = fopen(path, "rb");
	char *text;

	if (!file)
		return NULL;
	text = malloc(4096);
	*size = fread(text, 1, 4095, file);
	text[*size] = 0;
	fclose(file);
	return text;
}
void posix_random_bytes(void *buffer, unsigned long size)
{
	static unsigned char next = 1;
	unsigned char *bytes = buffer;

	while (size--)
		*bytes++ = next++;
}
void p2p_hex(const unsigned char *bytes, int size, char *text)
{
	int index;

	for (index = 0; index < size; index++)
		sprintf(text + 2 * index, "%02x", bytes[index]);
}
void p2p_ed25519_public(const unsigned char *seed, unsigned char *public_key, unsigned char *x25519_secret)
{
	int index;

	(void)x25519_secret;
	for (index = 0; index < 32; index++)
		public_key[index] = (unsigned char)(seed[index] ^ 0x5a);
}
void p2p_ed25519_sign(const unsigned char *seed, const unsigned char *public_key, const void *message, int size, unsigned char *signature)
{
	(void)seed; (void)public_key; (void)message; (void)size;
	memset(signature, 9, 64);
	signed_calls++;
}

int main(void)
{
	unsigned char first[32], again[32], signature[64];
	char path[300], player_path[300], line[200], code[AE_FRIEND_CODE_SIZE], other[AE_FRIEND_CODE_SIZE];
	unsigned char player_pub[32];
	struct stat info;
	FILE *file;

	snprintf(folder, sizeof(folder), "/tmp/ae_party_key_test_%d/", (int)getpid());
	mkdir(folder, 0700);
	snprintf(path, sizeof(path), "%sparty.key", folder);
	snprintf(player_path, sizeof(player_path), "%splayer.key", folder);

	/* made on first use, in party.key, readable by its owner alone; player.key is never touched */
	assert(ae_party_key_public(first));
	assert(stat(path, &info) == 0 && (info.st_mode & 077) == 0);
	assert(stat(player_path, &info) != 0);
	file = fopen(path, "rb");
	assert(file && fgets(line, sizeof(line), file) && line[0] == '#');
	fclose(file);

	/* the same key on the next use */
	assert(ae_party_key_public(again) && !memcmp(first, again, 32));

	/* a code from this key differs from one from another key (a player key) */
	memset(player_pub, 0x33, 32);
	assert(ae_friend_code_from_key(first, code) && ae_friend_code_from_key(player_pub, other) && strcmp(code, other));

	/* only party texts are signed */
	assert(ae_party_key_sign("ae-party-hello-2\nhttps://x/party\nabc\nkey", signature) && signed_calls == 1);
	assert(!ae_party_key_sign("opence-link-1 text", signature) && signed_calls == 1);
	assert(!ae_party_key_sign("", signature) && !ae_party_key_sign("ae-party", signature) && !ae_party_key_sign(NULL, signature));
	assert(signed_calls == 1);

	unlink(path);
	rmdir(folder);
	puts("ae_party_key_test: ok");
	return 0;
}
