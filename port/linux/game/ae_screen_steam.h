/*
AE_SCREEN_STEAM.H

(1.0 STEAM ACCOUNT) AE's Steam sign-in for MCC's files (ae_screen_steam.c).
A player without a full MCC install signs in with a Steam account that owns Halo: The Master Chief Collection and Halo:
Combat Evolved Anniversary: name and password, then Steam Guard (the app's approval, or a code), then MCC's three shared
map files and the Halo 1 Workshop's maps are downloaded into mcc_maps. The Steam client is the Android app's
(port/android/host/steam_downloads.c, built into the desktop game: port/linux/src/ae_steam_client.h). The password goes
to Steam only and is wiped here once passed; it is never logged or written.

Opened from SETTINGS > SYSTEM > MCC > STEAM ACCOUNT, and from anywhere else that offers the sign-in (the first-start
page): ae_screen_steam_open is the one door. Builds without the client (the dedicated server, 32-bit, the web) open
nothing (ae_screen_steam_available is 0).
*/

#ifndef __AE_SCREEN_STEAM_H
#define __AE_SCREEN_STEAM_H

/* whether this build has the Steam client */
int ae_screen_steam_available(void);

/* opens STEAM ACCOUNT over the stack; crumb: where it was opened from (unused: the crumb is SYSTEM, MCC, THIS PC); 0
when the build has no client or the stack is full */
int ae_screen_steam_open(const char *crumb);

/* DEV MODE's `menu steam <state> [<percent>]`: opened on a made-up client state (ae_steam_rules.h's stages), no Steam
traffic. 0 when the stack is full */
int ae_screen_steam_open_staged(int stage, int percent);

/* the settings row STEAM ACCOUNT's summary: NOT SIGNED IN, SIGNING IN, the account's name, NOT NEEDED: MCC FOUND */
void ae_screen_steam_summary(char *text, int size);

/* whether MAPS & MODS' WORKSHOP page can open on the real client: signed in with an account that owns MCC */
int ae_screen_steam_workshop_ready(void);

/* MAPS & MODS' WORKSHOP page: the Workshop list under the hub's tab strip (staged: a made-up page, no Steam traffic); B leaves the
hub too, LB / RB go to the hub's other pages. 0 when it cannot open */
int ae_screen_steam_open_workshop_hub(int staged);
/* the same on made-up collections: scene is an AE_COLL_SCENE_* (ae_collections_data.h), 1 or more */
int ae_screen_steam_open_collections_staged(int scene);

/* MAPS & MODS left: the account page it replaced is not brought back */
void ae_screen_steam_return_forget(void);

/* closes it wherever it is on the stack */
void ae_screen_steam_close(void);

#endif
