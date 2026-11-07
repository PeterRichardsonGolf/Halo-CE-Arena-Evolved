/* ae_server_lobby_rules_test.c: a dedicated server's lobby page (ae_server_lobby_rules.h) */
#include <stdio.h>
#include <string.h>

#include "ae_server_lobby_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void sample(struct ae_sl_wire *state)
{
	memset(state, 0, sizeof(*state));
	state->flags = AE_SL_FLAG_NOMINATE | AE_SL_FLAG_RTV | AE_SL_FLAG_MAP_VOTE;
	state->team_rule = AE_SL_TEAMS_BALANCED;
	state->join = AE_SL_JOIN_AE;
	state->min_players = 2;
	state->max_players = 8;
	state->bots_red = 3;
	state->bots_blue = 4;
	state->bots = 7;
	state->vote_seconds = 20;
	snprintf(state->region, sizeof(state->region), "NA WEST");
	snprintf(state->motd, sizeof(state->motd), "AE West test servers are up! Be kind.");
	state->count = 3;
	state->entries[0].roles = AE_SL_ENTRY_NOW | AE_SL_ENTRY_VOTE;
	snprintf(state->entries[0].map, AE_SL_MAP, "pat_prisoner@pro");
	snprintf(state->entries[0].gametype, AE_SL_GAMETYPE, "ae_br_ts");
	snprintf(state->entries[0].name, AE_SL_NAME, "AE BR TS");
	ae_sl_facts(4, "BR START", "50 KILLS", "15 MINUTES", state->entries[0].facts, AE_SL_FACTS);
	state->entries[1].roles = AE_SL_ENTRY_NEXT;
	snprintf(state->entries[1].map, AE_SL_MAP, "hektik@smp");
	snprintf(state->entries[1].gametype, AE_SL_GAMETYPE, "ev_team_sly");
	snprintf(state->entries[1].name, AE_SL_NAME, "EV TEAM SLY");
	state->entries[2].roles = AE_SL_ENTRY_NEXT | AE_SL_ENTRY_VOTE;
	snprintf(state->entries[2].map, AE_SL_MAP, "frigid_keep@smp");
	snprintf(state->entries[2].gametype, AE_SL_GAMETYPE, "ae_snd");
}

