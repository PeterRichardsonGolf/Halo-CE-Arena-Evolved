/*
WIN32_AE_HOST.C

HOST A SERVER's process work on Windows (port/linux/src/ae_host_dedicated.h; Linux's is ae_host_dedicated.c): this
program started again with --server and its settings file, as a process of its own with no window. Its standard input
is a pipe this game keeps (sv_stop), its output goes to a file; of this game's handles only those go to it (a handle
list), so its ports and files stay this game's. It outlives this game. A stop with no pipe (this game started again) is
the server's named stop event (win32_server.c), so it stops as Ctrl+C stops it; only "kill" ends it at once.
*/

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "ae_host_dedicated.h"
#include "posix.h"
#include "posix_server.h"

void platform_log(char const *format, ...);

static struct
{
	HANDLE process;      /* the server this game started, NULL none */
	DWORD pid;
	HANDLE input;        /* its standard input's write end, NULL none */
} child;

static int wide_of(char const *text, wchar_t *wide, int size)
{
	return MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, size) > 0;
}

int ae_host_dedicated_supported(void)
{
	return 1;
}

int ae_host_dedicated_port_free(int port, int tcp)
{
	WSADATA data;
	struct sockaddr_in address;
	SOCKET handle;
	int free_it;

	WSAStartup(MAKEWORD(2, 2), &data);
	handle = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (handle == INVALID_SOCKET)
		return 0;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons((unsigned short)port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	free_it = bind(handle, (struct sockaddr *)&address, sizeof(address)) == 0;
	closesocket(handle);
	return free_it;
}

static int skipped_variable(wchar_t const *entry)
{
	static wchar_t const *const names[] =
	{
		L"HALO_CONFIG_FILE=", L"HALO_TEST_INPUT=", L"HALO_NETWORK_TEST", L"HALO_SCREENSHOT", L"HALO_EXIT_AFTER=",
		L"HALO_DATA_ROOT=", L"HALO_ARENA_MENUS", L"HALO_DISPLAY", L"HALO_WINDOW", L"HALO_AE_TEST",
	};
	size_t index;

	for (index = 0; index < sizeof(names) / sizeof(names[0]); index++)
	{
		if (!_wcsnicmp(entry, names[index], wcslen(names[index])))
			return 1;
	}
	return 0;
}

/* this program's environment, without the settings that would make it a client's copy, and each "NAME=VALUE" of extra
(a double-NUL ended block, malloc'd) */
static wchar_t *environment_block(char const *const *extra, int extra_count)
{
	wchar_t *own = GetEnvironmentStringsW(), *at, *block, *out;
	size_t length = 2;
	int index;

	if (!own)
		return NULL;
	for (at = own; *at; at += wcslen(at) + 1)
		length += wcslen(at) + 1;
	for (index = 0; index < extra_count; index++)
		length += strlen(extra[index]) + 1;
	block = calloc(length + 1, sizeof(wchar_t));
	if (!block)
	{
		FreeEnvironmentStringsW(own);
		return NULL;
	}
	out = block;
	for (at = own; *at; at += wcslen(at) + 1)
	{
		/* (a drive's "=C:=..." entries have a name that begins with =: kept) */
		if (skipped_variable(at))
			continue;
		memcpy(out, at, (wcslen(at) + 1) * sizeof(wchar_t));
		out += wcslen(at) + 1;
	}
	for (index = 0; index < extra_count; index++)
	{
		int written = MultiByteToWideChar(CP_UTF8, 0, extra[index], -1, out, (int)(length - (size_t)(out - block)));

		if (written > 0)
			out += written;
	}
	*out = 0;
	FreeEnvironmentStringsW(own);
	return block;
}

long ae_host_dedicated_spawn(char const *config, char const *folder, char const *log, char const *const *extra,
	int extra_count, char *why, int why_size)
{
	wchar_t program[MAX_PATH * 2], wide_config[1100], wide_folder[1100], wide_log[1100], *command = NULL, *environment;
	SECURITY_ATTRIBUTES inheritable = { sizeof(inheritable), NULL, TRUE };
	HANDLE read_end = NULL, write_end = NULL, output = INVALID_HANDLE_VALUE, handles[2];
	STARTUPINFOEXW startup;
	PROCESS_INFORMATION information;
	SIZE_T list_size = 0;
	DWORD flags, error = 0;
	BOOL made;
	size_t command_size;

	if (!GetModuleFileNameW(NULL, program, (DWORD)(sizeof(program) / sizeof(program[0]))) ||
		!wide_of(config, wide_config, 1100) || !wide_of(folder, wide_folder, 1100) || !wide_of(log, wide_log, 1100))
	{
		snprintf(why, (size_t)why_size, "this program's path is not known");
		return 0;
	}
	if (!CreatePipe(&read_end, &write_end, &inheritable, 0))
	{
		snprintf(why, (size_t)why_size, "its input could not be made (%lu)", GetLastError());
		return 0;
	}
	/* (this game keeps the write end: the server does not get it) */
	SetHandleInformation(write_end, HANDLE_FLAG_INHERIT, 0);
	output = CreateFileW(wide_log, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (output == INVALID_HANDLE_VALUE)
	{
		snprintf(why, (size_t)why_size, "its log could not be made (%lu)", GetLastError());
		CloseHandle(read_end);
		CloseHandle(write_end);
		return 0;
	}
	command_size = wcslen(program) + wcslen(wide_config) + 64;
	command = malloc(command_size * sizeof(wchar_t));
	environment = environment_block(extra, extra_count);
	if (!command || !environment)
	{
		snprintf(why, (size_t)why_size, "no memory");
		free(command);
		free(environment);
		CloseHandle(read_end);
		CloseHandle(write_end);
		CloseHandle(output);
		return 0;
	}
	_snwprintf(command, command_size, L"\"%ls\" --server --config \"%ls\"", program, wide_config);
	command[command_size - 1] = 0;
	memset(&startup, 0, sizeof(startup));
	startup.StartupInfo.cb = sizeof(startup);
	startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	startup.StartupInfo.hStdInput = read_end;
	startup.StartupInfo.hStdOutput = output;
	startup.StartupInfo.hStdError = output;
	/* (of this game's handles only these three go with it: not its sockets, its files or its pipes) */
	handles[0] = read_end;
	handles[1] = output;
	InitializeProcThreadAttributeList(NULL, 1, 0, &list_size);
	startup.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, list_size);
	if (!startup.lpAttributeList || !InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &list_size) ||
		!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
		sizeof(handles), NULL, NULL))
	{
		snprintf(why, (size_t)why_size, "it could not be started (handles: %lu)", GetLastError());
		if (startup.lpAttributeList)
			HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
		free(command);
		free(environment);
		CloseHandle(read_end);
		CloseHandle(write_end);
		CloseHandle(output);
		return 0;
	}
	/* (no window, no console of its own to see; in no job of this game's that would end it with this game, where Windows
	lets it leave one) */
	flags = EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB;
	memset(&information, 0, sizeof(information));
	made = CreateProcessW(program, command, NULL, NULL, TRUE, flags, environment, wide_folder, &startup.StartupInfo,
		&information);
	if (!made && GetLastError() == ERROR_ACCESS_DENIED)
	{
		flags &= ~(DWORD)CREATE_BREAKAWAY_FROM_JOB;
		made = CreateProcessW(program, command, NULL, NULL, TRUE, flags, environment, wide_folder,
			&startup.StartupInfo, &information);
	}
	if (!made)
		error = GetLastError();
	DeleteProcThreadAttributeList(startup.lpAttributeList);
	HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
	free(command);
	free(environment);
	CloseHandle(read_end);
	CloseHandle(output);
	if (!made)
	{
		CloseHandle(write_end);
		snprintf(why, (size_t)why_size, "it could not be started (%lu)", error);
		return 0;
	}
	CloseHandle(information.hThread);
	if (child.process)
		CloseHandle(child.process);
	if (child.input)
		CloseHandle(child.input);
	child.process = information.hProcess;
	child.pid = information.dwProcessId;
	child.input = write_end;
	platform_log("ae host server: started halo --server (pid %lu)", (unsigned long)child.pid);
	return (long)child.pid;
}

