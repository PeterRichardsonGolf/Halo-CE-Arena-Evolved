# fsbex: Vorbis setup headers

FMOD's sound banks hold Vorbis audio without its setup header: each stream names
its header by a CRC-32, and a decoder supplies the header itself. fsbex, by
astral4, carries those headers as a lookup table, MIT or Apache-2.0 licensed as
the user chooses (`LICENSE` is its MIT text).

Upstream: https://github.com/astral4/fsbex, `src/encode/vorbis_lookup.rs`.

`port/linux/game/ae_mcc_music_fsbex.h` holds two of its headers unchanged, `FVS_AD11D38D` and
`FVS_632B1FAC`, the ones the music in an MCC installation's sound banks names
(the menus' music, `port/linux/game/ae_mcc_music.c`). It was made by taking those
two arrays' bytes out of `vorbis_lookup.rs` and writing them as C arrays.
