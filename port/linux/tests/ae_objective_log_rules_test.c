#include <stdio.h>
#include <string.h>
#include "ae_objective_log_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the log's contract: the events and their keys, pinned. A change here is a change of the log's format
(tools/ae_test/obj_report.py reads the same table out of ae_objective_log_rules.c). */
static struct { char const *ev; char const *keys; } const pinned[] =
{
	{ "game_start", "map teams players bots" },
	{ "game_end", "winner score_r score_b" },
	{ "snap", "score_r score_b time_s time_left_s o1 o2" },
	{ "ps", "p b team x y z hp shield job role goal objective d_obj state seat ft order" },
	{ "job_change", "p b team from to util_from util_to forced d_obj" },
	{ "obj_blocked", "p b team job objective d_obj reason since_s" },
	{ "flag_take", "flag p b team x y z from prev" },
	{ "flag_drop", "flag p b team x y z why held_s" },
	{ "flag_return", "flag how by b x y z" },
	{ "flag_capture", "flag p b team x y z carry_s dist" },
	{ "ball_take", "p b team x y z prev" },
	{ "ball_drop", "p b team x y z why held_s" },
	{ "jugg_change", "from to" },
	{ "hill_new", "x y z" },
	{ "hill_move", "px py pz x y z" },
	{ "hill_state", "state team occ_r occ_b" },
	{ "hill_enter", "p b team x y z" },
	{ "hill_leave", "p b team x y z" },
	{ "terr_capture", "terr hill team from_team score_r score_b" },
	{ "terr_state", "terr hill state team occ_r occ_b progress" },
	{ "bomb_take", "p b team x y z" },
	{ "bomb_drop", "p b team x y z why" },
	{ "plant_done", "p b team x y z" },
	{ "defuse_start", "p b team x y z" },
	{ "defuse_cancel", "p b team held_s" },
	{ "defuse_done", "p b team x y z" },
	{ "blast_kill", "p b team x y z" },
	{ "oob_kill", "p b team x y z" },
	{ "bomb_score", "p b team x y z score_r score_b" },
	{ "bomb_reset", "why x y z" },
	{ "round_start", "round attackers score_r score_b" },
	{ "round_end", "round winner why score_r score_b" },
	{ "plant_start", "p b team x y z" },
	{ "plant_cancel", "p b team held_s" },
	{ "veh_board", "p b team x y z veh seat" },
	{ "veh_exit", "p b team x y z veh seat held_s" },
	{ "item_take", "p b team x y z item" },
	{ "elim_round_start", "round max_rounds switched wins_r wins_b" },
	{ "elim_round_end", "round winner wins_r wins_b" },
	{ "elim_down", "p b team x y z zone" },
	{ "elim_out", "p b team x y z zone" },
	{ "elim_revive", "p b team x y z by revives" },
	{ "zone_phase", "round phase radius left_s" },
	{ "race_flag", "p b team x y z flag" },
	{ "race_lap", "p b team x y z lap lap_s" },
	{ "spawn", "p b team x y z weapon" },
	{ "death", "p b team x y z killer kb cause weapon" },
	{ "kill", "p b team x y z victim vx vy vz weapon head dist" },
	{ "damage", "p b victim amount shield_left body_left weapon head kind" },
	{ "fire", "p b weapon shots hits" },
	{ "pos", "p b x y z yaw pitch spd crouch air seat weapon hp shield" },
	{ "stuck", "p b team x y z count job goal_name role d_goal d_obj fighting tether" },
};

/* a line of an event with a demo value for every key of its table entry */
static void demo_fields(char const *keys, struct ae_obj_fields *fields)
{
	char key[24];

	ae_obj_fields_begin(fields);
	while (*keys)
	{
		size_t length = 0;

		while (keys[length] && keys[length] != ' ')
			length++;
		memcpy(key, keys, length);
		key[length] = 0;
		if (!strcmp(key, "x") || !strcmp(key, "y") || !strcmp(key, "z"))
			ae_obj_fields_real(fields, key, 1.25);
		else if (!strcmp(key, "job") || !strcmp(key, "state") || !strcmp(key, "why") || !strcmp(key, "veh") || !strcmp(key, "seat") || !strcmp(key, "item"))
			ae_obj_fields_text(fields, key, "two words");
		else
			ae_obj_fields_long(fields, key, 3);
		keys += length;
		if (*keys == ' ')
			keys++;
	}
}

