/*
AE_STEAM_SECRET.H

Where the desktop game keeps the secret that seals the Steam sign-in (steam_seal.c, port/android/host): a random
32-byte key held by the system's own secret store, never in the data folder beside the session file it seals:

- Windows: the key is made, then wrapped with DPAPI (CryptProtectData: only this Windows user can open it), and the
  wrapped bytes are kept in the steam folder;
- macOS: a generic password of the login Keychain (Security.framework);
- Linux: the Secret Service (libsecret, loaded when it is first asked for: nothing links to it).

Where there is none (no Secret Service running, a locked or refused Keychain) there is no key: the sign-in is not
written, and lasts until the game closes. Neither the key nor the token is ever logged.
*/

#ifndef AE_STEAM_SECRET_H
#define AE_STEAM_SECRET_H

struct steam_sha1;

#include <stddef.h>

#if defined(HALO_STEAM_CLIENT)

/* the folder the session file is in (steam_seal_folder's), where Windows keeps its wrapped key */
void ae_steam_secret_folder(char const *folder);
/* Windows: this user's own folder for the sign-in, %LOCALAPPDATA%\\Arena Evolved\\steam, replaces folder (made once with an
owner-only DACL; a sign-in left in the shared data folder's steam/ moves in once when its key is this user's): 1, or 0 and
folder as it was. Elsewhere there is none: 0 */
#if defined(_WIN32)
int ae_steam_user_folder(char const *root, char *folder, size_t size);
#else
static inline int ae_steam_user_folder(char const *root, char *folder, size_t size)
{ (void)root; (void)folder; (void)size; return 0; }
#endif
/* the key (made if there is none yet) added to sha: 1, or 0 where the system has no store to keep it in */
int ae_steam_secret(struct steam_sha1 *sha);
/* the key forgotten, after a log out */
void ae_steam_secret_forget(void);
/* what the last ask for the key found: -1 never asked, 0 no store here (or its folder cannot be written), 1 a store */
int ae_steam_secret_store(void);
/* 1 while the store is being asked (a locked keyring may be showing a prompt behind the game's window) */
int ae_steam_secret_waiting(void);

#else

static inline int ae_steam_user_folder(char const *root, char *folder, size_t size)
{ (void)root; (void)folder; (void)size; return 0; }
static inline void ae_steam_secret_folder(char const *folder) { (void)folder; }
static inline int ae_steam_secret(struct steam_sha1 *sha) { (void)sha; return 0; }
static inline void ae_steam_secret_forget(void) {}
static inline int ae_steam_secret_store(void) { return -1; }
static inline int ae_steam_secret_waiting(void) { return 0; }

#endif

#endif
