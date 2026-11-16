/*
AE_SETTINGS_SEARCH.C

FIND A SETTING's rules (ae_settings_search.h).
*/

#include <string.h>

#include "ae_settings_search.h"

int ae_search_normalize(const char *text, char *out, size_t size, short *map)
{
	size_t length = 0, at;
	int space = 1;

	if (!size)
		return 0;
	for (at = 0; text && text[at] && length + 1 < size; at++)
	{
		unsigned char c = (unsigned char)text[at];

		if (c == '-' || c == '/' || c == '.' || c == '\'')
			continue;
		if (c >= 'a' && c <= 'z')
			c = (unsigned char)(c - 'a' + 'A');
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 0x80)
		{
			out[length] = (char)c;
			if (map)
				map[length] = (short)at;
			length++;
			space = 0;
		}
		else if (!space)
		{
			out[length] = ' ';
			if (map)
				map[length] = (short)at;
			length++;
			space = 1;
		}
	}
	while (length && out[length - 1] == ' ')
		length--;
	out[length] = 0;
	return (int)length;
}

/* where word is in text: at a word's start (inside 0), else (when inside is allowed) anywhere (1); -1 none */
static int find_word(const char *text, const char *word, int inside_ok, int *inside)
{
	size_t length = strlen(word), at;
	const char *found;

	for (at = 0; text[at]; at++)
	{
		if ((at == 0 || text[at - 1] == ' ') && !strncmp(text + at, word, length))
		{
			*inside = 0;
			return (int)at;
		}
	}
	if (inside_ok && (found = strstr(text, word)) != NULL)
	{
		*inside = 1;
		return (int)(found - text);
	}
	return -1;
}

/* every word found in text: 1, with inside set when any was found only inside a word */
static int all_words(const char *text, char words[][AE_SEARCH_QUERY], int word_count, int inside_ok, int min_length,
	int *inside)
{
	int index, one;

	*inside = 0;
	for (index = 0; index < word_count; index++)
	{
		if ((int)strlen(words[index]) < min_length || find_word(text, words[index], inside_ok, &one) < 0)
			return 0;
		*inside |= one;
	}
	return 1;
}

static void append(char *text, size_t size, const char *more)
{
	size_t length = strlen(text);

	if (!more || !more[0] || length + 2 >= size)
		return;
	if (length)
		text[length++] = ' ';
	strncpy(text + length, more, size - length - 1);
	text[size - 1] = 0;
}

short ae_search_run(const char *query, short count, ae_search_item_fn get, void *context, struct ae_search_hit *hits,
	short maximum, short *hidden)
{
	char normal[AE_SEARCH_QUERY], words[AE_SEARCH_WORDS][AE_SEARCH_QUERY], joined[AE_SEARCH_QUERY];
	char joined_words[1][AE_SEARCH_QUERY];
	int word_count = 0, length, at, start;
	struct ae_search_hit found[512];
	short found_count = 0, index, total = 0;
	int key;

	if (hidden)
		*hidden = 0;
	length = ae_search_normalize(query, normal, sizeof(normal), NULL);
	if (!length)
		return 0;
	for (at = 0; at < length && word_count < AE_SEARCH_WORDS;)
	{
		start = at;
		while (at < length && normal[at] != ' ')
			at++;
		memcpy(words[word_count], normal + start, (size_t)(at - start));
		words[word_count][at - start] = 0;
		word_count++;
		while (at < length && normal[at] == ' ')
			at++;
	}
	joined[0] = 0;
	for (index = 0; index < word_count; index++)
		strcat(joined, words[index]);
	memcpy(joined_words[0], joined, sizeof(joined));
	for (index = 0; index < count && found_count < (short)(sizeof(found) / sizeof(found[0])); index++)
	{
		struct ae_search_item item;
		char label[AE_SEARCH_TEXT], section[AE_SEARCH_TEXT], keywords[AE_SEARCH_TEXT], help[AE_SEARCH_TEXT];
		char text[AE_SEARCH_TEXT * 2];
		short label_map[AE_SEARCH_TEXT];
		int tier, inside = 0, matched = 0, word;
		int single = word_count == 1 && (int)strlen(words[0]) >= 3;

		memset(&item, 0, sizeof(item));
		if (!get(context, index, &item))
			continue;
		ae_search_normalize(item.label, label, sizeof(label), label_map);
		section[0] = 0;
		/* (the section and the group in the label's text: tier 2) */
		strcpy(text, label);
		ae_search_normalize(item.section, section, sizeof(section), NULL);
		append(text, sizeof(text), section);
		ae_search_normalize(item.group, section, sizeof(section), NULL);
		append(text, sizeof(text), section);
		for (tier = AE_SEARCH_LABEL; tier <= AE_SEARCH_HELP && !matched; tier++)
		{
			const char *haystack;
			int min = 1;

			if (tier == AE_SEARCH_LABEL)
				haystack = label;
			else
			{
				if (tier == AE_SEARCH_KEYWORD)
				{
					ae_search_normalize(item.keywords, keywords, sizeof(keywords), NULL);
					append(text, sizeof(text), keywords);
				}
				else if (tier == AE_SEARCH_HELP)
				{
					ae_search_normalize(item.help, help, sizeof(help), NULL);
					append(text, sizeof(text), help);
					min = 3;
				}
				haystack = text;
			}
			if (all_words(haystack, words, word_count, 0, min, &inside) ||
				(word_count > 1 && all_words(haystack, joined_words, 1, 0, min, &inside)))
				matched = 1;
			else if (single && all_words(haystack, words, 1, 1, min, &inside))
				matched = 1;
			if (matched)
				break;
		}
		if (!matched)
			continue;
		if (item.hidden)
		{
			if (hidden)
				(*hidden)++;
			continue;
		}
		found[found_count].item = index;
		found[found_count].tier = (short)tier;
		found[found_count].inside = (short)inside;
		found[found_count].match_start = found[found_count].match_len = 0;
		/* (the first query word in the label, for the underline: a word's start before an inside match) */
		for (word = 0; word < word_count + 1; word++)
		{
			const char *w = word < word_count ? words[word] : joined;
			int place, ignored;

			if (word == word_count && word_count < 2)
				break;
			place = find_word(label, w, 0, &ignored);
			if (place < 0 && single)
				place = find_word(label, w, 1, &ignored);
			if (place >= 0)
			{
				int last = place + (int)strlen(w) - 1;

				found[found_count].match_start = label_map[place];
				found[found_count].match_len = (short)(label_map[last] + 1 - label_map[place]);
				break;
			}
		}
		found_count++;
	}
	/* (tier order, the word starts of a tier before its inside matches, the list's order within) */
	for (key = 0; key < 8; key++)
	{
		for (index = 0; index < found_count && total < maximum; index++)
		{
			if ((found[index].tier - 1) * 2 + found[index].inside == key)
				hits[total++] = found[index];
		}
	}
	return total;
}
