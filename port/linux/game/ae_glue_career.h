/*
AE_GLUE_CAREER.H

The career record's store (ae_career_record.h): one set a PC, in the save root's private folder (ae_private_files.h) as
career.txt. Read the first time it is asked for and kept for the session. A file the reader refuses or finds edited is kept
aside as career.txt.bad and a fresh record started; a file of a newer version is read for what it holds and never written
back. Menus' thread only. Not wired to a game's end yet (0.7.7 step 3).

Log: "ae career: career.txt: N career, M online entries" / "ae career: career.txt: none yet" / "ae career: ... refused: kept as
...bad, a fresh one started" / "ae career: ... could not be written" / "ae career: the file is of a newer version: not written".
*/

#ifndef __AE_GLUE_CAREER_H
#define __AE_GLUE_CAREER_H

#include "ae_career_record.h"

struct ae_career *ae_career_store(void);
/* the record written; 1 when written */
int ae_career_store_save(void);

#endif
