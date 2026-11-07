/*
AE_HOST_DEDICATED.C

HOST A SERVER's process work (ae_host_dedicated.h).
*/

#if !defined(_WIN32)
#define _GNU_SOURCE 1
#define _DEFAULT_SOURCE 1
#endif

#include "ae_host_dedicated.h"
#include "ae_lan_broadcast.h"
#include "port_config.h"
#include "posix.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);

void ae_host_dedicated_join_address(char *text, int size)
{
	unsigned long address;
	unsigned char const *bytes = (unsigned char const *)&address;
	char const *setting = config_string("network.address");

	snprintf(text, (size_t)size, "127.0.0.1");
	if (setting && setting[0])
		return;
	address = ae_lan_title_address(posix_local_ipv4_address());
	if (address && bytes[0] != 127)
		snprintf(text, (size_t)size, "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
}

#if !defined(_WIN32) && !defined(HALO_ANDROID) && !defined(HALO_WEB)

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static struct
{
	long pid;
	int input;      /* its standard input's write end, -1 none */
} child = { 0, -1 };

int ae_host_dedicated_supported(void)
{
	return 1;
}

int ae_host_dedicated_port_free(int port, int tcp)
{
	struct sockaddr_in address;
	int handle = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0), free_it;

	if (handle < 0)
		return 0;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons((unsigned short)port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	free_it = bind(handle, (struct sockaddr *)&address, sizeof(address)) == 0;
	close(handle);
	return free_it;
}

static int own_path(char *path, int size)
{
	ssize_t length = readlink("/proc/self/exe", path, (size_t)size - 1);

	if (length > 0)
	{
		path[length] = 0;
		return 1;
	}
	{
		char const *base = SDL_GetBasePath();

		if (!base)
			return 0;
		snprintf(path, (size_t)size, "%shalo", base);
	}
	return 1;
}

long ae_host_dedicated_spawn(char const *config, char const *folder, char const *log, char const *const *extra,
	int extra_count, char *why, int why_size)
{
	char program[1024];
	char *arguments[5];
	char **environment;
	int count = 0, index, pipe_ends[2] = { -1, -1 }, output;
	posix_spawn_file_actions_t actions;
	posix_spawnattr_t attributes;
	pid_t pid = 0;
	int status;

	if (!own_path(program, (int)sizeof(program)))
	{
		snprintf(why, (size_t)why_size, "this program's path is not known");
		return 0;
	}
	while (environ[count])
		count++;
	environment = calloc((size_t)(count + extra_count + 1), sizeof(char *));
	if (!environment)
	{
		snprintf(why, (size_t)why_size, "no memory");
		return 0;
	}
	/* (this one's, without the settings that would make it a client's copy: the server's own are its file's) */
	{
		int kept = 0;

		for (index = 0; index < count; index++)
		{
			if (!strncmp(environ[index], "HALO_CONFIG_FILE=", 17) || !strncmp(environ[index], "HALO_TEST_INPUT=", 16) ||
				!strncmp(environ[index], "HALO_NETWORK_TEST", 17) || !strncmp(environ[index], "HALO_SCREENSHOT", 15) ||
				!strncmp(environ[index], "HALO_EXIT_AFTER=", 16) || !strncmp(environ[index], "HALO_DATA_ROOT=", 15) ||
				!strncmp(environ[index], "HALO_ARENA_MENUS", 16) || !strncmp(environ[index], "HALO_DISPLAY", 12) ||
				!strncmp(environ[index], "HALO_WINDOW", 11) || !strncmp(environ[index], "HALO_AE_TEST", 12))
			{
				continue;
			}
			environment[kept++] = environ[index];
		}
		for (index = 0; index < extra_count; index++)
			environment[kept++] = (char *)extra[index];
		environment[kept] = NULL;
	}
	output = open(log, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (output < 0 || pipe(pipe_ends) != 0)
	{
		snprintf(why, (size_t)why_size, "its log or its input could not be made (%s)", strerror(errno));
		if (output >= 0)
			close(output);
		free(environment);
		return 0;
	}
	fcntl(pipe_ends[1], F_SETFD, FD_CLOEXEC);
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, pipe_ends[0], 0);
	posix_spawn_file_actions_adddup2(&actions, output, 1);
	posix_spawn_file_actions_adddup2(&actions, output, 2);
	posix_spawn_file_actions_addclose(&actions, pipe_ends[0]);
	/* (none of this game's other files and sockets go with it: its UDP and TCP ports stay this game's, and free when it
	quits, however long the server runs) */
	{
		int handle;

		for (handle = 3; handle < 1024; handle++)
		{
			int flags = fcntl(handle, F_GETFD);

			if (flags != -1 && !(flags & FD_CLOEXEC) && handle != pipe_ends[0] && handle != output)
				posix_spawn_file_actions_addclose(&actions, handle);
		}
	}
	posix_spawn_file_actions_addchdir_np(&actions, folder);
	posix_spawnattr_init(&attributes);
#ifdef POSIX_SPAWN_SETSID
	/* (a session of its own: it outlives this AE, and a terminal's signals to this one are not its) */
	posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSID);
#endif
	arguments[0] = program;
	arguments[1] = (char *)"--server";
	arguments[2] = (char *)"--config";
	arguments[3] = (char *)config;
	arguments[4] = NULL;
	status = posix_spawn(&pid, program, &actions, &attributes, arguments, environment);
	posix_spawn_file_actions_destroy(&actions);
	posix_spawnattr_destroy(&attributes);
	close(pipe_ends[0]);
	close(output);
	free(environment);
	if (status != 0)
	{
		close(pipe_ends[1]);
		snprintf(why, (size_t)why_size, "it could not be started (%s)", strerror(status));
		return 0;
	}
	if (child.input >= 0)
		close(child.input);
	child.pid = (long)pid;
	child.input = pipe_ends[1];
	/* (a write to it once it is gone is an error, not this program's end) */
	signal(SIGPIPE, SIG_IGN);
	platform_log("ae host server: started %s --server (pid %ld)", program, child.pid);
	return child.pid;
}

