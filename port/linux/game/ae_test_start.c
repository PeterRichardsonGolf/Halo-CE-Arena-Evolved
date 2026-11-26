/*
AE_TEST_START.C

A campaign level's start point for TEST RUN tests. See ae_test_start.h.
*/

#include <stdio.h>
#include <string.h>
#include <xtl.h>
#include "halo_port_capacity.h"

#include "ae_test_start.h"
#include "../src/ae_mcc_file_stamp.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
#if defined(HALO_WEB) || defined(HALO_ANDROID)
/* (the web build and the Android app have no program file of their own to read) */
static int update_executable_path(char *path, int size)
{
	if (size > 0)
		path[0] = 0;
	return 0;
}
#else
int update_executable_path(char *path, int size);
#endif
unsigned char player_ui_get_path_to_local_player_profile_directory(short local_player_index, char *path);
/* game_state.h's */
unsigned char game_state_port_write_live_to_persistent_storage(void);

static char file_name[160];
static char stamp_name[176];
static int armed, readable;
static unsigned long loads;

/* the program file's size and last write time as text, "" when they cannot be read (no start point is then read: the
file is made again each time; macOS takes no stamp). Its change time is left out: a copy of the same program (a test
harness's) has the same bytes and write time */
static void program_stamp(char *stamp, size_t size)
{
	char path[1024];
	struct ae_mcc_file_stamp taken;

	stamp[0] = 0;
	if (!update_executable_path(path, (int)sizeof(path)) || !ae_mcc_file_stamp(path, &taken))
		return;
	snprintf(stamp, size, "%u-%u-%u-%u", taken.size_low, taken.size_high, taken.written_seconds, taken.written_fraction);
}

static int profile_path(const char *name, char *path, size_t size)
{
	char directory[512];

	if (!player_ui_get_path_to_local_player_profile_directory(0, directory))
		return 0;
	snprintf(path, size, "%s%s", directory, name);
	return 1;
}

static int stamp_matches(void)
{
	char now[96], path[700], stored[96];
	HANDLE handle;
	DWORD read = 0;

	program_stamp(now, sizeof(now));
	if (!now[0] || !profile_path(stamp_name, path, sizeof(path)))
		return 0;
	handle = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		return 0;
	memset(stored, 0, sizeof(stored));
	if (!ReadFile(handle, stored, sizeof(stored) - 1, &read, NULL))
		read = 0;
	CloseHandle(handle);
	stored[read < sizeof(stored) ? read : sizeof(stored) - 1] = 0;
	return !strcmp(stored, now);
}

static int file_has_saved_game_size(void)
{
	char path[700];
	HANDLE handle;
	DWORD size;

	if (!profile_path(file_name, path, sizeof(path)))
		return 0;
	handle = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		return 0;
	size = GetFileSize(handle, NULL);
	CloseHandle(handle);
	return size == (DWORD)HALO_PORT_GAME_STATE_SIZE;
}

int ae_test_start_arm(const char *level, unsigned long variant)
{
	char clean[64];
	size_t index;

	for (index = 0; level && level[index] && index < sizeof(clean) - 1; index++)
	{
		char c = level[index];

		clean[index] = (c >= 'A' && c <= 'Z') ? (char)(c + 'a' - 'A') :
			((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) ? c : '_';
	}
	clean[index] = 0;
	if (!clean[0])
		return 0;
	snprintf(file_name, sizeof(file_name), "ae-test-start-%s-%08lx.bin", clean, variant & 0xffffffffUL);
	snprintf(stamp_name, sizeof(stamp_name), "%s.stamp", file_name);
	armed = 1;
	readable = file_has_saved_game_size() && stamp_matches();
	platform_log("test start: %s armed (%s)", file_name, readable ? "made by this program: read" : "none of this program's: the level will make one");
	return readable;
}

const char *ae_test_start_file(void)
{
	return armed ? file_name : NULL;
}

int ae_test_start_readable(void)
{
	return armed && readable;
}

void ae_test_start_note_loaded(void)
{
	loads++;
}

unsigned long ae_test_start_loads(void)
{
	return loads;
}

int ae_test_start_capture(void)
{
	char now[96], path[700];
	HANDLE handle;
	DWORD written = 0;
	unsigned long started = system_milliseconds();
	int done;

	if (!armed)
		return 0;
	/* (the old stamp goes first: a file that is half made is not read) */
	if (profile_path(stamp_name, path, sizeof(path)))
		DeleteFileA(path);
	done = game_state_port_write_live_to_persistent_storage();
	program_stamp(now, sizeof(now));
	if (!done || !now[0] || !profile_path(stamp_name, path, sizeof(path)))
	{
		platform_log("test start: %s not taken (%s)", file_name, !done ? "more than one player is spawned, or no header" : !now[0] ? "the program file's stamp cannot be read" : "no profile folder");
		return 0;
	}
	handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		return 0;
	done = WriteFile(handle, now, (DWORD)strlen(now), &written, NULL) && written == (DWORD)strlen(now);
	CloseHandle(handle);
	platform_log("test start: %s taken in %lu ms", file_name, system_milliseconds() - started);
	return done;
}

void ae_test_start_disarm(void)
{
	armed = 0;
	readable = 0;
}
