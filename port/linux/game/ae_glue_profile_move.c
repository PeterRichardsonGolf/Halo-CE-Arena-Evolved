/* Moving a profile's progress between PCs: the files (see the header). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/ae_exports_files.h"
#include "../src/ae_private_files.h"
#include "ae_bot_record_rules.h"
#include "ae_career_record.h"
#include "ae_dev_cmd_rules.h"
#include "ae_glue_bot_records.h"
#include "ae_glue_career.h"
#include "ae_glue_profile_move.h"
#include "ae_glue_service_record.h"
#include "ae_service_record.h"

void platform_log(char const *format, ...);

#define CAREER_NAME "career.txt"
#define SERVICE_NAME "service-record.txt"
#define BOTS_NAME "bot_records.txt"
#define SUFFIX ".aeprofile"
#define BACKUP_SUFFIX ".aebackup"
#define MARKER_NAME "profile-move.pending"
#define KEEP_BACKUPS 5
#define RETRY_SECONDS 3

enum { NAME_SHOWN = 24, BACKUP_TEXT = AE_CAREER_TEXT + AE_SR_TEXT + AE_BOT_RECORD_TEXT + 4096 };

static char file_text[AE_PM_FILE_MAX + 1];
/* the three record files' bytes as one backup (larger than a profile file can be: it is not one), and a backup read back */
static char backup_text[BACKUP_TEXT + 1];
/* this PC's three files as read, and as they would be written */
static char career_now[AE_CAREER_TEXT + 1], service_now[AE_SR_TEXT + 1], bots_now[AE_BOT_RECORD_TEXT + 1];
static char career_new[AE_CAREER_TEXT + 1], service_new[AE_SR_TEXT + 1], bots_new[AE_BOT_RECORD_TEXT + 1];
static char *const current[3] = { career_now, service_now, bots_now };
static char *const merged[3] = { career_new, service_new, bots_new };
static int present[3];
static long raw_length[3];
/* the mark of a write under way: the backup's name and the hashes of the new texts */
static char marker_text[512];
/* the readers, to judge a text as the stores will */
static struct ae_career check_career;
static struct ae_service_record check_record;
static struct ae_bot_records check_bots;

static char const *const file_names[3] = { CAREER_NAME, SERVICE_NAME, BOTS_NAME };
static long const text_limit[3] = { AE_CAREER_TEXT, AE_SR_TEXT, AE_BOT_RECORD_TEXT };

static void say(char *reason, size_t size, char const *text)
{
	if (reason && size)
		snprintf(reason, size, "%s", text);
}

/* a profile name as a file name's part: letters and digits, "-" and "_", the rest "_" */
static void name_part(char const *name, char *out, size_t size)
{
	size_t at = 0;

	for (; *name && at + 1 < size && at < NAME_SHOWN; name++)
	{
		char c = *name;

		out[at++] = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ? c : '_';
	}
	out[at] = 0;
	if (!at)
		snprintf(out, size, "profile");
}

static void key_of(char const *name, char *key, size_t size)
{
	ae_sr_key(name, key, size);
}

/* ---------- reading this PC's files ---------- */

/* the three files' bytes as they are on disk (a file that is not there is an empty text, present[] 0); 0 with the reason when one
is larger than its store reads */
static int read_raw(char *reason, size_t reason_size)
{
	int which;

	for (which = 0; which < 3; which++)
	{
		long length = ae_private_file_read(file_names[which], current[which], text_limit[which] + 1);

		present[which] = length >= 0;
		raw_length[which] = length >= 0 ? length : -1;
		if (length == -2)
		{
			say(reason, reason_size, "A record file is too large to change.");
			return 0;
		}
		if (length < 0)
			current[which][0] = 0;
	}
	return 1;
}

