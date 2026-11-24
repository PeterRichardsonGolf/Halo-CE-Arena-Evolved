/*
AE_WORKSHOP_COLLECTIONS.H

Which Workshop collection a Workshop map is in, answered by Steam (0.7.6). Steam's public web API tells, for a
collection's id, the items in it (ISteamRemoteStorage/GetCollectionDetails) and its title (GetPublishedFileDetails):
no key, no account. The question can only be asked in that direction, so the collections' ids are the one list the
game carries (ae_workshop_collections_id); what is in them, and what they are called, is Steam's, today's: a map added
to a collection later is grouped with it the next day, with no release.

This unit is the text of it: the requests' bodies, the answers read (server_json), the cache written and read, the
lookup. No network, no files, no threads (the platform's: ae_mcc_platform.c fetches on a thread of its own and keeps
the cache beside the saves). Unit test: port/linux/tests/ae_workshop_collections_test.c.

Both texts are untrusted: a huge, truncated or garbled answer or cache gives fewer collections, never a crash.
*/

#ifndef __AE_WORKSHOP_COLLECTIONS_H
#define __AE_WORKSHOP_COLLECTIONS_H

enum { AE_WSC_COLLECTIONS = 16, AE_WSC_ITEMS = 512, AE_WSC_NAME = 40, AE_WSC_ID = 24,
	/* the cache's largest text (a line an item id wide each, with the comment) */
	AE_WSC_CACHE_TEXT = AE_WSC_COLLECTIONS * (AE_WSC_ITEMS * (AE_WSC_ID + 1) + 128) + 256,
	AE_WSC_BODY = 1024,
	/* a cache older than this (seconds) is asked for again: a day */
	AE_WSC_REFRESH_SECONDS = 24 * 60 * 60 };

struct ae_wsc_collection
{
	char id[AE_WSC_ID];
	/* the title as the menus show it (upper case, "Halo CE - " cut), "" none yet */
	char name[AE_WSC_NAME];
	int count;
	char items[AE_WSC_ITEMS][AE_WSC_ID];
};

struct ae_wsc_table
{
	/* when Steam last answered (seconds since 1970), 0 never */
	long long fetched;
	int count;
	struct ae_wsc_collection collections[AE_WSC_COLLECTIONS];
};

/* the collections' ids the game asks about (decimal text): count, and the one at index ("" past the end) */
int ae_workshop_collections_id_count(void);
const char *ae_workshop_collections_id(int index);

void ae_wsc_table_init(struct ae_wsc_table *table);

/* the form bodies of the two requests, for the table's ids (an id is digits only, else it is left out):
GetCollectionDetails' "collectioncount=N&publishedfileids[0]=..." and GetPublishedFileDetails' "itemcount=N&...",
percent-encoded. How many ids it asks about; 0 and an empty body for none */
int ae_wsc_children_body(char *body, int size);
int ae_wsc_titles_body(char *body, int size);

/* GetCollectionDetails' answer read into the table (a collection read again is replaced): how many collections
it held; 0 for an answer that is not one */
int ae_wsc_read_children(struct ae_wsc_table *table, const char *text, int length);
/* GetPublishedFileDetails' answer: the title of each collection in the table; how many named */
int ae_wsc_read_titles(struct ae_wsc_table *table, const char *text, int length);
/* a title as a group's name: "Halo CE - Pro Edition" and "Halo: CE - Pro Edition" as PRO EDITION, upper case, cut
to size. A title with nothing left gives "" */
void ae_wsc_clean_title(const char *title, char *name, int size);

/* the table's collections that have a name and an item (drops the others); how many are left */
int ae_wsc_prune(struct ae_wsc_table *table);

/* the cache: text out (its length, -1 when it does not fit) and in (1 when read, bad lines skipped; the table is
emptied first). The time is the table's fetched */
long ae_wsc_cache_write(struct ae_wsc_table const *table, char *buffer, long size);
int ae_wsc_cache_read(struct ae_wsc_table *table, const char *text);
/* whether the table is older than a day at now, or holds nothing */
int ae_wsc_stale(struct ae_wsc_table const *table, long long now);

/* the collections (their indices, in the table's order) that hold an item; how many written (<= maximum) */
int ae_wsc_of(struct ae_wsc_table const *table, const char *item_id, int *indices, int maximum);

#endif
