#include <stdio.h>
#include <string.h>

#include "ae_record_columns_rules.h"
#include "ae_record_focus.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define STR(a, b) do { if (strcmp((a), (b))) { printf("FAIL %s:%d \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
	failures++; } } while (0)

static unsigned long medals[AE_MEDAL_COUNT];
static struct ae_rs_stats x;
static struct ae_rs_weapon weapons[AE_RS_WEAPONS];

/* a player of 218 games: flag 41 (24 won), hill 27, slayer 150; every game counted, 196 with the version 3 numbers */
static void player(struct ae_rv_in *in)
{
	memset(in, 0, sizeof(*in));
	memset(&x, 0, sizeof(x));
	memset(medals, 0, sizeof(medals));
	in->games = 218; in->wins = 120; in->kills = 3000; in->deaths = 2500; in->assists = 900;
	in->kind[AE_RS_KIND_SLAYER].games = 150; in->kind[AE_RS_KIND_SLAYER].wins = 80;
	in->kind[AE_RS_KIND_SLAYER].kills = 2100; in->kind[AE_RS_KIND_SLAYER].deaths = 1750; in->kind[AE_RS_KIND_SLAYER].assists = 615;
	in->kind[AE_RS_KIND_FLAG].games = 41; in->kind[AE_RS_KIND_FLAG].wins = 24;
	in->kind[AE_RS_KIND_HILL].games = 27; in->kind[AE_RS_KIND_HILL].wins = 15;
	in->kind[AE_RS_KIND_HILL].kills = 352; in->kind[AE_RS_KIND_HILL].deaths = 319; in->kind[AE_RS_KIND_HILL].assists = 140;
	x.counted = 218; x.counted_v3 = 196; x.best_spree = 14;
	x.flag_caps = 38; x.flag_returns = 61; x.flag_grabs = 87; x.as_carrier = 23; x.carrier_seconds = 2480;
	x.hill_ticks = 2970L * 30; x.fired = 41880; x.hit = 17170; x.kills_v3 = 2812; x.headshots = 612;
	x.dealt = 284310; x.taken = 236940; x.seconds_v3 = 196 * 600; x.hit_v3 = 15400; x.seconds = 218 * 600;
	medals[AE_MEDAL_FLAG_CARRIER_KILL] = 74;
	medals[AE_MEDAL_FLAG_CAPTURED] = 38;
	medals[AE_MEDAL_HILL_DEFENSE] = 23;
	medals[AE_MEDAL_HILL_DOMINANCE] = 2;
	medals[AE_MEDAL_PERFECTION] = 1;
	medals[AE_MEDAL_STEAKTACULAR] = 9;
	in->x = &x;
	in->medals = medals;
	snprintf(weapons[0].name, sizeof(weapons[0].name), "battle rifle"); weapons[0].kills = 1102;
	snprintf(weapons[1].name, sizeof(weapons[1].name), "sniper rifle"); weapons[1].kills = 388;
	in->weapons = weapons;
	in->weapon_count = 2;
}

