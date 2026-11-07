/*
AE_HOST_SERVER.C

HOST A SERVER: the server this PC started from the menus (ae_host_server.h).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "ae_glue_lobby.h"
#include "ae_host_server.h"
#include "ae_screen_create_game.h"
#include "../src/ae_host_dedicated.h"
#include "../src/ae_private_files.h"
#include "../src/port_config.h"
#include "../src/server_mode.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
/* (platform.h's, which the game's units do not include) */
const char *platform_data_root(void);
const char *platform_save_root(void);
int platform_folder_exists(const char *path);
#ifdef HALO_GAME_BROWSER
int browser_moderator_key(char *text, int size);
#endif

enum { PATH_SIZE = 1024, STATUS_SIZE = 16384, STATUS_EVERY_MS = 1000, ALIVE_EVERY_MS = 1000, JOIN_TIMEOUT_MS = 20000, JOIN_RETRY_MS = 1500, JOIN_AUTO_MS = 20000 };

static struct
{
	int read_running;               /* running.txt looked at once (a server from an earlier run) */
	int state;
	unsigned long state_since;
	int choices_made;
	struct ae_hd_choices choices;
	struct ae_hd_running running;
	int has_running;
	int spawned_here;               /* this run of AE started the process (its pid is ours) */
	struct ae_hd_status status;
	int has_status;
	unsigned long status_at, alive_at;
	int alive;
	char reason[200];
	char folder[PATH_SIZE], log[PATH_SIZE], status_path[PATH_SIZE], running_path[PATH_SIZE], secret[PATH_SIZE];
	char url[64];
	/* JOIN IT WHEN READY, then the join followed */
	int join_when_ready;
	int joining;
	unsigned long join_since;
	/* (JOIN IT WHEN READY: the server says ready a moment before its game port listens, so a refused first try is tried
	again every 1.5 s for 20 s, silently) */
	unsigned long join_auto_until, join_retry_at;
} host;

/* whether a signal may go to the pid: a process this run started, or one whose start time is the one the state file
holds (a pid can pass to another process after a reboot) */
static int signal_ok(void)
{
	return host.spawned_here || ae_host_dedicated_running_identity(&host.running, ae_host_dedicated_stamp(host.running.pid));
}

static void paths_make(void)
{
	if (host.folder[0])
		return;
	snprintf(host.folder, sizeof(host.folder), "%s/server", platform_save_root());
	snprintf(host.log, sizeof(host.log), "%s/out.txt", host.folder);
	snprintf(host.status_path, sizeof(host.status_path), "%s/status.json", host.folder);
	snprintf(host.running_path, sizeof(host.running_path), "%s/running.txt", host.folder);
	snprintf(host.secret, sizeof(host.secret), "%s/owner.secret", host.folder);
}

static void state_set(int state, unsigned long now)
{
	if (state == host.state)
		return;
	platform_log("ae host server: %s -> %s", host.state == AE_HD_IDLE ? "idle" : host.state == AE_HD_STARTING ? "starting" :
		host.state == AE_HD_READY ? "ready" : host.state == AE_HD_FAILED ? "failed" : "stopping",
		state == AE_HD_IDLE ? "idle" : state == AE_HD_STARTING ? "starting" : state == AE_HD_READY ? "ready" :
		state == AE_HD_FAILED ? "failed" : "stopping");
	host.state = state;
	host.state_since = now;
	if (state == AE_HD_IDLE || state == AE_HD_FAILED)
	{
		host.has_running = 0;
		host.has_status = 0;
		host.url[0] = 0;
		/* (gone: its state file with it, and the status it left) */
		remove(host.running_path);
		remove(host.status_path);
	}
}

static void choices_default(void)
{
	char name[AE_HD_NAME + 16] = "My Server";
	char playlist[AE_HD_ROTATION];

	if (host.choices_made)
		return;
	host.choices_made = 1;
	{
		char const *profile = config_string("player.name");

		(void)profile;
	}
	ae_host_dedicated_choices_init(&host.choices, name);
	if (ae_private_file_read("playlist.txt", playlist, sizeof(playlist)) > 0)
		ae_host_dedicated_rotation_line(playlist, host.choices.rotation, (int)sizeof(host.choices.rotation));
}

struct ae_hd_choices *ae_host_server_choices(void)
{
	choices_default();
	return &host.choices;
}

int ae_host_server_supported(void)
{
	return ae_host_dedicated_supported();
}

int ae_host_server_state(void)
{
	return host.state;
}

struct ae_hd_status const *ae_host_server_status(void)
{
	return host.has_status ? &host.status : NULL;
}

