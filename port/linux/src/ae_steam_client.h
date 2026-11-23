/*
AE_STEAM_CLIENT.H

The desktop game's door to the Steam client (port/android/host/steam_downloads.c, built in under HALO_STEAM_CLIENT:
tools/steam_client_build.py) for STEAM ACCOUNT (port/linux/game/ae_screen_steam.c): the client's state as a struct
ae_steam_status (ae_steam_rules.h) and its commands. A password or Steam Guard code goes to Steam only: the caller wipes
its copy once it has been passed. Builds without the client (the dedicated server, 32-bit, the web) get these as stubs
that report nothing: ae_steam_client_built() is 0 there.
*/

#ifndef AE_STEAM_CLIENT_H
#define AE_STEAM_CLIENT_H

#include "../game/ae_steam_rules.h"

/* the side of the focused Workshop item's square picture (the client's PREVIEW_SIDE) */
enum { AE_STEAM_PICTURE_SIDE = 256 };

/* a Workshop item asked for by a game server (ae_join_fetch.c): see steam_downloads_ae_fetch_begin in ae_steam_downloads.h
(why: its STEAM_AE_FETCH_ reasons) */
struct ae_steam_fetch
{
	int state, why;
	unsigned long long id, done, size, taken;
	char title[96];
};

/* a Workshop collection's item as the client has it (ae_steam_client_collection_item) */
struct ae_steam_collection_item
{
	unsigned long long id, size;
	char title[96];
	int known, failed, queued, getting, percent, outdated, recorded;
};

#if defined(HALO_STEAM_CLIENT)

int ae_steam_client_built(void);
/* whether the client was started this run (ae_steam_client_open) */
int ae_steam_client_opened(void);
/* the client's threads started and a kept sign-in resumed, once (the first call; later calls do nothing) */
void ae_steam_client_open(void);
/* once a frame while something shows the client's state: the snapshot taken */
void ae_steam_client_frame(void);
/* nonzero once after a download put files in mcc_maps: the game reads its map lists again */
int ae_steam_client_take_rescan(void);
void ae_steam_client_status(struct ae_steam_status *out);
/* the Workshop item row texts of the page (name, state) */
void ae_steam_client_item_row(int index, char *name, int name_size, char *state, int state_size);
void ae_steam_client_sign_in(char const *account, char const *password);
void ae_steam_client_guard_code(char const *code);
void ae_steam_client_cancel_sign_in(void);
void ae_steam_client_log_out(void);
void ae_steam_client_download_shared(void);
void ae_steam_client_cancel_shared(void);
void ae_steam_client_cancel_all(void);
void ae_steam_client_workshop(int open);
void ae_steam_client_search(char const *text);
void ae_steam_client_filter_step(int which);
void ae_steam_client_page(int delta);
void ae_steam_client_item_focus(int index);
void ae_steam_client_item_choose(int index);
/* the Workshop number of the page's item at that place (0: none): its page on Steam is
steamcommunity.com/sharedfiles/filedetails/?id=<number> */
unsigned long long ae_steam_client_item_id(int index);
/* a Workshop item downloaded here asked to be removed from this PC (1: taken) */
int ae_steam_client_item_remove(unsigned long long id);
int ae_steam_client_fetch_begin(unsigned long long id, unsigned long long item_cap, unsigned long long bytes_left);
void ae_steam_client_fetch_status(struct ae_steam_fetch *out);
void ae_steam_client_fetch_cancel(void);
void ae_steam_client_fetch_clear(void);

