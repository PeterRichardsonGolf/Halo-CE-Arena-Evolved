/*
AE_MCC_MUSIC_RULES.H

The rules of the menus' MCC music (ae_mcc_music.c), with no game, file or thread: an FMOD sound bank's (FSB5) table of
streams, which of its streams are music and how they make songs, the playlist's order, and the Ogg pages a stream's
Vorbis packets are wrapped in for the decoder. Unit tested: port/linux/tests/ae_mcc_music_rules_test.c.

A bank (version 1 here; version 0's header is 4 bytes longer) is a 60-byte header, the streams' headers, a table of
their names, then their data. Each stream's header is a 64-bit word (more chunks follow, the rate, the channels, where
its data starts in 32-byte units, its length in frames) and then its chunks: a channel count, a rate, and for Vorbis
the CRC-32 of the setup header the encoder used, which the bank leaves out (fsbex's table supplies it). A Vorbis
stream's data is its audio packets, each after its size (16 bits); a size of 0 or 0xFFFF ends it.

In the banks, music is stored as songs cut in pieces: an intro, loops, an outro (the in-game music system plays the
loops in turn), next to one another in the bank and named for their part ("in", "lp1", "loop2", "out", ...); a few
songs are one stream (named ..._nl, "no loop", or for their use). A song here is such a run, each piece played once.
*/

#ifndef __AE_MCC_MUSIC_RULES_H
#define __AE_MCC_MUSIC_RULES_H

#include <stddef.h>
#include <stdint.h>

#define AE_MCC_MUSIC_FSB5_HEADER 64        /* bytes enough to know the whole table's size */
#define AE_MCC_MUSIC_NAME 48
#define AE_MCC_MUSIC_RATE 48000            /* the menus' mix rate: only streams at it are used */
#define AE_MCC_MUSIC_SHORTEST_SONG 30      /* seconds: a shorter run is a sting, not a song */

struct ae_mcc_music_stream
{
	char name[AE_MCC_MUSIC_NAME];
	unsigned channels, rate;
	uint32_t frames;
	uint32_t setup_crc;                    /* 0: none given (not Vorbis, or no chunk) */
	uint64_t offset, size;                 /* in the file */
};

struct ae_mcc_music_song
{
	unsigned first, count;                 /* streams first .. first + count - 1 of one bank */
	uint32_t frames;
};

enum
{
	AE_MCC_MUSIC_ROLE_NONE,
	AE_MCC_MUSIC_ROLE_INTRO,
	AE_MCC_MUSIC_ROLE_LOOP,
	AE_MCC_MUSIC_ROLE_OUTRO,
	AE_MCC_MUSIC_ROLE_WHOLE
};

/* the size of the bank's header, streams' headers and names (what to read before the data), from its first
AE_MCC_MUSIC_FSB5_HEADER bytes; 0 when it is not a Vorbis bank this reads */
size_t ae_mcc_music_fsb5_table_size(unsigned char const *head, size_t size);
/* the streams of a bank, from its table (table_size bytes) and the file's size; returns how many were written
(at most maximum), or -1 for a damaged table. A stream whose data runs past the file is left out */
int ae_mcc_music_fsb5_streams(unsigned char const *table, size_t table_size, uint64_t file_size,
	struct ae_mcc_music_stream *streams, int maximum);
/* a stream's part in a song, from its name */
int ae_mcc_music_role(char const *name);
/* whether a stream's source path (a bank's .info: one 280-byte record a stream, the path at +24) is music: under
sound\\music\\, in a \\music\\ folder, or a cinematic's track named for its music; not a level's ambience. size: the
path's bytes at most */
int ae_mcc_music_path_is_music(char const *path, size_t size);
/* whether a stream can be played: stereo Vorbis at AE_MCC_MUSIC_RATE whose setup header is one of known (count) */
int ae_mcc_music_playable(struct ae_mcc_music_stream const *stream, uint32_t const *known, int known_count);
/* the songs of a bank's streams, in bank order; returns how many (at most maximum) */
int ae_mcc_music_songs(struct ae_mcc_music_stream const *streams, int count, uint32_t const *known, int known_count,
	struct ae_mcc_music_song *songs, int maximum);
/* whether two songs are the same music (a bank that carries another game's songs again): their pieces' lengths */
int ae_mcc_music_same_song(struct ae_mcc_music_stream const *a_streams, struct ae_mcc_music_song const *a,
	struct ae_mcc_music_stream const *b_streams, struct ae_mcc_music_song const *b);
/* order[0..count-1] the play order: shuffled (by seed) or in order */
void ae_mcc_music_order(int *order, int count, int shuffled, uint32_t seed);

/* a Vorbis stream's data (its sized packets) as an Ogg stream the decoder opens: the identification header made for
channels and rate, an empty comment header, setup (setup_size bytes), then the packets. Writes at most capacity bytes;
returns the Ogg stream's size, or 0 when it does not fit or the data is damaged. ae_mcc_music_ogg_capacity says how
much room a stream's data can need */
size_t ae_mcc_music_ogg_capacity(size_t data_size, size_t setup_size);
size_t ae_mcc_music_ogg(unsigned char const *data, size_t data_size, unsigned channels, unsigned rate, uint32_t frames,
	unsigned char const *setup, size_t setup_size, unsigned char *out, size_t capacity);

#endif