/* the files read, and fit to be the base of a merge: the readers accept them, none is of a newer version, no tournament game is open */
static int read_current(char *reason, size_t reason_size)
{
	if (!read_raw(reason, reason_size))
		return 0;
	if (present[0] && (ae_career_read(&check_career, current[0]) != AE_CAREER_READ_OK || check_career.newer))
	{
		say(reason, reason_size, "career.txt cannot be changed here: it is damaged or from a newer AE.");
		return 0;
	}
	if (present[1] && (!ae_sr_read(&check_record, current[1]) || check_record.newer))
	{
		say(reason, reason_size, "service-record.txt cannot be changed here: it is damaged or from a newer AE.");
		return 0;
	}
	if (present[2] && (ae_bot_records_read(&check_bots, current[2]) != AE_BOT_RECORD_READ_OK || check_bots.newer))
	{
		say(reason, reason_size, "bot_records.txt cannot be changed here: it is damaged or from a newer AE.");
		return 0;
	}
	if (present[0] && check_career.pending_count > 0)
	{
		say(reason, reason_size, "A tournament game is still open. Finish or leave it first.");
		return 0;
	}
	return 1;
}

/* a write of the records that is pending: 0 with the reason (the caller checks a running game itself) */
static int idle(char *reason, size_t reason_size)
{
	/* (a write that a crash cut off is put back before anything is decided on the files: once a run, here or at a store's first read) */
	ae_profile_move_recover();
	if (ae_bot_records_waiting())
	{
		say(reason, reason_size, "A game's record is still being written. Try again in a moment.");
		return 0;
	}
	if (ae_private_file_exists(MARKER_NAME))
	{
		say(reason, reason_size, "An earlier import was cut off. Restart the game to finish putting the records back.");
		return 0;
	}
	return 1;
}

/* the file read and parsed: 1, else 0 with the reason */
static int read_file(char const *file_name, struct ae_pm_file *file, char *reason, size_t reason_size)
{
	long length = ae_exports_read(0, file_name, file_text, (long)sizeof(file_text));
	int answer;

	if (length == -1)
	{
		say(reason, reason_size, "That file is not in the exports folder any more.");
		return 0;
	}
	if (length == -2)
	{
		say(reason, reason_size, ae_pm_error_text(AE_PM_ERR_SIZE));
		return 0;
	}
	answer = ae_pm_parse(file_text, (size_t)length, (unsigned long)time(NULL), file);
	if (answer)
	{
		say(reason, reason_size, ae_pm_error_text(answer));
		return 0;
	}
	return 1;
}

/* the merge into merged[], every merged text read back by its reader: 1, else 0 with the reason. What a reader leaves out is not
counted as taken from the file */
static int merge_all(struct ae_pm_file const *file, char const *to_key, int mode, struct ae_pm_summary *summary, char *reason,
	size_t reason_size)
{
	size_t length[3];
	int answer;

	if (summary)
		memset(summary, 0, sizeof(*summary));
	answer = ae_pm_merge_career(mode, file, to_key, current[0], merged[0], sizeof(career_new), &length[0], summary);
	if (!answer)
		answer = ae_pm_merge_service(mode, file, to_key, current[1], merged[1], sizeof(service_new), &length[1], summary);
	if (!answer)
		answer = ae_pm_merge_bots(mode, file, to_key, current[2], merged[2], sizeof(bots_new), &length[2], summary);
	if (answer)
	{
		say(reason, reason_size, answer == AE_PM_ERR_ROOM ? "The result is too large for the record files." : ae_pm_error_text(answer));
		return 0;
	}
	if (length[0] >= (size_t)text_limit[0] || length[1] >= (size_t)text_limit[1] || length[2] >= (size_t)text_limit[2] ||
		ae_career_read(&check_career, merged[0]) != AE_CAREER_READ_OK || !ae_sr_read(&check_record, merged[1]) ||
		ae_bot_records_read(&check_bots, merged[2]) != AE_BOT_RECORD_READ_OK)
	{
		say(reason, reason_size, "The result would not read as a record. Nothing was changed.");
		return 0;
	}
	if (summary)
	{
		int expected = summary->bots_from_file + summary->bots_kept, lost = expected - check_bots.count;

		if (lost > 0)
		{
			int from = lost < summary->bots_from_file ? lost : summary->bots_from_file;

			summary->dropped += lost;
			summary->bots_from_file -= from;
			summary->bots_kept -= lost - from;
		}
	}
	return 1;
}

/* now as a stamp: UTC, YYYYMMDD-HHMMSS */
static void stamp(char *out, size_t size)
{
	time_t now = time(NULL);
	struct tm *utc = gmtime(&now);

	if (!utc || !strftime(out, size, "%Y%m%d-%H%M%S", utc))
		snprintf(out, size, "00000000-000000");
}

