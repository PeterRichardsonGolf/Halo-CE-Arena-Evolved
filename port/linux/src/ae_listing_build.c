/*
AE_LISTING_BUILD.C

An Arena Evolved host's marker and version in its public listing's stamp (ae_listing_build.h).
*/

#include <stdlib.h>
#include <string.h>

#include "ae_listing_build.h"

void ae_listing_build_write(unsigned char *stamp, const char *version)
{
	int index;
	const char *cursor = version ? version : "";

	stamp[0] = 'A';
	for (index = 0; index < 3; index++)
	{
		long number = 0;

		if (*cursor >= '0' && *cursor <= '9')
		{
			number = strtol(cursor, (char **)&cursor, 10);
			if (*cursor == '.')
				cursor++;
		}
		stamp[1 + index] = (unsigned char)(number > 255 ? 255 : number);
	}
}

unsigned long ae_listing_build_read(const unsigned char *stamp)
{
	if (!stamp || stamp[0] != 'A')
		return 0;
	return 1ul << 24 | (unsigned long)stamp[1] << 16 | (unsigned long)stamp[2] << 8 | stamp[3];
}
