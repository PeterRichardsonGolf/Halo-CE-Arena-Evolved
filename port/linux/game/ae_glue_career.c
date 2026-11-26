/* The career record's store (see the header). */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../src/ae_private_files.h"
#include "ae_glue_career.h"
#include "ae_glue_profile_move.h"

void platform_log(char const *format, ...);

#define FILE_NAME "career.txt"
#define CRASH_MARK_NAME "career.crashed"

static struct
{
	struct ae_career career;
	char text[AE_CAREER_TEXT];
	int read;
} store;

struct ae_career *ae_career_store(void)
{
	long length;
	int result, crashed, forgive;

	if (store.read)
		return &store.career;
	store.read = 1;
	/* (a record write of a profile import that a crash cut off is put back before the first read) */
	ae_profile_move_recover();
	/* (the mark of a crash that ended a run with a game pending: read once, taken away after its settlement is saved) */
	crashed = ae_private_file_exists(CRASH_MARK_NAME);
	length = ae_private_file_read(FILE_NAME, store.text, sizeof(store.text));
	if (length < 0 && length != -2)
	{
		ae_career_init(&store.career);
		if (crashed)
			ae_private_file_remove(CRASH_MARK_NAME);
		platform_log("ae career: %s: none yet", FILE_NAME);
		return &store.career;
	}
	result = length == -2 ? AE_CAREER_READ_BAD : ae_career_read(&store.career, store.text);
	if (result != AE_CAREER_READ_OK)
	{
		ae_career_init(&store.career);
		if (crashed)
			ae_private_file_remove(CRASH_MARK_NAME);
		if (ae_private_file_set_aside(FILE_NAME))
			platform_log("ae career: %s refused: kept as %s.bad, a fresh one started", FILE_NAME, FILE_NAME);
		else
			platform_log("ae career: %s refused (not kept aside), a fresh one started", FILE_NAME);
	}
	else
	{
		platform_log("ae career: %s: %d career, %d online, %d social, %d match entries", FILE_NAME, store.career.count[0],
			store.career.count[1], store.career.social_count, store.career.match_count);
		/* (a tournament game that was started and never ended, found at the first read of a run: a real crash forgives it;
		Android and web cannot mark crashes, so they forgive every such game as before this rule. This platform-wide decision avoids
		changing the pending file format) */
		forgive = crashed || !ae_private_crash_mark_supported();
		if (store.career.pending_count > 0)
		{
			struct ae_career before = store.career;
			int index, forgiven = 0, quits = 0;

			for (index = 0; index < store.career.pending_count; index++)
				if (crashed)
					platform_log("ae social: a game ended by a crash: not counted (%s, %s)", store.career.pending[index].profile,
						store.career.pending[index].playlist);
				else if (forgive)
					platform_log("ae social: crashes cannot be marked on this platform: the unended game is not counted (%s, %s)",
						store.career.pending[index].profile, store.career.pending[index].playlist);
				else
					platform_log("ae social: a game was not ended (the game was closed): counted as a quit (%s, %s)",
						store.career.pending[index].profile, store.career.pending[index].playlist);
			ae_career_pending_settle(&store.career, forgive, (unsigned long)time(NULL), &forgiven, &quits);
			if (ae_career_store_save())
			{
				if (crashed)
					ae_private_file_remove(CRASH_MARK_NAME);
			}
			else
			{
				store.career = before;
				platform_log("ae social: the settled games could not be saved");
			}
		}
		else if (crashed)
			ae_private_file_remove(CRASH_MARK_NAME);
	}
	return &store.career;
}

void ae_career_store_forget(void)
{
	store.read = 0;
}

int ae_career_store_save(void)
{
	long length;
	struct ae_career *career = ae_career_store();
	/* (a profile import under way or cut off: the record files are not written over meanwhile) */
	if (ae_profile_move_pending())
	{
		platform_log("ae records: a profile import is not finished: not written");
		return 0;
	}

	if (career->newer)
	{
		platform_log("ae career: the file is of a newer version: not written");
		return 0;
	}
	length = ae_career_write(career, store.text, sizeof(store.text));
	if (length < 0 || !ae_private_file_write(FILE_NAME, store.text, length))
	{
		platform_log("ae career: %s could not be written", FILE_NAME);
		return 0;
	}
	return 1;
}

void ae_career_crash_mark_write(int on)
{
	if (on)
		ae_private_file_write(CRASH_MARK_NAME, "crashed\n", 8);
	else
		ae_private_file_remove(CRASH_MARK_NAME);
}

int ae_career_crash_mark_arm(void)
{
	return ae_private_crash_mark_arm(CRASH_MARK_NAME);
}

void ae_career_crash_mark_disarm(void)
{
	ae_private_crash_mark_disarm();
}
