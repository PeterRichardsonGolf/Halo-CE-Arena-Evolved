/*
AE_PROFILE_MOVE_RULES.H

Moving a profile's progress between PCs (PROFILES > a profile > EXPORT PROGRESS / IMPORT PROGRESS). Pure code (no engine
headers, no files, no allocation; callers own every buffer): port/linux/tests/ae_profile_move_rules_test.c runs it;
ae_glue_profile_move.c reads and writes the files and ae_screen_profiles.c asks.

The progress is the text of three record files (career.txt, service-record.txt, bot_records.txt), keyed by the profile's
key (ae_sr_key of its name). This code works on those texts line by line: a line it does not know is skipped, and the
record readers stay the judges of the result (the glue reads every merged text back through them before anything is
written). The player key (player.key) is not part of any of it.

THE FILE (.aeprofile, plain text, at most AE_PM_FILE_MAX bytes, lines at most AE_PM_LINE_MAX, at most AE_PM_LINES_MAX lines):

	# AE profile
	version = 1
	format = aeprofile
	profile = Goat            the name as typed
	key = goat                the profile's key: every keyed line of every section starts with it
	exported = 1795283200     seconds since 1970 (shown, not trusted)
	build = 1.0.0-rc5         shown only
	[career]                  career.txt's ladder lines (career, online: this PC's), and the profile's match, social
	                          and experience lines
	[service]                 service-record.txt's clock line, and every line whose key is the profile's
	[bots]                    bot_records.txt's clock line, every bot's lines, and the profile's versus and lastgame lines
	[end]
	check = <64 hex>          SHA-256 of every byte above this line (the [end] line included)

The check finds a damaged or hand-edited file; it does not stop someone who writes a new one. Progress here is this PC's view.

THE MERGE keeps, for every unit present on both sides, the side with the MORE GAMES (a tie: the later clock; again: this PC), whole
(a unit is never added to the other side's: the same games would count twice). A unit on one side only is kept. REPLACE takes
the file's units over this PC's (this PC's units of the same key that the file does not have are dropped; ladders and bots,
which belong to the PC rather than the profile, keep their entries the file does not have). A line of another profile is never
touched. The units: a ladder entry (career or online, a playlist), the profile's match and social line of a playlist and its
experience line, its player lines in the service record (player, playerx, map and lastgame as one group, by the player line's
games), its rivals (at most AE_PM_RIVALS, this PC's first) with their versus lines, a bot (its lines as one group, by the bot
line's games), and the profile's versus and lastgame against a bot (by the games together). The profile's pending line is
never moved. Into another profile (to_key differs from the file's key) the first field of the file's keyed lines is
rewritten; the names of other players in them are not.
*/

#ifndef __AE_PROFILE_MOVE_RULES_H
#define __AE_PROFILE_MOVE_RULES_H

#include <stddef.h>

enum
{
	AE_PM_FILE_MAX = 512 * 1024,
	AE_PM_LINE_MAX = 4096,
	AE_PM_LINES_MAX = 12000,
	AE_PM_KEY = 48,
	AE_PM_NAME = 48,
	AE_PM_RIVALS = 8,
	AE_PM_GAMES_MAX = 1000000,
	/* the clock a record line may carry past now: a day */
	AE_PM_CLOCK_SLACK = 86400,
	/* the most a record clock of a file may be (a long is 32 bits on Windows) */
	AE_PM_CLOCK_MAX = 1000000000,
	AE_PM_ROWS = 24
};

/* the modes */
enum { AE_PM_MERGE, AE_PM_REPLACE };

