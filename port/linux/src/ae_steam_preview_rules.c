/*
AE_STEAM_PREVIEW_RULES.C

ae_steam_preview_rules.h: which Workshop pictures are decoded, and how one is fitted into the card's square.
*/

#include "ae_steam_preview_rules.h"

#include <stddef.h>
#include <string.h>

static unsigned char const png_signature[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };

int ae_steam_preview_kind(unsigned char const *data, unsigned long size)
{
	if (!data)
		return AE_STEAM_PREVIEW_NONE;
	if (size >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff)
		return AE_STEAM_PREVIEW_JPEG;
	if (size >= sizeof(png_signature) && !memcmp(data, png_signature, sizeof(png_signature)))
		return AE_STEAM_PREVIEW_PNG;
	return AE_STEAM_PREVIEW_NONE;
}

unsigned long ae_steam_preview_jpeg_scans(unsigned char const *data, unsigned long size)
{
	unsigned long index, scans = 0;

	if (!data)
		return 0;
	for (index = 0; index + 1 < size; index++)
		if (data[index] == 0xff && data[index + 1] == 0xda)
			scans++;
	return scans;
}

int ae_steam_preview_png_empty_data(unsigned char const *data, unsigned long size)
{
	unsigned long at = sizeof(png_signature);

	if (!data)
		return 0;
	/* (each chunk: its length, its type, its bytes and a CRC; one cut short ends the walk) */
	while (at <= size && size - at >= 12)
	{
		unsigned long length = (unsigned long)data[at] << 24 | (unsigned long)data[at + 1] << 16 |
			(unsigned long)data[at + 2] << 8 | (unsigned long)data[at + 3];
		unsigned char const *type = data + at + 4;

		if (!length && !memcmp(type, "IDAT", 4))
			return 1;
		if (!memcmp(type, "IEND", 4) || length > size - at - 12)
			break;
		at += 12 + length;
	}
	return 0;
}

int ae_steam_preview_png_whole(unsigned char const *data, unsigned long size)
{
	unsigned long at = sizeof(png_signature);

	if (!data)
		return 0;
	while (at <= size && size - at >= 12)
	{
		unsigned long length = (unsigned long)data[at] << 24 | (unsigned long)data[at + 1] << 16 |
			(unsigned long)data[at + 2] << 8 | (unsigned long)data[at + 3];

		if (length > size - at - 12)
			break;
		if (!memcmp(data + at + 4, "IEND", 4))
			return 1;
		at += 12 + length;
	}
	return 0;
}

int ae_steam_preview_check_body(unsigned char const *data, unsigned long size)
{
	int kind;

	if (!data || !size)
		return AE_STEAM_PREVIEW_EMPTY;
	if (size > AE_STEAM_PREVIEW_MAXIMUM_BODY)
		return AE_STEAM_PREVIEW_TOO_LARGE;
	kind = ae_steam_preview_kind(data, size);
	if (kind == AE_STEAM_PREVIEW_NONE)
		return AE_STEAM_PREVIEW_NOT_A_PICTURE;
	if (kind == AE_STEAM_PREVIEW_JPEG && ae_steam_preview_jpeg_scans(data, size) > AE_STEAM_PREVIEW_MAXIMUM_SCANS)
		return AE_STEAM_PREVIEW_TOO_MANY_SCANS;
	if (kind == AE_STEAM_PREVIEW_PNG && ae_steam_preview_png_empty_data(data, size))
		return AE_STEAM_PREVIEW_EMPTY_DATA;
	/* (the walk for an empty IDAT reads whole chunks only; the decoder also reads a last chunk that has no CRC) */
	if (kind == AE_STEAM_PREVIEW_PNG && !ae_steam_preview_png_whole(data, size))
		return AE_STEAM_PREVIEW_CUT_SHORT;
	return AE_STEAM_PREVIEW_TAKEN;
}

int ae_steam_preview_check_size(int width, int height)
{
	if (width <= 0 || height <= 0 || width > AE_STEAM_PREVIEW_MAXIMUM_SIDE || height > AE_STEAM_PREVIEW_MAXIMUM_SIDE)
		return AE_STEAM_PREVIEW_BAD_SIZE;
	/* (both sides at most 2048 here: the product fits in 32 bits) */
	if ((unsigned long)width * (unsigned long)height > AE_STEAM_PREVIEW_MAXIMUM_PIXELS)
		return AE_STEAM_PREVIEW_TOO_MANY_PIXELS;
	return AE_STEAM_PREVIEW_TAKEN;
}

