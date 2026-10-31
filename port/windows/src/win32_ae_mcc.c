/*
WIN32_AE_MCC.C

Windows' Steam roots for AE's MCC detection (port/linux/src/ae_mcc.h,
ae_mcc_platform.c): HKCU\Software\Valve\Steam's SteamPath (as Steam writes
it: "c:/program files (x86)/steam"), then HKLM's InstallPath (the 32-bit
view, WOW6432Node, where the installer keeps it), then the default folder.
Each is looked in for steamapps\libraryfolders.vdf by the detection. The
paths are the ANSI code page's, as the port's other Windows file calls are.
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "ae_mcc.h"
#include "ae_mcc_workshop.h"

/* a registry string value, terminated; 0 if none */
static int registry_string(HKEY base, const char *subkey, const char *value, REGSAM view, char *text, DWORD size)
{
	HKEY key;
	DWORD type = 0, length = size - 1;
	LONG result;

	if (RegOpenKeyExA(base, subkey, 0, KEY_READ | view, &key) != ERROR_SUCCESS)
		return 0;
	result = RegQueryValueExA(key, value, NULL, &type, (BYTE *)text, &length);
	RegCloseKey(key);
	if (result != ERROR_SUCCESS || type != REG_SZ || !length)
		return 0;
	text[length < size ? length : size - 1] = 0;
	return text[0] != 0;
}

static int add_root(char (*roots)[AE_MCC_PATH_SIZE], int count, int maximum, const char *path)
{
	int index;

	if (!path[0] || count >= maximum || strlen(path) >= AE_MCC_PATH_SIZE)
		return count;
	/* (the same folder spelled by another source: once; Windows' names are case-blind) */
	for (index = 0; index < count; index++)
	{
		if (ae_mcc_same_folder(roots[index], path, 1))
			return count;
	}
	snprintf(roots[count], AE_MCC_PATH_SIZE, "%s", path);
	return count + 1;
}

int ae_mcc_windows_steam_roots(char (*roots)[AE_MCC_PATH_SIZE], int maximum)
{
	char path[AE_MCC_PATH_SIZE];
	int count = 0;

	if (registry_string(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", 0, path, sizeof(path)))
		count = add_root(roots, count, maximum, path);
	if (registry_string(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath", KEY_WOW64_64KEY,
		path, sizeof(path)) ||
		registry_string(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", "InstallPath", KEY_WOW64_32KEY, path, sizeof(path)))
	{
		count = add_root(roots, count, maximum, path);
	}
	count = add_root(roots, count, maximum, "C:\\Program Files (x86)\\Steam");
	return count;
}

/* ---------- the Workshop scan's file access (ae_mcc_workshop.h): ANSI code page paths, as above */

int ae_mcc_workshop_list_folder(const char *folder, ae_mcc_workshop_entry_function entry, void *context)
{
	char pattern[AE_MCC_PATH_SIZE + 4];
	WIN32_FIND_DATAA data;
	HANDLE handle;

	if (strlen(folder) >= AE_MCC_PATH_SIZE)
		return 0;
	snprintf(pattern, sizeof(pattern), "%s\\*", folder);
	handle = FindFirstFileA(pattern, &data);
	if (handle == INVALID_HANDLE_VALUE)
		return 0;
	do
	{
		int is_folder = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		int is_device = (data.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) != 0;

		if (!strcmp(data.cFileName, ".") || !strcmp(data.cFileName, ".."))
			continue;
		if (entry(context, data.cFileName, is_folder, !is_folder && !is_device))
			break;
	} while (FindNextFileA(handle, &data));
	FindClose(handle);
	return 1;
}

FILE *ae_mcc_workshop_open(const char *path)
{
	return fopen(path, "rb");
}

long long ae_mcc_workshop_file_size(FILE *file)
{
	if (_fseeki64(file, 0, SEEK_END) != 0)
		return -1;
	return (long long)_ftelli64(file);
}