int main(void)
{
	char line[900];
	struct ae_obj_fields fields;
	int index;

	/* the levels: off by default, bot events imply full */
	CHECK(ae_obj_rules_level(NULL, 0) == AE_OBJ_OFF);
	CHECK(ae_obj_rules_level("", 0) == AE_OBJ_OFF);
	CHECK(ae_obj_rules_level("off", 0) == AE_OBJ_OFF);
	CHECK(ae_obj_rules_level("nonsense", 0) == AE_OBJ_OFF);
	CHECK(ae_obj_rules_level("events", 0) == AE_OBJ_EVENTS);
	CHECK(ae_obj_rules_level("snap", 0) == AE_OBJ_SNAP);
	CHECK(ae_obj_rules_level("full", 0) == AE_OBJ_FULL);
	CHECK(ae_obj_rules_level("", 1) == AE_OBJ_FULL);
	CHECK(ae_obj_rules_level("events", 1) == AE_OBJ_FULL);
	/* the film level is never read from debug.objective_log */
	CHECK(ae_obj_rules_level("film", 0) == AE_OBJ_OFF);
	CHECK(AE_OBJ_FILM > AE_OBJ_FULL);
	/* the level each event is logged at */
	CHECK(ae_obj_rules_event_level("snap") == AE_OBJ_SNAP);
	CHECK(ae_obj_rules_event_level("ps") == AE_OBJ_FULL && ae_obj_rules_event_level("fire") == AE_OBJ_FULL);
	CHECK(ae_obj_rules_event_level("pos") == AE_OBJ_FILM && ae_obj_rules_event_level("damage") == AE_OBJ_FILM);
	CHECK(ae_obj_rules_event_level("spawn") == AE_OBJ_EVENTS && ae_obj_rules_event_level("kill") == AE_OBJ_EVENTS);
	CHECK(ae_obj_rules_event_level("flag_take") == AE_OBJ_EVENTS && ae_obj_rules_event_level(NULL) == AE_OBJ_EVENTS);
	/* the modes' words */
	CHECK(!strcmp(ae_obj_rules_mode_name(AE_OBJ_MODE_CTF), "ctf"));
	CHECK(!strcmp(ae_obj_rules_mode_name(AE_OBJ_MODE_CTF1), "ctf1"));
	CHECK(!strcmp(ae_obj_rules_mode_name(AE_OBJ_MODE_TERR), "terr"));
	CHECK(!strcmp(ae_obj_rules_mode_name(AE_OBJ_MODE_SND), "snd"));
	CHECK(!strcmp(ae_obj_rules_mode_name(AE_OBJ_MODE_ELIM), "elim"));
	CHECK(!strcmp(ae_obj_rules_mode_name(AE_OBJ_MODE_EXTERM), "exterm"));
	CHECK(!strcmp(ae_obj_rules_mode_name(AE_OBJ_MODE_ATTRIT), "attrit"));
	CHECK(!strcmp(ae_obj_rules_mode_name(AE_OBJ_MODES), "slayer"));
	/* the key table is the pinned one, in order */
	CHECK(ae_obj_rules_event_count() == (int)(sizeof(pinned) / sizeof(pinned[0])));
	for (index = 0; index < (int)(sizeof(pinned) / sizeof(pinned[0])) && index < ae_obj_rules_event_count(); index++)
	{
		CHECK(!strcmp(ae_obj_rules_event_name(index), pinned[index].ev));
		CHECK(!strcmp(ae_obj_rules_event_keys(pinned[index].ev), pinned[index].keys));
		/* a line built with every key of the entry conforms; one with a key moved does not */
		demo_fields(pinned[index].keys, &fields);
		ae_obj_rules_line(line, sizeof(line), 1234, 1, AE_OBJ_MODE_CTF, pinned[index].ev, &fields);
		CHECK(ae_obj_rules_line_conforms(line));
		CHECK(!strncmp(line, "ae obj: t=1234 host=1 mode=ctf ev=", 34));
	}
	CHECK(ae_obj_rules_event_keys("nonsense") == NULL);
	CHECK(!strcmp(ae_obj_rules_stuck_keys(), "team mode job goal_name role d_goal d_obj fighting tether"));
	/* the builder: spaces are underscores, reals one decimal, none for empty text */
	ae_obj_fields_begin(&fields);
	ae_obj_fields_text(&fields, "why", "a b");
	ae_obj_fields_real(&fields, "x", -0.04);
	ae_obj_fields_real(&fields, "d", 12.345);
	ae_obj_fields_text(&fields, "role", "");
	ae_obj_fields_long(&fields, "p", -1);
	CHECK(!strcmp(fields.text, " why=a_b x=-0.0 d=12.3 role=none p=-1"));
	ae_obj_rules_line(line, sizeof(line), 5, 0, AE_OBJ_MODE_KOTH, "hill_state", &fields);
	CHECK(!strcmp(line, "ae obj: t=5 host=0 mode=koth ev=hill_state why=a_b x=-0.0 d=12.3 role=none p=-1"));
	CHECK(!ae_obj_rules_line_conforms(line));
	CHECK(!ae_obj_rules_line_conforms("bot event: t=1 ev=pos"));
	CHECK(!ae_obj_rules_line_conforms("ae obj: t=1 host=1 mode=ctf ev=snap score_r=1"));
	/* a key moved or missing fails; extra trailing keys fail */
	CHECK(!ae_obj_rules_line_conforms("ae obj: t=1 host=1 mode=ctf ev=hill_new y=1 x=1 z=1"));
	CHECK(ae_obj_rules_line_conforms("ae obj: t=1 host=1 mode=koth ev=hill_new x=1.0 y=2.0 z=3.0"));
	CHECK(!ae_obj_rules_line_conforms("ae obj: t=1 host=1 mode=koth ev=hill_new x=1.0 y=2.0 z=3.0 w=1"));
	/* a field too many for the buffer is dropped whole, never cut mid-key */
	ae_obj_fields_begin(&fields);
	for (index = 0; index < 100; index++)
		ae_obj_fields_text(&fields, "somewhat_long_key", "a_rather_long_value_indeed");
	CHECK(fields.length < sizeof(fields.text));
	CHECK(fields.text[fields.length - 1] == 'd');
	/* the bomb's reset line, the items worth a line, the seat words */
	{
		char why[24];
		double x = 9, y = 9, z = 9;

		CHECK(ae_obj_rules_bomb_reset("reset: out of bounds at (1.50 -2.25 3.00)", why, sizeof(why), &x, &y, &z));
		CHECK(!strcmp(why, "out_of_bounds") && x == 1.5 && y == -2.25 && z == 3.0);
		CHECK(ae_obj_rules_bomb_reset("reset: out of reach at (0.00 0.00 0.00)", why, sizeof(why), &x, &y, &z));
		CHECK(!strcmp(why, "out_of_reach"));
		CHECK(ae_obj_rules_bomb_reset("reset: untouched at (4.00 5.00 6.00)", why, sizeof(why), &x, &y, &z) && !strcmp(why, "untouched"));
		CHECK(!ae_obj_rules_bomb_reset("reset: something else", why, sizeof(why), &x, &y, &z));
		CHECK(!ae_obj_rules_bomb_reset("grab player 1", why, sizeof(why), &x, &y, &z));
		CHECK(!ae_obj_rules_bomb_reset(NULL, why, sizeof(why), &x, &y, &z));
		CHECK(ae_obj_rules_item_logged("sniper rifle") && ae_obj_rules_item_logged("rocket launcher"));
		CHECK(!ae_obj_rules_item_logged("flag") && !ae_obj_rules_item_logged("ball") && !ae_obj_rules_item_logged("") &&
			!ae_obj_rules_item_logged(NULL));
		CHECK(!strcmp(ae_obj_rules_seat_name(0), "driver") && !strcmp(ae_obj_rules_seat_name(1), "gunner") &&
			!strcmp(ae_obj_rules_seat_name(5), "passenger"));
	}
	if (!failures)
		printf("ae_objective_log_rules_test: ok\n");
	return failures != 0;
}
