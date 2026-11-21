/*
AE_PACK_SHA256_CHECK.C

The weapon pack format's SHA-256 (port/linux/game/ae_pack_format.c, R1:
our own) against the port's p2p_sha256 (port/linux/src/p2p_crypto.c) on the
same buffer, a file's bytes: prints both digests. Built with the platform
layer's flags by tools/test_weapon_pack_format.py, which compares them with
each other and with hashlib (what the two units call and this check never
reaches is left unresolved, as tools/test_linux_port.py's checks do).
*/

#include "p2p_internal.h"
#include "../../port/linux/game/ae_pack_format.h"

#include <stdio.h>
#include <stdlib.h>

static void print_digest(unsigned char const *digest)
{
	int index;

	for (index = 0; index < 32; index++)
		printf("%02x", digest[index]);
	printf("\n");
}

int main(int argc, char **argv)
{
	FILE *file;
	unsigned char *data;
	long size;
	unsigned char ours[32], theirs[32];

	if (argc != 2 || !(file = fopen(argv[1], "rb")))
		return 2;
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = (unsigned char *)malloc((size_t)size + 1);
	if (!data || fread(data, 1, (size_t)size, file) != (size_t)size)
		return 2;
	fclose(file);
	ae_pack_sha256(data, (unsigned int)size, ours);
	p2p_sha256(data, (int)size, theirs);
	print_digest(ours);
	print_digest(theirs);
	free(data);
	return 0;
}
