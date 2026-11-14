/*
AE_GLUE_SERVICE_RECORD.H

The service record's store (ae_service_record.h): one set a PC, in the save root's private folder (ae_private_files.h) as
service-record.txt. Read the first time it is asked for and kept for the session; a change is written at once. A file the
reader refuses is kept aside as service-record.txt.bad and a fresh record started; a file of a newer version is read for
what it holds and never written back (its newer fields would be lost). Menus' thread only.

Log: "ae record: service-record.txt: N players, M rivals" / "ae record: service-record.txt: none yet" / "ae record: ...
refused: kept as ...bad, a fresh one started" / "ae record: game added (<kind>, <n> players)" / "ae record: rival <owner> ->
<name> on|off" / "ae record: ... could not be written" / "ae record: the file is of a newer version: not written".
*/

#ifndef __AE_GLUE_SERVICE_RECORD_H
#define __AE_GLUE_SERVICE_RECORD_H

#include "ae_pgcr_screen_rules.h"
#include "ae_service_record.h"

/* the record, read once */
struct ae_service_record *ae_record_store(void);
/* the record written; 1 when written */
int ae_record_store_save(void);
/* a report's game added once (the glue's guard) and written; 1 when it added anything */
int ae_record_store_game(struct ae_pgr_report const *report);
/* a rival set or taken off for the owner (a local player's name), written; 1 when done */
int ae_record_store_rival(char const *owner, char const *name, int on);
/* ae_pgr_mark_rivals' callback: the store's rivals */
int ae_record_is_rival(void *context, char const *owner, char const *name);
/* the engine's scoreboard asks the same by UTF-8 name: whether any local player of the game has this player as a rival */
int ae_record_any_owner_has_rival(char const *name);

#endif