struct ae_hd_running const *ae_host_server_running(void)
{
	return host.has_running ? &host.running : NULL;
}

char const *ae_host_server_reason(void)
{
	return host.reason;
}

char const *ae_host_server_log_path(void)
{
	paths_make();
	return host.log;
}

char const *ae_host_server_secret_path(void)
{
	paths_make();
	return host.secret;
}

char const *ae_host_server_web_url(void)
{
	return host.url;
}

static int port_free(int port, int tcp, void *context)
{
	(void)context;
	return ae_host_dedicated_port_free(port, tcp);
}

void ae_host_server_checks(int *mcc, int *games, int *tunnel, int *web)
{
	char const *path = config_string("game.mcc_path");
	char line[AE_HD_ROTATION];

	choices_default();
	*mcc = path && path[0] ? (platform_folder_exists(path) ? 1 : -1) : 0;
	*games = host.choices.rotation[0] ? ae_host_dedicated_rotation_line(host.choices.rotation, line, (int)sizeof(line)) : 0;
	/* (a comma list: its entries) */
	if (host.choices.rotation[0])
	{
		char const *at;

		*games = 1;
		for (at = host.choices.rotation; *at; at++)
			if (*at == ',')
				(*games)++;
	}
	if (host.state == AE_HD_READY || host.state == AE_HD_STARTING)
	{
		*tunnel = host.running.tunnel_port;
		*web = host.running.web_port;
		return;
	}
	/* (the ports looked at every 2 seconds, not every frame the page draws) */
	{
		static unsigned long looked_at;
		static int free_tunnel, free_web, looked;
		unsigned long now = system_milliseconds();

		if (!looked || now - looked_at >= 2000)
		{
			looked = 1;
			looked_at = now;
			if (!ae_host_dedicated_ports(port_free, NULL, &free_tunnel, &free_web))
				free_tunnel = free_web = 0;
		}
		*tunnel = free_tunnel;
		*web = free_web;
	}
}

/* the server's data root: the client's folders linked (maps, packs, ...), its files its own */
static void link_visit(char const *name, int folder, void *context)
{
	char const *data = context;
	char target[PATH_SIZE], link[PATH_SIZE];

	if (!ae_host_dedicated_link_wanted(name, folder))
		return;
	snprintf(target, sizeof(target), "%s/%s", data, name);
	snprintf(link, sizeof(link), "%s/data/%s", host.folder, name);
	if (!ae_host_dedicated_link(target, link))
		platform_log("ae host server: %s not linked into its data folder", name);
}

