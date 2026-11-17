/*
AE_MOTD_RULES.C

See ae_motd_rules.h.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSMN_STATIC
#include "../../third_party/jsmn/jsmn.h"
#include "../../third_party/monocypher/monocypher-ed25519.h"
#include "ae_motd_rules.h"

int ae_motd_version_compare(const char *a, const char *b)
{
	int part;

	for (part = 0; part < 4; part++)
	{
		long x = 0, y = 0;

		while (a && *a >= '0' && *a <= '9')
			x = x * 10 + (*a++ - '0');
		while (b && *b >= '0' && *b <= '9')
			y = y * 10 + (*b++ - '0');
		if (x != y)
			return x < y ? -1 : 1;
		a = a && *a == '.' ? a + 1 : NULL;
		b = b && *b == '.' ? b + 1 : NULL;
		if (!a && !b)
			break;
	}
	return 0;
}

int ae_motd_should_show(struct ae_motd_state const *s, const char *installed_version, int have_message, int message_rev,
	const char *today)
{
	if (!s->last_version[0] || ae_motd_version_compare(installed_version, s->last_version) > 0)
		return AE_MOTD_UPDATE;
	if (have_message && message_rev != s->closed_rev && strcmp(s->shown_day, today))
		return AE_MOTD_MESSAGE;
	return AE_MOTD_NONE;
}

int ae_motd_pick_source(int have_fresh, int have_cache)
{
	return have_fresh ? AE_MOTD_FROM_FRESH : have_cache ? AE_MOTD_FROM_CACHE : AE_MOTD_FROM_NOTES;
}

int ae_motd_parse_text(const char *text, struct ae_motd_block *out, int maximum)
{
	int count = 0, lines = 1, open = 0;
	const char *at;

	if (!text || strlen(text) > AE_MOTD_TEXT_LIMIT)
		return -1;
	for (at = text; *at; at++)
		lines += *at == '\n';
	if (lines > AE_MOTD_LINES_LIMIT)
		return -1;
	while (*text)
	{
		const char *end = strchr(text, '\n');
		size_t length = end ? (size_t)(end - text) : strlen(text);
		int bullet = length >= 2 && text[0] == '-' && text[1] == ' ';

		if (!length)
			open = 0;
		else if (count < maximum)
		{
			const char *words = bullet ? text + 2 : text;
			size_t size = length - (size_t)(words - text);

			if (open && !bullet && !out[count - 1].bullet)
			{
				size_t had = strlen(out[count - 1].text);

				if (had + 1 + size < sizeof(out[0].text))
				{
					out[count - 1].text[had] = ' ';
					memcpy(out[count - 1].text + had + 1, words, size);
					out[count - 1].text[had + 1 + size] = 0;
				}
			}
			else
			{
				if (size >= sizeof(out[0].text))
					size = sizeof(out[0].text) - 1;
				out[count].bullet = bullet;
				memcpy(out[count].text, words, size);
				out[count].text[size] = 0;
				count++;
			}
			open = !bullet;
		}
		text += length;
		if (*text == '\n')
			text++;
	}
	return count;
}

static int hex_value(char c)
{
	return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* a JSON string token copied, its escapes taken (\n \" \\ \/ \t; \uXXXX below 0x80, else '?') */
static void json_string(const char *body, jsmntok_t const *token, char *out, size_t size)
{
	int at = token->start;
	size_t used = 0;

	while (at < token->end && used + 1 < size)
	{
		char c = body[at++];

		if (c == '\\' && at < token->end)
		{
			char e = body[at++];

			c = e == 'n' ? '\n' : e == 't' ? '\t' : e;
			if (e == 'u' && at + 4 <= token->end)
			{
				int value = (hex_value(body[at]) << 12) | (hex_value(body[at + 1]) << 8) | (hex_value(body[at + 2]) << 4) |
					hex_value(body[at + 3]);

				c = value > 0 && value < 0x80 ? (char)value : '?';
				at += 4;
			}
		}
		out[used++] = c;
	}
	out[used] = 0;
}

