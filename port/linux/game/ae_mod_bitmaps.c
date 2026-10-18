/*
AE_MOD_BITMAPS.C

Arena Evolved: OpenCE's client-side mods without their bitmaps, for now.

OpenCE's mod_bitmaps.c reads a mod's Halo PC bitmap tags through OpenCE's
Custom Edition layer (custom_edition_bitmaps.c, cache_file_formats.c: the
channel orders Halo PC keeps, the HUD placements' walk, the drawable check),
which Arena Evolved does not have: its Custom Edition maps are
ChupathingyCE's (ce_*.c), which mark a texture's channels another way. Until
an adapter onto that is written, mod_bitmaps.c is left out (its AE hooks) and
these stand in: a mod's bitmap files are passed over, said once in the log,
and the map's own are drawn. Its sounds, text and models play.
*/

#include "cseries.h"
#include "mods.h"

void platform_log(char const *format, ...);

long mod_bitmaps_update(
	boolean remove_all,
	boolean at_load)
{
	static boolean said;

	(void)at_load;
	if (!remove_all && !said)
	{
		platform_log("mods: Arena Evolved does not play mods' bitmaps yet (their sounds, text and models play)");
		said = TRUE;
	}
	return 0;
}

void mod_bitmaps_tags_unloaded(
	void)
{
	return;
}

boolean mod_bitmaps_read(
	struct bitmap_data *bitmap,
	void *buffer)
{
	(void)bitmap;
	(void)buffer;
	return FALSE;
}

boolean mod_bitmaps_contain(
	struct bitmap_data const *bitmap)
{
	(void)bitmap;
	return FALSE;
}
