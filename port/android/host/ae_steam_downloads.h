/*
AE_STEAM_DOWNLOADS.H

(AE) What the desktop's STEAM ACCOUNT screen (port/linux/game/ae_screen_steam.c, through port/linux/src/ae_steam_client.c)
asks of the Steam client (steam_downloads.c): its state whole, and its commands directly rather than through the rows'
text model. The state is the frame's snapshot (steam_downloads_poll copies it). A password or code passed is wiped by the
host after use. Implemented in ae_steam_downloads_api.inc, which steam_downloads.c includes at its end.
*/

#ifndef AE_STEAM_DOWNLOADS_H
#define AE_STEAM_DOWNLOADS_H

#include <stdint.h>

struct steam_ae_status
{
	int session;  /* 0 out, 1 connecting, 2 Steam Guard, 3 signing in, 4 in, 5 unavailable */
	int checking;       /* the saved sign-in is being read (not given up on) */
	int store_waiting;  /* the system's password store is being asked (it may be showing a prompt) */
	int guard_app, guard_code;
	char account[64];
	char message[256];
	int owns, files_known, shared_present[3], downloading, queue_count, shared_queued;
	uint64_t done, size, speed;
	int packages, loading;
	uint32_t total;
	int page, item_count, sort, type;
	char search[128];
	int item_valid;  /* the focused Workshop item */
	char item_title[96], item_description[600], item_tags[160], item_author[100];
	uint64_t item_size;
	uint32_t item_subscribers;
	int item_installed, item_downloading, item_queued;
};
void steam_downloads_ae_status(struct steam_ae_status *out);
/* a sign-in by name and password (signed out only); the Steam Guard code typed while it waits for one */
void steam_downloads_ae_sign_in(char const *account, char const *password);
void steam_downloads_ae_guard_code(char const *code);
void steam_downloads_ae_cancel_sign_in(void);
void steam_downloads_ae_log_out(void);
/* MCC's shared files: asked for (nothing when asked already), or the one going cancelled / taken out of the queue */
void steam_downloads_ae_download_shared(void);
void steam_downloads_ae_cancel_shared(void);
/* every download under way or waiting cancelled (leaving the screen) */
void steam_downloads_ae_cancel_all(void);
/* the Workshop list shown (1) or the hub (0); a search; SORT or TYPE (0, 1) stepped on; the page moved by delta; the
focused item (an index of the page) and A on it (downloads it, or cancels it) */
void steam_downloads_ae_workshop(int open);
void steam_downloads_ae_search(char const *text);
void steam_downloads_ae_filter_step(int which);
void steam_downloads_ae_page(int delta);
void steam_downloads_ae_item_focus(int index);
void steam_downloads_ae_item_choose(int index);
/* the Workshop number of the page's item at that place; 0: no such item */
uint64_t steam_downloads_ae_item_id(int index);
/* the Workshop item downloaded here (its folder of mcc_maps and its record) asked to be removed: 1 when the request is
taken (the files' thread does it; the lists are read again after), 0 for an item not here, one being downloaded or
queued, or while another removal waits */
int steam_downloads_ae_item_remove(uint64_t id);
/* the row texts of the Workshop list: item index's title and state as the list shows them */
void steam_downloads_ae_item_row(int index, char *name, int name_size, char *state, int state_size);

/* a Workshop item asked for by a server the player joined (ae_join_fetch.c): fetched by its number alone, one at a time,
at most item_cap bytes of its own and bytes_left of the budget by what Steam says of it, queued behind any download under
way; 1 when begun, 0 when the client is not started or a fetch is going. taken: the manifest's total once checked */
enum
{
	STEAM_AE_FETCH_IDLE = 0, STEAM_AE_FETCH_RUNNING = 1, STEAM_AE_FETCH_DONE = 2, STEAM_AE_FETCH_FAILED = -1
};
enum
{
	STEAM_AE_FETCH_NONE, STEAM_AE_FETCH_BUSY, STEAM_AE_FETCH_NOT_SIGNED_IN, STEAM_AE_FETCH_REFUSED,
	STEAM_AE_FETCH_NO_ROOM, STEAM_AE_FETCH_TOO_LARGE, STEAM_AE_FETCH_LIMIT_BYTES, STEAM_AE_FETCH_NOT_MULTIPLAYER,
	STEAM_AE_FETCH_ALREADY, STEAM_AE_FETCH_CANCELLED
};
struct steam_ae_fetch
{
	int state, why;
	uint64_t id, done, size, taken;
	char title[96];
};
int steam_downloads_ae_fetch_begin(uint64_t id, uint64_t item_cap, uint64_t bytes_left);
void steam_downloads_ae_fetch_status(struct steam_ae_fetch *out);
void steam_downloads_ae_fetch_cancel(void);
void steam_downloads_ae_fetch_clear(void);

/* ---------- Workshop collections (the game's collection pages: a set of items asked for together) */

struct steam_ae_collection_item
{
	uint64_t id, size;
	uint32_t updated;
	char title[96];
	/* Steam has told of the item (size and title are set); failed: Steam was asked and did not answer (asked again later) */
	int known, failed;
	/* waiting in the download queue, being downloaded (percent set), an installed copy older than Steam's */
	int queued, getting, percent, outdated;
	/* a package this game's own downloader installed and recorded: the only kind a remove deletes */
	int recorded;
};
/* Steam asked for the items' sizes and titles, on a thread of its own (ids already known are not asked again; an id that
failed is asked again after a pause); nothing when the client is not signed in */
void steam_downloads_ae_collection_ask(uint64_t const *ids, int count);
/* the items fed to the download queue a few at a time, in the order given; an id that is installed and current, queued, being
downloaded or already in another get is left out. How many were taken (0: nothing to do, or no room for another get) */
int steam_downloads_ae_collection_get(uint64_t const *ids, int count);
/* those items taken out of every get and out of the queue, the one under way dropped; the other ids of a get go on */
void steam_downloads_ae_collection_stop(uint64_t const *ids, int count);
/* those items' packages removed, one after another on a thread of the client's: only ones the downloader recorded, and
none that is queued or being downloaded. How many were taken */
int steam_downloads_ae_collection_remove(uint64_t const *ids, int count);
/* the batch of removals so far: waiting, done, failed */
void steam_downloads_ae_collection_remove_status(int *pending, int *done, int *failed);
/* the last removal of this item in the batch: 0 waiting, 1 done, 2 failed; -1 when the batch holds none of it (forgotten, or never taken) */
int steam_downloads_ae_removal_result(uint64_t id);
/* one item as the client has it; 0 when Steam has not told of it yet (the queue and download state are still set) */
int steam_downloads_ae_collection_item(uint64_t id, struct steam_ae_collection_item *out);
/* the item's Workshop picture (a PREVIEW_SIDE square of 0xAARRGGBB pixels, copied when pixels is not NULL), or 0 while there is none;
asking is how the client fetches it, one picture at a time */
int steam_downloads_ae_collection_picture(uint64_t id, uint32_t *pixels);

#endif