/* the answers (ae_pm_error_text says each in words) */
enum
{
	AE_PM_OK = 0,
	AE_PM_ERR_EMPTY,       /* nothing of the profile to export */
	AE_PM_ERR_SIZE,        /* the file, a line or the lines are past the caps */
	AE_PM_ERR_FORMAT,      /* no header, no [end], a section missing or twice, a line that is not "key = value" */
	AE_PM_ERR_NEWER,       /* a version this build does not know */
	AE_PM_ERR_CHECK,       /* the check does not match */
	AE_PM_ERR_VALUE,       /* a number or name that cannot be real, a keyed line of another profile, a unit twice */
	AE_PM_ERR_ROOM,        /* the output buffer is too small */
	AE_PM_ERR_KEY,         /* a profile or key that is not usable */
	AE_PM_ERR_READ,        /* the file could not be read */
	AE_PM_ERR_BACKUP       /* a backup of the record files that is not one, or does not match */
};

/* a parsed file: its fields and where its sections lie in the text it was parsed from */
struct ae_pm_file
{
	char profile[AE_PM_NAME];
	char key[AE_PM_KEY];
	char build[24];
	unsigned long exported;
	char const *section[3];     /* career, service, bots: the text of each (the lines between its header and the next) */
	size_t length[3];
	int lines;
};

/* what a merge decided: units taken from the file, units kept as this PC has them, and the rows the dialog shows */
struct ae_pm_row
{
	char name[40];              /* "TEAM-SLAYER (RANKED)", "SERVICE RECORD", "BOTS" */
	long mine, theirs;          /* games on this PC, games in the file (-1: none) */
	int from_file;              /* 1: the file's is kept */
};
struct ae_pm_summary
{
	int from_file, kept;
	int rows, rows_dropped;
	struct ae_pm_row row[AE_PM_ROWS];
	int bots_from_file, bots_kept, rivals;
	/* lines of the file that are not in the result: a rival past the cap, a versus line of a rival or bot that is not there */
	int dropped;
};

/* the text of an error */
char const *ae_pm_error_text(int error);

/* a profile's progress into a file's text: out, at most size bytes (ended). Answers AE_PM_OK, AE_PM_ERR_EMPTY when no
line of the profile is in any of the three texts, AE_PM_ERR_ROOM or AE_PM_ERR_KEY. counts, when given, says how many lines
went into each section */
int ae_pm_export(char const *name, char const *key, char const *career, char const *service, char const *bots,
	unsigned long now, char const *build, char *out, size_t size, int counts[3]);

/* a file's text read: the caps, the header, the check, every line's key and numbers (against now). The sections point into text */
int ae_pm_parse(char const *text, size_t length, unsigned long now, struct ae_pm_file *file);

/* the three record files after the file's progress is taken in: target is this PC's text of the file (NULL or empty: none, as a
file with its header only), out its new text (the header, the lines in their kinds' order, the checksum line where the file has
one), at most size bytes. Answers AE_PM_OK or an error; the length in *length. summary (may be NULL) is added to */
int ae_pm_merge_career(int mode, struct ae_pm_file const *file, char const *to_key, char const *target, char *out, size_t size,
	size_t *length, struct ae_pm_summary *summary);
int ae_pm_merge_service(int mode, struct ae_pm_file const *file, char const *to_key, char const *target, char *out, size_t size,
	size_t *length, struct ae_pm_summary *summary);
int ae_pm_merge_bots(int mode, struct ae_pm_file const *file, char const *to_key, char const *target, char *out, size_t size,
	size_t *length, struct ae_pm_summary *summary);

/* the FNV-1a of text (the record files' checksum) */
unsigned long ae_pm_fnv(char const *text, size_t length);

/* a folder's path as a file: URL: "/" and ":" and the letters, digits and "-._~" as they are, a backslash a "/", every other byte
(a space, "%", "#", "?", a byte of UTF-8) as %XX, a drive path with a third slash after "file://". 0 when it does not fit */
int ae_pm_file_url(char const *path, char *out, size_t size);

/* ---------- the backup of the three record files, and the all-or-nothing write ---------- */

/* a record file's bytes (length -1: the file was not there) */
struct ae_pm_raw
{
	char const *text;
	long length;
};

