/*
WIN32_CAPTURE.C

The capture's system side on Windows (port/linux/src/capture_child.h; the
other systems' is in capture.c): ffmpeg started with CreateProcessW, with
- no console window (CREATE_NO_WINDOW): ffmpeg is a console program and
  the game is not, so it would open one, and take the focus from a
  fullscreen game;
- only its own handles inherited (PROC_THREAD_ATTRIBUTE_HANDLE_LIST): the
  read end of its input pipe and NUL, never the game's sockets or files,
  and those made inheritable only once made (never the game's end);
- an input pipe that waits (anonymous pipes do) with a 1 MB buffer, which
  the writer thread fills a frame at a time;
- its command line quoted as the C runtime's parser reads it back
  (CommandLineToArgvW's rules).
Files made with CREATE_NEW, so a name is never taken twice; an earlier
session's leftovers cleared without following a reparse point.

Paths and arguments are UTF-8.
*/

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "capture_child.h"

#define PIPE_BUFFER_BYTES (1 << 20)
/* CreateProcessW's command line, in characters with its terminator */
#define COMMAND_LINE_MAXIMUM 32767

struct capture_child
{
	HANDLE process;
	HANDLE input;
};

/* text in a new wide string (free()), or NULL */
static wchar_t *wide_from_utf8(const char *text)
{
	int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
	wchar_t *wide;

	if (length <= 0)
		return NULL;
	wide = malloc((size_t)length * sizeof(wchar_t));
	if (wide && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, length) <= 0)
	{
		free(wide);
		wide = NULL;
	}
	return wide;
}

struct command_line
{
	wchar_t *text;
	size_t length;
	int failed;
};

static void command_put(struct command_line *line, wchar_t character, size_t repeat)
{
	while (repeat-- > 0 && !line->failed)
	{
		if (line->length + 1 >= COMMAND_LINE_MAXIMUM)
		{
			line->failed = 1;
			return;
		}
		line->text[line->length++] = character;
	}
}

/* one argument, quoted when it has to be: backslashes are doubled only
before a quote (escaped) or the closing quote */
static void command_append(struct command_line *line, const wchar_t *argument)
{
	const wchar_t *at;

	if (line->length)
		command_put(line, L' ', 1);
	if (*argument && !wcspbrk(argument, L" \t\n\v\""))
	{
		for (at = argument; *at; at++)
			command_put(line, *at, 1);
		return;
	}
	command_put(line, L'"', 1);
	for (at = argument;; at++)
	{
		size_t backslashes = 0;

		while (*at == L'\\')
		{
			backslashes++;
			at++;
		}
		if (!*at)
		{
			command_put(line, L'\\', backslashes * 2);
			break;
		}
		if (*at == L'"')
		{
			command_put(line, L'\\', backslashes * 2 + 1);
			command_put(line, L'"', 1);
		}
		else
		{
			command_put(line, L'\\', backslashes);
			command_put(line, *at, 1);
		}
	}
	command_put(line, L'"', 1);
}

struct capture_child *capture_child_start(const char *const *arguments, int with_input)
{
	STARTUPINFOEXW startup;
	PROCESS_INFORMATION information;
	LPPROC_THREAD_ATTRIBUTE_LIST attributes = NULL;
	SIZE_T attributes_size = 0;
	HANDLE inherited[2];
	HANDLE read_end = NULL, write_end = NULL, nul = INVALID_HANDLE_VALUE;
	struct command_line line = { NULL, 0, 0 };
	wchar_t *application = NULL;
	struct capture_child *child = NULL;
	int index;

	line.text = malloc(COMMAND_LINE_MAXIMUM * sizeof(wchar_t));
	application = wide_from_utf8(arguments[0]);
	if (!line.text || !application)
		goto done;
	for (index = 0; arguments[index]; index++)
	{
		wchar_t *argument = wide_from_utf8(arguments[index]);

		if (!argument)
		{
			line.failed = 1;
			break;
		}
		command_append(&line, argument);
		free(argument);
	}
	if (line.failed)
		goto done;
	line.text[line.length] = 0;