char const *ae_steam_preview_reason(int answer)
{
	switch (answer)
	{
	case AE_STEAM_PREVIEW_TAKEN: return "";
	case AE_STEAM_PREVIEW_EMPTY: return "no bytes";
	case AE_STEAM_PREVIEW_TOO_LARGE: return "too many bytes";
	case AE_STEAM_PREVIEW_NOT_A_PICTURE: return "not a JPEG or PNG";
	case AE_STEAM_PREVIEW_BAD_SIZE: return "a side out of bounds";
	case AE_STEAM_PREVIEW_TOO_MANY_PIXELS: return "too many pixels";
	case AE_STEAM_PREVIEW_TOO_MANY_SCANS: return "too many scans";
	case AE_STEAM_PREVIEW_EMPTY_DATA: return "an empty data chunk";
	case AE_STEAM_PREVIEW_CUT_SHORT: return "cut short";
	default: return "refused";
	}
}

/* (a side of the picture, the square's side and the picture's longer side: the fitted length, rounded, at least 1) */
static int fitted_length(int length, int side, int longer)
{
	unsigned long fitted = (2UL * (unsigned long)length * (unsigned long)side + (unsigned long)longer) /
		(2UL * (unsigned long)longer);

	return fitted < 1 ? 1 : (int)fitted;
}

int ae_steam_preview_fit(int width, int height, int side, int *left, int *top, int *fit_width, int *fit_height)
{
	int longer;

	if (ae_steam_preview_check_size(width, height) != AE_STEAM_PREVIEW_TAKEN || side <= 0 ||
		side > AE_STEAM_PREVIEW_MAXIMUM_FIT)
		return 0;
	longer = width > height ? width : height;
	*fit_width = fitted_length(width, side, longer);
	*fit_height = fitted_length(height, side, longer);
	*left = (side - *fit_width) / 2;
	*top = (side - *fit_height) / 2;
	return 1;
}

int ae_steam_preview_fit_pixels(unsigned char const *rgba, int width, int height, int side, uint32_t *out)
{
	int left, top, fit_width, fit_height, x, y;
	size_t index, count;

	if (!rgba || !out || !ae_steam_preview_fit(width, height, side, &left, &top, &fit_width, &fit_height))
		return 0;
	count = (size_t)side * (size_t)side;
	for (index = 0; index < count; index++)
		out[index] = 0xff000000u;
	for (y = 0; y < fit_height; y++)
	{
		/* (the picture's rows this one covers: at least one; at most 1024 * 2048 in the products, within 32 bits) */
		unsigned long row0 = (unsigned long)y * (unsigned long)height / (unsigned long)fit_height;
		unsigned long row1 = (unsigned long)(y + 1) * (unsigned long)height / (unsigned long)fit_height;

		if (row1 <= row0)
			row1 = row0 + 1;
		for (x = 0; x < fit_width; x++)
		{
			unsigned long column0 = (unsigned long)x * (unsigned long)width / (unsigned long)fit_width;
			unsigned long column1 = (unsigned long)(x + 1) * (unsigned long)width / (unsigned long)fit_width;
			uint64_t red = 0, green = 0, blue = 0, cells, divisor;
			unsigned long row, column;

			if (column1 <= column0)
				column1 = column0 + 1;
			for (row = row0; row < row1; row++)
			{
				unsigned char const *pixel = rgba + ((size_t)row * (size_t)width + (size_t)column0) * 4;

				for (column = column0; column < column1; column++, pixel += 4)
				{
					red += (uint64_t)pixel[0] * pixel[3];
					green += (uint64_t)pixel[1] * pixel[3];
					blue += (uint64_t)pixel[2] * pixel[3];
				}
			}
			cells = (uint64_t)(row1 - row0) * (uint64_t)(column1 - column0);
			divisor = cells * 255u;
			red = (red + divisor / 2) / divisor;
			green = (green + divisor / 2) / divisor;
			blue = (blue + divisor / 2) / divisor;
			out[(size_t)(top + y) * (size_t)side + (size_t)(left + x)] = 0xff000000u | (uint32_t)red << 16 |
				(uint32_t)green << 8 | (uint32_t)blue;
		}
	}
	return 1;
}
