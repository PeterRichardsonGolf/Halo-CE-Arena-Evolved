#include <stdio.h>
#include <string.h>
#include "ae_recent_games.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_recent_games recent, again;
static char text[AE_RECENT_TEXT];

/* the maps and gametypes "here": every one but hotbox and AE 2V2 SLAY */
static int exists(const char *id, void *context)
{
	(*(int *)context)++;
	return strcmp(id, "hotbox") != 0 && strcmp(id, "AE 2V2 SLAY") != 0;
}

int main(void)
{
	short shown[AE_RECENT_SHOWN], count;
	char map[16];
	int index, asked = 0;

	ae_recent_games_init(&recent);
	CHECK(!recent.count);
	/* newest first; the same pair again moves to the top, another gametype on a map is another game */
	CHECK(ae_recent_games_add(&recent, "prisoner", "AE FFA SLAY"));
	CHECK(ae_recent_games_add(&recent, "bloodgulch", "AE BTB CTF"));
	CHECK(ae_recent_games_add(&recent, "hotbox", "AE 2V2 SLAY"));
	CHECK(recent.count == 3 && !strcmp(recent.games[0].map, "hotbox"));
	CHECK(ae_recent_games_add(&recent, "prisoner", "AE FFA SLAY"));
	CHECK(recent.count == 3 && !strcmp(recent.games[0].map, "prisoner") && !strcmp(recent.games[2].map, "bloodgulch"));
	CHECK(ae_recent_games_add(&recent, "prisoner", "saved:BR CTF 5CAP"));
	CHECK(recent.count == 4 && !strcmp(recent.games[1].gametype, "AE FFA SLAY"));
	/* bad ids: refused */
	CHECK(!ae_recent_games_add(&recent, "", "AE FFA SLAY") && !ae_recent_games_add(&recent, "a|b", "x"));
	CHECK(!ae_recent_games_add(&recent, "x", NULL) && recent.count == 4);

	/* RECENT per picker (owner 2026-10-28). The list: prisoner | BR CTF 5CAP, prisoner | AE FFA SLAY, hotbox | AE 2V2
	SLAY, bloodgulch | AE BTB CTF. CHANGE MAP's: each map once, newest first, hotbox not here (each map asked once) */
	count = ae_recent_games_by(&recent, AE_RECENT_BY_MAP, exists, &asked, shown, AE_RECENT_SHOWN);
	CHECK(count == 2 && shown[0] == 0 && shown[1] == 3 && asked == 3);
	/* CHANGE GAMETYPE's: each gametype once (the one prisoner played last first), AE 2V2 SLAY not here */
	asked = 0;
	count = ae_recent_games_by(&recent, AE_RECENT_BY_GAMETYPE, exists, &asked, shown, AE_RECENT_SHOWN);
	CHECK(count == 3 && shown[0] == 0 && shown[1] == 1 && shown[2] == 3 && asked == 4);
	CHECK(!strcmp(recent.games[shown[0]].gametype, "saved:BR CTF 5CAP") &&
		!strcmp(recent.games[shown[2]].gametype, "AE BTB CTF"));
	/* all there (exists NULL); the maximum; a kind of no picker's */
	CHECK(ae_recent_games_by(&recent, AE_RECENT_BY_MAP, NULL, NULL, shown, AE_RECENT_SHOWN) == 3);
	CHECK(ae_recent_games_by(&recent, AE_RECENT_BY_GAMETYPE, NULL, NULL, shown, 2) == 2 && shown[1] == 1);
	CHECK(!ae_recent_games_by(&recent, 2, NULL, NULL, shown, AE_RECENT_SHOWN));
	CHECK(!ae_recent_games_by(NULL, AE_RECENT_BY_MAP, NULL, NULL, shown, AE_RECENT_SHOWN));
	/* one map played with many gametypes: one row in CHANGE MAP's; CHANGE GAMETYPE's has each gametype once (AE MODE 3
	on Blood Gulch newest, on Prisoner not again), the first 8 */
	ae_recent_games_init(&again);
	for (index = 0; index < 12; index++)
	{
		char gametype[24];

		snprintf(gametype, sizeof(gametype), "AE MODE %d", index);
		CHECK(ae_recent_games_add(&again, "prisoner", gametype));
	}
	CHECK(ae_recent_games_add(&again, "bloodgulch", "AE MODE 3"));
	count = ae_recent_games_by(&again, AE_RECENT_BY_MAP, NULL, NULL, shown, AE_RECENT_SHOWN);
	CHECK(count == 2 && !strcmp(again.games[shown[0]].map, "bloodgulch") && !strcmp(again.games[shown[1]].map, "prisoner"));
	count = ae_recent_games_by(&again, AE_RECENT_BY_GAMETYPE, NULL, NULL, shown, AE_RECENT_SHOWN);
	CHECK(count == AE_RECENT_SHOWN && !strcmp(again.games[shown[0]].gametype, "AE MODE 3") &&
		!strcmp(again.games[shown[1]].gametype, "AE MODE 11") && !strcmp(again.games[shown[7]].gametype, "AE MODE 5"));

	/* 30 kept, 8 shown at most */
	for (index = 0; index < 40; index++)
	{
		snprintf(map, sizeof(map), "map%d", index);
		ae_recent_games_add(&recent, map, "AE FFA SLAY");
	}
	CHECK(recent.count == AE_RECENT_KEPT && !strcmp(recent.games[0].map, "map39") &&
		!strcmp(recent.games[AE_RECENT_KEPT - 1].map, "map10"));
	CHECK(ae_recent_games_by(&recent, AE_RECENT_BY_MAP, NULL, NULL, shown, 20) == AE_RECENT_SHOWN);

	/* the file, written and read back the same */
	CHECK(ae_recent_games_write(&recent, text, sizeof(text)) > 0);
	CHECK(strstr(text, "# AE recent games\ngame = map39 | AE FFA SLAY\n") == text);
	CHECK(ae_recent_games_read(&again, text) && again.count == AE_RECENT_KEPT &&
		!strcmp(again.games[29].map, "map10"));
	CHECK(ae_recent_games_write(&recent, text, 30) == -1);
	/* reading: spaces, CRLF, a bad id and a repeat dropped; a line of no known kind refuses the file */
	CHECK(ae_recent_games_read(&again, "# x\r\n game =  hotbox |AE 2V2 SLAY \r\ngame = | x\ngame = hotbox | AE 2V2 SLAY\n"));
	CHECK(again.count == 1 && !strcmp(again.games[0].map, "hotbox") && !strcmp(again.games[0].gametype, "AE 2V2 SLAY"));
	CHECK(!ae_recent_games_read(&again, "map = hotbox | x\n") && !again.count);
	CHECK(!ae_recent_games_read(&again, "game = hotbox\n"));
	CHECK(ae_recent_games_read(&again, "") && !again.count);
	/* the folded choice per picker: open by default, kept through write and read, a bad value refused */
	CHECK(!again.maps_folded && !again.gametypes_folded);
	ae_recent_games_init(&again);
	CHECK(ae_recent_games_add(&again, "hotbox", "AE 2V2 SLAY"));
	again.gametypes_folded = 1;
	CHECK(ae_recent_games_write(&again, text, sizeof(text)) > 0 && !strstr(text, "folded"));
	CHECK(ae_recent_folds_write(&again, text, sizeof(text)) > 0 && strstr(text, "folded = gametypes"));
	CHECK(!strstr(text, "folded = maps") && !strstr(text, "game ="));
	CHECK(ae_recent_games_read(&recent, text) && recent.count == 0 && !recent.maps_folded && recent.gametypes_folded);
	CHECK(ae_recent_games_read(&recent, "folded = maps\n") && recent.maps_folded && !recent.gametypes_folded);
	CHECK(!ae_recent_games_read(&recent, "folded = both\n"));

	if (failures) return 1;
	printf("ae_recent_games: ok\n");
	return 0;
}