int ae_host_dedicated_alive(long pid)
{
	if (pid <= 1)
		return 0;
	/* (ours: reaped once it ended, so it is no zombie) */
	if (pid == child.pid)
	{
		int status;

		if (waitpid((pid_t)pid, &status, WNOHANG) == (pid_t)pid)
		{
			child.pid = 0;
			if (child.input >= 0)
				close(child.input);
			child.input = -1;
			return 0;
		}
	}
	return kill((pid_t)pid, 0) == 0 || errno == EPERM;
}

unsigned long long ae_host_dedicated_stamp(long pid)
{
	char path[64], text[1024];
	char const *at;
	FILE *file;
	size_t size;
	int field;

	if (pid <= 1)
		return 0;
	snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
	file = fopen(path, "r");
	if (!file)
		return 0;
	size = fread(text, 1, sizeof(text) - 1, file);
	fclose(file);
	text[size] = 0;
	/* (after the last ")": the name may hold spaces and brackets; the state is field 3, the start time field 22) */
	at = strrchr(text, ')');
	if (!at)
		return 0;
	for (field = 2, at++; field < 22 && *at; at++)
	{
		if (*at == ' ')
			field++;
	}
	return field == 22 ? strtoull(at, NULL, 10) : 0;
}

int ae_host_dedicated_send(char const *line)
{
	size_t length = strlen(line);

	if (child.input < 0)
		return 0;
	if (write(child.input, line, length) != (ssize_t)length || write(child.input, "\n", 1) != 1)
		return 0;
	return 1;
}

int ae_host_dedicated_terminate(long pid, int kill_it)
{
	if (pid <= 1)
		return 0;
	return kill((pid_t)pid, kill_it ? SIGKILL : SIGTERM) == 0;
}

int ae_host_dedicated_link(char const *target, char const *link)
{
	char seen[8];

	/* (there already: a link, its target found or not, or a folder) */
	if (readlink(link, seen, sizeof(seen)) >= 0 || SDL_GetPathInfo(link, NULL))
		return 1;
	return symlink(target, link) == 0;
}