	/* every handle made not inheritable, then the child's own (NUL, the
	pipe's read end) marked so: the game's end of the pipe never is, even
	for a moment (a child of it would keep ffmpeg's input open) */
	nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, 0, NULL);
	if (nul == INVALID_HANDLE_VALUE || !SetHandleInformation(nul, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
		goto done;
	if (with_input)
	{
		if (!CreatePipe(&read_end, &write_end, NULL, PIPE_BUFFER_BYTES))
			goto done;
		if (!SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
			goto done;
	}
	inherited[0] = nul;
	inherited[1] = read_end;
	InitializeProcThreadAttributeList(NULL, 1, 0, &attributes_size);
	attributes = malloc(attributes_size);
	if (!attributes || !InitializeProcThreadAttributeList(attributes, 1, 0, &attributes_size))
	{
		free(attributes);
		attributes = NULL;
		goto done;
	}
	if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
		(with_input ? 2 : 1) * sizeof(HANDLE), NULL, NULL))
		goto done;

	memset(&startup, 0, sizeof(startup));
	startup.StartupInfo.cb = sizeof(startup);
	startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	startup.StartupInfo.hStdInput = with_input ? read_end : nul;
	startup.StartupInfo.hStdOutput = nul;
	startup.StartupInfo.hStdError = nul;
	startup.lpAttributeList = attributes;
	memset(&information, 0, sizeof(information));
	if (!CreateProcessW(application, line.text, NULL, NULL, TRUE, EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW,
		NULL, NULL, &startup.StartupInfo, &information))
		goto done;
	CloseHandle(information.hThread);
	child = calloc(1, sizeof(*child));
	if (!child)
	{
		TerminateProcess(information.hProcess, 1);
		CloseHandle(information.hProcess);
		goto done;
	}
	child->process = information.hProcess;
	child->input = write_end;
	write_end = NULL;

done:
	if (attributes)
	{
		DeleteProcThreadAttributeList(attributes);
		free(attributes);
	}
	if (read_end)
		CloseHandle(read_end);
	if (write_end)
		CloseHandle(write_end);
	if (nul != INVALID_HANDLE_VALUE)
		CloseHandle(nul);
	free(application);
	free(line.text);
	return child;
}

int capture_child_write(struct capture_child *child, const void *data, size_t size)
{
	const unsigned char *bytes = data;

	if (!child->input)
		return 0;
	while (size)
	{
		DWORD piece = size > 0x40000000u ? 0x40000000u : (DWORD)size, written = 0;

		if (!WriteFile(child->input, bytes, piece, &written, NULL) || !written)
			return 0;
		bytes += written;
		size -= written;
	}
	return 1;
}

void capture_child_close_input(struct capture_child *child)
{
	if (child->input)
	{
		CloseHandle(child->input);
		child->input = NULL;
	}
}

int capture_child_wait(struct capture_child *child, int timeout_ms, int *exit_code)
{
	DWORD code = 0;

	if (WaitForSingleObject(child->process, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms) != WAIT_OBJECT_0)
		return 0;
	if (exit_code)
		*exit_code = GetExitCodeProcess(child->process, &code) ? (int)code : -1;
	return 1;
}

void capture_child_kill(struct capture_child *child)
{
	TerminateProcess(child->process, 1);
}

void capture_child_free(struct capture_child *child)
{
	if (child->input)
		CloseHandle(child->input);
	CloseHandle(child->process);
	free(child);
}

