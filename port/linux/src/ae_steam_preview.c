/*
AE_STEAM_PREVIEW.C

ae_steam_preview.h: a Workshop picture decoded by stb_image (JPEG and PNG alone) after ae_steam_preview_rules.h's
checks, then fitted by ae_steam_preview_fit_pixels. Built with the Steam client's units (tools/steam_client_build.py).
*/

#include "ae_steam_preview.h"
#include "ae_steam_preview_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- stb_image's memory: counted, and refused past a budget set for each picture from the size its header
claims (a small compressed body can inflate to far more than its pixels need). One picture is decoded at a time: the
Steam client's preview thread is this unit's only caller. */

static size_t decode_budget, decode_bytes;

union block_head
{
	size_t size;
	double alignment_double;
	void *alignment_pointer;
	long long alignment_long_long;
};

static void *budget_malloc(size_t size)
{
	union block_head *head;

	if (size > decode_budget || decode_bytes > decode_budget - size || size > (size_t)-1 - sizeof(*head))
		return NULL;
	head = malloc(sizeof(*head) + size);
	if (!head)
		return NULL;
	head->size = size;
	decode_bytes += size;
	return head + 1;
}

static void budget_free(void *pointer)
{
	union block_head *head;

	if (!pointer)
		return;
	head = (union block_head *)pointer - 1;
	decode_bytes -= head->size;
	free(head);
}

static void *budget_realloc(void *pointer, size_t size)
{
	union block_head *head;
	void *moved;

	if (!pointer)
		return budget_malloc(size);
	head = (union block_head *)pointer - 1;
	moved = budget_malloc(size);
	if (!moved)
		return NULL;
	memcpy(moved, pointer, head->size < size ? head->size : size);
	budget_free(pointer);
	return moved;
}

/* (JPEG and PNG alone, from memory alone; static, so it never meets the game's other stb_image units; no picture
over the rules' side even inside stb_image) */
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS AE_STEAM_PREVIEW_MAXIMUM_SIDE
#define STBI_MALLOC(size) budget_malloc(size)
#define STBI_REALLOC(pointer, size) budget_realloc(pointer, size)
#define STBI_FREE(pointer) budget_free(pointer)
/* (stb_image's static API, of which this unit uses three calls: the compiler warns of the rest at the unit's end, so
the warning stays off for the whole unit) */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../../third_party/stb/stb_image.h"

/* the memory one picture may take while decoded: six times its RGBA pixels (a 16-bit PNG's rows and their 8-bit copy,
or a progressive JPEG's coefficients, with the result) and 4 MB besides */
#define DECODE_SLACK ((size_t)4 * 1024 * 1024)
#define DECODE_TIMES 6

static void say(char *why, int why_size, char const *text, char const *detail)
{
	if (why && why_size > 0)
		snprintf(why, (size_t)why_size, "%s%s%s%s", text, detail && detail[0] ? " (" : "", detail ? detail : "",
			detail && detail[0] ? ")" : "");
}

uint32_t *ae_steam_preview_decode(unsigned char const *data, unsigned long size, int side, char *why, int why_size)
{
	int answer, width = 0, height = 0, channels = 0, decoded_width = 0, decoded_height = 0;
	unsigned char *rgba;
	uint32_t *out;

	say(why, why_size, "", NULL);
	answer = ae_steam_preview_check_body(data, size);
	if (answer != AE_STEAM_PREVIEW_TAKEN)
	{
		say(why, why_size, ae_steam_preview_reason(answer), NULL);
		return NULL;
	}
	if (side <= 0 || side > AE_STEAM_PREVIEW_MAXIMUM_FIT)
	{
		say(why, why_size, "no square to fit into", NULL);
		return NULL;
	}
	/* (the header alone first, with the slack alone to read it in (a JPEG's decoder state): nothing is decoded for a
	size refused; the body is at most 2 MB, within an int) */
	decode_budget = DECODE_SLACK;
	decode_bytes = 0;
	answer = stbi_info_from_memory(data, (int)size, &width, &height, &channels);
	decode_budget = 0;
	if (!answer)
	{
		say(why, why_size, "an unreadable header", stbi_failure_reason());
		return NULL;
	}
	answer = ae_steam_preview_check_size(width, height);
	if (answer != AE_STEAM_PREVIEW_TAKEN)
	{
		say(why, why_size, ae_steam_preview_reason(answer), NULL);
		return NULL;
	}
	/* (at most 4 M pixels here: 6 * 4 bytes each and the slack come to at most 100 MB) */
	decode_budget = (size_t)width * (size_t)height * 4 * DECODE_TIMES + DECODE_SLACK;
	decode_bytes = 0;
	rgba = stbi_load_from_memory(data, (int)size, &decoded_width, &decoded_height, &channels, 4);
	if (!rgba)
	{
		say(why, why_size, "not decoded", stbi_failure_reason());
		decode_budget = 0;
		return NULL;
	}
	out = NULL;
	if (decoded_width != width || decoded_height != height)
		say(why, why_size, "a size unlike its header's", NULL);
	else if ((out = malloc(sizeof(*out) * (size_t)side * (size_t)side)) == NULL)
		say(why, why_size, "out of memory", NULL);
	else if (!ae_steam_preview_fit_pixels(rgba, width, height, side, out))
	{
		free(out);
		out = NULL;
		say(why, why_size, "not fitted", NULL);
	}
	stbi_image_free(rgba);
	decode_budget = 0;
	return out;
}
