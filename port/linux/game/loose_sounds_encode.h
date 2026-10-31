/*
LOOSE_SOUNDS_ENCODE.H

Arena Evolved: the encoder of loose_sounds.c (loose_sounds_encode.c), which
OpenCE declares in its custom_edition_cache.h.
*/

#ifndef LOOSE_SOUNDS_ENCODE_H
#define LOOSE_SOUNDS_ENCODE_H

/* The samples `data` (`data_bytes` of them) of a sound permutation of
`compression` (16-bit PCM, little-endian or `big_endian`; Xbox ADPCM; Ogg
Vorbis), `channels` channels at `rate`, encoded as Xbox ADPCM at
`encoded_rate`: a buffer of the game's allocator of *encoded_bytes; NULL
when they cannot be (loose_sounds.c) */
byte *custom_edition_sounds_encode(
	byte const *data,
	long data_bytes,
	short compression,
	boolean big_endian,
	long channels,
	long rate,
	long encoded_rate,
	unsigned long *encoded_bytes);

#endif
