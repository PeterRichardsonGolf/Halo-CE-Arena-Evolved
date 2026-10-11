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