/* ---------- the list ---------- */

struct collect
{
	struct ae_pm_listing *list;
	int count, maximum;
};

static void collect_one(char const *name, void *context)
{
	struct collect *collect = context;
	struct ae_pm_listing listing;
	struct ae_pm_file file;
	long length;
	int at;

	memset(&listing, 0, sizeof(listing));
	snprintf(listing.name, sizeof(listing.name), "%s", name);
	length = ae_exports_read(0, name, file_text, (long)sizeof(file_text));
	if (length < 0)
		listing.error = length == -2 ? AE_PM_ERR_SIZE : AE_PM_ERR_READ;
	else
	{
		listing.error = ae_pm_parse(file_text, (size_t)length, (unsigned long)time(NULL), &file);
		snprintf(listing.profile, sizeof(listing.profile), "%s", file.profile);
		listing.exported = file.exported;
	}
	/* (newest first: placed by its export time) */
	if (collect->count == collect->maximum)
	{
		if (listing.exported <= collect->list[collect->count - 1].exported)
			return;
		at = collect->count - 1;
	}
	else
		at = collect->count++;
	while (at > 0 && collect->list[at - 1].exported < listing.exported)
	{
		collect->list[at] = collect->list[at - 1];
		at--;
	}
	collect->list[at] = listing;
}

int ae_profile_move_list(struct ae_pm_listing *list, int maximum)
{
	struct collect collect;

	collect.list = list;
	collect.count = 0;
	collect.maximum = maximum;
	if (maximum <= 0 || ae_exports_list(0, SUFFIX, collect_one, &collect) < 0)
		return -1;
	return collect.count;
}

/* ---------- export ---------- */

int ae_profile_move_export(char const *profile_name, char *file_name, size_t size, int counts[3], char *reason, size_t reason_size)
{
	static char out[AE_PM_FILE_MAX + 1];
	char key[AE_PM_KEY], part[NAME_SHOWN + 2], date[16], name[AE_PM_FILE_NAME];
	int answer, number;
	time_t now = time(NULL);
	struct tm *local = localtime(&now);

	if (!read_current(reason, reason_size))
	{
		platform_log("ae profile move: export refused: %s", reason ? reason : "");
		return 0;
	}
	key_of(profile_name, key, sizeof(key));
	answer = ae_pm_export(profile_name, key, current[0], current[1], current[2], (unsigned long)now, "AE", out, sizeof(out), counts);
	if (answer)
	{
		char const *why = answer == AE_PM_ERR_ROOM || answer == AE_PM_ERR_SIZE ?
			"This PC's records are too large for one profile file (512 KiB). Nothing was written." : ae_pm_error_text(answer);

		say(reason, reason_size, why);
		platform_log("ae profile move: export of %s refused: %s", profile_name, why);
		return 0;
	}
	name_part(profile_name, part, sizeof(part));
	if (!local || !strftime(date, sizeof(date), "%Y-%m-%d", local))
		snprintf(date, sizeof(date), "0000-00-00");
	for (number = 1; number < 100; number++)
	{
		if (number == 1)
			snprintf(name, sizeof(name), "%s-%s" SUFFIX, part, date);
		else
			snprintf(name, sizeof(name), "%s-%s-%d" SUFFIX, part, date, number);
		if (!ae_exports_exists(0, name))
			break;
	}
	if (number >= 100 || !ae_exports_write(0, name, out, (long)strlen(out)))
	{
		say(reason, reason_size, "The file could not be written to the exports folder.");
		platform_log("ae profile move: export of %s: the file could not be written", profile_name);
		return 0;
	}
	snprintf(file_name, size, "%s", name);
	platform_log("ae profile move: exported %s (%s)", profile_name, name);
	return 1;
}

/* ---------- the write: all three files or none ---------- */

static char marker_backup[AE_PM_FILE_NAME];
/* an import is writing the three files: nothing else reads them into the shared buffers meanwhile */
static int applying;

static int op_stage(void *context, int which, char const *text)
{
	(void)context;
	return !text || ae_private_file_stage(file_names[which], text, (long)strlen(text));
}

static int op_commit(void *context, int which, int remove)
{
	(void)context;
	if (remove)
		return ae_private_file_remove(file_names[which]) || !ae_private_file_exists(file_names[which]);
	return ae_private_file_commit(file_names[which]);
}