int ae_host_dedicated_alive(long pid)
{
	HANDLE process;
	DWORD code = 0;
	int alive;

	if (pid <= 4)
		return 0;
	if (child.process && (DWORD)pid == child.pid)
	{
		if (GetExitCodeProcess(child.process, &code) && code == STILL_ACTIVE)
			return 1;
		CloseHandle(child.process);
		child.process = NULL;
		if (child.input)
			CloseHandle(child.input);
		child.input = NULL;
		return 0;
	}
	process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
	if (!process)
		return 0;
	alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
	CloseHandle(process);
	return alive;
}

unsigned long long ae_host_dedicated_stamp(long pid)
{
	HANDLE process;
	FILETIME created, exited, kernel, user;
	unsigned long long stamp = 0;

	if (pid <= 4)
		return 0;
	process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
	if (!process)
		return 0;
	if (GetProcessTimes(process, &created, &exited, &kernel, &user))
		stamp = ((unsigned long long)created.dwHighDateTime << 32) | created.dwLowDateTime;
	CloseHandle(process);
	return stamp;
}

int ae_host_dedicated_send(char const *line)
{
	DWORD written = 0;
	size_t length = strlen(line);
	char text[512];

	if (!child.input || length + 2 > sizeof(text))
		return 0;
	memcpy(text, line, length);
	text[length++] = '\n';
	return WriteFile(child.input, text, (DWORD)length, &written, NULL) && written == length;
}

