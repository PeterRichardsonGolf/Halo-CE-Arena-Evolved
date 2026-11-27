/* The AE pause menu's rows (port/linux/game/ae_pause_rows.c): which rows each game's pause has, headers skipped */
#include <stdio.h>
#include <string.h>

#include "ae_pause_rows.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int same(short const *rows, short count, short const *expected, short expected_count)
{
	return count == expected_count && !memcmp(rows, expected, (size_t)count * sizeof(*rows));
}

int main(void)
{
	struct ae_pause_facts facts;
	short rows[AE_PAUSE_ROWS_MAXIMUM], count;
	const char *title, *body, *yes;

	/* (a joiner in an FFA game: no CHANGE TEAMS, no host rows) */
	memset(&facts, 0, sizeof(facts));
	facts.game = AE_PAUSE_MULTIPLAYER;
	facts.local_players = 1;
	{
		static const short expected[] = { AE_PAUSE_RESUME, AE_PAUSE_SETTINGS, AE_PAUSE_SCOREBOARD, AE_PAUSE_QUIT };

		count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(same(rows, count, expected, 4));
		/* (no headers: QUIT stays in the list) */
		CHECK(!ae_pause_row_pinned(rows, count));
	}
	/* (0.9.3 a social career game: the host of its own offline game, with none of the host's rows and no CHANGE TEAMS; QUIT's
	help says what it costs) */
	facts.host = facts.teams = facts.lobby = facts.moves = facts.social = 1;
	snprintf(facts.social_quit_help, sizeof(facts.social_quit_help), "Leave the game: -1 EXP in TEAM SLAYER.");
	{
		static const short expected[] = { AE_PAUSE_RESUME, AE_PAUSE_SETTINGS, AE_PAUSE_SCOREBOARD, AE_PAUSE_QUIT };

		count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(same(rows, count, expected, 4));
		CHECK(!strcmp(ae_pause_row_help(AE_PAUSE_QUIT, &facts), "Leave the game: -1 EXP in TEAM SLAYER."));
	}
	facts.social = 0;
	facts.host = facts.teams = facts.lobby = facts.moves = 0;
	memset(facts.social_quit_help, 0, sizeof(facts.social_quit_help));
	facts.host = facts.teams = facts.lobby = facts.moves = 0;
	/* (the host of an AE lobby's team game: every row, as the mockups) */
	facts.host = facts.teams = facts.lobby = facts.moves = 1;
	{
		static const short expected[] = { AE_PAUSE_RESUME, AE_PAUSE_SETTINGS, AE_PAUSE_TEAMS, AE_PAUSE_SCOREBOARD,
			AE_PAUSE_HOST_HEADER, AE_PAUSE_OPTIONS, AE_PAUSE_MAP, AE_PAUSE_PEOPLE_HEADER, AE_PAUSE_PLAYERS, AE_PAUSE_BOTS,
			AE_PAUSE_GAME_HEADER, AE_PAUSE_RESTART_GAME, AE_PAUSE_END, AE_PAUSE_QUIT };

		count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(same(rows, count, expected, 14));
		/* (0.7.4: three groups, QUIT pinned apart) */
		CHECK(!strcmp(ae_pause_row_label(AE_PAUSE_PEOPLE_HEADER, &facts), "PEOPLE"));
		CHECK(!strcmp(ae_pause_row_label(AE_PAUSE_GAME_HEADER, &facts), "THIS GAME"));
		CHECK(ae_pause_row_look(AE_PAUSE_PEOPLE_HEADER) == AE_PAUSE_LOOK_HEADER);
		CHECK(ae_pause_row_look(AE_PAUSE_GAME_HEADER) == AE_PAUSE_LOOK_HEADER);
		CHECK(ae_pause_row_pinned(rows, count));
		CHECK(!strcmp(ae_pause_row_label(AE_PAUSE_HOST_HEADER, &facts), "GAME (HOST)"));
		CHECK(ae_pause_row_look(AE_PAUSE_HOST_HEADER) == AE_PAUSE_LOOK_HEADER);
		/* (down from SCOREBOARD lands past the header on GAME OPTIONS; up from it back on SCOREBOARD) */
		CHECK(ae_pause_row_skip(rows, count, 4, 1) == 5);
		CHECK(ae_pause_row_skip(rows, count, 4, -1) == 3);
		CHECK(ae_pause_row_skip(rows, count, 2, 1) == 2);
	}
	/* (a round game's host: RESTART ROUND after RESTART GAME; every row fits) */
	facts.rounds = 1;
	count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
	CHECK(count == 15 && rows[11] == AE_PAUSE_RESTART_GAME && rows[12] == AE_PAUSE_RESTART_ROUND &&
		rows[13] == AE_PAUSE_END);
	CHECK(count <= AE_PAUSE_ROWS_MAXIMUM);
	CHECK(ae_pause_row_confirm(AE_PAUSE_RESTART_GAME, &facts, &title, &body, &yes) && !strcmp(yes, "RESTART"));
	CHECK(ae_pause_row_confirm(AE_PAUSE_RESTART_ROUND, &facts, &title, &body, &yes) && !strcmp(yes, "RESTART ROUND"));
	facts.rounds = 0;
	/* (a host whose game is not an AE lobby's: no GAME OPTIONS, CHANGE MAP or RESTART GAME) */
	facts.lobby = 0;
	count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
	CHECK(count == 10 && rows[4] == AE_PAUSE_PEOPLE_HEADER && rows[5] == AE_PAUSE_PLAYERS && rows[6] == AE_PAUSE_BOTS &&
		rows[8] == AE_PAUSE_END);
	/* (a game without teams: no PLAYERS) */
	facts.teams = facts.moves = 0;
	count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
	CHECK(count == 8 && rows[3] == AE_PAUSE_PEOPLE_HEADER && rows[4] == AE_PAUSE_BOTS);
	facts.teams = facts.moves = 1;
	/* (0.7.6 INVITE A FRIEND: the host's, the first row under PEOPLE; a joiner's, after SCOREBOARD; no confirm) */
	facts.invite = 1;
	count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
	CHECK(count == 11 && rows[4] == AE_PAUSE_PEOPLE_HEADER && rows[5] == AE_PAUSE_INVITE && rows[6] == AE_PAUSE_PLAYERS);
	CHECK(count <= AE_PAUSE_ROWS_MAXIMUM);
	CHECK(!strcmp(ae_pause_row_label(AE_PAUSE_INVITE, &facts), "INVITE A FRIEND"));
	CHECK(!ae_pause_row_confirm(AE_PAUSE_INVITE, &facts, &title, &body, &yes));
	{
		struct ae_pause_facts joiner;

		memset(&joiner, 0, sizeof(joiner));
		joiner.game = AE_PAUSE_MULTIPLAYER;
		joiner.local_players = 1;
		joiner.invite = 1;
		count = ae_pause_rows(&joiner, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(count == 5 && rows[2] == AE_PAUSE_SCOREBOARD && rows[3] == AE_PAUSE_INVITE && rows[4] == AE_PAUSE_QUIT);
		CHECK(!ae_pause_row_pinned(rows, count));
		/* (0.7.7, E-2: a dedicated server's admin: PEOPLE's PLAYERS, SERVER's RUN THE SERVER, QUIT pinned apart) */
		joiner.invite = 0;
		joiner.teams = 1;
		joiner.server_role = 2;
		{
			static const short expected[] = { AE_PAUSE_RESUME, AE_PAUSE_SETTINGS, AE_PAUSE_TEAMS, AE_PAUSE_SCOREBOARD,
				AE_PAUSE_PEOPLE_HEADER, AE_PAUSE_PLAYERS, AE_PAUSE_SERVER_HEADER, AE_PAUSE_SERVER, AE_PAUSE_QUIT };

			count = ae_pause_rows(&joiner, rows, AE_PAUSE_ROWS_MAXIMUM);
			CHECK(same(rows, count, expected, 9));
			CHECK(ae_pause_row_pinned(rows, count));
			CHECK(!strcmp(ae_pause_row_label(AE_PAUSE_SERVER, &joiner), "RUN THE SERVER"));
			CHECK(ae_pause_row_look(AE_PAUSE_SERVER) == AE_PAUSE_LOOK_SUB);
			CHECK(ae_pause_row_look(AE_PAUSE_SERVER_HEADER) == AE_PAUSE_LOOK_HEADER);
			CHECK(strstr(ae_pause_row_help(AE_PAUSE_PLAYERS, &joiner), "ban") != NULL);
			joiner.server_role = 1;
			CHECK(strstr(ae_pause_row_help(AE_PAUSE_PLAYERS, &joiner), "ban") == NULL);
		}
		/* (no role: nothing new) */
		joiner.server_role = 0;
		count = ae_pause_rows(&joiner, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(count == 5 && rows[4] == AE_PAUSE_QUIT);
	}
	facts.invite = 0;
	/* (the confirms: END GAME, QUIT and CHANGE MAP ask; RESUME does not) */
	CHECK(ae_pause_row_confirm(AE_PAUSE_END, &facts, &title, &body, &yes) && !strcmp(yes, "END GAME"));
	CHECK(ae_pause_row_confirm(AE_PAUSE_MAP, &facts, &title, &body, &yes));
	CHECK(ae_pause_row_confirm(AE_PAUSE_QUIT, &facts, &title, &body, &yes) && !strcmp(yes, "LEAVE"));
	CHECK(!ae_pause_row_confirm(AE_PAUSE_RESUME, &facts, &title, &body, &yes));
	/* (CHANGE TEAMS asks too: it kills the player; owner 2026-10-25) */
	CHECK(ae_pause_row_confirm(AE_PAUSE_TEAMS, &facts, &title, &body, &yes) && !strcmp(yes, "CHANGE TEAMS"));
	/* (split screen: QUIT is this player's) */
	facts.local_players = 2;
	CHECK(strstr(ae_pause_row_help(AE_PAUSE_QUIT, &facts), "this player only") != NULL);
	/* (the campaign: REVERT TO SAVED, SAVE AND QUIT; co-op: FRIENDLY FIRE, QUIT; a map alone: QUIT) */
	memset(&facts, 0, sizeof(facts));
	facts.game = AE_PAUSE_CAMPAIGN;
	facts.host = facts.teams = 1;
	{
		static const short expected[] = { AE_PAUSE_RESUME, AE_PAUSE_SETTINGS, AE_PAUSE_REVERT, AE_PAUSE_QUIT };

		count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(same(rows, count, expected, 4));
		CHECK(!strcmp(ae_pause_row_label(AE_PAUSE_QUIT, &facts), "SAVE AND QUIT"));
		/* (a level that keeps no saved game file: the same rows; QUIT saves nothing and says so) */
		facts.unsaved = 1;
		count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(same(rows, count, expected, 4));
		CHECK(!strcmp(ae_pause_row_label(AE_PAUSE_QUIT, &facts), "QUIT"));
		CHECK(strstr(ae_pause_row_help(AE_PAUSE_QUIT, &facts), "no saved game") != NULL);
		{
			const char *title, *body, *yes;

			CHECK(ae_pause_row_confirm(AE_PAUSE_QUIT, &facts, &title, &body, &yes) && !strcmp(yes, "QUIT") &&
				strstr(body, "no saved game") != NULL);
		}
		facts.unsaved = 0;
	}
	facts.game = AE_PAUSE_COOP;
	{
		static const short expected[] = { AE_PAUSE_RESUME, AE_PAUSE_SETTINGS, AE_PAUSE_FRIENDLY_FIRE, AE_PAUSE_QUIT };

		count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(same(rows, count, expected, 4));
		CHECK(ae_pause_row_look(AE_PAUSE_FRIENDLY_FIRE) == AE_PAUSE_LOOK_VALUE);
	}
	facts.coop_revert = 1;
	{
		static const short expected[] = { AE_PAUSE_RESUME, AE_PAUSE_SETTINGS, AE_PAUSE_REVERT, AE_PAUSE_FRIENDLY_FIRE,
			AE_PAUSE_QUIT };

		count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(same(rows, count, expected, 5));
	}
	facts.coop_revert = 0;
	facts.game = AE_PAUSE_ALONE;
	count = ae_pause_rows(&facts, rows, AE_PAUSE_ROWS_MAXIMUM);
	CHECK(count == 3 && rows[2] == AE_PAUSE_QUIT);
	/* (1.0 split screen in progress: ADD PLAYER after SCOREBOARD when a free controller has a pad, saying why when it is not
	available; REMOVE PLAYER when more than one plays here; a social game's neither; the host's longest list fits) */
	{
		struct ae_pause_facts split;
		int at;

		memset(&split, 0, sizeof(split));
		split.game = AE_PAUSE_MULTIPLAYER;
		count = ae_pause_rows(&split, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(count == 4 && rows[2] == AE_PAUSE_SCOREBOARD && rows[3] == AE_PAUSE_QUIT);
		split.add_player = 1;
		count = ae_pause_rows(&split, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(count == 5 && rows[2] == AE_PAUSE_SCOREBOARD && rows[3] == AE_PAUSE_ADD_PLAYER && rows[4] == AE_PAUSE_QUIT);
		CHECK(!strcmp(ae_pause_row_label(AE_PAUSE_ADD_PLAYER, &split), "ADD PLAYER") && !ae_pause_row_confirm(AE_PAUSE_ADD_PLAYER, &split, &title, &body, &yes));
		CHECK(ae_pause_row_look(AE_PAUSE_ADD_PLAYER) == AE_PAUSE_LOOK_ACTION);
		split.add_player = 2;
		strcpy(split.add_reason, "THE HOST HAS TURNED OFF JOINING IN PROGRESS");
		CHECK(!strcmp(ae_pause_row_help(AE_PAUSE_ADD_PLAYER, &split), "THE HOST HAS TURNED OFF JOINING IN PROGRESS"));
		split.add_player = 0;
		split.remove_player = 1;
		count = ae_pause_rows(&split, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(count == 5 && rows[3] == AE_PAUSE_REMOVE_PLAYER);
		CHECK(!strcmp(ae_pause_row_label(AE_PAUSE_REMOVE_PLAYER, &split), "REMOVE PLAYER") && ae_pause_row_confirm(AE_PAUSE_REMOVE_PLAYER, &split, &title, &body, &yes) && !strcmp(yes, "REMOVE"));
		split.social = 1;
		split.add_player = 1;
		count = ae_pause_rows(&split, rows, AE_PAUSE_ROWS_MAXIMUM);
		for (at = 0; at < count; at++)
			CHECK(rows[at] != AE_PAUSE_ADD_PLAYER && rows[at] != AE_PAUSE_REMOVE_PLAYER);
		/* (the host's longest list: teams, an invite, the moves, rounds and a lobby, with both split rows) */
		memset(&split, 0, sizeof(split));
		split.game = AE_PAUSE_MULTIPLAYER;
		split.host = split.teams = split.lobby = split.moves = split.rounds = split.invite = 1;
		split.add_player = split.remove_player = 1;
		count = ae_pause_rows(&split, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(count == 18 && count <= AE_PAUSE_ROWS_MAXIMUM);
	}
	/* (every row has a label and a help line but the header's help) */
	{
		int row;

		facts.game = AE_PAUSE_MULTIPLAYER;
		for (row = 0; row < AE_PAUSE_ROW_KINDS; row++)
		{
			CHECK(ae_pause_row_label(row, &facts)[0] != 0);
			CHECK(ae_pause_row_look(row) == AE_PAUSE_LOOK_HEADER || ae_pause_row_help(row, &facts)[0] != 0);
		}
	}
	/* (rc9 a Forge session: SAVE MAP right after RESUME, its help by whether the map has been saved, and QUIT asks what leaving loses) */
	{
		static const short expected[] = { AE_PAUSE_RESUME, AE_PAUSE_SAVE_MAP, AE_PAUSE_SETTINGS, AE_PAUSE_QUIT };
		struct ae_pause_facts forge;

		memset(&forge, 0, sizeof(forge));
		forge.game = AE_PAUSE_ALONE;
		forge.forge = 1;
		count = ae_pause_rows(&forge, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(same(rows, count, expected, 4));
		CHECK(!strcmp(ae_pause_row_label(AE_PAUSE_SAVE_MAP, &forge), "SAVE MAP") && ae_pause_row_look(AE_PAUSE_SAVE_MAP) == AE_PAUSE_LOOK_ACTION);
		CHECK(!strncmp(ae_pause_row_help(AE_PAUSE_SAVE_MAP, &forge), "Name your map", 13));
		CHECK(ae_pause_row_confirm(AE_PAUSE_QUIT, &forge, &title, &body, &yes) && !strcmp(title, "Leave Forge?") && !strcmp(yes, "LEAVE") &&
			!strcmp(body, "Your map is not saved: everything placed is lost."));
		forge.forge_saved = 1;
		snprintf(forge.forge_name, sizeof(forge.forge_name), "GULCH REMIX");
		snprintf(forge.forge_time, sizeof(forge.forge_time), "21:14");
		CHECK(!strcmp(ae_pause_row_help(AE_PAUSE_SAVE_MAP, &forge), "Saves GULCH REMIX over its last save (21:14). Save a copy under another name from Forge's own menu."));
		CHECK(ae_pause_row_confirm(AE_PAUSE_QUIT, &forge, &title, &body, &yes) && !strcmp(body, "Anything placed since your last save (21:14) is lost."));
		forge.forge_time[0] = 0;
		CHECK(ae_pause_row_confirm(AE_PAUSE_QUIT, &forge, &title, &body, &yes) && !strcmp(body, "Anything placed since your last save is lost."));
		/* a game that is not Forge has no SAVE MAP */
		forge.forge = 0;
		count = ae_pause_rows(&forge, rows, AE_PAUSE_ROWS_MAXIMUM);
		CHECK(count == 3 && rows[1] == AE_PAUSE_SETTINGS);
		CHECK(ae_pause_row_confirm(AE_PAUSE_QUIT, &forge, &title, &body, &yes) && !strcmp(title, "Quit?"));
	}
	if (failures)
		return 1;
	printf("ae_pause_rows_test: ok\n");
	return 0;
}
