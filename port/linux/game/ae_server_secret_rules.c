/*
AE_SERVER_SECRET_RULES.C

See ae_server_secret_rules.h.
*/

#ifdef AE_SERVER_SECRET_STANDALONE
#define _DEFAULT_SOURCE
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>

#include "ae_server_secret_rules.h"

int ae_server_secret_path(
	char const *folder,
	char const *name,
	char *path,
	int path_size)
{
	int length;

	if (path_size < 2)
		return 0;
	path[0] = 0;
	if (!folder || !folder[0] || !name || !name[0])
		return 0;
	length = snprintf(path, (size_t)path_size, "%s%s%s", folder, folder[strlen(folder) - 1] == '/' ? "" : "/", name);
	if (length < 0 || length >= path_size)
	{
		path[0] = 0;
		return 0;
	}
	return 1;
}

#ifdef AE_SERVER_SECRET_STANDALONE
/* (the program writes through posix_server_write_private_file, posix_server.c: the same open with mode 0600 and the
same fchmod; this copy is what the unit test runs, as the engine's own build does not offer these calls here) */
int ae_server_secret_write(
	char const *path,
	char const *secret)
{
	size_t length = secret ? strlen(secret) : 0;
	int descriptor;
	int ok;

	if (!path || !path[0] || !secret)
		return 0;
	descriptor = open(path, O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR);
	if (descriptor < 0)
		return 0;
	ok = fchmod(descriptor, S_IRUSR | S_IWUSR) == 0 && write(descriptor, secret, length) == (ssize_t)length &&
		write(descriptor, "\n", 1) == 1;
	return close(descriptor) == 0 && ok;
}
#endif
