#include <stdio.h>
#include <string.h>
#include "ae_matchmaking_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static const char file[] =
	"# MATCHMAKING's playlists\n"
	"casual|CASUAL|Relaxed matches. Jump in, no setup.|play.arenaevolved.com:10660 play2.example:10660\n"
	"\n"
	"btb|BTB|Big team battles with vehicles.|btb.arenaevolved.com:10672\r\n"
	"comp|COMP|Competitive rules, small teams.|comp.arenaevolved.com:10683\n";

int main(void)
{
	struct ae_mm_playlist lists[4];
	struct ae_mm_server_seen seen[3];
	int count;

	/* playlist_parse_three: three playlists, comments and blank lines skipped, CRLF taken */
	count = ae_mm_playlists_parse(file, lists, 4);
	CHECK(count == 3);
	CHECK(strcmp(lists[0].id, "casual") == 0 && strcmp(lists[0].label, "CASUAL") == 0);
	CHECK(strcmp(lists[0].description, "Relaxed matches. Jump in, no setup.") == 0);
	CHECK(lists[0].address_count == 2);
	CHECK(strcmp(lists[0].addresses[0], "play.arenaevolved.com:10660") == 0);
	CHECK(strcmp(lists[0].addresses[1], "play2.example:10660") == 0);
	CHECK(strcmp(lists[1].addresses[0], "btb.arenaevolved.com:10672") == 0 && lists[1].address_count == 1);
	CHECK(strcmp(lists[2].id, "comp") == 0);
	/* the maximum is kept */
	CHECK(ae_mm_playlists_parse(file, lists, 2) == 2);

	/* playlist_parse_skips_bad_lines: too few fields, no address, an empty id or label */
	count = ae_mm_playlists_parse("a|A|no address|\nb|B\n|C|x|h:1\nd||x|h:1\nok|OK|fine|h:2\n", lists, 4);
	CHECK(count == 1 && strcmp(lists[0].id, "ok") == 0);
	CHECK(ae_mm_playlists_parse(NULL, lists, 4) == 0);
	CHECK(ae_mm_playlists_parse("", lists, 4) == 0);

	ae_mm_playlists_parse(file, lists, 4);
	memset(seen, 0, sizeof(seen));
	snprintf(seen[0].address, sizeof(seen[0].address), "play.arenaevolved.com:10660");
	seen[0].players = 15;
	seen[0].max_players = 16;
	seen[0].reachable = 1;
	snprintf(seen[1].address, sizeof(seen[1].address), "play2.example:10660");
	seen[1].players = 2;
	seen[1].max_players = 16;
	seen[1].reachable = 1;

	/* playlist_pick_first_with_room: a party of one fits the first; of two, only the second */
	CHECK(ae_mm_playlist_pick(&lists[0], seen, 2, 0, 1) == 0);
	CHECK(ae_mm_playlist_pick(&lists[0], seen, 2, 0, 2) == 1);
	/* playlist_pick_skips_tried */
	CHECK(ae_mm_playlist_pick(&lists[0], seen, 2, 1 << 0, 1) == 1);
	CHECK(ae_mm_playlist_pick(&lists[0], seen, 2, (1 << 0) | (1 << 1), 1) == -1);
	/* playlist_pick_party_too_big */
	CHECK(ae_mm_playlist_pick(&lists[0], seen, 2, 0, 15) == -1);
	/* an unreachable server is skipped; one not seen yet counts as reachable with room (tried) */
	seen[0].reachable = 0;
	CHECK(ae_mm_playlist_pick(&lists[0], seen, 2, 0, 1) == 1);
	CHECK(ae_mm_playlist_pick(&lists[1], seen, 2, 0, 4) == 0);
	/* playlist_pick_all_full */
	seen[0].reachable = 1;
	seen[0].players = 16;
	seen[1].players = 16;
	CHECK(ae_mm_playlist_pick(&lists[0], seen, 2, 0, 1) == -1);
	CHECK(ae_mm_playlist_pick(NULL, seen, 2, 0, 1) == -1);

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
