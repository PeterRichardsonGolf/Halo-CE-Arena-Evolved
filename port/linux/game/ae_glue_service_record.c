/* The service record's store (see the header). */
#include <stdio.h>
#include <string.h>

#include "../src/ae_private_files.h"
#include "ae_glue_service_record.h"

void platform_log(char const *format, ...);

#define FILE_NAME "service-record.txt"

static struct
{
	struct ae_service_record record;
	char text[AE_SR_TEXT];
	int read;
} store;

struct ae_service_record *ae_record_store(void)
{
	long length;

	if (store.read)
		return &store.record;
	store.read = 1;
	length = ae_private_file_read(FILE_NAME, store.text, sizeof(store.text));
	if (length < 0 && length != -2)
	{
		ae_sr_init(&store.record);
		platform_log("ae record: %s: none yet", FILE_NAME);
	}
	else if (length == -2 || !ae_sr_read(&store.record, store.text))
	{
		ae_sr_init(&store.record);
		if (ae_private_file_set_aside(FILE_NAME))
			platform_log("ae record: %s refused: kept as %s.bad, a fresh one started", FILE_NAME, FILE_NAME);
		else
			platform_log("ae record: %s refused (not kept aside), a fresh one started", FILE_NAME);
	}
	else
		platform_log("ae record: %s: %d players, %d rivals", FILE_NAME, store.record.player_count, store.record.rival_count);
	return &store.record;
}

int ae_record_store_save(void)
{
	long length;
	struct ae_service_record *record = ae_record_store();

	if (record->newer)
	{
		platform_log("ae record: the file is of a newer version: not written");
		return 0;
	}
	length = ae_sr_write(record, store.text, sizeof(store.text));
	if (length < 0 || !ae_private_file_write(FILE_NAME, store.text, length))
	{
		platform_log("ae record: %s could not be written", FILE_NAME);
		return 0;
	}
	platform_log("ae record: %s written (%d players, %d rivals)", FILE_NAME, record->player_count, record->rival_count);
	return 1;
}

int ae_record_store_game(struct ae_pgr_report const *report)
{
	static struct ae_sr_game game;
	struct ae_service_record *record = ae_record_store();

	if (!ae_pgr_to_record_game(report, &game) || !ae_sr_record_game(record, &game))
		return 0;
	platform_log("ae record: game added (%s, %d players)", ae_sr_mode_name(game.mode), game.count);
	ae_record_store_save();
	return 1;
}

int ae_record_store_rival(char const *owner, char const *name, int on)
{
	struct ae_service_record *record = ae_record_store();

	if (!ae_sr_rival_set(record, owner, name, on))
		return 0;
	platform_log("ae record: rival %s -> %s %s", owner, name, on ? "on" : "off");
	ae_record_store_save();
	return 1;
}

int ae_record_is_rival(void *context, char const *owner, char const *name)
{
	(void)context;
	return ae_sr_is_rival(ae_record_store(), owner, name);
}

int ae_record_any_owner_has_rival(char const *name)
{
	struct ae_service_record *record = ae_record_store();
	char key[AE_SR_KEY];
	int index;

	if (!record->rival_count)
		return 0;
	ae_sr_key(name, key, sizeof(key));
	for (index = 0; index < record->rival_count; index++)
	{
		if (!strcmp(record->rival[index].key, key))
			return 1;
	}
	return 0;
}
