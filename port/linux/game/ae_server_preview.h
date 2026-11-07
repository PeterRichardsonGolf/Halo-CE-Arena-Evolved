/*
AE_SERVER_PREVIEW.H

(0.7.7 preview) What a dedicated server owner reads at start, in plain words: the MCC folder found, the Workshop items found,
the rotation the server will play (and the entries it refused, with why). server_mode.c calls these (hook lines).
*/

#ifndef AE_SERVER_PREVIEW_H
#define AE_SERVER_PREVIEW_H

#include "../src/server_rotation.h"

/* no Xbox ui.map: boot from MCC's files when the client's route allows it (1, and says so); 0 otherwise (the caller says why not) */
int ae_server_preview_boot(void);
/* with an Xbox ui.map: the Workshop scan the client starts at launch, so that <name>@ws<id> entries resolve, and the
summary of what was found. Does nothing without MCC */
void ae_server_preview_scan(void);
/* the rotation it will play, one line an entry, after the entries were checked */
void ae_server_preview_rotation(struct server_rotation const *rotation);

/* a Custom Edition map's file (<name>@ce, or a name that is in no Xbox map) where the game looks for one: 1 and its path */
int ae_server_preview_ce_find(const char *file, char *path, int size);

#endif
