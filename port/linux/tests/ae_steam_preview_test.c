/* ae_steam_preview.c: Workshop pictures decoded from bytes built here (a PNG and a JPEG), and the refusals: a truncated
picture, a header claiming 65535 by 65535 or too many pixels (refused before decoding), a body that inflates past its
budget, bytes that are neither JPEG nor PNG */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_steam_preview.h"
#include "ae_steam_preview_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* an 8 by 8 baseline JPEG, one colour (#c86432), its SOF0's height at 163 and width at 165 */
static unsigned char const jpeg[] =
{
	0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
	0x00, 0x01, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x03, 0x02, 0x02, 0x03,
	0x03, 0x03, 0x03, 0x04, 0x03, 0x03, 0x04, 0x05, 0x08, 0x05, 0x05, 0x04, 0x04, 0x05, 0x0a, 0x07,
	0x07, 0x06, 0x08, 0x0c, 0x0a, 0x0c, 0x0c, 0x0b, 0x0a, 0x0b, 0x0b, 0x0d, 0x0e, 0x12, 0x10, 0x0d,
	0x0e, 0x11, 0x0e, 0x0b, 0x0b, 0x10, 0x16, 0x10, 0x11, 0x13, 0x14, 0x15, 0x15, 0x15, 0x0c, 0x0f,
	0x17, 0x18, 0x16, 0x14, 0x18, 0x12, 0x14, 0x15, 0x14, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x03, 0x04,
	0x04, 0x05, 0x04, 0x05, 0x09, 0x05, 0x05, 0x09, 0x14, 0x0d, 0x0b, 0x0d, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0xff, 0xc0,
	0x00, 0x11, 0x08, 0x00, 0x08, 0x00, 0x08, 0x03, 0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11,
	0x01, 0xff, 0xc4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00,
	0x15, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x07, 0x08, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01,
	0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00, 0x20, 0x3d, 0x48, 0x3f, 0xff, 0xd9,
};
enum { JPEG_HEIGHT_AT = 163, JPEG_WIDTH_AT = 165 };

/* ---------- a PNG written here: signature, IHDR, one IDAT, IEND, with their CRCs */

static unsigned long crc32_of(unsigned char const *data, size_t size)
{
	unsigned long crc = 0xffffffffUL;
	size_t i;
	int bit;

	for (i = 0; i < size; i++)
	{
		crc ^= data[i];
		for (bit = 0; bit < 8; bit++)
			crc = crc & 1 ? (crc >> 1) ^ 0xedb88320UL : crc >> 1;
	}
	return crc ^ 0xffffffffUL;
}

static void put32(unsigned char *at, unsigned long value)
{
	at[0] = (unsigned char)(value >> 24);
	at[1] = (unsigned char)(value >> 16);
	at[2] = (unsigned char)(value >> 8);
	at[3] = (unsigned char)value;
}

/* a chunk at out: its whole size */
static size_t chunk(unsigned char *out, char const *type, unsigned char const *data, size_t size)
{
	put32(out, (unsigned long)size);
	memcpy(out + 4, type, 4);
	if (size)
		memcpy(out + 8, data, size);
	put32(out + 8 + size, crc32_of(out + 4, size + 4));
	return 12 + size;
}