int capture_file_reserve(const char *path)
{
	wchar_t *wide = wide_from_utf8(path);
	HANDLE file;
	DWORD error;

	if (!wide)
		return -1;
	file = CreateFileW(wide, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	error = GetLastError();
	free(wide);
	if (file == INVALID_HANDLE_VALUE)
		return error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS ? 0 : -1;
	CloseHandle(file);
	return 1;
}

int capture_file_executable(const char *path)
{
	wchar_t *wide = wide_from_utf8(path);
	DWORD attributes;

	if (!wide)
		return 0;
	attributes = GetFileAttributesW(wide);
	free(wide);
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

/* the capture's leftovers (capture_child.h): the folder held open (and so
not renamed or replaced meanwhile), refused if it is a reparse point (a
link or junction); each file opened without following one
(FILE_FLAG_OPEN_REPARSE_POINT), looked at and removed through that handle */
int capture_folder_clean(const char *folder, int stale_seconds, int (*owned)(const char *name))
{
	wchar_t *wide = wide_from_utf8(folder), *pattern = NULL, *path = NULL;
	HANDLE directory, find = INVALID_HANDLE_VALUE;
	BY_HANDLE_FILE_INFORMATION information;
	WIN32_FIND_DATAW data;
	FILETIME now_time;
	ULONGLONG now;
	size_t folder_length;
	int removed = 0;

	if (!wide)
		return -1;
	directory = CreateFileW(wide, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	if (directory == INVALID_HANDLE_VALUE)
	{
		free(wide);
		return -1;
	}
	if (!GetFileInformationByHandle(directory, &information) ||
		(information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
		!(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		CloseHandle(directory);
		free(wide);
		return -1;
	}
	folder_length = wcslen(wide);
	pattern = malloc((folder_length + 3) * sizeof(wchar_t));
	path = malloc((folder_length + 2 + MAX_PATH) * sizeof(wchar_t));
	if (pattern && path)
	{
		memcpy(pattern, wide, folder_length * sizeof(wchar_t));
		pattern[folder_length] = L'\\';
		pattern[folder_length + 1] = L'*';
		pattern[folder_length + 2] = 0;
		find = FindFirstFileW(pattern, &data);
	}
	GetSystemTimeAsFileTime(&now_time);
	now = ((ULONGLONG)now_time.dwHighDateTime << 32) | now_time.dwLowDateTime;
	while (find != INVALID_HANDLE_VALUE)
	{
		char name[MAX_PATH * 4];
		size_t name_length = wcslen(data.cFileName);
		int kind = 0;

		if (!(data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
			name_length < MAX_PATH &&
			WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, data.cFileName, -1, name, (int)sizeof(name), NULL,
			NULL) > 0)
			kind = owned(name);
		if (kind)
		{
			HANDLE file;

			memcpy(path, wide, folder_length * sizeof(wchar_t));
			path[folder_length] = L'\\';
			memcpy(path + folder_length + 1, data.cFileName, (name_length + 1) * sizeof(wchar_t));
			file = CreateFileW(path, DELETE | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
			if (file != INVALID_HANDLE_VALUE)
			{
				BY_HANDLE_FILE_INFORMATION file_information;

				if (GetFileInformationByHandle(file, &file_information) &&
					!(file_information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT |
					FILE_ATTRIBUTE_DEVICE)) &&
					(kind != 2 || (!file_information.nFileSizeHigh && !file_information.nFileSizeLow)))
				{
					ULONGLONG written = ((ULONGLONG)file_information.ftLastWriteTime.dwHighDateTime << 32) |
						file_information.ftLastWriteTime.dwLowDateTime;

					/* (100 ns ticks) */
					if (now > written && (now - written) / 10000000ull >= (ULONGLONG)stale_seconds)
					{
						FILE_DISPOSITION_INFO disposition;

						disposition.DeleteFile = TRUE;
						if (SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof(disposition)))
							removed++;
					}
				}
				CloseHandle(file);
			}
		}
		if (!FindNextFileW(find, &data))
		{
			FindClose(find);
			find = INVALID_HANDLE_VALUE;
		}
	}
	free(pattern);
	free(path);
	CloseHandle(directory);
	free(wide);
	return removed;
}