int ae_host_server_start(void)
{
	char config[8192], path[PATH_SIZE], password[72], key[80] = "", why[160], text[256];
	char data_env[PATH_SIZE + 32], save_env[PATH_SIZE + 32], password_env[100], key_env[120];
	char const *extra[4];
	int extra_count = 0, tunnel, web;
	unsigned long now = system_milliseconds();
	long pid;

	paths_make();
	choices_default();
	host.reason[0] = 0;
	if (host.state == AE_HD_STARTING || host.state == AE_HD_READY || host.state == AE_HD_STOPPING)
	{
		snprintf(host.reason, sizeof(host.reason), "A server of this PC runs already.");
		return 0;
	}
	if (!ae_host_dedicated_supported())
	{
		snprintf(host.reason, sizeof(host.reason), "This build does not start a server from the menus yet.");
		state_set(AE_HD_FAILED, now);
		return 0;
	}
	if (!ae_host_dedicated_ports(port_free, NULL, &tunnel, &web))
	{
		snprintf(host.reason, sizeof(host.reason), "No free port: the game ports 2303 to 2330 or the web ports 8080 to 8099 "
			"are all taken.");
		state_set(AE_HD_FAILED, now);
		return 0;
	}
	host.choices.tunnel_port = tunnel;
	host.choices.web_port = web;
	snprintf(host.choices.mcc_path, sizeof(host.choices.mcc_path), "%s", config_string("game.mcc_path"));
	snprintf(host.choices.status_file, sizeof(host.choices.status_file), "%s", host.status_path);
	snprintf(path, sizeof(path), "%s/data", host.folder);
	if (!ae_host_dedicated_folder(path) || (snprintf(text, sizeof(text), "%s/saves", host.folder),
		!ae_host_dedicated_folder(text)))
	{
		snprintf(host.reason, sizeof(host.reason), "Its folder could not be made: %s", host.folder);
		state_set(AE_HD_FAILED, now);
		return 0;
	}
	ae_host_dedicated_list(platform_data_root(), link_visit, (void *)platform_data_root());
	if (ae_host_dedicated_config_text(&host.choices, config, (int)sizeof(config)) < 0)
	{
		snprintf(host.reason, sizeof(host.reason), "Its settings are too long (the playlist).");
		state_set(AE_HD_FAILED, now);
		return 0;
	}
	snprintf(path, sizeof(path), "%s/config.toml", host.folder);
	if (!ae_host_dedicated_write_private(path, config) || !ae_host_dedicated_random_hex(password, 24) ||
		!ae_host_dedicated_write_private(host.secret, password))
	{
		snprintf(host.reason, sizeof(host.reason), "Its settings could not be written in %s", host.folder);
		state_set(AE_HD_FAILED, now);
		return 0;
	}
	remove(host.status_path);
	snprintf(data_env, sizeof(data_env), "HALO_DATA_ROOT=%s/data", host.folder);
	snprintf(save_env, sizeof(save_env), "HALO_SAVE_ROOT=%s/saves", host.folder);
	snprintf(password_env, sizeof(password_env), "HALO_SERVER_WEB_OWNER_PASSWORD=%s", password);
	extra[extra_count++] = data_env;
	extra[extra_count++] = save_env;
	extra[extra_count++] = password_env;
#ifdef HALO_GAME_BROWSER
	/* (this PC's moderator key: its player is the server's owner in the game, ae_server_ingame.c) */
	if (browser_moderator_key(key, (int)sizeof(key)))
	{
		snprintf(key_env, sizeof(key_env), "HALO_SERVER_OWNER_KEY=%s", key);
		extra[extra_count++] = key_env;
	}
#endif
	pid = ae_host_dedicated_spawn(path, host.folder, host.log, extra, extra_count, why, (int)sizeof(why));
	memset(password, 0, sizeof(password));
	memset(password_env, 0, sizeof(password_env));
	if (!pid)
	{
		snprintf(host.reason, sizeof(host.reason), "The server could not be started: %s.", why);
		state_set(AE_HD_FAILED, now);
		return 0;
	}
	memset(&host.running, 0, sizeof(host.running));
	host.running.pid = pid;
	host.running.tunnel_port = tunnel;
	host.running.web_port = web;
	host.running.started = (unsigned long)time(NULL);
	host.running.stamp = ae_host_dedicated_stamp(pid);
	host.spawned_here = 1;
	snprintf(host.running.name, sizeof(host.running.name), "%s", host.choices.name);
	host.has_running = 1;
	if (ae_host_dedicated_running_text(&host.running, text, (int)sizeof(text)) > 0)
		ae_host_dedicated_write_private(host.running_path, text);
	snprintf(host.url, sizeof(host.url), "http://127.0.0.1:%d/", web);
	host.alive = 1;
	host.join_when_ready = host.choices.join_when_ready;
	state_set(AE_HD_STARTING, now);
	platform_log("ae host server: '%s' starting (game port %d, web page %d, %s)", host.choices.name, tunnel, web,
		host.choices.public_listing ? "public" : "private");
	/* (0.7.7) a server started from the game has no publisher key, so the master servers never list it: it is found on the LAN,
	by its invite link and by its address (JOIN BY ADDRESS). A listing needs a key, an address players can reach and the
	operator's say: docs/hosting.md */
	platform_log("ae host server: not listed on the master servers (no publisher key): found on the LAN, by invite and by address");
	return 1;
}

int ae_host_server_stop(int when_empty)
{
	unsigned long now = system_milliseconds();

	if (!host.has_running)
		return 0;
	host.join_when_ready = 0;
	host.join_auto_until = host.join_retry_at = 0;
	/* (the server this AE started hears it on its input: its own stop, now or once empty; else the signal's stop) */
	if (ae_host_dedicated_send(when_empty ? "sv_stop empty" : "sv_stop now"))
		platform_log("ae host server: asked to stop%s", when_empty ? " once empty" : "");
	else if (!when_empty && signal_ok() && ae_host_dedicated_terminate(host.running.pid, 0))
		platform_log("ae host server: told to stop (a signal)");
	else
		return 0;
	if (!when_empty)
		state_set(AE_HD_STOPPING, now);
	return 1;
}

