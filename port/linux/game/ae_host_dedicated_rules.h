#ifndef AE_HOST_DEDICATED_RULES_H
#define AE_HOST_DEDICATED_RULES_H

/*
AE_HOST_DEDICATED_RULES.H

The pure rules of HOST A SERVER (0.7.7, plan Task 9; the approved mockups A-1 to A-6, B-2, C-1, C-2): a dedicated server
started from the menus as a process of its own on this PC, with no window, which this PC then joins like any player.

The menus write the server's settings file (its config.toml, ae_host_dedicated_config_text), give it its owner's
password and this PC's moderator key in its environment (never in a file another program reads), start it, and read the
status file it writes every 2 seconds (server.status_file, ae_host_dedicated_status_read) to know it is up; a state
file (running.txt) says it runs, so that AE started again finds it (never a second server fighting for the ports).
Private by default: LAN and invites only, its web page on this PC only. No game, socket or file dependency:
tools/test_ae_units.py compiles ae_host_dedicated_rules_test.c on its own.
*/

enum
{
	AE_HD_NAME = 16,                       /* the network game's name: 15 characters */
	AE_HD_TEXT = 512,
	AE_HD_ROTATION = 4096,
	AE_HD_TUNNEL_FIRST = 2303,             /* the client keeps 2302 */
	AE_HD_TUNNEL_LAST = 2330,
	AE_HD_WEB_FIRST = 8080,
	AE_HD_WEB_LAST = 8099,
	/* the start: ready on the status file's first word of a lobby or a game, failed when the process ends first or
	after this long */
	AE_HD_START_TIMEOUT_MS = 60000,
	AE_HD_STOP_TIMEOUT_MS = 10000,
	AE_HD_MAX_PLAYERS_FIRST = 2, AE_HD_MAX_PLAYERS_LAST = 16,
	AE_HD_BOTS_STEPS = 7,
};

/* the states of a server the menus started */
enum { AE_HD_IDLE, AE_HD_STARTING, AE_HD_READY, AE_HD_FAILED, AE_HD_STOPPING };

struct ae_hd_choices
{
	char name[AE_HD_NAME];
	int public_listing;                    /* WHO CAN FIND IT: PRIVATE (0, the default) or PUBLIC */
	int max_players;
	int bots;                              /* BOTS FILL EMPTY SPOTS: 0 off, else up to that many players with bots */
	char rotation[AE_HD_ROTATION];         /* the playlist as the server's rotation ("map:gametype, ...") */
	int tunnel_port, web_port;
	char mcc_path[AE_HD_TEXT];             /* the client's game.mcc_path, "" none */
	char status_file[AE_HD_TEXT];          /* where the server writes its status */
	int join_when_ready;                   /* THIS PC: JOIN IT WHEN READY (the menus', not the server's) */
};

/* the defaults: the name given (a profile's "Goat's Server", cut to 15), PRIVATE, 16 players, bots up to 8 */
void ae_host_dedicated_choices_init(struct ae_hd_choices *choices, char const *name);
/* the settings file's text (TOML; strings quoted, their quotes and backslashes escaped, control characters dropped):
its length, -1 when it does not fit */
int ae_host_dedicated_config_text(struct ae_hd_choices const *choices, char *text, int size);
/* the first free tunnel port from AE_HD_TUNNEL_FIRST and web port from AE_HD_WEB_FIRST, by free(port, kind 0 UDP /
1 TCP, context): 1, else 0 (all taken) */
int ae_host_dedicated_ports(int (*free_port)(int port, int tcp, void *context), void *context, int *tunnel, int *web);
/* the start's state machine: the state now, whether the status file says the server hosts (a lobby or a game), whether
its process lives, the milliseconds since it started (or since STOPPING began): the state next */
int ae_host_dedicated_step(int state, int status_ready, int child_alive, unsigned long waited_ms);

/* ---------- the values of the setup's rows */

/* BOTS FILL EMPTY SPOTS: its steps 0 (OFF), 2, 4, 6, 8, 12, 16 */
int ae_host_dedicated_bots_step(int bots, int direction);
void ae_host_dedicated_bots_word(int bots, char *text, int size);   /* "OFF", "UP TO 8" */
int ae_host_dedicated_players_step(int players, int direction);   /* 2 .. 16, no wrap */

/* the playlist (a server rotation's text: playlist.txt's lines, # comments) as one line the settings file takes
("map:gametype, map:gametype"): the entries; 0 when none */
int ae_host_dedicated_rotation_line(char const *playlist, char *line, int size);

/* ---------- running.txt: what a running server is, for AE started again */

struct ae_hd_running
{
	long pid;
	int tunnel_port, web_port;
	unsigned long started;                 /* seconds since 1970 */
	unsigned long long stamp;              /* the process's start time (ae_host_dedicated_stamp), 0 if unknown */
	char name[AE_HD_NAME];
};
int ae_host_dedicated_running_text(struct ae_hd_running const *running, char *text, int size);
/* 1 if the text is a whole state (a pid, both ports in range) */
int ae_host_dedicated_running_read(char const *text, struct ae_hd_running *running);
/* (Review Focus 3) what the page offers for a state file: 1 (JOIN / STOP: a server of ours runs) when its process lives,
else 0 (the state is old: forget it, offer START) */
int ae_host_dedicated_running_reused(struct ae_hd_running const *running, int process_alive, unsigned long long current_stamp);
/* whether the process now holding the state's pid is the process the state was written for: the start times match (a
reboot hands the pid to another process; a start time that was never read, or cannot be read now, is no proof) */
int ae_host_dedicated_running_identity(struct ae_hd_running const *running, unsigned long long current_stamp);

/* ---------- the status file (the server's /api/status, written every 2 s) */

struct ae_hd_status
{
	char state[24];                        /* "lobby", "playing", "postgame", "starting" ... */
	char name[AE_HD_NAME + 16];
	char map[48], gametype[48];
	int players, max_players;
	long uptime;                           /* seconds */
	char invite[256];                      /* the listing's invite, "" none */
};
/* 1 if the JSON gave a state (the other fields as found) */
int ae_host_dedicated_status_read(char const *json, struct ae_hd_status *status);
/* whether a status says the server hosts: a lobby or a game, not still starting */
int ae_host_dedicated_status_ready(struct ae_hd_status const *status);

/* ---------- quitting AE with the server running (C-1 / C-2): the choices in order, KEEP RUNNING first and focused;
STOP WHEN EMPTY only with people on it; their count */
enum { AE_HD_QUIT_KEEP, AE_HD_QUIT_STOP_EMPTY, AE_HD_QUIT_STOP };
int ae_host_dedicated_quit_choices(int people_on, int *choices);
char const *ae_host_dedicated_quit_label(int choice);

/* ---------- the server's data folder: the client's folders it links (maps, packs, ...), never its files (debug.txt,
bans.txt) nor its own captures */
int ae_host_dedicated_link_wanted(char const *name, int is_folder);

#endif
