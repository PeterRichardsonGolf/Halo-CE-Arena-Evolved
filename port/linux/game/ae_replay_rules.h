/*
AE_REPLAY_RULES.H

A replay file's pure rules (network_replay.c writes the files, the playback reads them): the header's bytes written and
read back with every field bounded, the records' headers taken one at a time with their sizes checked against what is
left, the replays folder's own file names, which recordings are removed over the count and size caps, and whether
the disk has room. No engine header: unit test port/linux/tests/ae_replay_rules_test.c.

The file: a header of AE_REPLAY_HEADER_SIZE bytes, then one zlib stream of records, each AE_REPLAY_RECORD_HEADER_SIZE
bytes (its kind, a byte left 0, its size in 16 bits and its tick in 32, little-endian) and its bytes.
*/

#ifndef AE_REPLAY_RULES_H
#define AE_REPLAY_RULES_H

#include <stddef.h>

#define AE_REPLAY_MAGIC "OCREPLAY"
#define AE_REPLAY_FORMAT 5
#define AE_REPLAY_HEADER_SIZE 384
#define AE_REPLAY_RECORD_HEADER_SIZE 8
#define AE_REPLAY_EXTENSION ".ocreplay"
/* the header's text fields: where each is and how many bytes it has (its end within them) */
#define AE_REPLAY_MAP_OFFSET 40
#define AE_REPLAY_MAP_SIZE 128
#define AE_REPLAY_VARIANT_OFFSET 168
#define AE_REPLAY_VARIANT_SIZE 16
#define AE_REPLAY_VERSION_OFFSET 184
#define AE_REPLAY_VERSION_SIZE 96
#define AE_REPLAY_LENGTH_OFFSET 280
#define AE_REPLAY_FLAGS_OFFSET 284
#define AE_REPLAY_SERVER_OFFSET 288
#define AE_REPLAY_SERVER_SIZE 64
#define AE_REPLAY_PLAYER_OFFSET 352
#define AE_REPLAY_PLAYER_SIZE 32
/* flags: a client recorded it */
#define AE_REPLAY_FLAG_CLIENT 1
/* the game's ticks a second, and the longest game a header may say (a day) */
#define AE_REPLAY_TICKS_PER_SECOND 30
#define AE_REPLAY_LONGEST_TICKS (24L * 60L * 60L * AE_REPLAY_TICKS_PER_SECOND)
/* the latest start tick a header may carry: a game's clock stays under 0x3FFFFFFF (as a host's time a client takes)
with the longest game after it */
#define AE_REPLAY_START_TICK_MOST (0x3FFFFFFFUL - (unsigned long)AE_REPLAY_LONGEST_TICKS)
/* a record's bytes at most (its size field's 16 bits) */
#define AE_REPLAY_RECORD_MOST 0xFFFF
/* a recording's name: "<yyyymmdd>-<hhmmss>_<map><extension>", at most this many bytes with its end */
#define AE_REPLAY_NAME_SIZE 96
/* the caps' bounds: recordings kept, the folder's megabytes, and the free megabytes a recording needs */
#define AE_REPLAY_KEPT_LEAST 1
#define AE_REPLAY_KEPT_MOST 500
#define AE_REPLAY_BUDGET_LEAST_MB 100
#define AE_REPLAY_BUDGET_MOST_MB 100000
#define AE_REPLAY_DISK_RESERVE_MB 1024

/* the record kinds */
enum
{
	/* a datagram of the distributed netcode's unreliable messages, as a client is sent one */
	AE_REPLAY_RECORD_DATAGRAM = 1,
	/* a distributed message sent every client reliably */
	AE_REPLAY_RECORD_RELIABLE,
	/* a distributed message sent the recorder's machine alone, reliably */
	AE_REPLAY_RECORD_RELIABLE_TO_MACHINE,
	/* a message of the game's own sent every machine in the game */
	AE_REPLAY_RECORD_GAME_MESSAGE,
	/* the game as the host had it at its first tick (struct network_game) */
	AE_REPLAY_RECORD_GAME,
	/* a client's own players' input of a tick */
	AE_REPLAY_RECORD_OWN_ACTIONS,
	/* how the level was made */
	AE_REPLAY_RECORD_LEVEL_SETUP,
	AE_REPLAY_RECORD_KINDS
};

