/*
AE_SERVER_SECRET.H

(preview 2.1) A secret the dedicated server makes: on a terminal it is shown as before (server_log_private, the
standard output alone); anywhere else (a service's journal, a container's log) it is written to <data root>/<file_name>
with mode 0600 and only the path is logged. Rules: ae_server_secret_rules.c.
*/

#ifndef __AE_SERVER_SECRET_H
#define __AE_SERVER_SECRET_H

void ae_server_secret_show(
	char const *what,
	char const *file_name,
	char const *secret);

#endif
