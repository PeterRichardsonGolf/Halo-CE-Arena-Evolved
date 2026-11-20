/*
AE_COLLECTIONS_RULES.H

MAPS & MODS > WORKSHOP > SHOW: COLLECTIONS: what a Workshop collection's page says about its maps, as pure rules over a
plain model (ae_collections_data.h fills it; ae_screen_steam.c draws it). Which maps are to get, how many and how big, the
words of a row's value, when a question comes before a get, what a remove takes and what it leaves to Steam. Every text
here came from Steam (titles, sizes, ids): a copy is bounded, an id is digits. Unit test: port/linux/tests/ae_collections_rules_test.c.
*/

#ifndef AE_COLLECTIONS_RULES_H
#define AE_COLLECTIONS_RULES_H

enum
{
	AE_COLL_ITEMS = 64,       /* the most maps of one collection kept */
	AE_COLL_MOST = 20,        /* the most collections kept */
	AE_COLL_ID = 24,
	AE_COLL_NAME = 48,
	AE_COLL_TITLE = 64,
	/* a get of this many bytes or more is asked about first */
	AE_COLL_ASK_MEGABYTES = 500
};

/* where one map of a collection is */
enum
{
	AE_COLL_ITEM_ABSENT,      /* not on this PC */
	AE_COLL_ITEM_PRESENT,     /* on this PC, current */
	AE_COLL_ITEM_UPDATE,      /* on this PC, Steam has a newer one */
	AE_COLL_ITEM_WAITING,     /* in the download queue */
	AE_COLL_ITEM_GETTING      /* being downloaded (percent) */
};

struct ae_coll_item
{
	char id[AE_COLL_ID];
	char title[AE_COLL_TITLE];
	unsigned long long size;  /* 0 until Steam has told */
	int state;
	int percent;
	/* on this PC but not one the game's own downloader installed, so it is not removed here: 1 in a Steam library's folder,
	2 in the game's folder without a download record (put there by hand) */
	int steam_owned;
	/* Steam did not answer the details of this map (its size and name are not known) */
	int failed;
	/* the map's list name (<file>@ws<item>) when it is on this PC: its own picture is asked for by it ("" none) */
	char picture_name[96];
};

struct ae_coll
{
	char id[AE_COLL_ID];
	char name[AE_COLL_NAME];
	/* one of the two the offline playlists play */
	int playlist;
	int count;
	/* more maps than this view holds: the collection is listed but not opened */
	int cut;
	struct ae_coll_item items[AE_COLL_ITEMS];
};

struct ae_coll_summary
{
	int count;
	int present;              /* on this PC (current or with an update) */
	int absent;               /* not on this PC and not queued */
	int updates;
	int waiting;
	int getting;              /* 0 or 1 */
	int removable;            /* on this PC and installed by the game */
	int steam_kept;           /* on this PC and Steam's own */
	int sizes_known;          /* every map's size is known */
	int unknown_absent;       /* maps not here whose size is not known yet */
	int unknown_update;       /* maps with an update whose size is not known yet */
	int failed_get;           /* maps a get would ask for that Steam did not answer */
	int cut;                  /* the collection has more maps than the view holds */
	unsigned long long total_bytes;
	unsigned long long absent_bytes;
	unsigned long long update_bytes;
	unsigned long long removable_bytes;
	unsigned long long left_bytes; /* the queued and the one being got */
};

void ae_coll_summarize(struct ae_coll const *coll, struct ae_coll_summary *summary);

/* "96 MB", "1.8 GB"; "" for 0 */
void ae_coll_size_text(unsigned long long bytes, char *text, int size);
/* a Workshop title as the page names a map: the game's name before " - " and a trailing "SMP" tag cut */
void ae_coll_map_name(char const *title, char *name, int size);
/* a collection's title (any case) as a row names it: each word capitalised, a word with a digit in lower case */
void ae_coll_display_name(char const *title, char *name, int size);

/* the ink of a value */
enum { AE_COLL_INK_MUTED, AE_COLL_INK_ACCENT, AE_COLL_INK_WARNING };

/* a collection row's value on the list: "19 MAPS * 2.1 GB", "3 OF 19 ON THIS PC", "INSTALLED", "UPDATE", "GETTING 7 OF 19",
"WAITING", "NO ROOM"; room_ok 0 for a get that does not fit */
void ae_coll_row_value(struct ae_coll_summary const *summary, int room_ok, char *text, int size, int *ink);

/* the first action row of an opened collection */
enum
{
	AE_COLL_ACT_GET,          /* GET THE n MAPS NOT ON THIS PC */
	AE_COLL_ACT_GET_ALL,      /* GET ALL n MAPS */
	AE_COLL_ACT_UPDATES,      /* GET n UPDATES */
	AE_COLL_ACT_STOP,         /* STOP GETTING THEM */
	AE_COLL_ACT_STOP_WAITING, /* STOP WAITING */
	AE_COLL_ACT_NONE          /* everything is here, no update: REMOVE is the first row (or the collection is too large) */
};
int ae_coll_first_action(struct ae_coll_summary const *summary);
/* the action row's label and value (muted / accent / warning ink in *ink) */
void ae_coll_action_text(int action, struct ae_coll_summary const *summary, int room_ok, char *label, int label_size,
	char *value, int value_size, int *ink);
/* the maps a get asks for: the absent ones (the updates when only those are left); ids written, how many */
int ae_coll_get_ids(struct ae_coll const *coll, struct ae_coll_summary const *summary, char (*ids)[AE_COLL_ID], int maximum);
/* the ids a stop takes out of the queue: every map waiting or being got */
int ae_coll_stop_ids(struct ae_coll const *coll, char (*ids)[AE_COLL_ID], int maximum);
/* the ids a remove deletes: the maps on this PC that the game installed, and none that is queued or being got */
int ae_coll_remove_ids(struct ae_coll const *coll, char (*ids)[AE_COLL_ID], int maximum);
/* whether a get of that many bytes asks first (500 MB or more) */
int ae_coll_get_asks(unsigned long long bytes);
/* whether bytes fit in free_bytes; free_bytes 0: not known (the downloader checks each map before it starts), it fits */
int ae_coll_room(unsigned long long free_bytes, unsigned long long bytes);
/* a get's maps whose size Steam has not told yet, and whether the get can start (every size known, none unanswered) */
int ae_coll_get_unknown(struct ae_coll_summary const *summary);
int ae_coll_get_ready(struct ae_coll_summary const *summary);
/* the room check of a get: 1 while its sizes are not all known (the get waits for them instead) */
int ae_coll_room_for(struct ae_coll_summary const *summary, unsigned long long free_bytes);
/* the maps of a collection's get: the bytes the question and the room check count */
unsigned long long ae_coll_get_bytes(struct ae_coll_summary const *summary);

/* one map row's value: its size, INSTALLED, UPDATE, n %, WAITING, or "" while Steam has not told; ink as above */
void ae_coll_item_value(struct ae_coll_item const *item, char *text, int size, int *ink);

/* the confirm texts: "Get <name>?" body, "Remove <name>?" body */
void ae_coll_get_body(char const *name, struct ae_coll_summary const *summary, unsigned long long free_bytes, char *text, int size);
void ae_coll_remove_body(char const *name, struct ae_coll const *coll, struct ae_coll_summary const *summary, char *text, int size);

#endif
