/*
AE_CAREER_RECORD.H

The career record (0.7.7): a player's rating for each playlist, kept on this PC. Two ladders that never touch: "career"
(offline games against rated bots) and "online" (games against other players). Pure code (no engine headers):
port/linux/tests/ae_career_record_test.c runs it; ae_glue_career.c keeps it in a file.

Each entry is one playlist's standing: points, level (0 .. 49), the highest level reached, games, wins, quits, and the clock
of the game it was last moved in. Plain text, one line for each entry, in this order:

	# AE career
	version = 2
	career = team_slayer | 1450 6 6 12 7 0 31
	online = ffa | 220 2 2 3 1 1 14 | 14
	match = chief | team-slayer | 1712 23 1763201234
	social = chief | team-slayer | 37 70 33 4 3 1763201234
	pending = chief | team-slayer | 1763201234
	checksum = 3fa91c07

career / online: the playlist id, then points level highest games wins quits last, then (when there were quits) the clocks of the
last 16 of them after a second "|".

Version 2 (0.9.3, the offline SOCIAL career): match = the hidden match rating of a profile in a playlist (profile key, playlist
id, then points games last); social = a profile's EXP in a playlist (then exp games wins ties quits last; the losses are what is
left); pending = a game that was started and not yet ended (profile key, playlist id, the start clock; one a profile at most).
The checksum is the last line: data after it, a second checksum, a second header or a line twice (a playlist
on a ladder, a profile and playlist of match / social, a profile of pending) make a file read as edited. A version 1 file reads as before (no such lines). A 0.9.2 reader of a version 2 file skips these lines as unknown, sets newer and
never writes the file back, so a downgrade loses none of them. The checksum (FNV-1a of every byte before
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
	AE_CAREER_VERSION = 2,
	AE_CAREER_ID = 32,
	AE_CAREER_PLAYLISTS = 32,
	/* the last quits kept for each playlist */
	AE_CAREER_QUIT_HISTORY = 16,
	/* the social lines: 4 profiles x 16 playlists of each; one pending game a profile (a few profiles) */
	AE_CAREER_SOCIAL = 64,
	AE_CAREER_PENDING = 8,
	/* a line of the file, the newline in it, at most (the longest one is a ladder line with 16 quit clocks of 20 digits: 512);
	   the reader refuses a longer one */
	AE_CAREER_LINE = 640,
	/* the file's text: every table full of longest lines, the header and the checksum line; ae_career_write still checks its
	   bound and answers -1 when a file does not fit */
	AE_CAREER_TEXT = (2 * AE_CAREER_PLAYLISTS + 2 * AE_CAREER_SOCIAL + AE_CAREER_PENDING + 4) * AE_CAREER_LINE,
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

/* a profile's hidden match rating in a social playlist (0 .. 4999 points: ae_social_match_rules.h) */
struct ae_career_match
{
	char profile[AE_CAREER_ID];
	char playlist[AE_CAREER_ID];
	long points, games;
	unsigned long last;
};

/* a profile's social EXP in a playlist */
struct ae_career_social
{
	char profile[AE_CAREER_ID];
	char playlist[AE_CAREER_ID];
	long exp, games, wins, ties, quits;
	unsigned long last;
};

/* a social game started and not ended */
struct ae_career_pending
{
	char profile[AE_CAREER_ID];
	char playlist[AE_CAREER_ID];
	unsigned long start;
};

struct ae_career
{
	int newer;
	int edited;
	int count[2];
	struct ae_career_entry entry[2][AE_CAREER_PLAYLISTS];
	int match_count, social_count, pending_count;
	struct ae_career_match match[AE_CAREER_SOCIAL];
	struct ae_career_social social[AE_CAREER_SOCIAL];
	struct ae_career_pending pending[AE_CAREER_PENDING];
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

/* ---- the social lines (version 2) ---- */

/* the hidden match rating of a profile in a playlist, NULL for none; the same made (all zero) when there is none, NULL when the
   table is full. A profile key and a playlist id are made safe for the file as ae_career_id does */
struct ae_career_match *ae_career_match_find(struct ae_career *career, char const *profile, char const *playlist);
struct ae_career_match *ae_career_match_entry(struct ae_career *career, char const *profile, char const *playlist);
/* the social EXP line of a profile in a playlist, the same way */
struct ae_career_social *ae_career_social_find(struct ae_career *career, char const *profile, char const *playlist);
struct ae_career_social *ae_career_social_entry(struct ae_career *career, char const *profile, char const *playlist);
/* the sum of a profile's EXP over every playlist */
long ae_career_social_total_exp(struct ae_career const *career, char const *profile);
/* a game started: the profile's pending line set (the one it had is replaced); 0 when the table is full */
int ae_career_pending_set(struct ae_career *career, char const *profile, char const *playlist, unsigned long clock);
/* the profile's pending line, NULL for none; it cleared (1 when there was one) */
struct ae_career_pending *ae_career_pending_find(struct ae_career *career, char const *profile);
int ae_career_pending_clear(struct ae_career *career, char const *profile);
/* whether a profile's playlist has room for both its lines (EXP and match rating): a line it has, or a table with a free place */
int ae_career_social_has_room(struct ae_career const *career, char const *profile, char const *playlist);
/* a game counted at its end, both lines or neither: the EXP line as ae_career_social_record, and the hidden match rating moved to
   match_points (clamped), the game counted. 0 and nothing changed when either table is full */
int ae_career_social_commit(struct ae_career *career, char const *profile, char const *playlist, int outcome, long match_points,
	unsigned long clock);
/* the games started and never ended, found at a start, settled: with crashed the crash handler's mark, every pending game is
   forgiven (taken away, nothing counted); without it each is counted as a quit (-1 EXP, a quit, the hidden rating moved as a
   loss to opponents of its own level). Every pending line is taken away. forgiven / quits (NULL allowed) get the counts; a game
   whose lines did not fit a full table counts as forgiven */
void ae_career_pending_settle(struct ae_career *career, int crashed, unsigned long clock, int *forgiven, int *quits);
/* a game counted at its end: the EXP moved by the outcome's change (ae_social_rules.h: never below 0), the game counted, and a
   win / tie / quit counted; the profile's pending line cleared. outcome is AE_SOCIAL_WIN, _TIE, _QUIT or _LOSS. 0 when the
   table is full */
int ae_career_social_record(struct ae_career *career, char const *profile, char const *playlist, int outcome,
	unsigned long clock);
/* a profile's social lines taken away: one playlist's (match, EXP and, with it, nothing else), or every playlist's when playlist is
   NULL, and the profile's pending line with them; how many lines went */
int ae_career_social_reset(struct ae_career *career, char const *profile, char const *playlist);
/* the match rating moved to points after a game (a quit counts as a game) */
int ae_career_match_record(struct ae_career *career, char const *profile, char const *playlist, long points, unsigned long clock);

/* the file's text into buffer: its length, -1 when it does not fit */
long ae_career_write(struct ae_career const *career, char *buffer, long size);
/* a file's text read (the record made empty first): AE_CAREER_READ_OK, _EDITED (checksum wrong or missing: empty) or _BAD (no header) */
int ae_career_read(struct ae_career *career, char const *text);

#endif
