/*
AE_LISTING_BUILD.H

(0.7.5) An Arena Evolved host says so in its public listing (p2p_lobby.c): the listing's stamp is 8 bytes of which an older
reader takes only the first (the game's kind) and skips the rest, so the stamp's second byte carries the marker 'A' and the
next three the build's version numbers. Nothing about the listing's size or signature changes and the network version is
untouched: an older reader sees the same listing it always saw. A browser asks ae_listing_build_read of a listing's stamp
(AE GAMES). Pure C, no engine includes.
*/

#ifndef AE_LISTING_BUILD_H
#define AE_LISTING_BUILD_H

/* the stamp's bytes after its first (7 of them) from a version text ("0.7.5-beta": three numbers, the rest ignored; none
read: the marker alone) */
void ae_listing_build_write(unsigned char *stamp_after_kind, const char *version);
/* the build a stamp's bytes after its first name: 0 when the marker is not there (an older host's, another game's), else
1 << 24 | major << 16 | minor << 8 | patch (each cut to 255) */
unsigned long ae_listing_build_read(const unsigned char *stamp_after_kind);

#endif