/* a Workshop collection's maps: Steam's sizes and titles, and the queue state */
/* Steam asked about the items (a thread of the client's; nothing when signed out); the items queued as room comes; those
taken out of the queue and the one under way dropped; one item as the client has it (0: Steam has not told of it) */
void ae_steam_client_collection_ask(unsigned long long const *ids, int count);
int ae_steam_client_collection_get(unsigned long long const *ids, int count);
void ae_steam_client_collection_stop(unsigned long long const *ids, int count);
/* those items' packages removed, one after another (only ones the game's own downloader recorded): how many were taken, and
the batch's results so far (waiting, done, failed) */
int ae_steam_client_collection_remove(unsigned long long const *ids, int count);
void ae_steam_client_collection_remove_status(int *pending, int *done, int *failed);
/* one item's removal in the batch: 0 waiting, 1 done, 2 failed, -1 the batch holds none of it */
int ae_steam_client_removal_result(unsigned long long id);
int ae_steam_client_collection_item(unsigned long long id, struct ae_steam_collection_item *out);
/* a collection item's Workshop picture: AE_STEAM_PICTURE_SIDE with the pixels (0xAARRGGBB, side * side, when not NULL)
filled, or 0 while there is none; asking is how the client fetches it, one at a time. Never opens the client */
int ae_steam_client_collection_picture(unsigned long long id, unsigned int *pixels);
/* the secret store: -1 not asked yet, 0 none here, 1 there (ae_steam_secret.h) */
int ae_steam_client_store(void);
/* the focused Workshop item's picture, as the frame's snapshot has it: its side (AE_STEAM_PICTURE_SIDE) with key the
item (pixels, when not NULL, side * side of them, 0xAARRGGBB, filled), or 0 and key 0 while there is none */
int ae_steam_client_picture(unsigned long long *key, unsigned int *pixels);

#else

#include <string.h>

static inline int ae_steam_client_fetch_begin(unsigned long long id, unsigned long long item_cap, unsigned long long bytes_left)
{ (void)id; (void)item_cap; (void)bytes_left; return 0; }
static inline void ae_steam_client_fetch_status(struct ae_steam_fetch *out) { memset(out, 0, sizeof(*out)); }
static inline void ae_steam_client_fetch_cancel(void) {}
static inline void ae_steam_client_fetch_clear(void) {}


static inline int ae_steam_client_built(void) { return 0; }
static inline int ae_steam_client_opened(void) { return 0; }
static inline void ae_steam_client_open(void) {}
static inline void ae_steam_client_frame(void) {}
static inline int ae_steam_client_take_rescan(void) { return 0; }
static inline void ae_steam_client_status(struct ae_steam_status *out) { memset(out, 0, sizeof(*out)); out->owns = -1; }
static inline void ae_steam_client_item_row(int index, char *name, int name_size, char *state, int state_size)
{ (void)index; (void)name_size; (void)state_size; name[0] = state[0] = 0; }
static inline void ae_steam_client_sign_in(char const *account, char const *password) { (void)account; (void)password; }
static inline void ae_steam_client_guard_code(char const *code) { (void)code; }
static inline void ae_steam_client_cancel_sign_in(void) {}
static inline void ae_steam_client_log_out(void) {}
static inline void ae_steam_client_download_shared(void) {}
static inline void ae_steam_client_cancel_shared(void) {}
static inline void ae_steam_client_cancel_all(void) {}
static inline void ae_steam_client_workshop(int open) { (void)open; }
static inline void ae_steam_client_search(char const *text) { (void)text; }
static inline void ae_steam_client_filter_step(int which) { (void)which; }
static inline void ae_steam_client_page(int delta) { (void)delta; }
static inline void ae_steam_client_item_focus(int index) { (void)index; }
static inline void ae_steam_client_item_choose(int index) { (void)index; }
static inline unsigned long long ae_steam_client_item_id(int index) { (void)index; return 0; }
static inline int ae_steam_client_item_remove(unsigned long long id) { (void)id; return 0; }
static inline void ae_steam_client_collection_ask(unsigned long long const *ids, int count) { (void)ids; (void)count; }
static inline int ae_steam_client_collection_get(unsigned long long const *ids, int count) { (void)ids; (void)count; return 0; }
static inline int ae_steam_client_collection_remove(unsigned long long const *ids, int count) { (void)ids; (void)count; return 0; }
static inline void ae_steam_client_collection_remove_status(int *pending, int *done, int *failed) { *pending = *done = *failed = 0; }
static inline int ae_steam_client_removal_result(unsigned long long id) { (void)id; return -1; }
static inline void ae_steam_client_collection_stop(unsigned long long const *ids, int count) { (void)ids; (void)count; }
static inline int ae_steam_client_collection_item(unsigned long long id, struct ae_steam_collection_item *out)
{ memset(out, 0, sizeof(*out)); out->id = id; return 0; }
static inline int ae_steam_client_collection_picture(unsigned long long id, unsigned int *pixels)
{ (void)id; (void)pixels; return 0; }
static inline int ae_steam_client_store(void) { return -1; }
static inline int ae_steam_client_picture(unsigned long long *key, unsigned int *pixels)
{ (void)pixels; *key = 0; return 0; }

#endif

#endif
