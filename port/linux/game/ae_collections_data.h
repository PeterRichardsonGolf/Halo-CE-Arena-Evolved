/*
AE_COLLECTIONS_DATA.H

What the collections page (ae_screen_steam.c, SHOW: COLLECTIONS) reads: the model of ae_collections_rules.h filled from the
Workshop collections table (ae_mcc.h: Steam's public answer, kept a day), the Workshop maps this PC has (their folder tells
which the game installed) and the Steam client's sizes and queue (ae_steam_client.h); or, for DEV MODE's `menu maps
workshop <scene>`, made-up collections that read and ask nothing.
*/

#ifndef AE_COLLECTIONS_DATA_H
#define AE_COLLECTIONS_DATA_H

#include "ae_collections_rules.h"

struct ae_coll_model
{
	int count;
	struct ae_coll colls[AE_COLL_MOST];
	/* free bytes on the maps drive (0: not known) */
	unsigned long long free_bytes;
	/* Steam is signed in: sizes and gets work */
	int signed_in;
	/* Steam's collection list is not here yet (asked, not answered) */
	int loading;
	/* the request is over and there are no collections: Steam did not answer, or named none */
	int empty;
	/* free_bytes is a real reading: 0 is not known here (the downloader checks each map before it starts) */
	int free_known;
};

/* SORT's values on the list */
enum { AE_COLL_SORT_PLAYLISTS, AE_COLL_SORT_NAME, AE_COLL_SORT_SIZE, AE_COLL_SORTS };
char const *ae_coll_sort_name(int sort);

/* the staged scenes of `menu maps workshop <word>` */
enum
{
	AE_COLL_SCENE_NONE,
	AE_COLL_SCENE_SHOW,         /* the list with SHOW focused */
	AE_COLL_SCENE_LIST,         /* the list, Social Map Pack focused */
	AE_COLL_SCENE_OPEN,         /* Social Map Pack opened, GET focused */
	AE_COLL_SCENE_MAP,          /* ... a map's row focused */
	AE_COLL_SCENE_GETTING,      /* ... being got */
	AE_COLL_SCENE_INSTALLED,    /* ... all here, two updates */
	AE_COLL_SCENE_CONFIRM_GET,
	AE_COLL_SCENE_CONFIRM_REMOVE,
	AE_COLL_SCENE_NO_ROOM,
	AE_COLL_SCENES
};
/* the real model: read again at most once a second (or when asked with force); asks Steam for the collections and, when
signed in, for the maps' sizes */
void ae_coll_data_read(struct ae_coll_model *model, int sort, int force);
/* a made-up model for a scene */
void ae_coll_data_stage(struct ae_coll_model *model, int scene, int sort);

/* removing maps: handed to the client, which removes them one after another whatever page is up. Maps this PC is using now
(a lobby or game on them) are left out; *in_use counts them. How many the client took */
int ae_coll_data_remove_start(char (*ids)[AE_COLL_ID], int count, int *in_use);
/* the batch's results so far (waiting, done, failed) */
void ae_coll_data_remove_status(int *pending, int *done, int *failed);

#endif
