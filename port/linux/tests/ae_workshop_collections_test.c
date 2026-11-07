/* ae_workshop_collections.c: Steam's Workshop collections as text (bodies, answers, cache, lookup) */

#include <stdio.h>
#include <string.h>

#include "../src/ae_workshop_collections.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); failures++; } } while (0)

static const char children[] =
	"{\"response\":{\"result\":1,\"resultcount\":3,\"collectiondetails\":["
	"{\"publishedfileid\":\"3001370390\",\"result\":1,\"children\":[{\"publishedfileid\":\"2899666869\",\"sortorder\":1,"
	"\"filetype\":0},{\"publishedfileid\":\"2899343509\",\"sortorder\":2,\"filetype\":0}]},"
	"{\"publishedfileid\":\"2899609719\",\"result\":1,\"children\":[{\"publishedfileid\":\"2899747345\"},"
	"{\"publishedfileid\":\"2899770689\"}]},"
	"{\"publishedfileid\":\"3817409420\",\"result\":9}]}}";
static const char titles[] =
	"{\"response\":{\"result\":1,\"resultcount\":2,\"publishedfiledetails\":["
	"{\"publishedfileid\":\"3001370390\",\"result\":1,\"title\":\"Halo CE - Pro Edition\"},"
	"{\"publishedfileid\":\"2899609719\",\"result\":1,\"title\":\"Social Map Pack\"},"
	"{\"publishedfileid\":\"999\",\"result\":1,\"title\":\"Not Ours\"}]}}";

int main(void)
{
	static struct ae_wsc_table table, again;
	static char cache[AE_WSC_CACHE_TEXT];
	char body[AE_WSC_BODY], name[AE_WSC_NAME];
	int found[4];
	long length;

	/* the bodies */
	CHECK(ae_wsc_children_body(body, sizeof(body)) == ae_workshop_collections_id_count() &&
		!strncmp(body, "collectioncount=4&publishedfileids%5B0%5D=3001370390&publishedfileids%5B1%5D=", 76));
	CHECK(ae_wsc_titles_body(body, sizeof(body)) == 4 && !strncmp(body, "itemcount=4&", 12));
	CHECK(ae_wsc_children_body(body, 20) == 0 && body[0] == 0 && ae_wsc_children_body(NULL, 0) == 0);
	CHECK(!strcmp(ae_workshop_collections_id(0), "3001370390") && !strcmp(ae_workshop_collections_id(99), ""));

	/* titles: the game's name and its dash cut, upper case, plain ASCII */
	ae_wsc_clean_title("Halo CE - Pro Edition", name, sizeof(name));
	CHECK(!strcmp(name, "PRO EDITION"));
	ae_wsc_clean_title("Halo: CE - NHE Map Pack  ", name, sizeof(name));
	CHECK(!strcmp(name, "NHE MAP PACK"));
	ae_wsc_clean_title("Social Map Pack", name, sizeof(name));
	CHECK(!strcmp(name, "SOCIAL MAP PACK"));
	ae_wsc_clean_title("Halo: CE - 2v2 Maps \xe2\x80\x93 x", name, sizeof(name));
	CHECK(!strcmp(name, "2V2 MAPS   X") || name[0] == '2');
	ae_wsc_clean_title("Halo CE - ", name, sizeof(name));
	CHECK(!name[0]);
	ae_wsc_clean_title("A very long collection title that goes well past the room a name has", name, sizeof(name));
	CHECK(strlen(name) == AE_WSC_NAME - 1);
	ae_wsc_clean_title(NULL, name, sizeof(name));
	CHECK(!name[0]);

	/* the answers: only the ids the game asks about; a result that is not 1 is left out */
	ae_wsc_table_init(&table);
	CHECK(ae_wsc_read_children(&table, children, (int)strlen(children)) == 2 && table.count == 2);
	CHECK(ae_wsc_read_titles(&table, titles, (int)strlen(titles)) == 2);
	table.fetched = 1000000;
	CHECK(ae_wsc_prune(&table) == 2 && !strcmp(table.collections[0].name, "PRO EDITION") &&
		!strcmp(table.collections[1].name, "SOCIAL MAP PACK") && table.collections[0].count == 2);
	CHECK(ae_wsc_of(&table, "2899343509", found, 4) == 1 && found[0] == 0);
	CHECK(ae_wsc_of(&table, "2899770689", found, 4) == 1 && found[0] == 1 && ae_wsc_of(&table, "1", found, 4) == 0);
	CHECK(ae_wsc_read_children(&table, "{not json", 9) == 0 && ae_wsc_read_children(&table, "{}", 2) == 0 &&
		ae_wsc_read_titles(&table, "[]", 2) == 0 && ae_wsc_read_children(NULL, children, 5) == 0);
	CHECK(table.count == 2);

	/* the cache */
	length = ae_wsc_cache_write(&table, cache, sizeof(cache));
	CHECK(length > 0 && ae_wsc_cache_read(&again, cache) && again.count == 2 && again.fetched == 1000000 &&
		!strcmp(again.collections[1].name, "SOCIAL MAP PACK") && again.collections[1].count == 2 &&
		!strcmp(again.collections[1].items[1], "2899770689"));
	CHECK(ae_wsc_cache_write(&table, cache, 30) == -1);
	CHECK(ae_wsc_cache_read(&again, "garbage\ncollection\t1\tx\t1,2\ncollection\t3001370390\t\t5\ncollection\t3001370390\tPRO\t5,x,6\r\n") &&
		again.count == 1 && again.collections[0].count == 2 && !strcmp(again.collections[0].items[1], "6"));
	/* a day old, or never, or the clock back: asked again */
	CHECK(!ae_wsc_stale(&table, 1000000 + 3600) && ae_wsc_stale(&table, 1000000 + 86400) && ae_wsc_stale(&table, 5));
	ae_wsc_table_init(&again);
	CHECK(ae_wsc_stale(&again, 1000000) && ae_wsc_stale(NULL, 0));
	if (failures) return 1;
	printf("ae_workshop_collections: ok\n");
	return 0;
}
