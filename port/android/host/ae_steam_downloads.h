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
/* the row texts of the Workshop list: item index's title and state as the list shows them */
void steam_downloads_ae_item_row(int index, char *name, int name_size, char *state, int state_size);

#endif
