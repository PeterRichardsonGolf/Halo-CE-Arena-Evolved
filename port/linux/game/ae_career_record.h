/*
AE_CAREER_RECORD.H

The career record (0.7.7): a player's rating for each playlist, kept on this PC. Two ladders that never touch: "career"
(offline games against rated bots) and "online" (games against other players). Pure code (no engine headers):
port/linux/tests/ae_career_record_test.c runs it; ae_glue_career.c keeps it in a file.

Each entry is one playlist's standing: points, level (0 .. 49), the highest level reached, games, wins, quits, and the clock
of the game it was last moved in. Plain text, one line for each entry, in this order:

	# AE career
	version = 1
	career = team_slayer | 1450 6 6 12 7 0 31
	online = ffa | 220 2 2 3 1 1 14 | 14
	checksum = 3fa91c07

career / online: the playlist id, then points level highest games wins quits last, then (when there were quits) the clocks of the
last 16 of them after a second "|". The checksum (FNV-1a of every byte before
its line) guards against hand edits: a file whose checksum does not match, or that has none, is read as empty and flagged
edited so the glue keeps it aside. A reader skips lines of a kind it does not know and fields past the ones it knows, and a
version past its own sets newer (the glue then never writes the file back). A file with only the header is an empty record.
*/

#ifndef __AE_CAREER_RECORD_H
#define __AE_CAREER_RECORD_H

#include "ae_rating_result.h"
#include "ae_rating_rules.h"

enum
{
	AE_CAREER_VERSION = 1,
	AE_CAREER_ID = 32,
	AE_CAREER_PLAYLISTS = 32,
	AE_CAREER_TEXT = 24 * 1024,
	/* the last quits kept for each playlist */
	AE_CAREER_QUIT_HISTORY = 16,
	/* the ladders */
	AE_CAREER_OFFLINE = 0,
	AE_CAREER_ONLINE = 1,
	/* ae_career_read's answers */
	AE_CAREER_READ_BAD = 0,
	AE_CAREER_READ_OK = 1,
	AE_CAREER_READ_EDITED = 2
};

struct ae_career_entry
{
	char playlist[AE_CAREER_ID];
	struct ae_rating_state rating;
	long games, wins, quits;
	unsigned long last;
	/* the clocks of the last quits, oldest first (data only: nothing here refuses a player) */
	int quit_stamps;
	unsigned long quit_stamp[AE_CAREER_QUIT_HISTORY];
};

struct ae_career
{
	int newer;
	int edited;
	int count[2];
	struct ae_career_entry entry[2][AE_CAREER_PLAYLISTS];
};

void ae_career_init(struct ae_career *career);
/* an id made safe for the file: "|" and control characters turned to "?", cut to the id's size */
void ae_career_id(char const *playlist, char *id, size_t size);
/* the entry of a playlist on a ladder, NULL for none */
struct ae_career_entry *ae_career_find(struct ae_career *career, int ladder, char const *playlist);
/* the same, made (all zero) when there is none; NULL when the ladder is full */
struct ae_career_entry *ae_career_entry(struct ae_career *career, int ladder, char const *playlist);
/* a finished rated game: the points moved by change, the game counted (a win when won) */
int ae_career_record_game(struct ae_career *career, int ladder, char const *playlist, long change, int won, unsigned long clock);
/* a game left before its end: the quit counted, and the game; the points moved by change (the loss of ae_rating_quit_change) */
int ae_career_record_quit(struct ae_career *career, int ladder, char const *playlist, long change, unsigned long clock);
/* a finished rated game of the result record, for the player at index self: the points moved by the result's change (quit
   protection), the game counted, a win for first place that did not quit, the quit counted and stamped when the result says so.
   0 for a player not rated or not in it */
int ae_career_record_result(struct ae_career *career, int ladder, struct ae_rating_result const *result, int self,
	unsigned long clock);
/* the highest level over every playlist of a ladder: the player's overall level (0 for none) */
int ae_career_overall_level(struct ae_career const *career, int ladder);

/* the file's text into buffer: its length, -1 when it does not fit */
long ae_career_write(struct ae_career const *career, char *buffer, long size);
/* a file's text read (the record made empty first): AE_CAREER_READ_OK, _EDITED (checksum wrong or missing: empty) or _BAD (no header) */
int ae_career_read(struct ae_career *career, char const *text);

#endif
