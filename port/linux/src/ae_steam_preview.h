/*
AE_STEAM_PREVIEW.H

A Steam Workshop item's picture decoded on the desktop (Linux, Windows, macOS), as Android's HaloActivity.steamPreview
does it there: fitted whole into a square on black, opaque, 0xAARRGGBB. Called by the Steam client's preview thread
(port/android/host/steam_downloads.c), never on the game's thread. The bytes are the network's: ae_steam_preview_rules.h
says what is refused before anything is decoded. No file is read or written.
*/

#ifndef AE_STEAM_PREVIEW_H
#define AE_STEAM_PREVIEW_H

#include <stdint.h>

/* a picture's bytes fitted into a side square (malloc'd, side * side pixels: the caller frees it), or NULL and why
in why (a few words, nothing of the picture's address) */
uint32_t *ae_steam_preview_decode(unsigned char const *data, unsigned long size, int side, char *why, int why_size);

#endif