/* what a header says */
struct ae_replay_header
{
	unsigned long format;
	unsigned long network_version;
	unsigned long build;
	unsigned long ticks_per_second;
	unsigned long start_tick;
	unsigned long header_size;
	unsigned long long time;
	char map[AE_REPLAY_MAP_SIZE + 1];
	char variant[AE_REPLAY_VARIANT_SIZE + 1];
	char version[AE_REPLAY_VERSION_SIZE + 1];
	unsigned long length_ticks;
	unsigned long flags;
	char server[AE_REPLAY_SERVER_SIZE + 1];
	char player[AE_REPLAY_PLAYER_SIZE + 1];
};

/* a header read: what it is */
enum
{
	AE_REPLAY_HEADER_OK,
	/* too short, or not this magic */
	AE_REPLAY_HEADER_NOT_REPLAY,
	/* a format this build does not read */
	AE_REPLAY_HEADER_OTHER_FORMAT,
	/* recorded by a build of another network version */
	AE_REPLAY_HEADER_OTHER_NETWORK,
	/* a field out of its bounds */
	AE_REPLAY_HEADER_DAMAGED
};

/* the header's AE_REPLAY_HEADER_SIZE bytes (the text fields cut to their sizes, printable ASCII, UTF-8 for the server's
and player's names) */
void ae_replay_header_write(unsigned char *bytes, const struct ae_replay_header *header);
/* the size bytes read into header: an AE_REPLAY_HEADER_* answer and why, in a line, into why (any of them may be
NULL). network_version is this build's; the header must say it. */
int ae_replay_header_read(const unsigned char *bytes, size_t size, unsigned long network_version,
	struct ae_replay_header *header, char *why, size_t why_size);

/* a record's header */
struct ae_replay_record
{
	int kind;
	unsigned long size;
	unsigned long tick;
};

enum
{
	AE_REPLAY_RECORD_OK,
	/* the bytes end inside it: more to come, or a file cut short */
	AE_REPLAY_RECORD_NEED_MORE,
	/* not a record: an unknown kind, the byte not 0, a tick before the last or past the game's length */
	AE_REPLAY_RECORD_BAD
};

/* the record at the start of size bytes: an AE_REPLAY_RECORD_* answer; when OK, record says it and its bytes follow
its header. Its tick must be at least last_tick and at most most_tick. */
int ae_replay_record_take(const unsigned char *bytes, size_t size, unsigned long last_tick, unsigned long most_tick,
	struct ae_replay_record *record);
/* a record's header into AE_REPLAY_RECORD_HEADER_SIZE bytes; 0 if the kind or size is out of bounds */
int ae_replay_record_header_write(unsigned char *bytes, int kind, unsigned long size, unsigned long tick);

/* the records' bytes a playback inflates at most, and a file's bytes it opens at most */
#define AE_REPLAY_INFLATED_MOST (512UL * 1024UL * 1024UL)
#define AE_REPLAY_FILE_MOST (128UL * 1024UL * 1024UL)

/* what a whole stream of records holds, once each is checked */
struct ae_replay_stream
{
	unsigned long records;
	unsigned long kinds[AE_REPLAY_RECORD_KINDS];
	unsigned long first_tick;
	unsigned long last_tick;
	/* where the game record's bytes are, and the first record after the game (and its level setup) */
	size_t game_at;
	size_t play_from;
	/* the stream ends inside a record: a file cut short, played to the last whole record */
	int cut;
	/* where the whole records end */
	size_t end;
};

/* every record of the inflated stream checked before any is played: the first the game, of game_size bytes; then at
most one level setup of setup_size; every kind known, the byte left 0, the ticks in order from start_tick and within
the longest game, each message record (datagram, reliable, to the recorder, game message) at least message_least bytes,
a datagram at most datagram_most and every other message at most message_most (what a live host can send), and no
record but those after the first two. 1 and stream filled when it can be played, else 0 and why in a line. */
int ae_replay_stream_check(const unsigned char *bytes, size_t size, unsigned long start_tick, unsigned long game_size,
	unsigned long setup_size, unsigned long message_least, unsigned long datagram_most, unsigned long message_most,
	struct ae_replay_stream *stream, char *why, size_t why_size);