int ae_host_dedicated_write_private(char const *path, char const *text)
{
	char temporary[1100];
	size_t length = strlen(text);
	int handle;

	snprintf(temporary, sizeof(temporary), "%s.tmp", path);
	handle = open(temporary, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (handle < 0)
		return 0;
	if (write(handle, text, length) != (ssize_t)length)
	{
		close(handle);
		unlink(temporary);
		return 0;
	}
	close(handle);
	return rename(temporary, path) == 0;
}

int ae_host_dedicated_folder(char const *path)
{
	SDL_PathInfo information;

	/* (its parents too) */
	if (!SDL_CreateDirectory(path))
		return 0;
	return SDL_GetPathInfo(path, &information) && information.type == SDL_PATHTYPE_DIRECTORY;
}

long ae_host_dedicated_read(char const *path, char *text, long size)
{
	FILE *file = fopen(path, "rb");
	size_t length;

	if (!file || size <= 0)
	{
		if (file)
			fclose(file);
		return -1;
	}
	length = fread(text, 1, (size_t)size - 1, file);
	fclose(file);
	text[length] = 0;
	return (long)length;
}

int ae_host_dedicated_random_hex(char *text, int count)
{
	unsigned char bytes[64];
	FILE *file;
	int index;

	if (count <= 0 || count > (int)sizeof(bytes))
		return 0;
	file = fopen("/dev/urandom", "rb");
	if (!file)
		return 0;
	if (fread(bytes, 1, (size_t)count, file) != (size_t)count)
	{
		fclose(file);
		return 0;
	}
	fclose(file);
	for (index = 0; index < count; index++)
	{
		text[index * 2] = "0123456789abcdef"[bytes[index] >> 4];
		text[index * 2 + 1] = "0123456789abcdef"[bytes[index] & 15];
	}
	text[count * 2] = 0;
	memset(bytes, 0, sizeof(bytes));
	return 1;
}

int ae_host_dedicated_list(char const *path, void (*visit)(char const *name, int folder, void *context), void *context)
{
	DIR *folder = opendir(path);
	struct dirent *entry;

	if (!folder)
		return 0;
	while ((entry = readdir(folder)) != NULL)
	{
		char full[1100];
		SDL_PathInfo information;

		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
		visit(entry->d_name, SDL_GetPathInfo(full, &information) && information.type == SDL_PATHTYPE_DIRECTORY, context);
	}
	closedir(folder);
	return 1;
}

#elif !defined(_WIN32)

int ae_host_dedicated_supported(void) { return 0; }
int ae_host_dedicated_port_free(int port, int tcp) { (void)port; (void)tcp; return 0; }
long ae_host_dedicated_spawn(char const *config, char const *folder, char const *log, char const *const *extra,
	int extra_count, char *why, int why_size)
{
	(void)config; (void)folder; (void)log; (void)extra; (void)extra_count;
	snprintf(why, (size_t)why_size, "this build does not start a server from the menus yet");
	return 0;
}
int ae_host_dedicated_alive(long pid) { (void)pid; return 0; }
unsigned long long ae_host_dedicated_stamp(long pid) { (void)pid; return 0; }
int ae_host_dedicated_send(char const *line) { (void)line; return 0; }
int ae_host_dedicated_terminate(long pid, int kill_it) { (void)pid; (void)kill_it; return 0; }
int ae_host_dedicated_link(char const *target, char const *link) { (void)target; (void)link; return 0; }
int ae_host_dedicated_write_private(char const *path, char const *text) { (void)path; (void)text; return 0; }
int ae_host_dedicated_folder(char const *path) { (void)path; return 0; }
long ae_host_dedicated_read(char const *path, char *text, long size) { (void)path; (void)text; (void)size; return -1; }
int ae_host_dedicated_random_hex(char *text, int count) { (void)text; (void)count; return 0; }
int ae_host_dedicated_list(char const *path, void (*visit)(char const *name, int folder, void *context), void *context)
{
	(void)path; (void)visit; (void)context;
	return 0;
}

#endif

int ae_host_dedicated_open_url(char const *url)
{
#ifdef HALO_SERVER
	/* (the separate server program: no browser to open) */
	(void)url;
	return 0;
#else
	return SDL_OpenURL(url);
#endif
}
