/*
AE_CE_ARCHIVE.H

Browsing and downloading Custom Edition maps from the community map archive (ae_ce_archive.c; its pure rules:
ae_ce_archive_rules.h). The library behind a screen and DEV MODE's `archive` command (port/linux/game/
ae_glue_ce_archive.h); no screen code lives here.

What it does:
- the signed list of the archive's maps is fetched (the HTTPS address in ae_ce_archive.c), taken only when its
  signature is by one of the keys this build pins (a list that does not verify is refused whole), and kept in the
  archive's cache folder for the next start, where it is checked again;
- the list is searched, sorted and paged in memory;
- one map at a time is downloaded to a temporary file beside its place in custom_maps, checked against the signed
  list's size and BLAKE2b and a Custom Edition cache file's header, and only then given its final name, which comes
  from the signed list and nothing else; a map already there is never overwritten unless the caller says so;
- the game's map lists are told to look again (the installed hook, ae_ce_archive_poll).

Nothing is asked of the network until the setting game.ce_archive is on and the first-use consent
(game.ce_archive_consented) is given. No install key, no id and no cookie is sent: a request is a GET with the
updater's User-Agent. The list comes over HTTPS; the map files over plain HTTP (see ae_ce_archive.c), so the signed
BLAKE2b is the only trust a map has, and nothing of a response but its bytes is believed.

Every call takes the lock briefly and never waits on the network or the disk; a thread does that work. Plain types
only: the game's units (64-bit builds rewrite their long) and the platform's both include it.
*/

#ifndef AE_CE_ARCHIVE_H
#define AE_CE_ARCHIVE_H

#include "ae_ce_archive_rules.h"

/* whether this build can reach the archive at all (not the browser's, not Android's guest) */
int ae_ce_archive_available(void);
/* game.ce_archive: the whole feature, off by default */
int ae_ce_archive_enabled(void);
/* game.ce_archive_consented: the first-use card was accepted; set writes it to config.toml at once */
int ae_ce_archive_consented(void);
void ae_ce_archive_set_consent(int yes);

/* ---------- the list */

enum ae_ce_archive_state
{
	AE_CE_ARCHIVE_IDLE,
	AE_CE_ARCHIVE_LOADING,
	AE_CE_ARCHIVE_READY,
	AE_CE_ARCHIVE_FAILED
};

struct ae_ce_archive_status
{
	int state;
	/* READY: the list in use came from the network (1) or the kept copy (0) */
	int from_network;
	/* a few words: why it failed, or (READY) why the network's was not taken */
	char error[160];
	unsigned long long serial;
	/* the maps in the list, the lines left out, and the maps the query shows */
	int count;
	int skipped;
	int view_count;
};

/* starts reading the list (the kept copy at once, then the network's) on a thread: 1 if it is started or the list
is in use already; else 0 and why (feature off, no consent, a run under way) */
int ae_ce_archive_start(char *why, int why_size);
void ae_ce_archive_get_status(struct ae_ce_archive_status *status);

/* ---------- search, sort, page (the list read; any thread) */

/* the query's text, sort, order and show: the view rebuilt; its row count */
int ae_ce_archive_query(struct ae_ce_archive_query const *query);

struct ae_ce_archive_row
{
	/* the file name it is installed under, lowercase */
	char file[AE_CE_ARCHIVE_FILE_SIZE];
	unsigned long long size;
	int installed;
};

/* the view's row at place (0 ...): 1, or 0 past the end */
int ae_ce_archive_row(int place, struct ae_ce_archive_row *row);

/* ---------- a download (one at a time) */

enum ae_ce_archive_download_state
{
	AE_CE_ARCHIVE_DOWNLOAD_NONE,
	AE_CE_ARCHIVE_DOWNLOAD_RUNNING,
	AE_CE_ARCHIVE_DOWNLOAD_DONE,
	AE_CE_ARCHIVE_DOWNLOAD_FAILED,
	AE_CE_ARCHIVE_DOWNLOAD_CANCELLED
};

