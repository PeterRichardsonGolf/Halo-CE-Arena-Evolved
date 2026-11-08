/*
AE_SERVER_SECRET_RULES.H

A secret the dedicated server makes (the first admin password, the invite link) is shown on a terminal and nowhere
else: on a pipe, which a service manager keeps in its journal, it goes to a file made with mode 0600 beside the
server's data. Pure but for the file system (port/linux/tests/ae_server_secret_rules_test.c).
*/

#ifndef __AE_SERVER_SECRET_RULES_H
#define __AE_SERVER_SECRET_RULES_H

/* <folder>/<name> into path (size at least 2): 1 if it fits and neither part is empty, else 0 (path empty) */
int ae_server_secret_path(
	char const *folder,
	char const *name,
	char *path,
	int path_size);

/* the secret and a newline written to path, the file made with mode 0600 (an old file is cut to nothing and made
0600 too): 1 written, 0 not */
int ae_server_secret_write(
	char const *path,
	char const *secret);

#endif