int ae_motd_check(const char *body, size_t length, const char *signature_hex, const unsigned char key[32],
	unsigned long now, struct ae_motd *out)
{
	unsigned char signature[64];
	jsmntok_t tokens[32];
	jsmn_parser parser;
	int count, index, have_rev = 0, have_title = 0, have_text = 0;

	if (!signature_hex || strlen(signature_hex) != 128)
		return AE_MOTD_BAD_SIGNATURE;
	for (index = 0; index < 64; index++)
	{
		int high = hex_value(signature_hex[index * 2]), low = hex_value(signature_hex[index * 2 + 1]);

		if (high < 0 || low < 0)
			return AE_MOTD_BAD_SIGNATURE;
		signature[index] = (unsigned char)(high << 4 | low);
	}
	if (crypto_ed25519_check(signature, key, (const unsigned char *)body, length) != 0)
		return AE_MOTD_BAD_SIGNATURE;
	memset(out, 0, sizeof(*out));
	jsmn_init(&parser);
	count = jsmn_parse(&parser, body, length, tokens, (unsigned int)(sizeof(tokens) / sizeof(tokens[0])));
	if (count < 1 || tokens[0].type != JSMN_OBJECT)
		return AE_MOTD_MALFORMED;
	for (index = 1; index + 1 < count; index += 2)
	{
		jsmntok_t const *name = &tokens[index], *value = &tokens[index + 1];
		int size = name->end - name->start;
		char number[24];

		if (name->type != JSMN_STRING)
			return AE_MOTD_MALFORMED;
		if (value->type == JSMN_PRIMITIVE)
		{
			int digits = value->end - value->start;

			if (digits >= (int)sizeof(number))
				digits = (int)sizeof(number) - 1;
			memcpy(number, body + value->start, (size_t)digits);
			number[digits] = 0;
		}
		else
			number[0] = 0;
		if (size == 3 && !strncmp(body + name->start, "rev", 3) && value->type == JSMN_PRIMITIVE)
		{
			out->rev = strtol(number, NULL, 10);
			have_rev = 1;
		}
		else if (size == 5 && !strncmp(body + name->start, "title", 5) && value->type == JSMN_STRING)
		{
			json_string(body, value, out->title, sizeof(out->title));
			have_title = out->title[0] != 0;
		}
		else if (size == 4 && !strncmp(body + name->start, "text", 4) && value->type == JSMN_STRING)
		{
			json_string(body, value, out->text, sizeof(out->text));
			have_text = 1;
		}
		else if (size == 6 && !strncmp(body + name->start, "posted", 6))
			out->posted = strtoul(number, NULL, 10);
		else if (size == 4 && !strncmp(body + name->start, "ends", 4))
			out->ends = strtoul(number, NULL, 10);
		/* (a value that is an object or an array: its tokens skipped) */
		if (value->type == JSMN_OBJECT || value->type == JSMN_ARRAY)
			return AE_MOTD_MALFORMED;
	}
	if (!have_rev || !have_title || !have_text || strlen(out->title) > AE_MOTD_TITLE_LIMIT)
		return AE_MOTD_MALFORMED;
	if (out->ends && now >= out->ends)
		return AE_MOTD_EXPIRED;
	return AE_MOTD_OK;
}

int ae_motd_state_read(const char *text, struct ae_motd_state *s)
{
	memset(s, 0, sizeof(*s));
	s->closed_rev = -1;
	while (text && *text)
	{
		const char *end = strchr(text, '\n'), *space;
		size_t length = end ? (size_t)(end - text) : strlen(text);
		char line[300];

		if (length >= sizeof(line))
			length = sizeof(line) - 1;
		memcpy(line, text, length);
		line[length] = 0;
		if (length && line[length - 1] == '\r')
			line[length - 1] = 0;
		space = strchr(line, ' ');
		if (space)
		{
			const char *value = space + 1;
			size_t key = (size_t)(space - line);

			if (key == 12 && !strncmp(line, "last_version", 12))
				snprintf(s->last_version, sizeof(s->last_version), "%s", value);
			else if (key == 10 && !strncmp(line, "closed_rev", 10))
				s->closed_rev = strtol(value, NULL, 10);
			else if (key == 9 && !strncmp(line, "shown_day", 9))
				snprintf(s->shown_day, sizeof(s->shown_day), "%s", value);
			else if (key == 16 && !strncmp(line, "whats_new_opened", 16))
				snprintf(s->whats_new_opened, sizeof(s->whats_new_opened), "%s", value);
			else if (key == 14 && !strncmp(line, "whats_new_seen", 14))
				snprintf(s->whats_new_seen, sizeof(s->whats_new_seen), "%s", value);
		}
		text += end ? (size_t)(end - text) + 1 : strlen(text);
	}
	return 1;
}

int ae_motd_state_write(struct ae_motd_state const *s, char *out, int size)
{
	int length = snprintf(out, (size_t)size,
		"last_version %s\nclosed_rev %ld\nshown_day %s\nwhats_new_opened %s\nwhats_new_seen %s\n",
		s->last_version, s->closed_rev, s->shown_day, s->whats_new_opened, s->whats_new_seen);

	return length > 0 && length < size ? length : 0;
}
