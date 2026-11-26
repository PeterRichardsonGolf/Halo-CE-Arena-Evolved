/*
AE_FIREFIGHT_DEV.H

DEV MODE's `menu firefight <scene> ...` (ae_firefight_dev.c): Firefight's screens opened as staged scenes for a TEST RUN, each on the scene its setup
lines name. Scenes: lobby [variant <1-16>] [map <name>] [lan] [content]; rules [variant <1-16>] [change <option>=<value> ...]; skulls; content
[mode vanilla|include|only] [custom]; tags; loadout; objectives; data ready|preparing|needsmaps|failed|auto (the data's state START GAME and
the FIREFIGHT row wait on). Scenes with content use a made-up map (ae_glue_firefight.h). From the menus only.
*/

#ifndef __AE_FIREFIGHT_DEV_H
#define __AE_FIREFIGHT_DEV_H

#include "ae_dev_cmd_rules.h"

/* the scene opened (or the state set); 1 when done, else 0 with why written. The words are the command's arguments after `menu` */
int ae_firefight_dev_menu(struct ae_dev_cmd const *cmd, char *why, int size);

#endif
