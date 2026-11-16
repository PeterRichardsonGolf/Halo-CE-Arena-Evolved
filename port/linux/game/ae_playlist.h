/*
AE_PLAYLIST.H

(0.7.6) A hosted playlist, CREATE GAME's PLAYLIST... (mockups-076-ui2 case 7 A): the games a host's lobby plays in turn,
each a map and a gametype with a weight 1-5 (how often it comes up), and the rules between games (MAP VOTE: OFF, or a
vote among 3 games and SAME AGAIN; NO REPEATS). Pure C for the unit test (port/linux/tests/ae_playlist_test.c): the
entries and their weights, the chances, the order a host drags them into, the weighted pick of the next game and the
vote's choices, and the file.

The file is a dedicated server's rotation (port/linux/src/server_rotation.h: entry := map [":" gametype], entries
separated by newlines, # a comment to the line's end), so the same file runs on a server; what a server does not know
rides in comments it ignores:

    # playlist: 4V4 ARENA
    # map vote: 3
    # no repeats: 1
    sali_creek:file=AE TEAM SLY  # weight 3
    sanctuary:file=AE KING  # weight 2

A gametype the lobby saved is written "file=<its name>", as the rotation names a saved gametype.
*/

#ifndef AE_PLAYLIST_H
#define AE_PLAYLIST_H

enum
{
	AE_PLAYLIST_ENTRIES = 64,          /* a rotation's most (server_rotation.h) */
	AE_PLAYLIST_NAME = 32,
	AE_PLAYLIST_MAP = 64,
	AE_PLAYLIST_GAMETYPE = 32,
	AE_PLAYLIST_WEIGHT_MAX = 5,
	AE_PLAYLIST_TEXT = 8192,
	/* the vote's choices: AE_PLAYLIST_VOTE_GAMES games, then SAME AGAIN */
	AE_PLAYLIST_VOTE_GAMES = 3,
	AE_PLAYLIST_VOTE_CHOICES = AE_PLAYLIST_VOTE_GAMES + 1
};

struct ae_playlist_entry
{
	char map[AE_PLAYLIST_MAP];
	char gametype[AE_PLAYLIST_GAMETYPE];
	short weight;                      /* 1 .. AE_PLAYLIST_WEIGHT_MAX */
};

struct ae_playlist
{
	int on;                            /* the lobby plays it (not saved in the file: the lobby's own switch) */
	char name[AE_PLAYLIST_NAME];
	short count;
	struct ae_playlist_entry entries[AE_PLAYLIST_ENTRIES];
	int map_vote;                      /* 0: no vote, else 1: a vote among games, with SAME AGAIN the last choice */
	int no_repeats;
};

void ae_playlist_init(struct ae_playlist *playlist);
/* an entry at the end (weight 2, as a new one is); the index, -1 when full or the map / gametype is empty */
int ae_playlist_add(struct ae_playlist *playlist, char const *map, char const *gametype);
void ae_playlist_remove(struct ae_playlist *playlist, int index);
/* the weight one step (direction +1 / -1), stopped at 1 and AE_PLAYLIST_WEIGHT_MAX: 1 when it changed */
int ae_playlist_weight_step(struct ae_playlist *playlist, int index, int direction);
/* an entry moved to another place, the ones between sliding over: 1 when it moved */
int ae_playlist_move(struct ae_playlist *playlist, int from, int to);
/* the chance, in percent (weight over the total, rounded; the entries' sum may be 99 or 101) */
int ae_playlist_chance(struct ae_playlist const *playlist, int index);

/* the next game: an entry picked by weight (random: any number), never the one just played (last, -1 for none) while
there is another and NO REPEATS is on; -1 when there are none */
int ae_playlist_pick(struct ae_playlist const *playlist, int last, unsigned long random);
/* the vote's choices after the game at index current: up to AE_PLAYLIST_VOTE_GAMES other entries picked by weight, each
once, then SAME AGAIN (current again) always the last. choices gets entry indices; the count. Without a vote (MAP VOTE
off), or with no game to repeat, 0 */
int ae_playlist_vote_choices(struct ae_playlist const *playlist, int current, unsigned long random, int *choices);
/* whether the vote's choice is SAME AGAIN (the last one, when it is the entry just played) */
int ae_playlist_vote_is_same_again(int count, int choice);

/* the file's text (a server's rotation, with the comments above); its length, -1 when size is too small */
long ae_playlist_write(struct ae_playlist const *playlist, char *text, long size);
/* the entries read from text (comments, blank lines and entries a server would not take are skipped; weights from
"# weight N", else 2); the entries read. The playlist's name and rules from its header comments, kept as they are when
the header has none */
int ae_playlist_read(struct ae_playlist *playlist, char const *text);

#endif
