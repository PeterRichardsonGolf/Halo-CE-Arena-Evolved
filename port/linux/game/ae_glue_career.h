/*
AE_GLUE_CAREER.H

The career record's store (ae_career_record.h): one set a PC, in the save root's private folder (ae_private_files.h) as
career.txt. Read the first time it is asked for and kept for the session. A file the reader refuses or finds edited is kept
aside as career.txt.bad and a fresh record started; a file of a newer version is read for what it holds and never written
back. Menus' thread only. Not wired to a game's end yet (0.7.7 step 3); the social career (ae_glue_social.h) keeps its
lines in it. A pending tournament game found at the first read of a run is forgiven when the crash mark is there (a real crash), else counted as a quit (logged), and taken away.

Log: "ae career: career.txt: N career, M online entries" / "ae career: career.txt: none yet" / "ae career: ... refused: kept as
...bad, a fresh one started" / "ae career: ... could not be written" / "ae career: the file is of a newer version: not written".
*/

#ifndef __AE_GLUE_CAREER_H
#define __AE_GLUE_CAREER_H

#include "ae_career_record.h"

struct ae_career *ae_career_store(void);
/* the record written; 1 when written */
int ae_career_store_save(void);
/* forget what was read: the next ae_career_store() reads career.txt again, as at the next start (DEV social pending) */
void ae_career_store_forget(void);

/* the crash mark (ae_private_files.h) of this store: armed while a tournament game is pending, so a real crash forgives it; DEV MODE's
`social pending <id> crashed` writes it as a crash would have. Disarm when no game is pending */
int ae_career_crash_mark_arm(void);
void ae_career_crash_mark_disarm(void);
void ae_career_crash_mark_write(int on);

#endif