struct ae_ce_archive_download
{
	int state;
	char file[AE_CE_ARCHIVE_FILE_SIZE];
	unsigned long long done;
	unsigned long long total;
	/* bytes a second over the last seconds */
	unsigned int rate;
	char error[160];
};

/* the map of that file name (any case) in the list downloaded into custom_maps on a thread: 1 if it started; else 0
and why (feature off, none under way allowed, not in the list, a file of that name there and overwrite 0, not
enough free disk). Nothing is fetched when it returns 0 */
int ae_ce_archive_download(char const *file, int overwrite, char *why, int why_size);
/* the download stops at once, wherever it is: no file with a final name is left, its temporary file is deleted */
void ae_ce_archive_cancel(void);
void ae_ce_archive_download_status(struct ae_ce_archive_download *status);
/* a finished download's result forgotten (back to NONE) */
void ae_ce_archive_download_reset(void);

/* ---------- the game's side */

/* called on the game's thread for each map put in custom_maps since the last poll: the file name (the game's map
lists look again, the map is marked as downloaded: ae_glue_ce_archive.c) */
typedef void (*ae_ce_archive_installed_proc)(char const *file);
void ae_ce_archive_set_installed_hook(ae_ce_archive_installed_proc hook);
/* the game's thread, every frame while a screen is open: the hook called for each map installed */
void ae_ce_archive_poll(void);
/* which of the list's maps are in custom_maps looked at again (a thread does it). Any thread may call it at any time, the
menus' included: it takes the lock only briefly, never waits on the disk, calls made while one runs make it go round once more,
and its result is dropped when a new list was taken meanwhile (that list has its own marks) */
void ae_ce_archive_rescan(void);

/* whether a map is loaded in the game (the game's answer; with none set, a map is never replaced) */
typedef int (*ae_ce_archive_map_loaded_proc)(void);
void ae_ce_archive_set_map_loaded_proc(ae_ce_archive_map_loaded_proc proc);

/* the two hosts the archive is asked at, for the first-use card: the list's and the map files' (a test archive's while one is
set) */
void ae_ce_archive_hosts(char *list_host, int list_size, char *file_host, int file_size);

/* ---------- DEV MODE and the tests */

/* the list of downloaded maps a map's name is written to before it has its final name (the game's own when not set) */
void ae_ce_archive_set_marks_file(char const *path);

/* a made-up archive at base (http://127.0.0.1:<port>[/folder]: only this machine) with its list signed by the key
(64 hex digits) in place of the pinned ones: the list at <base>/map_torrents.txt (+ ".sig"), the maps at
<base>/maps/<name>. 1, else 0 and why. Nothing else turns the pinned keys off */
int ae_ce_archive_set_test_server(char const *base, char const *key_hex, char *why, int why_size);
/* DEV MODE only: AE_CE_ARCHIVE_BASE and AE_CE_ARCHIVE_TEST_KEY read as set_test_server's two arguments; 1 if they
were set and taken (its words in text), 0 if not set (the archive's own), -1 if refused (why in text) */
int ae_ce_archive_use_dev_environment(char *text, int text_size);

struct ae_ce_archive_limits
{
	/* nothing arrives for this long: the transfer ends (milliseconds) */
	unsigned int stall_ms;
	/* a file's whole transfer, and the list's, at most */
	unsigned int file_deadline_ms;
	unsigned int list_deadline_ms;
	/* nonzero: the disk's free bytes are this (a test's small disk) */
	int fake_free_set;
	unsigned long long fake_free_bytes;
};
void ae_ce_archive_limits_default(struct ae_ce_archive_limits *limits);
void ae_ce_archive_set_limits(struct ae_ce_archive_limits const *limits);
/* the folders (custom_maps, and the kept list's) when not the game's own */
void ae_ce_archive_set_folders(char const *custom_maps, char const *cache);
/* a function giving the free bytes of the disk a folder is on (the game's, where the platform layer has none) */
typedef unsigned long long (*ae_ce_archive_disk_free_proc)(char const *folder);
void ae_ce_archive_set_disk_free(ae_ce_archive_disk_free_proc proc);

#endif
