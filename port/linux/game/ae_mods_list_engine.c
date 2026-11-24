/*
AE_MODS_LIST_ENGINE.C

The mods list's engine (ae_mods_list.h's struct ae_mods_source): the mods folder's mods as port_mods.c finds them, and
mods.c playing them on the loaded map.
*/

#include "cseries.h"
#include "mods.h"
#include "ae_mods_list.h"

/* (port/linux/src/port_mods.c) */
const char *port_mods_name(long index);
int port_mods_status(long index, char *text, unsigned long size);
long port_mods_move(long position, long places);
void port_mods_turn(long index, int on);
int port_mods_save(void);
int port_mods_fields(long index, const char **id, const char **version, const char **authors, const char **description,
	const char **refusal, long *map_count, long *played_place, long *file_count, int *flags);
const char *port_mods_map(long index, long map);
long port_mods_files(void);
const char *port_mods_file_key(long file, long *mod);
/* (mods.c) */
boolean mods_active(void);

static void engine_apply(void)
{
	mods_apply();
}

static int engine_active(void)
{
	return mods_active() ? 1 : 0;
}

static const struct ae_mods_source engine =
{
	port_mods_count,
	port_mods_at,
	port_mods_folder,
	port_mods_name,
	port_mods_fields,
	port_mods_map,
	port_mods_status,
	port_mods_files,
	port_mods_file_key,
	port_mods_move,
	port_mods_turn,
	port_mods_save,
	engine_apply,
	engine_active,
};

const struct ae_mods_source *ae_mods_list_engine(void)
{
	return &engine;
}