static void op_discard(void *context, int which)
{
	(void)context;
	ae_private_file_discard(file_names[which]);
}

static int op_put(void *context, int which, char const *text, long length)
{
	(void)context;
	if (length < 0)
		return ae_private_file_remove(file_names[which]) || !ae_private_file_exists(file_names[which]);
	return ae_private_file_write(file_names[which], text, length);
}

static int op_marker(void *context, int on)
{
	(void)context;
	if (on)
		return ae_private_file_write(MARKER_NAME, marker_text, (long)strlen(marker_text));
	return ae_private_file_remove(MARKER_NAME) || !ae_private_file_exists(MARKER_NAME);
}

static struct ae_pm_ops const ops = { op_stage, op_commit, op_discard, op_put, op_marker };

/* a backup's raw bytes, from the files as read now */
static void raw_now(struct ae_pm_raw raw[3])
{
	int which;

	for (which = 0; which < 3; which++)
	{
		raw[which].text = current[which];
		raw[which].length = raw_length[which];
	}
}

/* the apply's answer in words for the reason (and the log): 1 when it is done */
static int applied(int answer, char *reason, size_t reason_size)
{
	if (answer == AE_PM_APPLY_OK)
		return 1;
	if (answer == AE_PM_APPLY_MARKER_LEFT)
	{
		platform_log("ae profile move: the mark of the write could not be removed: the next start finds the new files in place and drops it");
		return 1;
	}
	if (answer == AE_PM_APPLY_BROKEN)
	{
		char text[240];

		snprintf(text, sizeof(text), "A record file could not be written back. The backup %s in exports/backup puts the records back "
			"when the game starts again.", marker_backup);
		say(reason, reason_size, text);
	}
	else
		say(reason, reason_size, "The record files could not be written. They are as they were.");
	return 0;
}

static void forget_stores(void)
{
	ae_career_store_forget();
	ae_record_store_forget();
	ae_bot_records_store_forget();
}

/* a mark that was left after a done write is taken away once the files on disk are the texts it names: tried at most every
RETRY_SECONDS while the stores ask, and never while an import is writing */
int ae_profile_move_pending(void)
{
	static unsigned long last;
	static char mark[512];
	struct ae_pm_raw now[3];
	char name[AE_PM_FILE_NAME];
	long length;

	if (!ae_private_file_exists(MARKER_NAME))
		return 0;
	if (applying || !ae_pm_retry_due(&last, (unsigned long)time(NULL), RETRY_SECONDS))
		return 1;
	length = ae_private_file_read(MARKER_NAME, mark, (long)sizeof(mark));
	if (length > 0 && read_raw(NULL, 0))
	{
		raw_now(now);
		if (ae_pm_marker_decide(mark, (size_t)length, now, name, sizeof(name)) == AE_PM_RECOVER_DONE && op_marker(NULL, 0))
		{
			platform_log("ae profile move: the mark of a done write is gone: the records are saved again");
			return 0;
		}
	}
	return 1;
}

/* a write that a crash cut off, decided from its mark: the files already the new texts (only the mark is left: it is dropped), or
the three files put back from the backup the mark names; once a run, before a store first reads */
void ae_profile_move_recover(void)
{
	static int done;
	static char mark[512];
	struct ae_pm_raw raw[3], now[3];
	char name[AE_PM_FILE_NAME];
	long length;
	int which, answer;

	if (done)
		return;
	done = 1;
	if (!ae_private_file_exists(MARKER_NAME))
		return;
	length = ae_private_file_read(MARKER_NAME, mark, (long)sizeof(mark));
	name[0] = 0;
	if (!read_raw(NULL, 0))
		return;
	raw_now(now);
	answer = length > 0 ? ae_pm_marker_decide(mark, (size_t)length, now, name, sizeof(name)) : AE_PM_RECOVER_BAD;
	if (answer == AE_PM_RECOVER_DONE)
		platform_log("ae profile move: a write that was done only had its mark left: dropped (backup %s)", name);
	else if (answer == AE_PM_RECOVER_ROLLBACK && (length = ae_exports_read(1, name, backup_text, (long)sizeof(backup_text))) > 0 &&
		ae_pm_backup_read(backup_text, (size_t)length, file_names, raw) == AE_PM_OK && ae_pm_rollback(&ops, NULL, raw))
		platform_log("ae profile move: an import that was cut off is undone: the record files are back from %s", name);
	else
	{
		platform_log("ae profile move: an import that was cut off could not be undone (backup %s): the record files are as they were left",
			name[0] ? name : "unknown");
		for (which = 0; which < 3; which++)
			ae_private_file_discard(file_names[which]);
		return;
	}
	for (which = 0; which < 3; which++)
		ae_private_file_discard(file_names[which]);
	ae_private_file_remove(MARKER_NAME);
}

