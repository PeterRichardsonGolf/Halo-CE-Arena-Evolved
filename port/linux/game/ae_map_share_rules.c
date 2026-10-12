/*
AE_MAP_SHARE_RULES.C

Arena Evolved: map sharing's safety rules (ae_map_share_rules.h). From
DamnationCE v0.3.41 (CC0), the address ranges as its address_local.
*/

#include "ae_map_share_rules.h"

#include <ctype.h>
#include <string.h>

int ae_map_share_address_local(
	unsigned long address)
{
	/* (an unsigned long of 64 bits: nothing above the 32 of an address) */
	if ((address >> 16) >> 16)
		return 0;
	return (address & 0xFF000000UL) == 0x0A000000UL || /* 10.0.0.0/8 */
		(address & 0xFFF00000UL) == 0xAC100000UL || /* 172.16.0.0/12 */
		(address & 0xFFFF0000UL) == 0xC0A80000UL || /* 192.168.0.0/16 */
		(address & 0xFFFF0000UL) == 0xA9FE0000UL || /* 169.254.0.0/16 */
		(address & 0xFF000000UL) == 0x7F000000UL; /* 127.0.0.0/8 */
}

int ae_map_share_path_in_folder(
	char const *path,
	char const *folder)
{
	size_t folder_length;
	size_t name_length;
	char const *name;
	size_t index;

	if (!path || !folder || !folder[0])
		return 0;
	folder_length = strlen(folder);
	if (strncmp(path, folder, folder_length) != 0)
		return 0;
	name = path + folder_length;
	name_length = strlen(name);
	/* (a name before .map) */
	if (name_length <= 4)
		return 0;
	for (index = 0; index < name_length; index++)
	{
		unsigned char c = (unsigned char)name[index];

		/* (no other folder: no separator, drive, or control character) */
		if (c == '\\' || c == '/' || c == ':' || c < 0x20)
			return 0;
	}
	/* (neither "." nor ".." nor anything that starts with them) */
	if (name[0] == '.' || strstr(name, ".."))
		return 0;
	return tolower((unsigned char)name[name_length - 4]) == '.' &&
		tolower((unsigned char)name[name_length - 3]) == 'm' &&
		tolower((unsigned char)name[name_length - 2]) == 'a' &&
		tolower((unsigned char)name[name_length - 1]) == 'p';
}

int ae_map_share_discard(
	char const *const *paths,
	int count,
	char const *folder,
	int (*delete_file)(char const *path, void *context),
	void *context)
{
	int removed = 0;
	int index;

	if (!paths || !delete_file)
		return 0;
	for (index = 0; index < count; index++)
	{
		if (!ae_map_share_path_in_folder(paths[index], folder))
			continue;
		if (delete_file(paths[index], context))
			removed++;
	}
	return removed;
}
