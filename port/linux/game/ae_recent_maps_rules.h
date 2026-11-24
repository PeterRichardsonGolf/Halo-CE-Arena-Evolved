/*
AE_RECENT_MAPS_RULES.H

The recent-maps memory of the AE TOURNAMENT playlists: the last few maps each playlist played, so picks and vote offers keep clear of
them. Pure code (no engine headers): port/linux/tests/ae_recent_maps_rules_test.c runs it.

- A MAP is its entry's root: a stand-in (the entry that plays while a Workshop map is not here) and the Workshop map it stands for are
  the same map, so the memory keeps the root entry's map name.
- WINDOW: a third of the playlist's playable maps, at most AE_RECENT_MAX, and never so many that fewer than AE_SOCIAL_VOTE_CHOICES
  others remain.
- HIDDEN: the recent maps are left out of an offer while enough others remain; a small playlist readmits them oldest first.
- TEXT: one line a playlist, "<playlist>", then a tab and a map's name each (a name may hold blanks), the newest map first (the file in the
  profile's private files).
*/

#ifndef __AE_RECENT_MAPS_RULES_H
#define __AE_RECENT_MAPS_RULES_H

#include "ae_social_playlists.h"

enum
{
	AE_RECENT_MAX = 8,
	AE_RECENT_KEY = AE_SOCIAL_MAP,
	AE_RECENT_LISTS = 16,
	AE_RECENT_OFFER = 3,                /* the maps a vote offers */
	AE_RECENT_TEXT = 12288   /* AE_RECENT_LISTS lines of a playlist id and AE_RECENT_MAX full-length keys: about 8.6 KB */
};

struct ae_recent_list
{
	char playlist[AE_SOCIAL_ID];
	int count;                          /* maps kept, the newest first */
	char key[AE_RECENT_MAX][AE_RECENT_KEY];
};

struct ae_recent_maps
{
	int count;
	struct ae_recent_list list[AE_RECENT_LISTS];
};

/* the map an entry stands for: the root entry's map name (a stand-in's is the Workshop map's); "" for a bad index */
void ae_recent_key_of(struct ae_social_playlist const *playlist, int entry, char *key, int size);
/* the distinct maps among the candidate entries */
int ae_recent_playable(struct ae_social_playlist const *playlist, int const *candidates, int count);
/* how many recent maps count for a playlist that has playable maps */
int ae_recent_window(int playable);

/* the playlist's list (created when create is set; with all AE_RECENT_LISTS taken the oldest list makes room); NULL when none */
struct ae_recent_list *ae_recent_list_get(struct ae_recent_maps *recent, char const *playlist, int create);
/* a map was played: it goes first (the same map earlier moves up), the list keeps at most AE_RECENT_MAX */
void ae_recent_push(struct ae_recent_list *list, char const *key);
/* a map's age among the newest window of the list: 0 the newest, -1 not recent */
int ae_recent_age(struct ae_recent_list const *list, char const *key, int window);

/* which candidates (indexes into candidates[]) an offer of `wanted` distinct maps leaves out: hidden[i] 1 for a recent map's entries.
   The recent maps stay hidden while `wanted` other maps remain; else the oldest recent maps come back until `wanted` maps are on offer
   (or all are). list may be NULL. Returns how many candidates are hidden */
int ae_recent_hidden(struct ae_social_playlist const *playlist, int const *candidates, int count, struct ae_recent_list const *list,
	int wanted, int *hidden);

/* the memory as text, and back; the length written (-1: size too small) and the lists read (a bad line is skipped) */
int ae_recent_write(struct ae_recent_maps const *recent, char *text, int size);
int ae_recent_read(struct ae_recent_maps *recent, char const *text);

#endif
