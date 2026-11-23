/*
AE_GLUE_RECENT_MAPS.H

The recent-maps memory of the AE TOURNAMENT (rules: ae_recent_maps_rules.h) in the profile's private files (recent_maps.txt): the picks
of a playlist's next map and the map vote's offers leave out the playlist's recent maps while enough others remain, a map that starts
goes into the memory, and DEV MODE shows, sets and clears it. Log prefix "ae social: recent maps".
*/

#ifndef __AE_GLUE_RECENT_MAPS_H
#define __AE_GLUE_RECENT_MAPS_H

#include "ae_social_playlists.h"
#include "ae_social_vote_rules.h"

/* ae_social_playlist_pick / ae_social_vote_pick_choices with the playlist's recent maps left out of what can play (logged once each:
   the window and the maps skipped) */
int ae_glue_recent_pick(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context, int last,
	unsigned long random);
int ae_glue_recent_vote(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context, int last,
	unsigned long seed, struct ae_social_vote_choice *out, int max);
/* the playlist's entry started as a game: its map goes first in the memory, which is written */
void ae_glue_recent_played(struct ae_social_playlist const *playlist, int entry);
/* DEV MODE `social recent show|clear|set <id> [<map,map,...>]`: 1 when it applied; text gets the answer */
int ae_glue_recent_dev(char const *verb, char const *playlist, char const *maps, char *text, int size);

#endif
