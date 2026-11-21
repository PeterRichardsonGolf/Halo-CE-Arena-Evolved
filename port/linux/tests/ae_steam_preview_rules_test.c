/* ae_steam_preview_rules.c: which Workshop pictures are decoded (their first bytes, body size, sides and pixel count)
and how one is fitted into the card's square */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_steam_preview_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static unsigned char const png_signature[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
static unsigned char const jpeg_start[4] = { 0xff, 0xd8, 0xff, 0xe0 };

int main(void)
{
	unsigned char *big;
	unsigned char gif[6] = { 'G', 'I', 'F', '8', '9', 'a' };
	unsigned char rgba[4 * 4 * 4];
	uint32_t *out;
	int left, top, width, height, i;

	/* the kinds, by their first bytes alone */
	CHECK(ae_steam_preview_kind(png_signature, sizeof(png_signature)) == AE_STEAM_PREVIEW_PNG);
	CHECK(ae_steam_preview_kind(jpeg_start, sizeof(jpeg_start)) == AE_STEAM_PREVIEW_JPEG);
	CHECK(ae_steam_preview_kind(jpeg_start, 3) == AE_STEAM_PREVIEW_JPEG);
	CHECK(ae_steam_preview_kind(jpeg_start, 2) == AE_STEAM_PREVIEW_NONE);
	CHECK(ae_steam_preview_kind(png_signature, 7) == AE_STEAM_PREVIEW_NONE);
	CHECK(ae_steam_preview_kind(gif, sizeof(gif)) == AE_STEAM_PREVIEW_NONE);
	CHECK(ae_steam_preview_kind(NULL, 100) == AE_STEAM_PREVIEW_NONE);
	CHECK(ae_steam_preview_kind((unsigned char const *)"<html>", 6) == AE_STEAM_PREVIEW_NONE);

	/* the body: none, too large (whatever its first bytes), not a picture */
	CHECK(ae_steam_preview_check_body(NULL, 0) == AE_STEAM_PREVIEW_EMPTY);
	CHECK(ae_steam_preview_check_body(png_signature, 0) == AE_STEAM_PREVIEW_EMPTY);
	CHECK(ae_steam_preview_check_body(gif, sizeof(gif)) == AE_STEAM_PREVIEW_NOT_A_PICTURE);
	CHECK(ae_steam_preview_check_body(png_signature, sizeof(png_signature)) == AE_STEAM_PREVIEW_CUT_SHORT);
	big = calloc(1, AE_STEAM_PREVIEW_MAXIMUM_BODY + 1);
	CHECK(big != NULL);
	if (big)
	{
		memcpy(big, "\xff\xd8\xff", 3);
		CHECK(ae_steam_preview_check_body(big, AE_STEAM_PREVIEW_MAXIMUM_BODY) == AE_STEAM_PREVIEW_TAKEN);
		CHECK(ae_steam_preview_check_body(big, AE_STEAM_PREVIEW_MAXIMUM_BODY + 1) == AE_STEAM_PREVIEW_TOO_LARGE);
		CHECK(ae_steam_preview_check_body(big, 0xffffffffUL) == AE_STEAM_PREVIEW_TOO_LARGE);
		free(big);
	}

	/* a JPEG's scans, counted exactly by their markers: 0, 1, 100 taken, 101 refused before decoding */
	big = calloc(1, 4 + 2 * 101);
	CHECK(big != NULL);
	if (big)
	{
		size_t at;

		memcpy(big, jpeg_start, 4);
		CHECK(ae_steam_preview_jpeg_scans(big, 4) == 0);
		CHECK(ae_steam_preview_check_body(big, 4) == AE_STEAM_PREVIEW_TAKEN);
		for (at = 4; at < 4 + 2 * 101; at += 2)
		{
			big[at] = 0xff;
			big[at + 1] = 0xda;
		}
		CHECK(ae_steam_preview_jpeg_scans(big, 4 + 2) == 1);
		CHECK(ae_steam_preview_check_body(big, 4 + 2) == AE_STEAM_PREVIEW_TAKEN);
		CHECK(ae_steam_preview_jpeg_scans(big, 4 + 2 * 100) == 100);
		CHECK(ae_steam_preview_check_body(big, 4 + 2 * 100) == AE_STEAM_PREVIEW_TAKEN);
		CHECK(ae_steam_preview_jpeg_scans(big, 4 + 2 * 101) == 101);
		CHECK(ae_steam_preview_check_body(big, 4 + 2 * 101) == AE_STEAM_PREVIEW_TOO_MANY_SCANS);
		/* a body ending in FF (its DA cut off) counts nothing for it; a fill FF before a marker counts once */
		CHECK(ae_steam_preview_jpeg_scans(big, 4 + 2 * 100 + 1) == 100);
		CHECK(ae_steam_preview_jpeg_scans((unsigned char const *)"\xff\xff\xda", 3) == 1);
		CHECK(ae_steam_preview_jpeg_scans((unsigned char const *)"\xff\x00\xda", 3) == 0);
		CHECK(ae_steam_preview_jpeg_scans((unsigned char const *)"\xda\xff", 2) == 0);
		CHECK(ae_steam_preview_jpeg_scans(NULL, 10) == 0);
		/* a PNG's bytes are not counted: FF DA is no marker there (this one has no chunks: cut short) */
		memcpy(big, png_signature, sizeof(png_signature));
		CHECK(ae_steam_preview_check_body(big, 4 + 2 * 101) == AE_STEAM_PREVIEW_CUT_SHORT);
		free(big);
	}
	CHECK(strlen(ae_steam_preview_reason(AE_STEAM_PREVIEW_TOO_MANY_SCANS)) > 0);

	/* a PNG's chunks: an IDAT of no bytes refused wherever it is; one with bytes, a cut-short chunk, or one after IEND
	is not (stb_image refuses or never reads those) */
	{
		static unsigned char const ihdr[] = { 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 1, 0, 0, 0, 1, 8, 6, 0, 0, 0,
			1, 2, 3, 4 };
		static unsigned char const empty_idat[] = { 0, 0, 0, 0, 'I', 'D', 'A', 'T', 1, 2, 3, 4 };
		static unsigned char const idat[] = { 0, 0, 0, 2, 'I', 'D', 'A', 'T', 0x78, 0x01, 1, 2, 3, 4 };
		static unsigned char const iend[] = { 0, 0, 0, 0, 'I', 'E', 'N', 'D', 1, 2, 3, 4 };
		static unsigned char const huge[] = { 0xff, 0xff, 0xff, 0xf0, 't', 'E', 'X', 't', 1, 2, 3, 4 };
		unsigned char body[128];
		size_t size;

#define PUT(chunk) (memcpy(body + size, chunk, sizeof(chunk)), size += sizeof(chunk))
		size = 0;
		PUT(png_signature); PUT(ihdr); PUT(idat); PUT(iend);
		CHECK(!ae_steam_preview_png_empty_data(body, size));
		CHECK(ae_steam_preview_check_body(body, size) == AE_STEAM_PREVIEW_TAKEN);
		size = 0;
		PUT(png_signature); PUT(ihdr); PUT(empty_idat); PUT(iend);
		CHECK(ae_steam_preview_png_empty_data(body, size));
		CHECK(ae_steam_preview_check_body(body, size) == AE_STEAM_PREVIEW_EMPTY_DATA);
		size = 0;
		PUT(png_signature); PUT(ihdr); PUT(idat); PUT(empty_idat);
		CHECK(ae_steam_preview_check_body(body, size) == AE_STEAM_PREVIEW_EMPTY_DATA);
		/* (cut inside the empty IDAT's CRC: a chunk not whole is not read) */
		CHECK(!ae_steam_preview_png_empty_data(body, size - 1));
		size = 0;
		PUT(png_signature); PUT(ihdr); PUT(iend); PUT(empty_idat);
		CHECK(!ae_steam_preview_png_empty_data(body, size));
		size = 0;
		PUT(png_signature); PUT(huge); PUT(empty_idat);
		CHECK(!ae_steam_preview_png_empty_data(body, size));
		CHECK(!ae_steam_preview_png_empty_data(body, 3));
		CHECK(!ae_steam_preview_png_empty_data(NULL, 100));
		CHECK(strlen(ae_steam_preview_reason(AE_STEAM_PREVIEW_EMPTY_DATA)) > 0);
		/* a PNG is whole when its chunks reach an IEND with its CRC; one that ends on an empty IDAT with no CRC, which
		the walk for an empty IDAT does not read, is refused as cut short */
		size = 0;
		PUT(png_signature); PUT(ihdr); PUT(idat); PUT(iend);
		CHECK(ae_steam_preview_png_whole(body, size));
		CHECK(!ae_steam_preview_png_whole(body, size - 1));
		CHECK(ae_steam_preview_check_body(body, size - 1) == AE_STEAM_PREVIEW_CUT_SHORT);
		size = 0;
		PUT(png_signature); PUT(ihdr); PUT(idat); PUT(empty_idat);
		CHECK(!ae_steam_preview_png_empty_data(body, size - 4));
		CHECK(!ae_steam_preview_png_whole(body, size - 4));
		CHECK(ae_steam_preview_check_body(body, size - 4) == AE_STEAM_PREVIEW_CUT_SHORT);
		CHECK(ae_steam_preview_check_body(body, size - 1) == AE_STEAM_PREVIEW_CUT_SHORT);
		size = 0;
		PUT(png_signature); PUT(huge); PUT(iend);
		CHECK(!ae_steam_preview_png_whole(body, size));
		CHECK(!ae_steam_preview_png_whole(body, sizeof(png_signature)));
		CHECK(!ae_steam_preview_png_whole(body, 3));
		CHECK(!ae_steam_preview_png_whole(NULL, 100));
		CHECK(strlen(ae_steam_preview_reason(AE_STEAM_PREVIEW_CUT_SHORT)) > 0);
#undef PUT
	}

	/* the size its header claims: sides, then the pixels in all */
	CHECK(ae_steam_preview_check_size(1, 1) == AE_STEAM_PREVIEW_TAKEN);
	CHECK(ae_steam_preview_check_size(0, 10) == AE_STEAM_PREVIEW_BAD_SIZE);
	CHECK(ae_steam_preview_check_size(10, 0) == AE_STEAM_PREVIEW_BAD_SIZE);
	CHECK(ae_steam_preview_check_size(-1, 10) == AE_STEAM_PREVIEW_BAD_SIZE);
	CHECK(ae_steam_preview_check_size(2048, 1) == AE_STEAM_PREVIEW_TAKEN);
	CHECK(ae_steam_preview_check_size(2049, 1) == AE_STEAM_PREVIEW_BAD_SIZE);
	CHECK(ae_steam_preview_check_size(1, 2049) == AE_STEAM_PREVIEW_BAD_SIZE);
	CHECK(ae_steam_preview_check_size(1920, 1080) == AE_STEAM_PREVIEW_TAKEN);
	CHECK(ae_steam_preview_check_size(4096, 1) == AE_STEAM_PREVIEW_BAD_SIZE);
	CHECK(ae_steam_preview_check_size(65535, 65535) == AE_STEAM_PREVIEW_BAD_SIZE);
	CHECK(ae_steam_preview_check_size(0x7fffffff, 0x7fffffff) == AE_STEAM_PREVIEW_BAD_SIZE);
	/* (the pixel count's limit is the square of the side's: a guard should either move) */
	CHECK(ae_steam_preview_check_size(2048, 2048) == AE_STEAM_PREVIEW_TAKEN);
	CHECK((unsigned long)AE_STEAM_PREVIEW_MAXIMUM_SIDE * AE_STEAM_PREVIEW_MAXIMUM_SIDE <= AE_STEAM_PREVIEW_MAXIMUM_PIXELS);
	CHECK(!strcmp(ae_steam_preview_reason(AE_STEAM_PREVIEW_TAKEN), ""));
	CHECK(strlen(ae_steam_preview_reason(AE_STEAM_PREVIEW_TOO_MANY_PIXELS)) > 0);
	CHECK(strlen(ae_steam_preview_reason(99)) > 0);

	/* the fit: whole, centred, at least a pixel each way */
	CHECK(ae_steam_preview_fit(400, 225, 256, &left, &top, &width, &height));
	CHECK(left == 0 && width == 256 && height == 144 && top == 56);
	CHECK(ae_steam_preview_fit(225, 400, 256, &left, &top, &width, &height));
	CHECK(top == 0 && height == 256 && width == 144 && left == 56);
	CHECK(ae_steam_preview_fit(1, 1, 256, &left, &top, &width, &height));
	CHECK(left == 0 && top == 0 && width == 256 && height == 256);
	CHECK(ae_steam_preview_fit(1, 2048, 256, &left, &top, &width, &height));
	CHECK(width == 1 && height == 256 && left == 127 && top == 0);
	CHECK(!ae_steam_preview_fit(0, 10, 256, &left, &top, &width, &height));
	CHECK(!ae_steam_preview_fit(2049, 10, 256, &left, &top, &width, &height));
	CHECK(!ae_steam_preview_fit(10, 10, 0, &left, &top, &width, &height));
	CHECK(!ae_steam_preview_fit(10, 10, AE_STEAM_PREVIEW_MAXIMUM_FIT + 1, &left, &top, &width, &height));

	out = malloc(sizeof(*out) * 256 * 256);
	CHECK(out != NULL);
	if (out)
	{
		/* a 1x1 picture fills the square; transparent is black; every pixel opaque */
		rgba[0] = 200; rgba[1] = 100; rgba[2] = 50; rgba[3] = 255;
		CHECK(ae_steam_preview_fit_pixels(rgba, 1, 1, 256, out));
		CHECK(out[0] == 0xffc86432u && out[256 * 256 - 1] == 0xffc86432u && out[128 * 256 + 7] == 0xffc86432u);
		rgba[3] = 0;
		CHECK(ae_steam_preview_fit_pixels(rgba, 1, 1, 256, out));
		CHECK(out[0] == 0xff000000u && out[300] == 0xff000000u);
		rgba[3] = 128;
		CHECK(ae_steam_preview_fit_pixels(rgba, 1, 1, 256, out));
		CHECK(out[0] == 0xff643219u);

		/* 2x1, white then black: letterboxed on black, white on the left half */
		memset(rgba, 0, sizeof(rgba));
		rgba[0] = rgba[1] = rgba[2] = rgba[3] = 255;
		rgba[7] = 255;
		CHECK(ae_steam_preview_fit_pixels(rgba, 2, 1, 256, out));
		CHECK(out[0] == 0xff000000u);                /* the bar above */
		CHECK(out[64 * 256 + 0] == 0xffffffffu);     /* the picture's first row, left */
		CHECK(out[64 * 256 + 127] == 0xffffffffu);
		CHECK(out[64 * 256 + 128] == 0xff000000u);
		CHECK(out[191 * 256 + 5] == 0xffffffffu);    /* its last row */
		CHECK(out[192 * 256 + 5] == 0xff000000u);    /* the bar below */

		/* 4x4 into 2x2: each the mean of its 2x2 block (red 0, 2, 4, 6 ... by place) */
		for (i = 0; i < 16; i++)
		{
			rgba[4 * i] = (unsigned char)(10 * i);
			rgba[4 * i + 1] = 0;
			rgba[4 * i + 2] = 0;
			rgba[4 * i + 3] = 255;
		}
		CHECK(ae_steam_preview_fit_pixels(rgba, 4, 4, 2, out));
		/* (0 + 10 + 40 + 50) / 4 = 25; (20 + 30 + 60 + 70) / 4 = 45; 105; 125 */
		CHECK(out[0] == 0xff190000u && out[1] == 0xff2d0000u && out[2] == 0xff690000u && out[3] == 0xff7d0000u);

		/* refused sizes leave out alone */
		out[0] = 0x12345678u;
		CHECK(!ae_steam_preview_fit_pixels(rgba, 0, 4, 2, out));
		CHECK(!ae_steam_preview_fit_pixels(rgba, 4, 2049, 2, out));
		CHECK(!ae_steam_preview_fit_pixels(NULL, 4, 4, 2, out));
		CHECK(!ae_steam_preview_fit_pixels(rgba, 4, 4, 2, NULL));
		CHECK(out[0] == 0x12345678u);
		free(out);
	}

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
