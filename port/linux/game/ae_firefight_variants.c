/*
AE_FIREFIGHT_VARIANTS.C

See ae_firefight_variants.h.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_private_files.h"
#include "ae_firefight_variants.h"
#include "halo_firefight.h"

void platform_log(char const *format, ...);

#define FILE_NAME "firefight_variants.txt"

typedef char verify_builtins[AE_FFV_BUILTINS == NUMBER_OF_BUILT_IN_FIREFIGHT_PROFILES ? 1 : -1];

/* (firefight.c's built-in profiles, by the names it builds them under) */
static struct { const char *key, *name; } const builtins[AE_FFV_BUILTINS] = {
	{ "firefight", "Firefight" }, { "generator_defense", "Generator Defense" }, { "gruntpocalypse", "Gruntpocalypse" },
	{ "rocketfight", "Rocketfight" }, { "floodfight", "Floodfight" }, { "mythic_firefight", "Mythic Firefight" },
	{ "fiesta", "Fiesta" }, { "grunt_fiesta", "Grunt Fiesta" } };

const char *ae_ffv_builtin_key(int index)
{
	return index >= 0 && index < AE_FFV_BUILTINS ? builtins[index].key : NULL;
}

const char *ae_ffv_builtin_name(int index)
{
	return index >= 0 && index < AE_FFV_BUILTINS ? builtins[index].name : NULL;
}

int ae_ffv_saved_load(struct ae_ffv_record *records, int maximum)
{
	static char text[AE_FFV_SAVED * (AE_FFV_LINE + 128)];
	long length = ae_private_file_read(FILE_NAME, text, (long)sizeof(text));

	if (length < 0)
		return 0;
	return ae_ffv_file_read(text, records, maximum);
}

int ae_ffv_saved_store(struct ae_ffv_record const *records, int count)
{
	static char text[AE_FFV_SAVED * (AE_FFV_LINE + 128)];

	if (!ae_ffv_file_write(records, count, text, sizeof(text)))
	{
		platform_log("ae firefight variants: the file could not be made");
		return 0;
	}
	return ae_private_file_write(FILE_NAME, text, (long)strlen(text));
}

int ae_ffv_name_taken(const char *name)
{
	struct ae_ffv_record records[AE_FFV_SAVED];
	int count = ae_ffv_saved_load(records, AE_FFV_SAVED), index;

	for (index = 0; index < AE_FFV_BUILTINS; index++)
	{
		if (ae_ffv_name_same(builtins[index].name, name))
			return 1;
	}
	for (index = 0; index < count; index++)
	{
		if (ae_ffv_name_same(records[index].name, name))
			return 1;
	}
	return 0;
}

int ae_ffv_entries(char (*names)[24], int *builtin, int maximum)
{
	struct ae_ffv_record records[AE_FFV_SAVED];
	int count = ae_ffv_saved_load(records, AE_FFV_SAVED), index, written = 0;

	for (index = 0; index < AE_FFV_BUILTINS && written < maximum; index++, written++)
	{
		snprintf(names[written], 24, "%s", builtins[index].name);
		if (builtin)
			builtin[written] = 1;
	}
	for (index = 0; index < count && written < maximum; index++, written++)
	{
		snprintf(names[written], 24, "%s", records[index].name);
		if (builtin)
			builtin[written] = 0;
	}
	return written;
}