/* ---------- import ---------- */

int ae_profile_move_preview(char const *file_name, char const *to_name, int mode, struct ae_pm_summary *summary, char *reason,
	size_t reason_size)
{
	struct ae_pm_file file;
	char key[AE_PM_KEY];

	if (!read_file(file_name, &file, reason, reason_size) || !read_current(reason, reason_size))
		return 0;
	key_of(to_name, key, sizeof(key));
	return merge_all(&file, key, mode, summary, reason, reason_size);
}

/* a profile's backups beyond the newest KEEP_BACKUPS taken away, of one suffix */
struct prune
{
	char names[64][AE_PM_FILE_NAME];
	char const *pointers[64];
	int count;
};

static void prune_one(char const *name, void *context)
{
	struct prune *prune = context;

	if (prune->count < 64)
	{
		snprintf(prune->names[prune->count], AE_PM_FILE_NAME, "%s", name);
		prune->pointers[prune->count] = prune->names[prune->count];
		prune->count++;
	}
}

static void prune_backups(char const *stem, char const *suffix, char const *protect)
{
	static struct prune prune;
	int victims[64], count, index;

	prune.count = 0;
	if (ae_exports_list(1, suffix, prune_one, &prune) < 0)
		return;
	count = ae_pm_prune_select(prune.pointers, prune.count, stem, suffix, KEEP_BACKUPS, protect, victims, 64);
	for (index = 0; index < count; index++)
		ae_exports_remove(1, prune.names[victims[index]]);
}

/* the raw backup of this PC's files written (and a portable .aeprofile of the profile's progress beside it when it has any and fits):
its name in backup; 1, else 0 with the reason */
static int write_backups(char const *to_name, char const *key, char *backup, size_t backup_size, char *reason, size_t reason_size)
{
	static char saved[AE_PM_FILE_MAX + 1];
	struct ae_pm_raw raw[3];
	char stem[64], now[24], base[AE_PM_FILE_NAME], name[AE_PM_FILE_NAME];
	size_t length;
	int answer, clash;

	raw_now(raw);
	answer = ae_pm_backup_write(raw, file_names, backup_text, sizeof(backup_text), &length);
	if (answer)
	{
		say(reason, reason_size, "This PC's records are too large to back up, so nothing was changed.");
		return 0;
	}
	ae_pm_backup_stem(key, stem, sizeof(stem));
	stamp(now, sizeof(now));
	for (clash = 1; clash < 100; clash++)
	{
		if (!ae_pm_backup_name(stem, now, clash, BACKUP_SUFFIX, name, sizeof(name)))
			break;
		if (!ae_exports_exists(1, name))
			break;
	}
	if (clash >= 100 || !ae_pm_backup_name(stem, now, clash, BACKUP_SUFFIX, name, sizeof(name)) ||
		!ae_exports_write(1, name, backup_text, (long)length))
	{
		say(reason, reason_size, "The backup could not be written, so nothing was changed.");
		return 0;
	}
	snprintf(backup, backup_size, "%s", name);
	/* (the portable copy: the same base name, .aeprofile; the profile may have nothing of its own, or too much for one file) */
	if (ae_pm_backup_name(stem, now, clash, SUFFIX, base, sizeof(base)) &&
		ae_pm_export(to_name, key, current[0], current[1], current[2], (unsigned long)time(NULL), "AE", saved, sizeof(saved), NULL) ==
		AE_PM_OK)
		ae_exports_write(1, base, saved, (long)strlen(saved));
	prune_backups(stem, BACKUP_SUFFIX, name);
	prune_backups(stem, SUFFIX, base);
	return 1;
}