/* a PNG of width by height, RGBA 8-bit, around zlib (its stream): its size */
static size_t png(unsigned char *out, unsigned long width, unsigned long height, unsigned char const *zlib,
	size_t zlib_size)
{
	static unsigned char const signature[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	unsigned char header[13];
	size_t size = 8;

	memcpy(out, signature, 8);
	put32(header, width);
	put32(header + 4, height);
	header[8] = 8;  /* bits */
	header[9] = 6;  /* RGBA */
	header[10] = header[11] = header[12] = 0;
	size += chunk(out + size, "IHDR", header, sizeof(header));
	size += chunk(out + size, "IDAT", zlib, zlib_size);
	size += chunk(out + size, "IEND", NULL, 0);
	return size;
}

/* zlib around one stored deflate block of raw: its size */
static size_t zlib_stored(unsigned char *out, unsigned char const *raw, size_t size)
{
	unsigned long a = 1, b = 0;
	size_t i;

	out[0] = 0x78;
	out[1] = 0x01;
	out[2] = 1; /* the last block, stored */
	out[3] = (unsigned char)size;
	out[4] = (unsigned char)(size >> 8);
	out[5] = (unsigned char)~size;
	out[6] = (unsigned char)(~size >> 8);
	memcpy(out + 7, raw, size);
	for (i = 0; i < size; i++)
	{
		a = (a + raw[i]) % 65521;
		b = (b + a) % 65521;
	}
	put32(out + 7 + size, b << 16 | a);
	return 11 + size;
}

/* ---------- a deflate stream that inflates to far more than its size: a 0, then matches of 258 at distance 1 */

struct bits
{
	unsigned char *out;
	size_t size;
	unsigned int bit;
};

static void put_bit(struct bits *bits, int value)
{
	if (!bits->bit)
		bits->out[bits->size++] = 0;
	if (value)
		bits->out[bits->size - 1] |= (unsigned char)(1u << bits->bit);
	bits->bit = (bits->bit + 1) & 7;
}

/* a Huffman code, its first bit first */
static void put_code(struct bits *bits, unsigned int code, int length)
{
	while (length-- > 0)
		put_bit(bits, (code >> length) & 1);
}

static size_t zlib_bomb(unsigned char *out, unsigned long matches)
{
	struct bits bits;
	unsigned long i;

	out[0] = 0x78;
	out[1] = 0x01;
	bits.out = out + 2;
	bits.size = 0;
	bits.bit = 0;
	put_bit(&bits, 1);              /* the last block */
	put_bit(&bits, 1);              /* fixed codes (01, its low bit first) */
	put_bit(&bits, 0);
	put_code(&bits, 0x30, 8);       /* the literal 0 */
	for (i = 0; i < matches; i++)
	{
		put_code(&bits, 0xc5, 8);   /* length 258 (code 285) */
		put_code(&bits, 0, 5);      /* distance 1 */
	}
	put_code(&bits, 0, 7);          /* the block's end */
	memset(out + 2 + bits.size, 0, 4); /* (the checksum: never read) */
	return 2 + bits.size + 4;
}

int main(void)
{
	static unsigned char file[3 * 1024 * 1024], zlib[2 * 1024 * 1024];
	unsigned char raw[2 * (1 + 8)], copy[sizeof(jpeg)];
	char why[128];
	uint32_t *pixels;
	size_t size, zlib_size;

	/* a valid 2 by 2 PNG: red, green / blue, transparent white (each row: filter 0, then its pixels) */
	{
		static unsigned char const rows[sizeof(raw)] = { 0, 255, 0, 0, 255, 0, 255, 0, 255,
			0, 0, 0, 255, 255, 255, 255, 255, 0 };

		memcpy(raw, rows, sizeof(raw));
	}
	zlib_size = zlib_stored(zlib, raw, sizeof(raw));
	size = png(file, 2, 2, zlib, zlib_size);
	pixels = ae_steam_preview_decode(file, size, 256, why, sizeof(why));
	CHECK(pixels != NULL);
	CHECK(!strcmp(why, ""));
	if (pixels)
	{
		CHECK(pixels[0] == 0xffff0000u);
		CHECK(pixels[255] == 0xff00ff00u);
		CHECK(pixels[255 * 256] == 0xff0000ffu);
		CHECK(pixels[255 * 256 + 255] == 0xff000000u);
		free(pixels);
	}
	/* the same at 1 by 1 (one red pixel) fills the square */
	zlib_size = zlib_stored(zlib, raw, 5);
	size = png(file, 1, 1, zlib, zlib_size);
	pixels = ae_steam_preview_decode(file, size, 256, why, sizeof(why));
	CHECK(pixels != NULL);
	if (pixels)
	{
		CHECK(pixels[0] == 0xffff0000u && pixels[256 * 256 - 1] == 0xffff0000u);
		free(pixels);
	}
	/* truncated in its IDAT: refused, and said */
	zlib_size = zlib_stored(zlib, raw, sizeof(raw));
	size = png(file, 2, 2, zlib, zlib_size);
	pixels = ae_steam_preview_decode(file, 8 + 25 + 14, 256, why, sizeof(why));
	CHECK(pixels == NULL && why[0]);
	/* an IDAT of no bytes: refused before stb_image copies it */
	size = png(file, 2, 2, zlib, 0);
	pixels = ae_steam_preview_decode(file, size, 256, why, sizeof(why));
	CHECK(pixels == NULL && !strcmp(why, "an empty data chunk"));
	/* a header claiming 65535 by 65535: refused before decoding */
	size = png(file, 65535, 65535, zlib, zlib_size);
	pixels = ae_steam_preview_decode(file, size, 256, why, sizeof(why));
	CHECK(pixels == NULL);
	CHECK(why[0] && strncmp(why, "not decoded", 11) != 0);
	/* one side a pixel over the bound: refused before decoding */
	size = png(file, 2049, 16, zlib, zlib_size);
	pixels = ae_steam_preview_decode(file, size, 256, why, sizeof(why));
	CHECK(pixels == NULL && why[0] && strncmp(why, "not decoded", 11) != 0);
	/* a 16 by 16 PNG whose IDAT inflates to 20 MB (in about 130 KB): stopped at its memory budget */
	zlib_size = zlib_bomb(zlib, 20UL * 1024 * 1024 / 258);
	CHECK(zlib_size < 200 * 1024);
	size = png(file, 16, 16, zlib, zlib_size);
	pixels = ae_steam_preview_decode(file, size, 256, why, sizeof(why));
	CHECK(pixels == NULL && !strncmp(why, "not decoded", 11));

	/* the JPEG: one colour (within JPEG's error) in the whole square */
	pixels = ae_steam_preview_decode(jpeg, sizeof(jpeg), 256, why, sizeof(why));
	CHECK(pixels != NULL);
	if (pixels)
	{
		uint32_t pixel = pixels[128 * 256 + 128];
		int red = (int)(pixel >> 16 & 255), green = (int)(pixel >> 8 & 255), blue = (int)(pixel & 255);

		CHECK((pixel >> 24) == 255);
		CHECK(red > 0xc8 - 8 && red < 0xc8 + 8 && green > 0x64 - 8 && green < 0x64 + 8 && blue > 0x32 - 8 &&
			blue < 0x32 + 8);
		CHECK(pixels[0] == pixel);
		free(pixels);
	}
	/* truncated: never a crash, and never more than the square */
	pixels = ae_steam_preview_decode(jpeg, 170, 256, why, sizeof(why));
	CHECK(pixels == NULL);
	pixels = ae_steam_preview_decode(jpeg, sizeof(jpeg) - 20, 256, why, sizeof(why));
	free(pixels);
	/* a header claiming 65535 by 65535: refused before decoding */
	memcpy(copy, jpeg, sizeof(jpeg));
	copy[JPEG_HEIGHT_AT] = copy[JPEG_HEIGHT_AT + 1] = 0xff;
	copy[JPEG_WIDTH_AT] = copy[JPEG_WIDTH_AT + 1] = 0xff;
	pixels = ae_steam_preview_decode(copy, sizeof(copy), 256, why, sizeof(why));
	CHECK(pixels == NULL && why[0] && strncmp(why, "not decoded", 11) != 0);

	/* the JPEG with 100 more scan markers after it (101 in all): refused before decoding */
	{
		static unsigned char scans[sizeof(jpeg) + 200];
		int scan;

		memcpy(scans, jpeg, sizeof(jpeg));
		for (scan = 0; scan < 100; scan++)
		{
			scans[sizeof(jpeg) + 2 * scan] = 0xff;
			scans[sizeof(jpeg) + 2 * scan + 1] = 0xda;
		}
		pixels = ae_steam_preview_decode(scans, sizeof(scans), 256, why, sizeof(why));
		CHECK(pixels == NULL && !strcmp(why, "too many scans"));
		pixels = ae_steam_preview_decode(scans, sizeof(scans) - 2, 256, why, sizeof(why));
		free(pixels);
		CHECK(strcmp(why, "too many scans") != 0);
	}

	/* not JPEG nor PNG; empty; over the body's limit; no square */
	pixels = ae_steam_preview_decode((unsigned char const *)"GIF89a\1\0\1\0", 10, 256, why, sizeof(why));
	CHECK(pixels == NULL && !strcmp(why, "not a JPEG or PNG"));
	pixels = ae_steam_preview_decode(jpeg, 0, 256, why, sizeof(why));
	CHECK(pixels == NULL && !strcmp(why, "no bytes"));
	pixels = ae_steam_preview_decode(jpeg, AE_STEAM_PREVIEW_MAXIMUM_BODY + 1, 256, why, sizeof(why));
	CHECK(pixels == NULL && !strcmp(why, "too many bytes"));
	pixels = ae_steam_preview_decode(jpeg, sizeof(jpeg), 0, why, sizeof(why));
	CHECK(pixels == NULL && why[0]);
	/* (no room for why: still refused) */
	pixels = ae_steam_preview_decode(jpeg, 0, 256, NULL, 0);
	CHECK(pixels == NULL);

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
