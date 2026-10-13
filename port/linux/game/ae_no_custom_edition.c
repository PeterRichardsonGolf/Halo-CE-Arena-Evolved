/*
AE_NO_CUSTOM_EDITION.C

A build without Custom Edition map support (no HALO_CUSTOM_EDITION: the web
build) still links Arena Evolved's units that ask about Custom Edition's
files. Here they are told there are none: no resource map is found, no map of
that family is found, no Ogg Vorbis sound is decoded. With
HALO_CUSTOM_EDITION this file is empty (map_families.c and ce_vorbis.c have
the real ones).
*/

#include "cseries.h"

#ifndef HALO_CUSTOM_EDITION

boolean map_family_resource(
	char const *name,
	char *path,
	long size)
{
	(void)name;
	if (path && size > 0)
		path[0] = 0;
	return FALSE;
}

boolean map_family_find(
	short family,
	char const *file,
	char *path,
	long size)
{
	(void)family;
	(void)file;
	if (path && size > 0)
		path[0] = 0;
	return FALSE;
}

int ce_vorbis_decode(
	const unsigned char *data,
	int size,
	int *channels,
	int *sample_rate,
	short **samples)
{
	(void)data;
	(void)size;
	if (channels)
		*channels = 0;
	if (sample_rate)
		*sample_rate = 0;
	if (samples)
		*samples = NULL;
	return 0;
}

void ce_vorbis_free(
	short *samples)
{
	(void)samples;
}

#endif /* !HALO_CUSTOM_EDITION */
