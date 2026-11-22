/* The community map archive's library, glued to the game (see the header). */
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_ce_archive.h"
#include "ae_dev_cmd_rules.h"
#include "ae_glue_ce_archive.h"
#include "ae_map_share.h"
#include "halo_map_families.h"

/* a map installed: marked as downloaded (as map sharing's and the repositories' are) and the map lists told */
static void installed(char const *file)
{
	char map_name[64];
	size_t stem = strlen(file) >= 4 ? strlen(file) - 4 : 0;

	snprintf(map_name, sizeof(map_name), "%.*s@ce", (int)stem, file);
	map_downloaded_remember(map_name);
	custom_edition_maps_look_again();
}

/* whether a Custom Edition map is open in the game: a map there is not replaced then */
static int map_loaded(void)
{
	return custom_edition_cache_loaded_path() != NULL;
}

void ae_glue_ce_archive_poll(void)
{
	ae_ce_archive_set_installed_hook(installed);
	ae_ce_archive_set_map_loaded_proc(map_loaded);
	ae_ce_archive_poll();
}

static int same(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
	{
		if ((*a >= 'A' && *a <= 'Z' ? *a + 32 : *a) != *b)
			return 0;
	}
	return !*a && !*b;
}

static void size_text(unsigned long long bytes, char *text, int size)
{
	if (bytes >= 1024ULL * 1024)
		snprintf(text, (size_t)size, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
	else
		snprintf(text, (size_t)size, "%llu KB", (bytes + 1023) / 1024);
}

int ae_ce_archive_dev(struct ae_dev_cmd const *cmd, char *text, int size)
{
	char why[160], note[160];
	char const *word = cmd->words ? cmd->word[0] : "status";
	int env;

	if (!ae_ce_archive_available())
	{
		snprintf(text, (size_t)size, "archive: not in this build");
		return 0;
	}
	/* (DEV MODE only: a test archive's address and key from the environment) */
	env = ae_ce_archive_use_dev_environment(note, (int)sizeof(note));
	if (env < 0)
	{
		snprintf(text, (size_t)size, "archive: %s", note);
		return 0;
	}
	ae_glue_ce_archive_poll();
	if (same(word, "cancel"))
	{
		ae_ce_archive_cancel();
		snprintf(text, (size_t)size, "archive: cancel asked");
		return 1;
	}
	if (same(word, "get"))
	{
		struct ae_ce_archive_row row;

		if (!ae_ce_archive_row((int)cmd->number - 1, &row))
		{
			snprintf(text, (size_t)size, "archive: no row %ld (archive list first)", cmd->number);
			return 0;
		}
		if (!ae_ce_archive_download(row.file, 0, why, (int)sizeof(why)))
		{
			snprintf(text, (size_t)size, "archive: %s", why);
			return 0;
		}
		size_text(row.size, note, (int)sizeof(note));
		snprintf(text, (size_t)size, "archive: getting %s (%s)", row.file, note);
		return 1;
	}
	if (same(word, "list"))
	{
		struct ae_ce_archive_status status;
		struct ae_ce_archive_query query;
		struct ae_ce_archive_row row;
		int rows, place, used;

		if (!ae_ce_archive_start(why, (int)sizeof(why)))
		{
			snprintf(text, (size_t)size, "archive: %s", why);
			return 0;
		}
		ae_ce_archive_get_status(&status);
		if (status.state != AE_CE_ARCHIVE_READY)
		{
			snprintf(text, (size_t)size, "archive: %s", status.state == AE_CE_ARCHIVE_FAILED ? status.error :
				"the list is loading (archive status, then archive list again)");
			return status.state != AE_CE_ARCHIVE_FAILED;
		}
		memset(&query, 0, sizeof(query));
		if (cmd->words > 1)
			snprintf(query.text, sizeof(query.text), "%s", cmd->word[1]);
		rows = ae_ce_archive_query(&query);
		used = snprintf(text, (size_t)size, "archive: %d of %d maps%s%s:", rows, status.count,
			query.text[0] ? " for " : "", query.text);
		for (place = 0; place < 5 && used < size && ae_ce_archive_row(place, &row); place++)
			used += snprintf(text + used, (size_t)(size - used), " %d %s%s", place + 1, row.file, row.installed ? "*" : "");
		return 1;
	}
	/* status */
	{
		struct ae_ce_archive_status status;
		struct ae_ce_archive_download download;
		char done[32], total[32];

		ae_ce_archive_get_status(&status);
		ae_ce_archive_download_status(&download);
		size_text(download.done, done, (int)sizeof(done));
		size_text(download.total, total, (int)sizeof(total));
		snprintf(text, (size_t)size, "archive: %s, %d maps, serial %llu%s%s; download %s %s %s of %s%s%s%s",
			!ae_ce_archive_enabled() ? "off (game.ce_archive)" : !ae_ce_archive_consented() ? "no consent" :
			status.state == AE_CE_ARCHIVE_READY ? (status.from_network ? "ready (network)" : "ready (kept list)") :
			status.state == AE_CE_ARCHIVE_LOADING ? "loading" : status.state == AE_CE_ARCHIVE_FAILED ? "failed" : "idle",
			status.count, status.serial, status.error[0] ? ", " : "", status.error,
			download.state == AE_CE_ARCHIVE_DOWNLOAD_RUNNING ? "running" :
			download.state == AE_CE_ARCHIVE_DOWNLOAD_DONE ? "done" :
			download.state == AE_CE_ARCHIVE_DOWNLOAD_FAILED ? "failed" :
			download.state == AE_CE_ARCHIVE_DOWNLOAD_CANCELLED ? "cancelled" : "none",
			download.file, done, total, download.error[0] ? " (" : "", download.error, download.error[0] ? ")" : "");
		return 1;
	}
}