static int import_file(char const *file_name, char const *to_name, int mode, struct ae_pm_summary *summary, char *backup,
	size_t backup_size, char *reason, size_t reason_size)
{
	struct ae_pm_file file;
	struct ae_pm_raw raw[3];
	char key[AE_PM_KEY];
	char const *texts[3];
	int answer;

	if (backup && backup_size)
		backup[0] = 0;
	if (!idle(reason, reason_size) || !read_file(file_name, &file, reason, reason_size) || !read_current(reason, reason_size))
		return 0;
	key_of(to_name, key, sizeof(key));
	if (!merge_all(&file, key, mode, summary, reason, reason_size))
		return 0;
	/* (no backup written, no import) */
	if (!write_backups(to_name, key, marker_backup, sizeof(marker_backup), reason, reason_size))
		return 0;
	if (backup && backup_size)
		snprintf(backup, backup_size, "%s", marker_backup);
	raw_now(raw);
	texts[0] = merged[0];
	texts[1] = merged[1];
	texts[2] = merged[2];
	if (ae_pm_marker_write(marker_backup, texts, marker_text, sizeof(marker_text)))
	{
		say(reason, reason_size, "The write could not be marked, so nothing was changed.");
		return 0;
	}
	applying = 1;
	answer = ae_pm_apply(&ops, NULL, texts, raw);
	applying = 0;
	if (!applied(answer, reason, reason_size))
		return 0;
	forget_stores();
	return 1;
}

int ae_profile_move_import(char const *file_name, char const *to_name, int mode, struct ae_pm_summary *summary, char *backup,
	size_t backup_size, char *reason, size_t reason_size)
{
	int done = import_file(file_name, to_name, mode, summary, backup, backup_size, reason, reason_size);

	if (done)
		platform_log("ae profile move: import %s into %s: %s, %d units from the file, %d kept, %d lines not used, backup %s", file_name,
			to_name, mode == AE_PM_REPLACE ? "replace" : "merge", summary ? summary->from_file : 0, summary ? summary->kept : 0,
			summary ? summary->dropped : 0, backup && backup[0] ? backup : "none");
	else
		platform_log("ae profile move: import %s refused: %s", file_name, reason ? reason : "");
	return done;
}

int ae_profile_move_undo(char const *backup_name, char const *to_name, char *reason, size_t reason_size)
{
	struct ae_pm_raw raw[3], now[3];
	char const *texts[3];
	char name[AE_PM_FILE_NAME];
	long length;
	int answer, which;

	if (!idle(reason, reason_size))
		return 0;
	length = ae_exports_read(1, backup_name, backup_text, (long)sizeof(backup_text));
	if (length < 0)
	{
		say(reason, reason_size, "The backup is not there any more.");
		return 0;
	}
	answer = ae_pm_backup_read(backup_text, (size_t)length, file_names, raw);
	if (answer)
	{
		say(reason, reason_size, ae_pm_error_text(answer));
		return 0;
	}
	if (!read_raw(reason, reason_size))
		return 0;
	raw_now(now);
	for (which = 0; which < 3; which++)
		texts[which] = raw[which].length >= 0 ? raw[which].text : NULL;
	/* (the backup's bytes are staged as texts that end at their length: copied to ended strings in merged[]) */
	for (which = 0; which < 3; which++)
	{
		if (raw[which].length < 0)
			continue;
		if (raw[which].length >= text_limit[which] + 1)
		{
			say(reason, reason_size, "The backup does not fit the record files.");
			return 0;
		}
		memcpy(merged[which], raw[which].text, (size_t)raw[which].length);
		merged[which][raw[which].length] = 0;
		texts[which] = merged[which];
	}
	snprintf(name, sizeof(name), "%s", backup_name);
	snprintf(marker_backup, sizeof(marker_backup), "%s", name);
	if (ae_pm_marker_write(marker_backup, texts, marker_text, sizeof(marker_text)))
	{
		say(reason, reason_size, "The write could not be marked, so nothing was changed.");
		return 0;
	}
	applying = 1;
	answer = ae_pm_apply(&ops, NULL, texts, now);
	applying = 0;
	if (!applied(answer, reason, reason_size))
		return 0;
	forget_stores();
	platform_log("ae profile move: undo: %s put back (%s)", backup_name, to_name);
	return 1;
}