/* the three record files' bytes as one text (names: the files' names, in the order career, service, bots): header, each file's
bytes, a SHA-256 check. out at most size bytes; AE_PM_OK, AE_PM_ERR_ROOM */
int ae_pm_backup_write(struct ae_pm_raw const raw[3], char const *const names[3], char *out, size_t size, size_t *length);
/* the text read back: the check, the names, the lengths; the bytes point into text. AE_PM_OK, AE_PM_ERR_BACKUP */
int ae_pm_backup_read(char const *text, size_t length, char const *const names[3], struct ae_pm_raw raw[3]);

/* a backup's file name: <the key as a file name's part>-<8 hex of the key's FNV>-<YYYYMMDD-HHMMSS>[-<n>]<suffix>; the stem is
what a profile's backups all start with (the key's part, the hash and the dash) */
void ae_pm_backup_stem(char const *key, char *stem, size_t size);
/* the name for a stamp ("20261121-193012", UTC); clash 2 and up add "-<clash>"; 0 when it does not fit */
int ae_pm_backup_name(char const *stem, char const *stamp, int clash, char const *suffix, char *out, size_t size);
/* whether a name is this stem's (the key's exactly: another key that starts the same way is not) and suffix's */
int ae_pm_backup_matches(char const *name, char const *stem, char const *suffix);
/* which of the names are this stem's backups past the newest keep: their indices into victims (at most max); the count. Newest
means the later stamp, then the larger clash number (as a number: -10 is after -9). protect (a name, or NULL) is never one of them:
the backup that was just written stays whatever its stamp says */
int ae_pm_prune_select(char const *const *names, int count, char const *stem, char const *suffix, int keep, char const *protect,
	int *victims, int max);

/* the files' operations of a write, so that a test can fail any of them: stage writes the new text beside a file (text NULL: the
file is to go), commit puts the staged text in the file's place (or takes the file away), discard drops what was staged, put
writes a file at once (length -1: takes it away), marker sets or clears the mark that says a write is under way. Each answers 1
when done */
struct ae_pm_ops
{
	int (*stage)(void *context, int which, char const *text);
	int (*commit)(void *context, int which, int remove);
	void (*discard)(void *context, int which);
	int (*put)(void *context, int which, char const *text, long length);
	int (*marker)(void *context, int on);
};
enum
{
	AE_PM_APPLY_OK,
	AE_PM_APPLY_UNCHANGED,       /* nothing was changed (a stage or the marker failed) */
	AE_PM_APPLY_ROLLED_BACK,     /* a commit failed and the files already changed were put back */
	AE_PM_APPLY_BROKEN,          /* a commit failed and a file could not be put back: the marker stays, the backup holds the bytes */
	AE_PM_APPLY_MARKER_LEFT      /* done, and the marker could not be taken away (the next start finds the files are the new ones) */
};
/* the new texts (NULL: that file is taken away) written all or none: every one staged first, the marker set, then each committed;
a commit that fails puts the earlier files back from raw */
int ae_pm_apply(struct ae_pm_ops const *ops, void *context, char const *const texts[3], struct ae_pm_raw const raw[3]);
/* the mark of a write under way: the backup's name and the SHA-256 of each new text ("absent" for a file the write takes away).
The next start decides from it: the files on disk are the new texts (the write was done, only its mark is left: drop the mark), or
they are not (put the backup back) */
enum { AE_PM_RECOVER_DONE, AE_PM_RECOVER_ROLLBACK, AE_PM_RECOVER_BAD };
int ae_pm_marker_write(char const *backup, char const *const texts[3], char *out, size_t size);
/* the answer, and the backup's name into backup; now: the three files as they are on disk */
int ae_pm_marker_decide(char const *marker, size_t length, struct ae_pm_raw const now[3], char *backup, size_t size);

/* whether a retry is due: the first time always, then once every `every` seconds of now (a clock that went back is due); a due retry
is noted in *last */
int ae_pm_retry_due(unsigned long *last, unsigned long now, unsigned long every);

/* the three files put back from a backup (the recovery of a write that was cut off): 1 when every one is */
int ae_pm_rollback(struct ae_pm_ops const *ops, void *context, struct ae_pm_raw const raw[3]);

#endif
