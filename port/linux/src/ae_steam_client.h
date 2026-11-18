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

#if defined(HALO_STEAM_CLIENT)

int ae_steam_client_built(void);
/* the client's threads started and a kept sign-in resumed, once (the first call; later calls do nothing) */
void ae_steam_client_open(void);
/* once a frame while something shows the client's state: the snapshot taken */
void ae_steam_client_frame(void);
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
/* the secret store: -1 not asked yet, 0 none here, 1 there (ae_steam_secret.h) */
int ae_steam_client_store(void);
/* the focused Workshop item's picture, as the frame's snapshot has it: its side (AE_STEAM_PICTURE_SIDE) with key the
item (pixels, when not NULL, side * side of them, 0xAARRGGBB, filled), or 0 and key 0 while there is none */
int ae_steam_client_picture(unsigned long long *key, unsigned int *pixels);

#else

#include <string.h>

static inline int ae_steam_client_built(void) { return 0; }
static inline void ae_steam_client_open(void) {}
static inline void ae_steam_client_frame(void) {}
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
static inline int ae_steam_client_store(void) { return -1; }
static inline int ae_steam_client_picture(unsigned long long *key, unsigned int *pixels)
{ (void)pixels; *key = 0; return 0; }

#endif

#endif