int ae_host_server_join(void)
{
	struct ae_result result;
	char address[32];

	if (host.state != AE_HD_READY || host.joining)
		return 0;
	/* (the address the game hosts on: with a VPN on, 127.0.0.1 is not it) */
	ae_host_dedicated_join_address(address, (int)sizeof(address));
	result = ae_lobby_join_host(address, ae_lobby_host_port());
	if (!result.ok)
	{
		if (system_milliseconds() < host.join_auto_until)
		{
			host.join_retry_at = system_milliseconds() + JOIN_RETRY_MS;
			platform_log("ae host server: join not yet (%s): trying again", result.reason);
			return 0;
		}
		snprintf(host.reason, sizeof(host.reason), "Can't join it: %s", result.reason);
		platform_log("ae host server: join failed: %s", result.reason);
		return 0;
	}
	host.joining = 1;
	host.join_since = system_milliseconds();
	platform_log("ae host server: joining it at %s", address);
	return 1;
}

static void join_follow(unsigned long now)
{
	static struct ae_lobby_roster roster;
	struct ae_result failure;
	int state;

	if (!host.joining)
		return;
	state = ae_lobby_join_state(&failure);
	if (state < 0 || (state == 0 && now - host.join_since > JOIN_TIMEOUT_MS))
	{
		host.joining = 0;
		if (now < host.join_auto_until && host.state == AE_HD_READY)
		{
			host.join_retry_at = now + JOIN_RETRY_MS;
			platform_log("ae host server: join not yet (%s): trying again", failure.reason[0] ? failure.reason :
				"it did not answer");
			ae_lobby_leave();
			return;
		}
		snprintf(host.reason, sizeof(host.reason), "Can't join it: %s", failure.reason[0] ? failure.reason :
			"it did not answer.");
		platform_log("ae host server: join failed: %s", host.reason);
		ae_lobby_leave();
		return;
	}
	if (state == 0 || !ae_lobby_roster(&roster).ok || !roster.joined)
		return;
	host.joining = 0;
	if (!ae_lobby_add_local_player(0).ok)
	{
		ae_lobby_leave();
		snprintf(host.reason, sizeof(host.reason), "Can't join it: the game is full.");
		return;
	}
	platform_log("ae host server: joined it");
	ae_screen_create_game_lobby_joined("MY SERVER");
}

void ae_host_server_tick(unsigned long now)
{
	/* (a dedicated server itself starts none) */
	if (halo_server_mode())
		return;
	paths_make();
	/* (AE started again: a server of an earlier run found by its state file, if its process lives) */
	if (!host.read_running)
	{
		char text[512];

		host.read_running = 1;
		if (ae_host_dedicated_read(host.running_path, text, sizeof(text)) > 0 &&
			ae_host_dedicated_running_read(text, &host.running))
		{
			if (ae_host_dedicated_running_reused(&host.running, ae_host_dedicated_alive(host.running.pid),
				ae_host_dedicated_stamp(host.running.pid)))
			{
				host.has_running = 1;
				snprintf(host.url, sizeof(host.url), "http://127.0.0.1:%d/", host.running.web_port);
				host.state = AE_HD_READY;
				host.state_since = now;
				host.alive = 1;
				platform_log("ae host server: '%s' runs from an earlier start (pid %ld, game port %d)", host.running.name,
					host.running.pid, host.running.tunnel_port);
			}
			else
				remove(host.running_path);
		}
	}
	if (host.state == AE_HD_IDLE || host.state == AE_HD_FAILED)
		return;
	if (now - host.alive_at >= ALIVE_EVERY_MS)
	{
		host.alive_at = now;
		host.alive = ae_host_dedicated_alive(host.running.pid);
	}
	if (now - host.status_at >= STATUS_EVERY_MS)
	{
		static char text[STATUS_SIZE];
		struct ae_hd_status status;

		host.status_at = now;
		if (ae_host_dedicated_read(host.status_path, text, sizeof(text)) > 0 && ae_host_dedicated_status_read(text, &status))
		{
			host.status = status;
			host.has_status = 1;
		}
	}
	{
		int before = host.state;
		int next = ae_host_dedicated_step(host.state, host.has_status && ae_host_dedicated_status_ready(&host.status),
			host.alive, now - host.state_since);

		if (next == AE_HD_FAILED && before == AE_HD_STARTING)
		{
			snprintf(host.reason, sizeof(host.reason), host.alive ? "The server did not get ready in a minute." :
				"The server stopped as it started.");
			if (host.alive && signal_ok())
				ae_host_dedicated_terminate(host.running.pid, 0);
		}
		state_set(next, now);
		if (next == AE_HD_READY && before == AE_HD_STARTING && host.join_when_ready)
		{
			host.join_when_ready = 0;
			host.join_auto_until = now + JOIN_AUTO_MS;
			ae_host_server_join();
		}
	}
	join_follow(now);
	if (host.join_retry_at && now >= host.join_retry_at && !host.joining)
	{
		host.join_retry_at = 0;
		if (host.state == AE_HD_READY)
			ae_host_server_join();
	}
}
