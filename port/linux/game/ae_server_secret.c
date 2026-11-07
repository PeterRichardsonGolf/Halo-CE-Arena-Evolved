/*
AE_SERVER_SECRET.C

See ae_server_secret.h.
*/

#include "cseries.h"

#include "ae_server_secret.h"
#include "ae_server_secret_rules.h"
#include "../src/server_log.h"
#include "../src/server_mode.h"

/* (the dedicated server's: the Android and web builds compile it to nothing, as server_mode.h says) */
#if HALO_SERVER_BUILD


#include <stdio.h>
#include <string.h>

const char *platform_data_root(void);
int posix_server_stdout_is_terminal(void);
int posix_server_write_private_file(const char *path, const void *data, unsigned long size);

static int secret_write(
	char const *path,
	char const *secret)
{
	char line[512];

	if (snprintf(line, sizeof(line), "%s\n", secret) >= (int)sizeof(line))
		return 0;
	return posix_server_write_private_file(path, line, strlen(line));
}

void ae_server_secret_show(
	char const *what,
	char const *file_name,
	char const *secret)
{
	char path[1024];

	if (posix_server_stdout_is_terminal())
	{
		server_log_private(_server_log_info, "server", "%s: %s", what, secret);
		return;
	}
	if (ae_server_secret_path(platform_data_root(), file_name, path, sizeof(path)) && secret_write(path, secret))
		server_log(_server_log_info, "server", "%s is in %s (mode 0600)", what, path);
	else
		server_log(_server_log_warning, "server", "%s could not be written to a file (the data folder is not writable): "
			"it is not shown", what);
}

#endif /* HALO_SERVER_BUILD */
