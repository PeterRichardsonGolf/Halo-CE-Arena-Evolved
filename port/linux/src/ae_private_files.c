/*
AE_PRIVATE_FILES.C

AE's small text files in the save root's private folder (ae_private_files.h).
*/

#include "ae_private_files.h"
#include "port_folders.h"
#include "posix.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
/* (platform_log, and SDL's file calls for the settings file: every platform) */
#include "platform.h"
#include <SDL3/SDL.h>
#if defined(_WIN32)
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#if !defined(__EMSCRIPTEN__) && !defined(HALO_ANDROID)
#include <signal.h>
#define CRASH_MARK_SIGNALS
#endif
#endif

const char *platform_folder(enum port_folder folder);

/* a file's path in the private folder, with suffix after its name: 0 when it does not fit or the name is no bare
name */
static int file_path(const char *name, const char *suffix, char *path, size_t size)
{
	const char *folder = platform_folder(_port_folder_private);
	int length;

	if (!folder || !*folder || !name || !*name || strchr(name, '/') || strchr(name, '\\') || strstr(name, ".."))
		return 0;
	length = snprintf(path, size, "%s/%s%s", folder, name, suffix);
	return length > 0 && (size_t)length < size;
}

long ae_private_file_read(const char *name, char *text, long size)
{
	char path[1024];
	FILE *file;
	size_t count;

	if (!text || size <= 0)
		return -1;
	text[0] = 0;
	if (!file_path(name, "", path, sizeof(path)) || !(file = fopen(path, "rb")))
		return -1;
	count = fread(text, 1, (size_t)size, file);
	fclose(file);
	if (count >= (size_t)size)
	{
		text[0] = 0;
		return -2;
	}
	text[count] = 0;
	return (long)count;
}

long ae_save_root_file_read(const char *name, char *text, long size)
{
	char path[1024];
	const char *folder = platform_folder(_port_folder_private);
	size_t length;
	FILE *file;
	size_t count;

	if (!text || size <= 0 || !name || !*name || strchr(name, '/') || strchr(name, '\\') || strstr(name, "..") ||
		!folder || !*folder)
	{
		return -1;
	}
	text[0] = 0;
	length = strlen(folder);
	/* (the save root: the private folder's parent) */
	if (length > 8 && !strcmp(folder + length - 8, "/private") && length - 8 + strlen(name) + 2 < sizeof(path))
	{
		snprintf(path, sizeof(path), "%.*s/%s", (int)(length - 8), folder, name);
		if ((file = fopen(path, "rb")))
		{
			count = fread(text, 1, (size_t)size, file);
			fclose(file);
			if (count >= (size_t)size)
			{
				text[0] = 0;
				return -2;
			}
			text[count] = 0;
			return (long)count;
		}
	}
	return ae_private_file_read(name, text, size);
}

int ae_private_file_write(const char *name, const char *text, long length)
{
	char path[1024], temporary[1024];
	FILE *file;

	if (!text || length < 0 || !file_path(name, "", path, sizeof(path)) ||
		!file_path(name, ".new", temporary, sizeof(temporary)))
		return 0;
	posix_make_directory(platform_folder(_port_folder_private));
	if (!(file = fopen(temporary, "wb")))
		return 0;
	if (fwrite(text, 1, (size_t)length, file) != (size_t)length || fflush(file))
	{
		fclose(file);
		remove(temporary);
		return 0;
	}
	if (fclose(file) || posix_replace_file(temporary, path))
	{
		remove(temporary);
		return 0;
	}
	return 1;
}

int ae_private_file_set_aside(const char *name)
{
	char path[1024], bad[1024];

	if (!file_path(name, "", path, sizeof(path)) || !file_path(name, ".bad", bad, sizeof(bad)))
		return 0;
	return posix_replace_file(path, bad) == 0;
}

int ae_private_file_exists(const char *name)
{
	char path[1024];
	FILE *file;

	if (!file_path(name, "", path, sizeof(path)) || !(file = fopen(path, "rb")))
		return 0;
	fclose(file);
	return 1;
}

int ae_private_file_remove(const char *name)
{
	char path[1024];

	return file_path(name, "", path, sizeof(path)) && remove(path) == 0;
}

/* ---------- the crash mark */

/* the mark's path, made when it is armed: the crash handler only opens it (open and close are safe in a signal handler) */
static char crash_mark_path[1024];

void ae_private_crash_mark_touch(void)
{
	int file;

	if (!crash_mark_path[0])
		return;
#if defined(_WIN32)
	file = _open(crash_mark_path, _O_WRONLY | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE);
	if (file >= 0)
		_close(file);
#else
	file = open(crash_mark_path, O_WRONLY | O_CREAT, 0600);
	if (file >= 0)
	{
		ssize_t written = write(file, "crashed\n", 8);

		(void)written;
		close(file);
	}
#endif
}

#ifdef CRASH_MARK_SIGNALS
static const int crash_mark_signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
static struct sigaction crash_mark_before[sizeof(crash_mark_signals) / sizeof(crash_mark_signals[0])];
static char crash_mark_stack[32 * 1024];
static int crash_mark_installed;