/* the recorded game's own numbers, as a host's settings record is checked (network_client_manager.c's live path):
machines 0..machines_most, players 0..players_most, the most players 1..players_most, the difficulty one of
difficulties. 1, or 0 and why in a line. */
int ae_replay_game_counts_check(long machine_count, long player_count, long maximum_players, long difficulty,
	long machines_most, long players_most, long difficulties, char *why, size_t why_size);
/* the recorded game's players (count of them, each its machine and controller): a player with either out of range
is no player (as the live path skips it); two players of one machine's one controller refuse the game (a machine with
more players than its local ones stops the game as it loads). machines_most at most 256, locals_most at most 8. */
int ae_replay_players_check(const long *machines, const long *controllers, int count, long machines_most,
	long locals_most, char *why, size_t why_size);
/* the map name of the recorded game (size bytes) is one the playback may look up in the maps folders: ended inside
size; only letters, digits and _ - . space \ @ [ ] ( ) +; no "..", '/' or ':'; its last part (after the last '\')
not empty, not only dots and spaces, not a Windows device name (con prn aux nul, com0-9 lpt0-9, whatever follows). */
int ae_replay_map_name_safe(const char *name, size_t size);

/* the level setup record (AE_REPLAY_SETUP_SIZE bytes): byte 0 its layout (2), byte 1 whether MCC's tags were loaded,
byte 2 whether a weapon cache was, bytes 4-7 that cache's serial (little-endian) and bytes 8-15 the first
AE_REPLAY_CACHE_ID_SIZE bytes of its SHA-256. A record from before the cache was written has byte 2 at 0. */
#define AE_REPLAY_SETUP_SIZE 80
#define AE_REPLAY_CACHE_ID_SIZE 8
/* the weapon cache's part of a setup record: serial 0 or id NULL writes none */
void ae_replay_setup_cache_write(unsigned char *setup, unsigned long serial, const unsigned char *id);
/* whether this machine's weapon cache (serial 0 or id NULL: none loaded) is the one the recording's game played with:
1, or 0 and why in a line. A recording that wrote no cache matches any machine. */
int ae_replay_setup_cache_matches(const unsigned char *setup, unsigned long serial, const unsigned char *id, char *why,
	size_t why_size);

/* a recording's name from the time stamp ("20261118-183000") and the map's name (its letters, digits and '-' kept,
the rest '_', "map" when none); 0 if it does not fit */
int ae_replay_name_make(char *name, size_t size, const char *stamp, const char *map);
/* whether a file name is one the recorder makes (the stamp, '_', the map, the extension): only those are removed */
int ae_replay_name_is_own(const char *name);
/* whether a name names a file of the folder (not empty, no '/' or '\\', no "..", ends in the extension, fits) */
int ae_replay_name_is_safe(const char *name);

/* the marker file a KEPT recording has beside it ("<name>.keep"; it does not end in the recordings' extension, so it is no
recording of its own); 0 if the name is not a recording's or the marker does not fit */
#define AE_REPLAY_KEEP_EXTENSION ".keep"
int ae_replay_keep_marker(const char *name, char *marker, size_t size);

/* the setting's value in its bounds */
long ae_replay_kept(long value);
long ae_replay_budget_mb(long value);

/* a file of the folder, for the caps */
struct ae_replay_file
{
	const char *name;
	unsigned long long bytes;
};

/* which of count files to remove before a recording starts, so that the folder keeps at most kept recordings with
the new one and stays under budget_bytes with incoming_bytes more: their indexes, the oldest first (names sort by
time), into picked (most of them); how many. Files whose names are not the recorder's are never picked, nor counted. */
int ae_replay_pick_removals(const struct ae_replay_file *files, int count, long kept, unsigned long long budget_bytes,
	unsigned long long incoming_bytes, int *picked, int most);

/* whether free_bytes leaves the disk reserve (AE_REPLAY_DISK_RESERVE_MB) */
int ae_replay_disk_allows(unsigned long long free_bytes);

/* the recorder's cost on the game thread: ticks counted, their microseconds in all, the most of one */
struct ae_replay_cost
{
	unsigned long ticks;
	unsigned long long total_us;
	unsigned long most_us;
};
void ae_replay_cost_add(struct ae_replay_cost *cost, unsigned long us);
/* the mean of a tick, in microseconds (0 with none) */
unsigned long ae_replay_cost_mean(const struct ae_replay_cost *cost);

#endif
