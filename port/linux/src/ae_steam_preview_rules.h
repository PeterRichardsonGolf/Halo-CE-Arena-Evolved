/*
AE_STEAM_PREVIEW_RULES.H

A Steam Workshop item's picture on the desktop (WORKSHOP MAPS' card): what is decoded and how it is fitted. Pure: stdlib
only (unit test: port/linux/tests/ae_steam_preview_rules_test.c). The bytes come from the network and anyone can publish
an item, so a picture is taken only as JPEG or PNG by its first bytes, only within AE_STEAM_PREVIEW_MAXIMUM_BODY bytes,
and only with a size within the limits below, all checked before anything is decoded (ae_steam_preview.c).
*/

#ifndef AE_STEAM_PREVIEW_RULES_H
#define AE_STEAM_PREVIEW_RULES_H

#include <stdint.h>

/* the most bytes of a picture's body fetched and decoded */
#define AE_STEAM_PREVIEW_MAXIMUM_BODY (2UL * 1024UL * 1024UL)
/* the widest and tallest picture decoded, in pixels, and the most pixels in all */
#define AE_STEAM_PREVIEW_MAXIMUM_SIDE 2048
#define AE_STEAM_PREVIEW_MAXIMUM_PIXELS (4UL * 1024UL * 1024UL)
/* the most scans (start-of-scan markers, FF DA) a JPEG may hold: each scan is a pass over every block of the picture */
#define AE_STEAM_PREVIEW_MAXIMUM_SCANS 100
/* the largest square fitted into (the client's PREVIEW_SIDE is 256) */
#define AE_STEAM_PREVIEW_MAXIMUM_FIT 1024

/* the kinds of picture taken */
enum { AE_STEAM_PREVIEW_NONE = 0, AE_STEAM_PREVIEW_JPEG = 1, AE_STEAM_PREVIEW_PNG = 2 };

/* the answers: 0 taken, else why it is refused */
enum
{
	AE_STEAM_PREVIEW_TAKEN = 0,
	AE_STEAM_PREVIEW_EMPTY,        /* no bytes */
	AE_STEAM_PREVIEW_TOO_LARGE,    /* more bytes than AE_STEAM_PREVIEW_MAXIMUM_BODY */
	AE_STEAM_PREVIEW_NOT_A_PICTURE, /* neither JPEG nor PNG by its first bytes */
	AE_STEAM_PREVIEW_BAD_SIZE,     /* a side of 0, below it, or over AE_STEAM_PREVIEW_MAXIMUM_SIDE */
	AE_STEAM_PREVIEW_TOO_MANY_PIXELS, /* width times height over AE_STEAM_PREVIEW_MAXIMUM_PIXELS */
	AE_STEAM_PREVIEW_TOO_MANY_SCANS, /* a JPEG with more scans than AE_STEAM_PREVIEW_MAXIMUM_SCANS */
	AE_STEAM_PREVIEW_EMPTY_DATA,     /* a PNG with an IDAT chunk of no bytes (stb_image copies it from no buffer) */
	AE_STEAM_PREVIEW_CUT_SHORT       /* a PNG whose chunks do not reach a whole IEND */
};

/* the kind of a picture by its first bytes (JPEG: FF D8 FF; PNG: its 8-byte signature) */
int ae_steam_preview_kind(unsigned char const *data, unsigned long size);
/* the start-of-scan markers (FF DA) in a body: exact for a JPEG, whose entropy-coded data follows every FF with 00 or
a restart marker */
unsigned long ae_steam_preview_jpeg_scans(unsigned char const *data, unsigned long size);
/* whether a PNG's chunks, read up to IEND or as far as the body holds them whole, include an IDAT of no bytes */
int ae_steam_preview_png_empty_data(unsigned char const *data, unsigned long size);
/* whether a PNG's chunks, each whole (length, type, bytes, CRC), reach an IEND chunk */
int ae_steam_preview_png_whole(unsigned char const *data, unsigned long size);
/* whether a body is decoded at all: AE_STEAM_PREVIEW_TAKEN, EMPTY, TOO_LARGE, NOT_A_PICTURE, TOO_MANY_SCANS,
EMPTY_DATA or CUT_SHORT */
int ae_steam_preview_check_body(unsigned char const *data, unsigned long size);
/* whether a picture of this size (as its header says) is decoded: TAKEN, BAD_SIZE or TOO_MANY_PIXELS */
int ae_steam_preview_check_size(int width, int height);
/* a refusal's words for the log ("" for TAKEN) */
char const *ae_steam_preview_reason(int answer);

/* where a width by height picture lies, fitted whole into a side square and centred: its left, top, width and height
(at least 1 each); 0 when width, height or side is out of the limits above */
int ae_steam_preview_fit(int width, int height, int side, int *left, int *top, int *fit_width, int *fit_height);
/* a width by height RGBA picture (4 bytes a pixel, rows top first) fitted into a side square on black, as
ae_steam_preview_fit places it: each pixel the mean of the picture's pixels it covers, put on black by its alpha, and
opaque, as 0xAARRGGBB into out (side * side of them); 0 (out left as it was) when the sizes are out of the limits */
int ae_steam_preview_fit_pixels(unsigned char const *rgba, int width, int height, int side, uint32_t *out);

#endif