/* ---------- words and the folder ---------- */

void ae_profile_move_summary_text(struct ae_pm_summary const *summary, int rows, char *text, size_t size)
{
	size_t at;
	int index, shown = 0;

	at = (size_t)snprintf(text, size, "%d from the file, %d kept as this PC has them.", summary->from_file, summary->kept);
	for (index = 0; index < summary->rows && shown < rows && at + 40 < size; index++)
	{
		struct ae_pm_row const *row = &summary->row[index];

		if (row->theirs < 0 && row->mine < 0)
			continue;
		at += (size_t)snprintf(text + at, size - at, " %s: %s (this PC %ld, file %ld).", row->name,
			row->from_file ? "the file's" : "this PC's", row->mine < 0 ? 0L : row->mine, row->theirs < 0 ? 0L : row->theirs);
		shown++;
	}
	if (summary->rows > shown && rows > 0 && at + 24 < size)
		at += (size_t)snprintf(text + at, size - at, " And %d more.", summary->rows - shown);
	if (summary->dropped > 0 && at + 60 < size)
		snprintf(text + at, size - at, " %d line%s of the file cannot be used.", summary->dropped, summary->dropped == 1 ? "" : "s");
}

int ae_profile_move_open_folder(char *path, size_t size)
{
	char folder[1024], url[4200];

	if (!ae_exports_folder(0, folder, sizeof(folder)))
		return 0;
	if (path && size)
		snprintf(path, size, "%s", folder);
	return ae_pm_file_url(folder, url, sizeof(url)) && ae_exports_open_url(url);
}

/* ---------- DEV MODE ---------- */

struct resolve
{
	char const *word;
	char found[AE_PM_FILE_NAME];
};

static int same_lower(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
	{
		char x = *a >= 'A' && *a <= 'Z' ? (char)(*a + 32) : *a, y = *b >= 'A' && *b <= 'Z' ? (char)(*b + 32) : *b;

		if (x != y)
			return 0;
	}
	return !*a && !*b;
}

static void resolve_one(char const *name, void *context)
{
	struct resolve *resolve = context;

	if (!resolve->found[0] && same_lower(name, resolve->word))
		snprintf(resolve->found, sizeof(resolve->found), "%s", name);
}

/* a file name typed in DEV MODE (its words are lower case) as the folder spells it */
static int resolve_file(int backup, char const *word, char *out, size_t size)
{
	struct resolve resolve;

	resolve.word = word;
	resolve.found[0] = 0;
	if (ae_exports_list(backup, SUFFIX, resolve_one, &resolve) < 0 || !resolve.found[0])
		return 0;
	snprintf(out, size, "%s", resolve.found);
	return 1;
}

/* a made-up profile's progress as a file in the exports folder (never in the record files): a staged import's source */
static int make_sample(char const *name, long games, char *file_name, size_t size)
{
	static char out[AE_PM_FILE_MAX + 1], career[2048], service[2048];
	char key[AE_PM_KEY], part[NAME_SHOWN + 2];
	unsigned long now = (unsigned long)time(NULL);
	size_t at;
	int index;

	key_of(name, key, sizeof(key));
	snprintf(career, sizeof(career), "match = %s | team-slayer | 1500 %ld %lu\nsocial = %s | team-slayer | %ld %ld %ld 1 0 %lu\n"
		"social = %s | ctf | 40 %ld 1 0 0 %lu\nexperience = %s | 2\n", key, games, now, key, games * 10, games, games / 2, now, key,
		games / 2, now, key);
	at = (size_t)snprintf(service, sizeof(service), "player = %s | %s | %ld | %ld %ld 400 300 80 |", key, name, games, games, games / 2);
	for (index = 0; index < 35; index++)
		at += (size_t)snprintf(service + at, sizeof(service) - at, " 0");
	snprintf(service + at, sizeof(service) - at, " | 5:2\nmap = %s | sample map | %ld %ld 40 30\n", key, games, games / 2);
	if (ae_pm_export(name, key, career, service, "", now, "sample", out, sizeof(out), NULL))
		return 0;
	name_part(name, part, sizeof(part));
	snprintf(file_name, size, "%s-sample" SUFFIX, part);
	return ae_exports_write(0, file_name, out, (long)strlen(out));
}

