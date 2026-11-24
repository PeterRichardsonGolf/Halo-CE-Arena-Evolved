/*
AE_GLUE_PROFILE_MOVE.H

The files of PROFILES > a profile > EXPORT PROGRESS / IMPORT PROGRESS (ae_profile_move_rules.h has the rules and the file's
format). An export writes <save root>/exports/<NAME>-<date>.aeprofile; an import reads one of those files, takes a backup of the
profile's progress as it is (<save root>/exports/backup/<NAME>-before-import-<stamp>.aeprofile, the five newest kept), merges
or replaces, reads every merged text back through the record readers, writes the three record files all or none (each staged
beside its file first, a mark kept while they are put in place, a failure puts the earlier ones back), and forgets the stores so
the screens read the new files. Before a write it keeps the three files' bytes in exports/backup/<key>-<hash>-<UTC stamp>.aebackup
(UNDO writes those bytes back) and, when the profile has progress, the same progress as a .aeprofile beside it; the five newest of
each are kept. A write cut off by a crash is put back from its backup when the game starts (ae_profile_move_recover). Menus'
thread only. The player key is not touched.

Log: "ae profile move: exported <name> (<file>)" / "ae profile move: import <file> into <name>: merge|replace, N units from the
file, M kept, backup <file>" / "ae profile move: <what> refused: <why>".
*/

#ifndef __AE_GLUE_PROFILE_MOVE_H
#define __AE_GLUE_PROFILE_MOVE_H

#include <stddef.h>

#include "ae_profile_move_rules.h"

enum { AE_PM_LIST_MAX = 16, AE_PM_FILE_NAME = 96 };

/* a file of the exports folder as the import's list shows it */
struct ae_pm_listing
{
	char name[AE_PM_FILE_NAME];
	char profile[AE_PM_NAME];
	unsigned long exported;
	int error;                  /* AE_PM_OK, or why the file is not usable (ae_pm_error_text) */
};

/* the exports folder's files, the newest first: the count (at most maximum); -1 when the folder cannot be read */
int ae_profile_move_list(struct ae_pm_listing *list, int maximum);

/* a profile's progress written to the exports folder: 1, with the file's name (and how many lines of the three files); else 0
with the reason */
int ae_profile_move_export(char const *profile_name, char *file_name, size_t size, int counts[3], char *reason, size_t reason_size);

/* what an import of the file into the profile would do (nothing written): 1, else 0 with the reason */
int ae_profile_move_preview(char const *file_name, char const *to_name, int mode, struct ae_pm_summary *summary, char *reason,
	size_t reason_size);

/* the import: 1, else 0 with the reason (nothing changed). backup gets the backup's file name ("" when the profile had no
progress to keep) */
int ae_profile_move_import(char const *file_name, char const *to_name, int mode, struct ae_pm_summary *summary, char *backup,
	size_t backup_size, char *reason, size_t reason_size);

/* the three record files put back to the bytes a backup (the .aebackup an import named) holds: 1, else 0 with the reason */
int ae_profile_move_undo(char const *backup_name, char const *to_name, char *reason, size_t reason_size);

/* the summary in words for a dialog: the totals, then up to rows playlist rows, and the lines of the file that cannot be used */
void ae_profile_move_summary_text(struct ae_pm_summary const *summary, int rows, char *text, size_t size);

/* a write of the record files that a crash cut off, put back from its backup: asked before a store first reads (once a run) */
void ae_profile_move_recover(void);
/* a write of the record files is under way or was cut off and not yet put back: the stores do not save meanwhile (1 then) */
int ae_profile_move_pending(void);

/* the exports folder shown by the system's file browser; its path into path (may be NULL): 1 when it opened */
int ae_profile_move_open_folder(char *path, size_t size);

/* DEV MODE's profile_move command: staging and checks without the dialogs. The answer's text; 1 done, 0 refused */
struct ae_dev_cmd;
int ae_profile_move_dev(struct ae_dev_cmd const *cmd, char *text, int size);

#endif