int main(void)
{
	struct ae_rv_in in;
	struct ae_rc_column columns[AE_RC_COMBAT_COLUMNS > AE_RC_MODE_COLUMNS ? AE_RC_COMBAT_COLUMNS : AE_RC_MODE_COLUMNS];
	struct ae_rc_chip chips[AE_RS_KINDS];
	struct ae_rc_head head;
	struct ae_sr_last last;
	int missing[AE_RC_OBJECTIVE_KINDS], missing_count, count, column, first;
	char text[160];

	/* pages go round */
	STR(ae_rc_page_name(AE_RC_WEAPONS_MAPS), "WEAPONS & MAPS");
	CHECK(ae_rc_page_step(AE_RC_MODES, -1) == AE_RC_MEDALS && ae_rc_page_step(AE_RC_MEDALS, 1) == AE_RC_MODES);
	/* every box with a page opens one; MEDALS and VS YOU open none of the window's */
	CHECK(ae_rc_page_of_box(AE_RF_OBJECTIVES, &column) == AE_RC_MODES && column == -1);
	CHECK(ae_rc_page_of_box(AE_RF_DAMAGE, &column) == AE_RC_COMBAT && column == 2);
	CHECK(ae_rc_page_of_box(AE_RF_MAPS, &column) == AE_RC_WEAPONS_MAPS && column == 1);
	CHECK(ae_rc_page_of_box(AE_RF_TOTALS, &column) == AE_RC_MODES && ae_rc_page_of_box(AE_RF_IDENTITY, &column) == AE_RC_WEAPONS_MAPS);
	CHECK(ae_rc_page_of_box(AE_RF_MEDALS, NULL) == -1 && ae_rc_page_of_box(AE_RF_VERSUS, NULL) == -1);

	/* clocks: m:ss, h:mm:ss from an hour */
	ae_rc_clock(44, text, sizeof(text)); STR(text, "0:44");
	ae_rc_clock(2970, text, sizeof(text)); STR(text, "49:30");
	ae_rc_clock(3725, text, sizeof(text)); STR(text, "1:02:05");

	/* the head */
	player(&in);
	memset(&last, 0, sizeof(last));
	last.has = 1; last.result = 1; last.mode = AE_RS_KIND_FLAG;
	snprintf(last.map, sizeof(last.map), "hang 'em high");
	ae_rc_head(&in, &last, "haloae.com", &head);
	STR(head.games, "218"); STR(head.win_rate, "55%"); STR(head.kd, "1.20"); STR(head.best_spree, "14");
	STR(head.last, "LAST: LOST ON HANG 'EM HIGH, FLAG"); STR(head.initial, "H");
	ae_rc_head(&in, NULL, "_7x", &head);
	STR(head.last, ""); STR(head.initial, "7");

	/* chips: the kinds played, in the columns' order, the favourite on */
	count = ae_rc_chips(&in, chips);
	CHECK(count == 3);
	STR(chips[0].name, "SLAYER"); STR(chips[0].count, "150 GAMES  53%"); CHECK(chips[0].on && !chips[1].on);
	STR(chips[0].games, "150 GAMES");
	STR(chips[1].name, "FLAG"); STR(chips[2].name, "HILL");
	CHECK(ae_rc_favourite_kind(&in) == AE_RS_KIND_SLAYER);
	/* the lit chip is the ringed column's kind; none for the NOT PLAYED YET column */
	CHECK(ae_rc_chips_light(chips, count, AE_RS_KIND_HILL) == 2 && chips[2].on && !chips[0].on && !chips[1].on);
	CHECK(ae_rc_chips_light(chips, count, AE_RC_RING_NOT_PLAYED) == -1 && !chips[0].on && !chips[1].on && !chips[2].on);

	/* MODES: a column for each kind played (SLAYER and OTHER too), the objective kinds not played share one */
	count = ae_rc_mode_columns(&in, columns, missing, &missing_count);
	CHECK(count == 3 && missing_count == 3);
	CHECK(missing[0] == AE_RS_KIND_BALL && missing[1] == AE_RS_KIND_BOMB && missing[2] == AE_RS_KIND_RACE);
	STR(columns[0].name, "SLAYER"); STR(columns[1].name, "FLAG"); STR(columns[2].name, "HILL");
	CHECK(columns[0].kind == AE_RS_KIND_SLAYER && columns[1].kind == chips[1].kind && columns[2].kind == chips[2].kind);
	/* slayer: kills and assists a game from every game of the kind, K/D, deaths and the spread, slayer's own medals */
	STR(columns[0].games, "150 GAMES  80 W  70 L"); STR(columns[0].meter_word, "53% WON");
	STR(columns[0].hero[0].label, "KILLS"); STR(columns[0].hero[0].value, "2,100"); STR(columns[0].hero[0].side, "14.0 A GAME");
	STR(columns[0].hero[1].label, "K/D"); STR(columns[0].hero[1].value, "1.20"); STR(columns[0].hero[1].side, "");
	STR(columns[0].hero[2].label, "ASSISTS"); STR(columns[0].hero[2].value, "615"); STR(columns[0].hero[2].side, "4.1 A GAME");
	STR(columns[0].row[0].label, "DEATHS"); STR(columns[0].row[0].value, "1,750");
	STR(columns[0].row[1].label, "KILL SPREAD"); STR(columns[0].row[1].value, "+350");
	STR(columns[0].note, ""); STR(columns[0].medals_head, "SLAYER MEDALS");
	CHECK(columns[0].medal_count == 2 && columns[0].medals_earned == 2);
	CHECK((columns[0].medal[0] == AE_MEDAL_PERFECTION && columns[0].medal_n[1] == 9) ||
		(columns[0].medal[1] == AE_MEDAL_PERFECTION && columns[0].medal_n[0] == 9));
	STR(columns[1].games, "41 GAMES  24 W  17 L"); STR(columns[1].meter_word, "59% WON");
	STR(columns[1].tag, "BEST WIN RATE"); STR(columns[0].tag, ""); STR(columns[2].tag, "");
	STR(columns[1].hero[0].label, "CAPTURES"); STR(columns[1].hero[0].value, "38"); STR(columns[1].hero[0].side, "0.9 A GAME");
	STR(columns[1].hero[2].label, "CARRIERS KILLED"); STR(columns[1].hero[2].value, "74");
	STR(columns[1].row[2].label, "TIME AS CARRIER"); STR(columns[1].row[2].value, "41:20");
	STR(columns[1].note, "KILLS AND TIME AS CARRIER FROM 196 OF 218 GAMES");
	STR(columns[1].medals_head, "FLAG MEDALS");
	CHECK(columns[1].medal_count == 2 && columns[1].medal[0] == AE_MEDAL_FLAG_CAPTURED);
	/* hill: the time a clock, its per game a clock; kills a game from every game; K/D has no per game */
	STR(columns[2].hero[0].value, "49:30"); STR(columns[2].hero[0].side, "1:50 A GAME");
	STR(columns[2].hero[1].side, "13.0 A GAME"); STR(columns[2].hero[2].side, "");
	CHECK(columns[2].medal_count == 2 && columns[2].medal[0] == AE_MEDAL_HILL_DOMINANCE);
	/* the ring: the favourite kind's column at open, a kind's column, an index kept in range, the NOT PLAYED YET column */
	CHECK(ae_rc_mode_ring(columns, count, 1, -1, -1, ae_rc_favourite_kind(&in)) == 0);
	CHECK(ae_rc_mode_ring(columns, count, 1, AE_RS_KIND_HILL, -1, AE_RS_KIND_SLAYER) == 2);
	CHECK(ae_rc_mode_ring(columns, count, 1, AE_RS_KIND_BALL, 1, AE_RS_KIND_SLAYER) == 0);
	CHECK(ae_rc_mode_ring(columns, count, 1, -1, 1, AE_RS_KIND_SLAYER) == 1);
	CHECK(ae_rc_mode_ring(columns, count, 1, -1, 9, AE_RS_KIND_SLAYER) == 3);
	CHECK(ae_rc_mode_ring(columns, count, 0, -1, 9, AE_RS_KIND_SLAYER) == 2);
	CHECK(ae_rc_mode_ring(columns, count, 1, AE_RC_RING_NOT_PLAYED, -1, AE_RS_KIND_SLAYER) == 3);
	CHECK(ae_rc_mode_ring(columns, count, 0, AE_RC_RING_NOT_PLAYED, -1, AE_RS_KIND_SLAYER) == 0);
	CHECK(ae_rc_mode_ring(columns, 0, 1, -1, -1, -1) == 0 && ae_rc_mode_ring(columns, 0, 0, -1, -1, -1) == -1);
	/* three columns and NOT PLAYED YET fit at the usual widths: no run */
	CHECK(ae_rc_mode_shown(3, 1) == 4 && ae_rc_mode_shown(4, 1) == 5 && ae_rc_mode_shown(5, 0) == 5 && ae_rc_mode_shown(0, 1) == 1);

	/* every kind played: seven columns, OTHER last with no medals of its own; a run of five, never narrower than five across */
	in.kind[AE_RS_KIND_BALL].games = 20; in.kind[AE_RS_KIND_BALL].wins = 9;
	in.kind[AE_RS_KIND_BOMB].games = 12; in.kind[AE_RS_KIND_BOMB].wins = 6;
	in.kind[AE_RS_KIND_RACE].games = 8; in.kind[AE_RS_KIND_RACE].wins = 2;
	in.kind[AE_RS_KIND_OTHER].games = 6; in.kind[AE_RS_KIND_OTHER].wins = 3;
	in.kind[AE_RS_KIND_OTHER].kills = 61; in.kind[AE_RS_KIND_OTHER].deaths = 70; in.kind[AE_RS_KIND_OTHER].assists = 12;
	count = ae_rc_mode_columns(&in, columns, missing, &missing_count);
	CHECK(count == 7 && missing_count == 0);
	STR(columns[3].name, "BALL"); STR(columns[4].name, "BOMB"); STR(columns[5].name, "RACE"); STR(columns[6].name, "OTHER");
	STR(columns[6].hero[0].value, "61"); STR(columns[6].hero[0].side, "10.2 A GAME"); STR(columns[6].hero[1].value, "0.87");
	STR(columns[6].row[1].value, "-9"); STR(columns[6].medals_head, "OTHER MEDALS");
	STR(columns[6].medals_empty, "These games have no medals of their own.");
	CHECK(ae_rc_chips(&in, chips) == 7);
	for (column = 0; column < 7; column++)
		CHECK(chips[column].kind == columns[column].kind);
	CHECK(ae_rc_mode_shown(7, 0) == 5 && ae_rc_mode_shown(6, 0) == 5 && ae_rc_mode_shown(5, 1) == 5);
	CHECK(ae_rc_column_width(5, 0) >= ae_rc_column_width(4, 1) - 1.0f);
	/* the run moves the least to keep the ringed column in view, within the columns */
	first = ae_rc_mode_first(7, 5, 0, 0); CHECK(first == 0);
	first = ae_rc_mode_first(7, 5, first, 4); CHECK(first == 0);
	first = ae_rc_mode_first(7, 5, first, 5); CHECK(first == 1);
	first = ae_rc_mode_first(7, 5, first, 6); CHECK(first == 2);
	first = ae_rc_mode_first(7, 5, first, 3); CHECK(first == 2);
	first = ae_rc_mode_first(7, 5, first, 1); CHECK(first == 1);
	CHECK(ae_rc_mode_first(7, 5, 9, -1) == 2 && ae_rc_mode_first(4, 5, 3, 3) == 0 && ae_rc_mode_first(7, 5, -4, 0) == 0);
	player(&in);
	/* widths: two columns beside NOT PLAYED YET, five without */
	CHECK(ae_rc_column_width(2, 1) == 677.0f);
	CHECK(ae_rc_column_width(5, 0) > 326.0f && ae_rc_column_width(5, 0) < 327.0f);
	ae_rc_not_played_text("Marmotxd", 1, 0, text, sizeof(text));
	STR(text, "A mode gets its column after Marmotxd's first game of it on this PC.");

	/* the divisor: a record from before the numbers were kept shows no per game of the objective numbers, kills keep theirs */
	x.counted = 100;
	CHECK(!ae_rc_per_game_known(&in, 0) && !ae_rc_per_game_known(&in, 1));
	count = ae_rc_mode_columns(&in, columns, missing, &missing_count);
	STR(columns[1].hero[0].value, "38"); STR(columns[1].hero[0].side, "");
	STR(columns[1].hero[2].side, "1.8 A GAME");
	STR(columns[2].hero[1].side, "13.0 A GAME");
	STR(columns[0].hero[0].side, "14.0 A GAME");
	/* a bot's kinds are counted with its numbers: the basic per game shows, the version 3 one while every counted game had it */
	in.spree_from_record = 1;
	CHECK(ae_rc_per_game_known(&in, 0) && !ae_rc_per_game_known(&in, 1));
	x.counted_v3 = 100;
	CHECK(ae_rc_per_game_known(&in, 1));
	/* a bot's record: its kinds take columns the same way, SLAYER first and lit at open */
	count = ae_rc_mode_columns(&in, columns, missing, &missing_count);
	CHECK(count == 3 && columns[0].kind == AE_RS_KIND_SLAYER);
	STR(columns[0].hero[0].side, "14.0 A GAME"); STR(columns[0].hero[2].side, "4.1 A GAME");
	CHECK(ae_rc_chips(&in, chips) == 3 && chips[0].on);
	CHECK(ae_rc_mode_ring(columns, count, missing_count > 0, -1, -1, ae_rc_favourite_kind(&in)) == 0);
	/* bomb: no medals of its own; one column alone takes no BEST tag */
	player(&in);
	memset(in.kind, 0, sizeof(in.kind));
	in.kind[AE_RS_KIND_BOMB].games = 17; in.kind[AE_RS_KIND_BOMB].wins = 10;
	x.bomb_plants = 14;
	count = ae_rc_mode_columns(&in, columns, missing, &missing_count);
	CHECK(count == 1 && missing_count == 4);
	STR(columns[0].tag, ""); STR(columns[0].medals_empty, "Bomb games have no medals of their own.");
	STR(columns[0].hero[0].value, "14"); STR(columns[0].hero[0].side, "");
	/* nothing counted: dashes, never a made-up zero */
	memset(&x, 0, sizeof(x));
	count = ae_rc_mode_columns(&in, columns, missing, &missing_count);
	STR(columns[0].hero[0].value, AE_RV_DASH); CHECK(columns[0].hero[0].dash);

	/* COMBAT */
	player(&in);
	CHECK(ae_rc_combat_columns(&in, columns) == AE_RC_COMBAT_COLUMNS);
	STR(columns[0].games, "41,880 SHOTS"); STR(columns[0].meter_word, "41% LANDED");
	STR(columns[0].hero[0].value, "22%"); STR(columns[0].hero[0].side, "612 KILLS");
	STR(columns[0].hero[1].value, "78.8"); STR(columns[0].row[1].value, "24,710");
	STR(columns[1].games, "2,812 KILLS COUNTED"); STR(columns[1].bar[0].share, "22%"); CHECK(columns[1].bar[0].length == 1.0f);
	STR(columns[1].note, "SHARE OF THE KILLS FROM 196 OF 218 GAMES; A KILL CAN BE TWO KINDS");
	STR(columns[2].meter_word, "1.20 RATIO"); STR(columns[2].hero[0].value, "145.1");
	STR(columns[3].hero[0].value, "13.8"); STR(columns[3].row[1].value, "10:00"); STR(columns[3].row[2].value, "14");
	STR(columns[3].medals_empty, "None earned yet.");

	/* TOP WEAPONS: a share of all kills while every game was counted, of the top 5 otherwise */
	ae_rc_weapons_column(&in, &columns[0]);
	STR(columns[0].games, "SHARE OF ALL KILLS"); STR(columns[0].bar[0].label, "BATTLE RIFLE"); STR(columns[0].bar[0].share, "37%");
	x.counted = 100;
	ae_rc_weapons_column(&in, &columns[0]);
	STR(columns[0].games, "SHARE OF TOP 5"); STR(columns[0].bar[0].share, "74%");
	STR(columns[0].note, "WEAPONS FROM 100 OF 218 GAMES");

	/* MEDALS: one column a family */
	player(&in);
	CHECK(ae_rc_medal_columns(&in, columns) == AE_RC_MEDAL_COLUMNS);
	CHECK(columns[3].medal_count == 4 && columns[0].medal_count == 0);
	STR(columns[0].medals_empty, "None earned yet."); STR(columns[3].medals_head, "RAREST FIRST");
	CHECK(columns[3].medals_earned == 4);
	/* the RECORDS card */
	player(&in);
	ae_rc_card(&in, &last, &columns[0]);
	STR(columns[0].games, "218 GAMES  120 W  98 L"); STR(columns[0].meter_word, "55% WON");
	STR(columns[0].hero[0].value, "13.8"); STR(columns[0].hero[0].side, "K/D 1.20");
	STR(columns[0].hero[2].value, "SLAYER"); STR(columns[0].hero[2].side, "150 GAMES");
	STR(columns[0].row[0].value, "LOSS, HANG 'EM HIGH"); STR(columns[0].row[2].value, "BATTLE RIFLE");
	ae_rc_card(&in, NULL, &columns[0]);
	STR(columns[0].row[0].label, "BEST MAP");
	if (failures)
		return 1;
	printf("ae_record_columns_rules_test: ok\n");
	return 0;
}