static void test_wire(void)
{
	struct ae_sl_wire state, read;
	unsigned char bytes[AE_SL_WIRE_MAXIMUM + 8];
	int size;

	sample(&state);
	size = ae_sl_wire_encode(&state, bytes, (int)sizeof(bytes));
	CHECK(size == AE_SL_HEAD + 3 * AE_SL_ENTRY);
	CHECK(ae_sl_wire_decode(bytes, size, &read));
	CHECK(read.flags == state.flags && read.team_rule == state.team_rule && read.join == AE_SL_JOIN_AE);
	CHECK(read.min_players == 2 && read.max_players == 8 && read.bots_red == 3 && read.bots_blue == 4 && read.bots == 7);
	CHECK(read.vote_seconds == 20 && read.count == 3);
	CHECK(!strcmp(read.region, "NA WEST") && !strcmp(read.motd, state.motd));
	CHECK(!strcmp(read.entries[0].facts, "4 V 4 \xE2\x80\xA2 BR START \xE2\x80\xA2 50 KILLS \xE2\x80\xA2 15 MIN"));
	CHECK(!strcmp(read.entries[2].map, "frigid_keep@smp") && read.entries[2].roles == (AE_SL_ENTRY_NEXT | AE_SL_ENTRY_VOTE));
	/* (the whole message fits a distributed message: 4095 bytes less its header) */
	CHECK(AE_SL_WIRE_MAXIMUM < 4000);
	/* (short, long, another version, a count that does not match the size: refused) */
	CHECK(!ae_sl_wire_decode(bytes, size - 1, &read));
	CHECK(!ae_sl_wire_decode(bytes, AE_SL_HEAD - 1, &read));
	bytes[0] = 2;
	CHECK(!ae_sl_wire_decode(bytes, size, &read));
	bytes[0] = AE_SL_WIRE_VERSION;
	bytes[10] = 2;
	CHECK(!ae_sl_wire_decode(bytes, size, &read));
	bytes[10] = 3;
	/* (a text not ended, a control character, an unknown flag, a rule out of range, an entry with no role) */
	memset(bytes + 11, 'x', AE_SL_REGION);
	CHECK(!ae_sl_wire_decode(bytes, size, &read));
	bytes[11] = '\n';
	bytes[12] = 0;
	CHECK(!ae_sl_wire_decode(bytes, size, &read));
	bytes[11] = 'N';
	CHECK(ae_sl_wire_decode(bytes, size, &read));
	bytes[1] = 0x40;
	CHECK(!ae_sl_wire_decode(bytes, size, &read));
	bytes[1] = 0;
	bytes[2] = 3;
	CHECK(!ae_sl_wire_decode(bytes, size, &read));
	bytes[2] = 0;
	bytes[AE_SL_HEAD] = 0;
	CHECK(!ae_sl_wire_decode(bytes, size, &read));
	bytes[AE_SL_HEAD] = 8;
	CHECK(!ae_sl_wire_decode(bytes, size, &read));
	bytes[AE_SL_HEAD] = 1;
	/* (an entry with no map) */
	bytes[AE_SL_HEAD + 1] = 0;
	CHECK(!ae_sl_wire_decode(bytes, size, &read));
	/* (too many entries, an unknown role: not encoded) */
	state.count = AE_SL_ENTRIES + 1;
	CHECK(!ae_sl_wire_encode(&state, bytes, (int)sizeof(bytes)));
	sample(&state);
	state.entries[1].roles = 8;
	CHECK(!ae_sl_wire_encode(&state, bytes, (int)sizeof(bytes)));
	sample(&state);
	CHECK(!ae_sl_wire_encode(&state, bytes, AE_SL_HEAD));
	/* (no entries at all: still a message) */
	state.count = 0;
	size = ae_sl_wire_encode(&state, bytes, (int)sizeof(bytes));
	CHECK(size == AE_SL_HEAD && ae_sl_wire_decode(bytes, size, &read) && read.count == 0);
}

static void test_entries(void)
{
	struct ae_sl_wire state;
	char text[64];

	sample(&state);
	CHECK(ae_sl_entry_with(&state, AE_SL_ENTRY_NOW, 0) == 0);
	CHECK(ae_sl_entry_with(&state, AE_SL_ENTRY_NEXT, 0) == 1);
	CHECK(ae_sl_entry_with(&state, AE_SL_ENTRY_NEXT, 1) == 2);
	CHECK(ae_sl_entry_with(&state, AE_SL_ENTRY_NEXT, 2) == -1);
	CHECK(ae_sl_entry_with(&state, AE_SL_ENTRY_VOTE, 1) == 2);
	CHECK(ae_sl_entry_of(&state, "frigid_keep@smp", "ae_snd") == 2);
	CHECK(ae_sl_entry_of(&state, "frigid_keep@smp", "ae_ctf") == -1);
	ae_sl_gametype_word("ae_br_ts", text, (int)sizeof(text));
	CHECK(!strcmp(text, "AE BR TS"));
	ae_sl_gametype_word("ae_btb_plus_ctf+score=3", text, (int)sizeof(text));
	CHECK(!strcmp(text, "AE BTB+ CTF"));
	ae_sl_gametype_word("team_slayer+score=50+time=1", text, (int)sizeof(text));
	CHECK(!strcmp(text, "TEAM SLAYER"));
	ae_sl_gametype_word("file=gametypes/my_swat.toml", text, (int)sizeof(text));
	CHECK(!strcmp(text, "MY SWAT"));
	ae_sl_gametype_word("ae_team_sly", text, 6);
	CHECK(!strcmp(text, "AE TE"));
	ae_sl_facts(0, "PISTOL START", "25 KILLS", "NO TIME LIMIT", text, (int)sizeof(text));
	CHECK(!strcmp(text, "FREE FOR ALL \xE2\x80\xA2 PISTOL START \xE2\x80\xA2 25 KILLS"));
	ae_sl_facts(4, "", "3 CAPTURES", "1 MINUTE", text, (int)sizeof(text));
	CHECK(!strcmp(text, "4 V 4 \xE2\x80\xA2 3 CAPTURES \xE2\x80\xA2 1 MIN"));
	/* (cut in its middle: never half a dot) */
	ae_sl_facts(4, "BR START", "50 KILLS", "15 MINUTES", text, 9);
	CHECK(!strcmp(text, "4 V 4"));
}

