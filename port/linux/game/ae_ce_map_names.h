/*
AE_CE_MAP_NAMES.H

Arena Evolved's names for Custom Edition maps whose file names say nothing (ui_map_list.c's ui_map_list_family_name,
before the file's name made readable). The file keeps its name: joiners and map sharing find a map by it.
*/

#ifndef __AE_CE_MAP_NAMES_H
#define __AE_CE_MAP_NAMES_H

#include <wchar.h>

static struct
{
	char const *file;
	wchar_t const *name;
} const ae_ce_map_names[] =
{
	/* (Zteam's remake of The Pit, its beta: owner 2026-10-26) */
	{ "cyberdyneb2", L"The Pit" },
};

/* the map's AE name by its file's name (any case), or NULL */
static inline wchar_t const *ae_ce_map_name(char const *file)
{
	unsigned int index;

	for (index = 0; file && index < sizeof(ae_ce_map_names) / sizeof(ae_ce_map_names[0]); index++)
	{
		char const *a = file, *b = ae_ce_map_names[index].file;

		while (*a && *b && (*a | 0x20) == (*b | 0x20))
			a++, b++;
		if (!*a && !*b)
			return ae_ce_map_names[index].name;
	}
	return NULL;
}

#endif
