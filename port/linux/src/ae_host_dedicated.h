/*
AE_HOST_DEDICATED.H

HOST A SERVER's process work (0.7.7; the menus' side: port/linux/game/ae_host_server.c): the server started as a process
of its own with no window (this program with --server and its settings file), its standard input kept for commands
while this AE runs (sv_stop), whether it lives, its stop, a free port, a folder linked, a private file written. Plain
types only across this boundary. Linux and macOS: posix_spawn in a session of its own; Windows (port/windows/src/win32_ae_host.c): CreateProcess with no
window, its input a pipe, its stop a named event (Android and the browser build: not supported; ae_host_dedicated_supported
says so and the page tells the player).
*/

#ifndef AE_HOST_DEDICATED_H
#define AE_HOST_DEDICATED_H

/* whether this build starts a server from the menus */
int ae_host_dedicated_supported(void);
/* whether nothing holds the port on this PC (UDP or TCP, every address) */
int ae_host_dedicated_port_free(int port, int tcp);
/* starts this program as a server: --server --config <config>, in folder, its output to log, the environment this
one's with each "NAME=VALUE" of extra added (secrets travel here, never in a file). Its pid, or 0 and why */
long ae_host_dedicated_spawn(char const *config, char const *folder, char const *log, char const *const *extra,
	int extra_count, char *why, int why_size);
/* whether the process lives (and is not a zombie of ours) */
int ae_host_dedicated_alive(long pid);
/* the process's start time as a number that differs between two processes that held the same pid (Linux: field 22 of
/proc/<pid>/stat; Windows: the creation time of GetProcessTimes); 0 when it cannot be read or the process is gone */
unsigned long long ae_host_dedicated_stamp(long pid);
/* a line to the server's standard input (the server this AE started): 1 if written */
int ae_host_dedicated_send(char const *line);
/* asks the process to stop (SIGTERM, or on Windows its named stop event: the server's own stop, "now"); kill: at once */
int ae_host_dedicated_terminate(long pid, int kill);
/* link (a folder of the server's data root) made to point at target, unless it exists: 1 if it is there */
int ae_host_dedicated_link(char const *target, char const *link);
/* a file written whole, readable by this user alone (0600): 1 if written */
int ae_host_dedicated_write_private(char const *path, char const *text);
/* a folder made (and its parents): 1 if it is there */
int ae_host_dedicated_folder(char const *path);
/* a whole small file read (NUL ended), its length, -1 if none */
long ae_host_dedicated_read(char const *path, char *text, long size);
/* random bytes as hex (count bytes: 2 * count characters and an end): 1 if made */
int ae_host_dedicated_random_hex(char *text, int count);
/* the address this PC's own server is joined at: 127.0.0.1 where network.address is set (the game maps it), else the address
the game hosts on (xnet.c's title address: a VPN's tunnel is no local network's, so the LAN address then): dotted text */
void ae_host_dedicated_join_address(char *text, int size);
/* a web address opened in the player's browser: 1 if asked */
int ae_host_dedicated_open_url(char const *url);
/* each name in a folder (not . or ..), with whether it is a folder */
int ae_host_dedicated_list(char const *path, void (*visit)(char const *name, int folder, void *context), void *context);

#endif
