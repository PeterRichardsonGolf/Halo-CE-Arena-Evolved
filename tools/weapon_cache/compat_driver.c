/*
COMPAT_DRIVER.C

One "machine" of the cache compatibility harness (tools/test_cache_compat.py): the join name field's pure rules of ONE
build, compiled from that build's own ae_silent_rules.[ch] and ae_weapon_slots.[ch]. What the build does is found by
which macros its header has (the level character: 0.7.6 widen; the feature mark: grenade layer), so the same file
serves every build.

  info                          the build's facts, "key=value" words
  encode <level> <gren> <name>  the 32-character machine name a join request of this build carries (hex, 4 per
                                character); gren 0 none, 1-7 a grenade layer version
  features <gren> <hex...>      the given name with this build's feature mark written over it (the order hazard)
  decode <hex...>               what this build's host reads of such a name: "cap=N features=0xNN" (features=na
                                on a build without the feature mark)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_silent_rules.h"
#include "ae_weapon_slots.h"

#define LENGTH 32

static void print_name(unsigned short const *name)
{
	int index;

	for (index = 0; index < LENGTH; index++)
		printf("%04X%s", name[index], index + 1 < LENGTH ? " " : "\n");
}

int main(int argc, char **argv)
{
	unsigned short name[LENGTH];
	int index;

	if (argc >= 2 && !strcmp(argv[1], "info"))
	{
		printf("slot_count=%d ", (int)AE_WEAPON_SLOT_COUNT);
#ifdef AE_SILENT_JOIN_LEVEL_MARK
		printf("level_char=1 ");
#else
		printf("level_char=0 ");
#endif
#ifdef AE_SILENT_JOIN_FEATURE_MARK
		printf("feature_mark=1 ");
#else
		printf("feature_mark=0 ");
#endif
#ifdef AE_SILENT_JOIN_CACHE_MARK
		printf("cache_mark=1\n");
#else
		printf("cache_mark=0\n");
#endif
		return 0;
	}
	if (argc >= 5 && !strcmp(argv[1], "encode"))
	{
		int level = atoi(argv[2]);
		int grenades = atoi(argv[3]);
		char const *text = argv[4];

		memset(name, 0, sizeof(name));
		for (index = 0; text[index] && index < LENGTH - 1; index++)
			name[index] = (unsigned char)text[index];
		/* (the request's order, ae_silent.c ae_silent_join_request_mark: the mark, the level, the features) */
		ae_silent_join_mark(name, LENGTH);
		if (level >= AE_RIFLE_CAPABILITY)
		{
#ifdef AE_SILENT_JOIN_LEVEL_MARK
			ae_silent_join_level(name, LENGTH, level);
#else
			name[LENGTH - 1] = (unsigned short)(AE_SILENT_JOIN_MARK | (unsigned int)level);
#endif
		}
#ifdef AE_SILENT_JOIN_FEATURE_MARK
		if (grenades > 0)
			ae_silent_join_features(name, LENGTH, ae_silent_join_feature_grenades(grenades));
#endif
#ifdef AE_SILENT_JOIN_CACHE_MARK
		/* (the cache identity last: argv[5] format, [6] generation, [7] digest; see COMPAT.md) */
		if (argc >= 8)
		{
			unsigned long digest = strtoul(argv[7], NULL, 0);
			unsigned char bytes[4] = { (unsigned char)digest, (unsigned char)(digest >> 8), (unsigned char)(digest >> 16),
				(unsigned char)(digest >> 24) };

			ae_silent_join_cache(name, LENGTH, atoi(argv[5]), atoi(argv[6]), bytes);
		}
#endif
		print_name(name);
		return 0;
	}
	if (argc >= 3 + LENGTH && !strcmp(argv[1], "features"))
	{
		for (index = 0; index < LENGTH; index++)
			name[index] = (unsigned short)strtoul(argv[3 + index], NULL, 16);
#ifdef AE_SILENT_JOIN_FEATURE_MARK
		ae_silent_join_features(name, LENGTH, ae_silent_join_feature_grenades(atoi(argv[2])));
#endif
		print_name(name);
		return 0;
	}
	if (argc >= 2 + LENGTH && !strcmp(argv[1], "decode"))
	{
		for (index = 0; index < LENGTH; index++)
			name[index] = (unsigned short)strtoul(argv[2 + index], NULL, 16);
		printf("cap=%d ", ae_silent_join_capability(name, LENGTH));
#ifdef AE_SILENT_JOIN_FEATURE_MARK
		printf("features=0x%02X", ae_silent_join_features_told(name, LENGTH));
#else
		printf("features=na");
#endif
#ifdef AE_SILENT_JOIN_CACHE_MARK
		{
			unsigned int format = 0, generation = 0;
			unsigned char bytes[4] = { 0, 0, 0, 0 };

			ae_silent_join_cache_told(name, LENGTH, &format, &generation, bytes);
			printf(" cache=%u/%u/0x%08lX", format, generation,
				(unsigned long)bytes[0] | ((unsigned long)bytes[1] << 8) | ((unsigned long)bytes[2] << 16) |
				((unsigned long)bytes[3] << 24));
		}
#endif
		printf("\n");
		return 0;
	}
	fprintf(stderr, "usage: info | encode level grenades name | decode 32x hex\n");
	return 2;
}
