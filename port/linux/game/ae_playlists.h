/*
AE_PLAYLISTS.H

(0.9.4) Several named playlists, CREATE GAME's PLAYLISTS board: the set of up to AE_PLAYLISTS_MAX playlists (each an
ae_playlist.h list of up to 64 games), the one in use (or none: OFF), the names, the file <save root>/private/playlists.txt,
the migration of today's one-list playlist.txt, and what plays next by each playlist's ORDER. Pure C
(port/linux/tests/ae_playlists_test.c); ae_playlists_store.c reads and writes the files.

The file, one section a playlist (a section is today's playlist.txt, so playlist.txt is a one-section file):

    # Arena Evolved playlists (CREATE GAME > PLAYLIST). One section a playlist.
    # in use: 4V4 ARENA
    # playlist: 4V4 ARENA
    # map vote: 3
    # no repeats: 1
    # order: shuffle
    # next: 3
    damnation:file=AE TEAM SLY  # weight 3
    # playlist: FFA NIGHT
    ...

IN TURN's place ("# next:", 1-based) is kept so a restart goes on where it stopped; a SHUFFLE pass lives in memory only.
*/

#ifndef AE_PLAYLISTS_H
#define AE_PLAYLISTS_H

#include "ae_playlist.h"

enum
{
	AE_PLAYLISTS_MAX = 16,
	AE_PLAYLISTS_NAME_MAX = 24,        /* characters of a playlist's name */
	AE_PLAYLISTS_FILE = 131072,        /* the file's most, 128 KB: a larger one is kept and never written over */
	AE_PLAYLISTS_SECTION = AE_PLAYLIST_TEXT
};

struct ae_playlists
{
	int count;
	int in_use;                        /* the index in use, -1: OFF */
	short next[AE_PLAYLISTS_MAX];      /* IN TURN: the place (1-based) of the game to come up, 0: the first */
	struct ae_playlist lists[AE_PLAYLISTS_MAX];
};

void ae_playlists_init(struct ae_playlists *set);

/* ---------- names */

enum { AE_PLAYLISTS_NAME_OK, AE_PLAYLISTS_NAME_EMPTY, AE_PLAYLISTS_NAME_LONG, AE_PLAYLISTS_NAME_CHARACTER,
	AE_PLAYLISTS_NAME_OFF, AE_PLAYLISTS_NAME_TAKEN };

/* a typed name as stored: the ends trimmed, capitals; A-Z, 0-9, space, - ' & only, 1 to AE_PLAYLISTS_NAME_MAX characters,
not OFF. except: the playlist whose own name may be kept in another case (rename; -1: none). The reason, AE_PLAYLISTS_NAME_OK
when fine; name (size at least AE_PLAYLISTS_NAME_MAX + 1) gets the cleaned text either way */
int ae_playlists_name_check(struct ae_playlists const *set, char const *typed, int except, char *name, int size);
/* the message line's words for a refused name ("A playlist is already called 4V4 ARENA.") */
void ae_playlists_name_message(int reason, char const *name, char *text, int size);
/* the index of the playlist called that (any case), -1 for none */
int ae_playlists_find(struct ae_playlists const *set, char const *name);
/* the name NEW PLAYLIST offers: "PLAYLIST <n>" with the first free number from the count + 1 */
void ae_playlists_default_name(struct ae_playlists const *set, char *name, int size);

/* ---------- the set */

/* a new empty playlist (RANDOM, MAP VOTE and NO REPEATS on): its index, -1 when the name is refused (see
ae_playlists_name_check) or AE_PLAYLISTS_MAX are made. reason gets the refusal (may be NULL) */
int ae_playlists_new(struct ae_playlists *set, char const *typed, int *reason);
/* renamed: AE_PLAYLISTS_NAME_OK when it was */
int ae_playlists_rename(struct ae_playlists *set, int index, char const *typed);
/* a copy at the end called "<NAME> 2" (the first free number; the name cut to fit): its index, -1 when full */
int ae_playlists_duplicate(struct ae_playlists *set, int index);
/* taken away: the in use one turns OFF when it is this one (a later one's index moves up) */
void ae_playlists_delete(struct ae_playlists *set, int index);
/* the in use one: -1 for OFF; 1 when it changed */
int ae_playlists_use(struct ae_playlists *set, int index);
/* a list with no games is not kept (no empty playlists on disk): taken away, the in use one turning OFF. 1 when one went */
int ae_playlists_drop_empty(struct ae_playlists *set, int index);