int ae_host_dedicated_terminate(long pid, int kill_it)
{
	if (pid <= 4)
		return 0;
	if (kill_it)
	{
		HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
		int done;

		if (!process)
			return 0;
		done = TerminateProcess(process, 1) != 0;
		CloseHandle(process);
		return done;
	}
	{
		wchar_t name[64];
		HANDLE stop;
		int done;

		/* (win32_server.c's: the server waits on it and stops as Ctrl+C stops it) */
		_snwprintf(name, sizeof(name) / sizeof(name[0]), L"Local\\AEHostServerStop%lu", (unsigned long)pid);
		stop = OpenEventW(EVENT_MODIFY_STATE, FALSE, name);
		if (!stop)
			return 0;
		done = SetEvent(stop) != 0;
		CloseHandle(stop);
		return done;
	}
}

/* a folder's link: a junction (no privilege needed), made by the command shell */
int ae_host_dedicated_link(char const *target, char const *link)
{
	wchar_t wide_target[1100], wide_link[1100], command[2400], system_folder[MAX_PATH], shell[MAX_PATH + 16];
	STARTUPINFOW startup;
	PROCESS_INFORMATION information;
	DWORD code = 1;
	size_t index;

	if (!wide_of(target, wide_target, 1100) || !wide_of(link, wide_link, 1100))
		return 0;
	/* (there already: a link, its target found or not, or a folder) */
	if (GetFileAttributesW(wide_link) != INVALID_FILE_ATTRIBUTES)
		return 1;
	for (index = 0; wide_target[index]; index++)
		if (wide_target[index] == L'/')
			wide_target[index] = L'\\';
	for (index = 0; wide_link[index]; index++)
		if (wide_link[index] == L'/')
			wide_link[index] = L'\\';
	if (!GetSystemDirectoryW(system_folder, MAX_PATH))
		return 0;
	_snwprintf(shell, sizeof(shell) / sizeof(shell[0]), L"%ls\\cmd.exe", system_folder);
	_snwprintf(command, sizeof(command) / sizeof(command[0]), L"\"%ls\" /d /c mklink /J \"%ls\" \"%ls\"", shell,
		wide_link, wide_target);
	memset(&startup, 0, sizeof(startup));
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_HIDE;
	if (!CreateProcessW(shell, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &information))
		return 0;
	WaitForSingleObject(information.hProcess, 10000);
	GetExitCodeProcess(information.hProcess, &code);
	CloseHandle(information.hProcess);
	CloseHandle(information.hThread);
	return code == 0 && GetFileAttributesW(wide_link) != INVALID_FILE_ATTRIBUTES;
}

/* readable by this user alone (win32_server.c's ACL) */
int ae_host_dedicated_write_private(char const *path, char const *text)
{
	return posix_server_write_private_file(path, text, (unsigned long)strlen(text)) != 0;
}

int ae_host_dedicated_folder(char const *path)
{
	SDL_PathInfo information;

	if (!SDL_CreateDirectory(path))
		return 0;
	return SDL_GetPathInfo(path, &information) && information.type == SDL_PATHTYPE_DIRECTORY;
}

long ae_host_dedicated_read(char const *path, char *text, long size)
{
	size_t length = 0;
	void *data;

	if (size <= 0)
		return -1;
	data = SDL_LoadFile(path, &length);
	if (!data)
		return -1;
	if (length > (size_t)size - 1)
		length = (size_t)size - 1;
	memcpy(text, data, length);
	text[length] = 0;
	SDL_free(data);
	return (long)length;
}

int ae_host_dedicated_random_hex(char *text, int count)
{
	unsigned char bytes[64];
	int index;

	if (count <= 0 || count > (int)sizeof(bytes))
		return 0;
	posix_random_bytes(bytes, (posix_ulong)count);
	for (index = 0; index < count; index++)
	{
		text[index * 2] = "0123456789abcdef"[bytes[index] >> 4];
		text[index * 2 + 1] = "0123456789abcdef"[bytes[index] & 15];
	}
	text[count * 2] = 0;
	memset(bytes, 0, sizeof(bytes));
	return 1;
}

struct list_state
{
	char const *path;
	void (*visit)(char const *name, int folder, void *context);
	void *context;
};

static SDL_EnumerationResult SDLCALL list_entry(void *user, char const *directory, char const *name)
{
	struct list_state *state = user;
	char full[1100];
	SDL_PathInfo information;

	(void)directory;
	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return SDL_ENUM_CONTINUE;
	snprintf(full, sizeof(full), "%s/%s", state->path, name);
	state->visit(name, SDL_GetPathInfo(full, &information) && information.type == SDL_PATHTYPE_DIRECTORY,
		state->context);
	return SDL_ENUM_CONTINUE;
}

int ae_host_dedicated_list(char const *path, void (*visit)(char const *name, int folder, void *context), void *context)
{
	struct list_state state = { path, visit, context };

	return SDL_EnumerateDirectory(path, list_entry, &state) ? 1 : 0;
}
