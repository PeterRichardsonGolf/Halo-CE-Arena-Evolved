/*
AE_HOST_SERVER.H

HOST A SERVER (0.7.7, plan Task 9; the approved mockups A-1 to A-6, B-2, C-1, C-2): the server this PC started from the
menus, as the menus see it (ae_host_server.c; the rules ae_host_dedicated_rules.h, the process work
../src/ae_host_dedicated.h). Its files are the save root's server/ folder: config.toml (its settings), data/ (its data
root: the client's maps, packs and other folders linked, its own debug.txt, bans and admins), saves/, status.json (its
status every 2 s), running.txt (what runs), owner.secret (its owner's password, 0600) and out.txt (its output).
*/

#ifndef AE_HOST_SERVER_H
#define AE_HOST_SERVER_H

#include "ae_host_dedicated_rules.h"

/* each frame (ae_hooks.c): the start followed, the status read, a server found from an earlier run */
void ae_host_server_tick(unsigned long now);
/* its state (AE_HD_IDLE ..) */
int ae_host_server_state(void);
/* the choices the page shows and starts with (the defaults, then the player's) */
struct ae_hd_choices *ae_host_server_choices(void);
/* the status last read (NULL before the first) */
struct ae_hd_status const *ae_host_server_status(void);
/* the running server's state file (pid, ports): NULL when none runs */
struct ae_hd_running const *ae_host_server_running(void);
/* why the last start failed, and the server's output file, for the page */
char const *ae_host_server_reason(void);
char const *ae_host_server_log_path(void);
/* START SERVER: the settings written, the process started: 1, else 0 and the reason (ae_host_server_reason) */
int ae_host_server_start(void);
/* STOP SERVER (now) / STOP WHEN EMPTY / CANCEL while starting: 1 if asked */
int ae_host_server_stop(int when_empty);
/* JOIN MY SERVER (and JOIN IT WHEN READY): this PC's player 1 joins it at 127.0.0.1: 1 if the join began */
int ae_host_server_join(void);
/* the checks the page shows before a start: MCC files (1 found, 0 none set, -1 set and not found), the playlist's
games, the ports it would take (0: all taken) */
void ae_host_server_checks(int *mcc, int *games, int *tunnel, int *web);
/* whether this build starts a server from the menus (Linux and macOS: yes) */
int ae_host_server_supported(void);
/* the web page's address (http://127.0.0.1:<port>/), "" when none runs */
char const *ae_host_server_web_url(void);
/* the owner's password file (save root's server/owner.secret) */
char const *ae_host_server_secret_path(void);

#endif
