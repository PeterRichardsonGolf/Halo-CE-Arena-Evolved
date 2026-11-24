/*
AE_SERVER_SHUFFLE.H

server.rotation_shuffle (HALO_SERVER_ROTATION_SHUFFLE): the dedicated server plays each pass over its rotation in a new
random order (ae_rotation_shuffle.c's rules), not the order of the file. Off by default. server_director.c calls these
at the start and when a pass ends.
*/

#ifndef __AE_SERVER_SHUFFLE_H
#define __AE_SERVER_SHUFFLE_H

#include "../src/server_rotation.h"

/* the rotation just read: put in a first random order (if the setting is on) */
void ae_server_shuffle_start(
	struct server_rotation *rotation);

/* the entry at *index was just put in play: if it ends a pass (no entry that can be played comes after it) and the
setting is on, the rotation is put in a new order whose first entry is not on the map just played. *index becomes the
last place, so the next game is the new first entry; *current_index follows the entry played (NONE if it is not found) */
void ae_server_shuffle_pass_end(
	struct server_rotation *rotation,
	long *index,
	struct server_entry const *played,
	long *current_index);

#endif
