/*
AE_SETTINGS_SEARCH.H

The rules of SETTINGS' FIND A SETTING (ae_settings_search.c; the spec: notes/specs 2026-11-15, 3.1). Pure: no engine
includes (unit test: port/linux/tests/ae_settings_search_test.c). Generic over a list of items (a row's group, section,
label, keywords and help), so GAME OPTIONS' FIND AN OPTION can use the same rules.

A text is normalized before it is compared: upper case, '-' '/' '.' and the apostrophe dropped ("V-SYNC" reads
"VSYNC"), any other mark a space, runs of spaces folded. Every word of the query must match at the start of a word
somewhere in the item's text; the query with its spaces taken out counts as one word too ("v sync" finds V-SYNC). A
query of a single word of 3 letters or more may also match inside a word, ranked under the word starts.

An item's tier is where its words are first all found: 1 its label, 2 its label, section and group, 3 those and its
keywords, 4 those and its help (only words of 3 letters or more are read in the help). Hits come in tier order, inside
matches after the word starts of the same tier, then in the list's order. match_start / match_len: the first query
word's letters in the label (as the label is written: a length of 0 when no word is in it) for the underline.
*/

#ifndef __AE_SETTINGS_SEARCH_H
#define __AE_SETTINGS_SEARCH_H

#include <stddef.h>

enum
{
	AE_SEARCH_QUERY = 32,      /* the query's bytes, with its end */
	AE_SEARCH_WORDS = 6,
	AE_SEARCH_TEXT = 640,      /* one item's normalized text */
	AE_SEARCH_LABEL = 1, AE_SEARCH_SECTION, AE_SEARCH_KEYWORD, AE_SEARCH_HELP
};

struct ae_search_item
{
	const char *group, *section, *label, *keywords, *help;   /* NULL: none */
	int hidden;                /* the item cannot be used now (a device it needs): counted, never listed */
};

struct ae_search_hit
{
	short item, tier;
	short inside;              /* 1: found inside a word only */
	short match_start, match_len;
};

/* the item at index (0 when the index is not an item to search); context is the caller's */
typedef int (*ae_search_item_fn)(void *context, short index, struct ae_search_item *item);

/* text normalized into out (size bytes); map (NULL or size entries) gives each out letter's byte in text; the length */
int ae_search_normalize(const char *text, char *out, size_t size, short *map);

/* the hits of query over the items 0 to count - 1, in order, at most maximum; *hidden (NULL ok): the matching items
that are hidden. An empty query has no hits. Returns the hits */
short ae_search_run(const char *query, short count, ae_search_item_fn get, void *context, struct ae_search_hit *hits,
	short maximum, short *hidden);

#endif
