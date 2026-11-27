/*
AE_GLUE_CE_ARCHIVE.H

The community map archive's library (port/linux/src/ae_ce_archive.h) glued to the game: a map put in custom_maps is
marked as a downloaded map (its scripts are held to a downloaded map's rules) and the game's map lists look again;
and DEV MODE's `archive` command (ae_dev_cmd_rules.h). No screen code.
*/

#ifndef __AE_GLUE_CE_ARCHIVE_H
#define __AE_GLUE_CE_ARCHIVE_H

struct ae_dev_cmd;

/* the game's thread, every frame while a screen shows the archive: the hook installed, the maps put in custom_maps since
the last call marked and the map lists told */
void ae_glue_ce_archive_poll(void);

/* `archive status | list [<text>] | get <row> | cancel` (ae_dev_cmd_rules.h): runs it, its one line into text; 1 when
it ran (the library's refusals are a 0 with their words) */
int ae_ce_archive_dev(struct ae_dev_cmd const *cmd, char *text, int size);

#endif