/* the mark, then the signal as it would have gone without this handler (an earlier handler, or the default end).
A write to a page the memory watch holds read-only is no crash: it is the watch's own fault, which its handler (the one
this replaced) makes writable, and the write goes on. It goes to that handler at once, with no mark and no re-raise (a
re-raised signal is a signal sent by the program, with no fault address: the watch took it for a crash) */
static void crash_mark_handler(int signal_number, siginfo_t *information, void *context)
{
	size_t index;

	for (index = 0; index < sizeof(crash_mark_signals) / sizeof(crash_mark_signals[0]); index++)
	{
		if (crash_mark_signals[index] != signal_number)
			continue;
		if ((signal_number == SIGSEGV || signal_number == SIGBUS) && information &&
			(crash_mark_before[index].sa_flags & SA_SIGINFO) && crash_mark_before[index].sa_sigaction &&
			memory_watch_owns_fault(information->si_addr))
		{
			crash_mark_before[index].sa_sigaction(signal_number, information, context);
			return;
		}
	}
	ae_private_crash_mark_touch();
	for (index = 0; index < sizeof(crash_mark_signals) / sizeof(crash_mark_signals[0]); index++)
		if (crash_mark_signals[index] == signal_number)
			sigaction(signal_number, &crash_mark_before[index], NULL);
	raise(signal_number);
}
#endif

int ae_private_crash_mark_arm(const char *name)
{
#if defined(HALO_ANDROID) || defined(__EMSCRIPTEN__)
	(void)name;
	return 0;
#else
	char path[1024];

	if (!file_path(name, "", path, sizeof(path)))
		return 0;
	memcpy(crash_mark_path, path, strlen(path) + 1);
#ifdef CRASH_MARK_SIGNALS
	if (!crash_mark_installed)
	{
		struct sigaction action;
		stack_t stack, current;
		size_t index;

		/* (room for the handler after a stack overflow, on this thread, unless it has a stack for it already) */
		if (sigaltstack(NULL, &current) || (current.ss_flags & SS_DISABLE))
		{
			stack.ss_sp = crash_mark_stack;
			stack.ss_size = sizeof(crash_mark_stack);
			stack.ss_flags = 0;
			sigaltstack(&stack, NULL);
		}
		memset(&action, 0, sizeof(action));
		action.sa_sigaction = crash_mark_handler;
		action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
		sigemptyset(&action.sa_mask);
		for (index = 0; index < sizeof(crash_mark_signals) / sizeof(crash_mark_signals[0]); index++)
			sigaction(crash_mark_signals[index], &action, &crash_mark_before[index]);
		crash_mark_installed = 1;
	}
#endif
	return 1;
#endif
}

int ae_private_crash_mark_supported(void)
{
#if defined(HALO_ANDROID) || defined(__EMSCRIPTEN__)
	return 0;
#else
	return 1;
#endif
}

void ae_private_crash_mark_disarm(void)
{
	crash_mark_path[0] = 0;
}

/* (the Android guest links no SDL and its headers have no stat(): stdio there, SDL's calls elsewhere) */
#if defined(HALO_ANDROID)
static int config_save_whole(const char *path, const char *text, size_t length)
{
	FILE *file = fopen(path, "wb");
	int good;

	if (!file)
		return 0;
	good = fwrite(text, 1, length, file) == length;
	good = fflush(file) == 0 && good;
	return fclose(file) == 0 && good;
}
#define CONFIG_SAVE(path, text, length) config_save_whole((path), (text), (length))
#define CONFIG_RENAME(from, to) (rename((from), (to)) == 0)
#define CONFIG_REMOVE(path) remove(path)
static int config_there(const char *path)
{
	FILE *file = fopen(path, "rb");

	if (!file)
		return 0;
	fclose(file);
	return 1;
}
#else
#define CONFIG_SAVE(path, text, length) SDL_SaveFile((path), (text), (length))
#define CONFIG_RENAME(from, to) SDL_RenamePath((from), (to))
#define CONFIG_REMOVE(path) SDL_RemovePath(path)
#define config_there(path) SDL_GetPathInfo((path), NULL)
#endif

int ae_config_file_save(const char *path, const char *text)
{
	char temporary[1100];
	size_t length;

	if (!path || !text)
		return 0;
	length = strlen(text);
	if (snprintf(temporary, sizeof(temporary), "%s.new", path) >= (int)sizeof(temporary))
		return CONFIG_SAVE(path, text, length) ? 1 : 0;
	/* (a full disk or a crash mid-write cuts the copy aside, never the file) */
	if (!CONFIG_SAVE(temporary, text, length))
	{
		CONFIG_REMOVE(temporary);
		return 0;
	}
	if (CONFIG_RENAME(temporary, path))
		return 1;
	CONFIG_REMOVE(temporary);
	return CONFIG_SAVE(path, text, length) ? 1 : 0;
}

int ae_config_file_refuse(const char *path)
{
	static int logged;

	if (!logged)
		platform_log("config: %s could not be read at start: not written this run (settings changed now are not "
			"saved)", path ? path : "config.toml");
	logged = 1;
	return 0;
}

int ae_config_file_there(const char *path)
{
	return path && config_there(path);
}