int ae_profile_move_dev(struct ae_dev_cmd const *cmd, char *text, int size)
{
	static struct ae_pm_listing list[AE_PM_LIST_MAX];
	static char last_backup[AE_PM_FILE_NAME];
	char reason[160], name[AE_PM_FILE_NAME], backup[AE_PM_FILE_NAME];
	struct ae_pm_summary summary;
	int counts[3];

	if (cmd->words == 0 || !strcmp(cmd->word[0], "list"))
	{
		int count = ae_profile_move_list(list, AE_PM_LIST_MAX), index;
		size_t at = 0;

		if (count < 0)
		{
			snprintf(text, (size_t)size, "profile_move: the exports folder cannot be read");
			return 0;
		}
		at += (size_t)snprintf(text, (size_t)size, "%d file%s:", count, count == 1 ? "" : "s");
		for (index = 0; index < count && at + 60 < (size_t)size; index++)
			at += (size_t)snprintf(text + at, (size_t)size - at, " %s%s", list[index].name, list[index].error ? " (refused)" : "");
		return 1;
	}
	if (!strcmp(cmd->word[0], "sample"))
	{
		char const *who = cmd->words >= 2 ? cmd->word[1] : "devmove";
		long games = cmd->words >= 3 ? cmd->number : 30;

		if (!make_sample(who, games, name, sizeof(name)))
		{
			snprintf(text, (size_t)size, "profile_move: the sample could not be written");
			return 0;
		}
		snprintf(text, (size_t)size, "a made-up %s with %ld games is in exports/%s (the record files are not touched)", who, games, name);
		return 1;
	}
	if (!strcmp(cmd->word[0], "export") && cmd->words >= 2)
	{
		if (!ae_profile_move_export(cmd->word[1], name, sizeof(name), counts, reason, sizeof(reason)))
		{
			snprintf(text, (size_t)size, "profile_move: %s", reason);
			return 0;
		}
		snprintf(text, (size_t)size, "exported %s to exports/%s (%d career, %d service, %d bot lines)", cmd->word[1], name, counts[0],
			counts[1], counts[2]);
		return 1;
	}
	if ((!strcmp(cmd->word[0], "import") || !strcmp(cmd->word[0], "preview")) && cmd->words >= 3)
	{
		int mode = cmd->words >= 4 && !strcmp(cmd->word[3], "replace") ? AE_PM_REPLACE : AE_PM_MERGE;
		int preview = !strcmp(cmd->word[0], "preview");
		char words[320], file[AE_PM_FILE_NAME];

		if (!resolve_file(0, cmd->word[1], file, sizeof(file)))
		{
			snprintf(text, (size_t)size, "profile_move: no file %s in the exports folder", cmd->word[1]);
			return 0;
		}
		backup[0] = 0;
		if (!(preview ? ae_profile_move_preview(file, cmd->word[2], mode, &summary, reason, sizeof(reason)) :
			ae_profile_move_import(file, cmd->word[2], mode, &summary, backup, sizeof(backup), reason, sizeof(reason))))
		{
			snprintf(text, (size_t)size, "profile_move: %s", reason);
			return 0;
		}
		if (!preview)
			snprintf(last_backup, sizeof(last_backup), "%s", backup);
		ae_profile_move_summary_text(&summary, 3, words, sizeof(words));
		snprintf(text, (size_t)size, "%s into %s (%s): %s%s%s", preview ? "preview" : "imported", cmd->word[2],
			mode == AE_PM_REPLACE ? "replace" : "merge", words, !preview && backup[0] ? " Backup " : "", !preview ? backup : "");
		return 1;
	}
	if (!strcmp(cmd->word[0], "undo") && cmd->words >= 2)
	{
		if (!last_backup[0] || !ae_profile_move_undo(last_backup, cmd->word[1], reason, sizeof(reason)))
		{
			snprintf(text, (size_t)size, "profile_move: %s", last_backup[0] ? reason : "no import to undo in this run");
			return 0;
		}
		snprintf(text, (size_t)size, "put back %s into %s", last_backup, cmd->word[1]);
		return 1;
	}
	snprintf(text, (size_t)size, "profile_move: list | sample [<name> [<games>]] | export <name> | preview|import <file> <name> [merge|replace] | undo <name>");
	return 0;
}
