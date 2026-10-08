#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_strings.h"
#include "ae_font.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the formats: each id with %d or %s, and how many of each (every other id has none) */
static const struct { int id, d, s; } formats[] =
{
	{ AE_STR_MORE, 1, 0 }, { AE_STR_DEFAULT, 0, 1 }, { AE_STR_CHANGED_FROM, 0, 1 }, { AE_STR_COUNT, 2, 0 },
	{ AE_STR_REVERTING, 1, 0 }, { AE_STR_CONTROLLER_N, 1, 0 }, { AE_STR_PLAYER_N, 1, 0 }, { AE_STR_ERR_VERSION, 2, 0 },
	{ AE_STR_ERR_MAP, 0, 1 }, { AE_STR_ERR_GAMETYPE, 0, 1 },
};

static int count(const char *text, const char *what)
{
	int n = 0;

	while ((text = strstr(text, what)) != NULL)
	{
		n++;
		text += strlen(what);
	}
	return n;
}

static unsigned char *read_file(const char *path, int *size)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data = NULL;
	long length;

	*size = 0;
	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (length > 0 && (data = malloc((size_t)length)) != NULL && fread(data, 1, (size_t)length, file) == (size_t)length)
		*size = (int)length;
	fclose(file);
	return data;
}

int main(void)
{
	unsigned char *font;
	int size, id, index;

	font = read_file("port/assets/fonts/Overpass-900.ttf", &size);
	CHECK(font && ae_font_load(AE_FACE_OVERPASS_900, font, size));
	CHECK(AE_NUMBER_OF_STRINGS == 77 && !strcmp(ae_string(AE_STR_SELECT), "Select") &&
		!strcmp(ae_string(AE_STR_ERR_IN_GAME), "Not while a game is running") && !strcmp(ae_string(-1), "") &&
		!strcmp(ae_string(AE_NUMBER_OF_STRINGS), ""));
	for (id = 0; id < AE_NUMBER_OF_STRINGS; id++)
	{
		const char *text = ae_string(id), *cursor = text;
		int d = 0, s = 0;
		unsigned int codepoint;

		CHECK(text && *text);
		/* never U+00B7 (Overpass: a combining mark, drawn with no advance) */
		CHECK(!strstr(text, "\xC2\xB7"));
		for (index = 0; index < (int)(sizeof(formats) / sizeof(formats[0])); index++)
		{
			if (formats[index].id == id)
			{
				d = formats[index].d;
				s = formats[index].s;
			}
		}
		if (count(text, "%d") != d || count(text, "%s") != s || count(text, "%") != d + s)
			printf("string %d: formats differ: %s\n", id, text);
		CHECK(count(text, "%d") == d && count(text, "%s") == s && count(text, "%") == d + s);
		/* every code point drawable in Overpass 900 */
		while ((codepoint = ae_font_utf8_next(&cursor)) != 0)
		{
			if (!ae_font_has(AE_FACE_OVERPASS_900, codepoint))
				printf("string %d: U+%04X not in Overpass 900\n", id, codepoint);
			CHECK(codepoint == ' ' || ae_font_has(AE_FACE_OVERPASS_900, codepoint));
		}
	}
	free(font);
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