static void test_words(void)
{
	char text[96];
	struct ae_sl_help help;

	CHECK(ae_sl_state(1, 5, 1, 0) == AE_SL_STATE_DOWNLOAD);
	CHECK(ae_sl_state(0, 0, 1, 0) == AE_SL_STATE_COUNTDOWN);
	CHECK(ae_sl_state(0, -1, 1, 2) == AE_SL_STATE_RESULT);
	CHECK(ae_sl_state(0, -1, 1, -1) == AE_SL_STATE_VOTE);
	CHECK(ae_sl_state(0, -1, 0, -1) == AE_SL_STATE_WAITING);
	CHECK(!strcmp(ae_sl_join_word(AE_SL_JOIN_AE), "AE") && !strcmp(ae_sl_join_word(AE_SL_JOIN_ANYONE), "ANYONE"));
	CHECK(!strcmp(ae_sl_join_word(AE_SL_JOIN_AE_MCC), "AE+MCC"));
	ae_sl_people_line(1, 8, 7, text, (int)sizeof(text));
	CHECK(!strcmp(text, "1 OF 8 PEOPLE \xE2\x80\xA2 7 BOTS"));
	ae_sl_people_line(4, 8, 1, text, (int)sizeof(text));
	CHECK(!strcmp(text, "4 OF 8 PEOPLE \xE2\x80\xA2 1 BOT"));
	ae_sl_people_line(8, 8, 0, text, (int)sizeof(text));
	CHECK(!strcmp(text, "8 OF 8 PEOPLE"));
	ae_sl_team_count(1, 3, text, (int)sizeof(text));
	CHECK(!strcmp(text, "1 + 3 BOTS"));
	ae_sl_team_count(3, 1, text, (int)sizeof(text));
	CHECK(!strcmp(text, "3 + 1 BOT"));
	ae_sl_team_count(4, 0, text, (int)sizeof(text));
	CHECK(!strcmp(text, "4"));
	ae_sl_bots_line(3, text, (int)sizeof(text));
	CHECK(!strcmp(text, "+ 3 BOTS, LEAVING AS PEOPLE JOIN"));
	ae_sl_bots_line(1, text, (int)sizeof(text));
	CHECK(!strcmp(text, "+ 1 BOT, LEAVING AS PEOPLE JOIN"));
	ae_sl_bots_line(0, text, (int)sizeof(text));
	CHECK(!text[0]);
	ae_sl_waiting_line(2, 1, 0, text, (int)sizeof(text));
	CHECK(!strcmp(text, "Starts when 2 people are in. 1 is in now."));
	ae_sl_waiting_line(4, 3, 0, text, (int)sizeof(text));
	CHECK(!strcmp(text, "Starts when 4 people are in. 3 are in now."));
	ae_sl_waiting_line(1, 0, 0, text, (int)sizeof(text));
	CHECK(!strcmp(text, "Starts once someone is in."));
	ae_sl_waiting_line(1, 1, 0, text, (int)sizeof(text));
	CHECK(!strcmp(text, "Everyone it waits for is in. It starts in a moment."));
	ae_sl_waiting_line(8, 1, 6, text, (int)sizeof(text));
	CHECK(!strcmp(text, "Starts when 8 players are in, bots counted. 7 are in now."));
	ae_sl_waiting_line(8, 2, 6, text, (int)sizeof(text));
	CHECK(!strcmp(text, "Everyone it waits for is in. It starts in a moment."));
	ae_sl_votes_word(1, text, (int)sizeof(text));
	CHECK(!strcmp(text, "1 VOTE"));
	ae_sl_votes_word(0, text, (int)sizeof(text));
	CHECK(!strcmp(text, "0 VOTES"));
	ae_sl_won_line(3, 4, text, (int)sizeof(text));
	CHECK(!strcmp(text, "3 of 4 votes. The game starts in a moment."));
	CHECK(ae_sl_pips(4, 3) == 4 && ae_sl_pips(1, 3) == 3 && ae_sl_pips(0, 0) == 1 && ae_sl_pips(20, 2) == 8);
	memset(&help, 0, sizeof(help));
	help.team_game = 1;
	help.team_rule = AE_SL_TEAMS_OPEN;
	help.focus = AE_SL_FOCUS_TEAM;
	ae_sl_help_text(&help, text, (int)sizeof(text));
	CHECK(!strcmp(text, "Left or right: the other team. The server allows team changes before a game."));
	help.team_rule = AE_SL_TEAMS_LOCKED;
	ae_sl_help_text(&help, text, (int)sizeof(text));
	CHECK(!strcmp(text, "This server puts players on teams itself."));
	help.state = AE_SL_STATE_VOTE;
	help.focus = AE_SL_FOCUS_LIST;
	ae_sl_help_text(&help, text, (int)sizeof(text));
	CHECK(!strcmp(text, "{A} votes for a game. You can change your vote until the time is up."));
	help.split = 1;
	ae_sl_help_text(&help, text, (int)sizeof(text));
	CHECK(!strcmp(text, "One vote for this PC. Each controller changes its own team with {Y}."));
	help.state = AE_SL_STATE_RESULT;
	help.map = "Hektik";
	ae_sl_help_text(&help, text, (int)sizeof(text));
	CHECK(!strcmp(text, "The vote is over. Hektik is next."));
	help.state = AE_SL_STATE_DOWNLOAD;
	help.pack_map = 1;
	ae_sl_help_text(&help, text, (int)sizeof(text));
	CHECK(!strcmp(text, "Hektik is a pack map: the server sends it. {B} leaves the server and stops the download."));
	help.focus = AE_SL_FOCUS_SERVER;
	ae_sl_help_text(&help, text, (int)sizeof(text));
	CHECK(!strncmp(text, "The server's page", 17));
	help.focus = AE_SL_FOCUS_PLAYER;
	help.player = "Ruckus";
	help.player_bot = 1;
	ae_sl_help_text(&help, text, (int)sizeof(text));
	CHECK(!strcmp(text, "Ruckus is a bot: it leaves the game as people join."));
}

static void test_title(void)
{
	struct ae_sl_run runs[8];
	int count = ae_sl_title_runs("AE OFFICIAL #1", runs, 8);

	CHECK(count == 2);
	CHECK(runs[0].start == 0 && runs[0].length == 12 && !runs[0].numerals);
	CHECK(runs[1].start == 12 && runs[1].length == 2 && runs[1].numerals);
	count = ae_sl_title_runs("2V2 #10 EU", runs, 8);
	CHECK(count == 6 && runs[0].numerals && runs[1].length == 1 && !runs[1].numerals && runs[2].numerals &&
		!runs[3].numerals && runs[4].numerals && runs[4].length == 3 && !runs[5].numerals && runs[5].length == 3);
	CHECK(ae_sl_title_runs("", runs, 8) == 0);
	CHECK(ae_sl_title_runs("A1B2C3", runs, 2) == 2);
}

int main(void)
{
	test_wire();
	test_entries();
	test_words();
	test_title();
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_server_lobby_rules: ok\n");
	return 0;
}