/* ---------- the file */

/* the file's text (only playlists with games); its length, -1 when size is too small */
long ae_playlists_write(struct ae_playlists const *set, char *text, long size);
/* the set from a file's text (a set already there is replaced); the playlists read. A text with more than AE_PLAYLISTS_MAX
sections keeps the first ones. Games before any section go in one called MY PLAYLIST. Names are cleaned (and made unique) */
int ae_playlists_read(struct ae_playlists *set, char const *text);
/* today's playlist.txt as the first and only playlist, in use OFF (ORDER RANDOM unless it says); the playlists made (0 or 1) */
int ae_playlists_migrate(struct ae_playlists *set, char const *legacy_text);

/* ---------- what plays next */

struct ae_playlist_run
{
	short current;                     /* the game the lobby is set up with (UP NEXT), -1 none */
	short previous;                    /* the game played before it, -1 none (PLAYED) */
	short pass[AE_PLAYLIST_ENTRIES];   /* SHUFFLE: this pass's order */
	short pass_count, pass_at;
};

void ae_playlist_run_reset(struct ae_playlist_run *run);
/* a SHUFFLE pass given up (the list was edited): the next pick makes a new one */
void ae_playlist_run_forget_pass(struct ae_playlist_run *run);
/* the next game: by the list's ORDER (IN TURN goes on after the current one, or from place (0-based) when there is none;
SHUFFLE takes the pass's next, a new pass when it is spent; RANDOM draws by weight, never the current one while NO REPEATS is
on and there is another). playable[i] 0 skips a game this PC cannot play (NULL: all can). after_game: the game staged was
played, so it becomes PREVIOUS. The new current game, -1 when no game can be played (the run stays as it was). seed: any
number */
int ae_playlist_run_advance(struct ae_playlist const *list, struct ae_playlist_run *run, int place,
	unsigned char const *playable, int after_game, unsigned long seed);
/* an entry's tag in the games' list */
enum { AE_PLAYLIST_TAG_NONE, AE_PLAYLIST_TAG_UP_NEXT, AE_PLAYLIST_TAG_PLAYED };
int ae_playlist_run_tag(struct ae_playlist_run const *run, int entry);
/* a game taken out, or moved from -> to, shifts the run's indexes */
void ae_playlist_run_removed(struct ae_playlist_run *run, int entry);
void ae_playlist_run_moved(struct ae_playlist_run *run, int from, int to);

/* the order's words IN TURN, SHUFFLE, RANDOM; the lobby row's "4V4 ARENA  ·  SHUFFLE" (the middle dot as UTF-8) */
char const *ae_playlist_order_label(int order);
void ae_playlist_row_text(struct ae_playlist const *list, char *text, int size);
/* the board's value: "7 GAMES  ·  SHUFFLE" */
void ae_playlist_board_text(struct ae_playlist const *list, char *text, int size);

/* ---------- HOST A SERVER */

/* the playlist as a server's settings: rotation ("map:file=GAMETYPE +weight=N, ..."; the weights only in RANDOM), rotation_shuffle
(SHUFFLE and RANDOM: true, IN TURN: false), map_vote (MAP VOTE on, with AE_PLAYLIST_VOTE_CHOICES choices) and vote_no_repeat
(the games the vote keeps out: 2, or 0 with NO REPEATS off). Returns the games written; -1 when the line does not fit size.
Entries a rotation could not hold (a comma in a name) are left out */
struct ae_playlist_host
{
	int shuffle, map_vote, map_vote_choices, vote_no_repeat;
};
int ae_playlist_host_rotation(struct ae_playlist const *list, char *line, int size, struct ae_playlist_host *host);
/* HOST A SERVER's READY TO START line: "Playlist: 4V4 ARENA, 7 games, shuffled" (in turn, shuffled, random) */
void ae_playlist_host_summary(struct ae_playlist const *list, char *text, int size);

#endif
