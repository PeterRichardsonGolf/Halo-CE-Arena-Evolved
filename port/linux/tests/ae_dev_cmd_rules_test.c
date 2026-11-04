#include <stdio.h>
#include <string.h>
#include "ae_dev_cmd_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_dev_cmd cmd;

/* the line parses as a dev command that can run */
static int ok(const char *line, int bare)
{
	return ae_dev_cmd_parse(line, bare, &cmd) && cmd.error[0] == 0;
}

/* the line parses as a dev command that cannot run (a usage line or why) */
static int refused(const char *line)
{
	return ae_dev_cmd_parse(line, 1, &cmd) && cmd.error[0] != 0;
}

/* BARE AND DEV: bare names only with DEV MODE on; "dev <name>" always; anything else goes to the script evaluator */
static void prefix_tests(void)
{
	CHECK(ok("give dmr", 1) && cmd.id == AE_DEV_CMD_GIVE && cmd.words == 1 && !strcmp(cmd.word[0], "dmr"));
	CHECK(!ae_dev_cmd_parse("give dmr", 0, &cmd));
	CHECK(ok("dev give dmr", 0) && cmd.id == AE_DEV_CMD_GIVE);
	CHECK(ok("DEV Give DMR", 0) && !strcmp(cmd.word[0], "dmr"));
	CHECK(!ae_dev_cmd_parse("object_create warthog", 1, &cmd));
	CHECK(!ae_dev_cmd_parse("   ", 1, &cmd) && !ae_dev_cmd_parse("", 1, &cmd) && !ae_dev_cmd_parse(NULL, 1, &cmd));
	/* (dev alone is help; dev with no command is still ours, a line saying so, never handed on) */
	CHECK(ok("dev", 0) && cmd.id == AE_DEV_CMD_HELP);
	/* (help is the script console's own: dev help only) */
	CHECK(!ae_dev_cmd_parse("help", 1, &cmd) && !ae_dev_cmd_parse("help give", 1, &cmd));
	CHECK(ok("dev help give", 1) && cmd.id == AE_DEV_CMD_HELP);
	CHECK(refused("dev object_create warthog"));
	CHECK(ok("  give\tsniper_rifle  \r\n", 1) && !strcmp(cmd.word[0], "sniper_rifle"));
}

/* ARGUMENTS: each command's shapes, and the usage line when they are wrong */
static void argument_tests(void)
{
	CHECK(ok("give all", 1));
	CHECK(refused("give") && !strncmp(cmd.error, "usage: give", 11));
	CHECK(refused("give a b"));
	CHECK(ok("spawn ghost", 1) && ok("spawn warthog ahead", 1) && ok("spawn hog bay", 1));
	CHECK(refused("spawn") && refused("spawn a b c"));
	CHECK(ok("bots 8", 1) && cmd.number == 8);
	CHECK(ok("bots 0", 1) && cmd.number == 0 && ok("bots 127", 1) && cmd.number == 127);
	CHECK(refused("bots 128") && refused("bots -1") && refused("bots x") && refused("bots"));
	CHECK(ok("bots 4 against legendary", 1) && ok("bots 4 demon even", 1) && ok("bots 4 heroic", 1));
	CHECK(refused("bots 4 even against") && refused("bots 4 easy hard") && refused("bots 4 red"));
	CHECK(ok("powerup camo", 1) && ok("powerup overshield now", 1) && refused("powerup camo later"));
	CHECK(ok("respawn", 1) && ok("respawn all", 1) && refused("respawn bots"));
	CHECK(ok("kill", 1) && ok("kill bots", 1) && refused("kill red"));
	CHECK(ok("end round", 1) && ok("end game", 1) && refused("end") && refused("end match"));
	CHECK(ok("score red 3", 1) && cmd.number == 3 && ok("score me 0", 1) && refused("score green 1"));
	CHECK(refused("score red") && refused("score red 10000"));
	CHECK(ok("time 90", 1) && cmd.number == 90 && ok("time 1:30", 1) && cmd.number == 90);
	CHECK(ok("time 0", 1) && cmd.number == 0 && refused("time 1:5") && refused("time :30") && refused("time 2:60"));
	CHECK(refused("time 10000") && refused("time 1.5"));
	CHECK(ok("goto armoury", 1) && !cmd.has_position);
	CHECK(ok("goto start 3", 1) && cmd.number == 3 && refused("goto start") && refused("goto start x"));
	CHECK(ok("goto flag_blue facing red_base", 1) && !cmd.has_position && !strcmp(cmd.word[2], "red_base"));
	CHECK(refused("goto start facing x") && refused("goto a facing"));
	CHECK(ok("goto 1.5 -20 3", 1) && cmd.has_position && cmd.position[0] == 1.5f && cmd.position[1] == -20.0f);
	CHECK(refused("goto 1 2") && refused("goto 1 2 x") && refused("goto nan 1 1") && refused("goto 1e9 1 1"));
	CHECK(ok("shot", 1) && refused("shot now") && ok("reset", 1) && refused("reset all"));
	CHECK(ok("dev help", 1) && ok("dev help give", 1) && refused("dev help nothing"));
	CHECK(ok("list weapons", 1) && ok("list spots", 1) && ok("list options", 1) && refused("list") &&
		refused("list maps"));
	CHECK(ok("view bots", 1) && ok("view net off", 1) && refused("view bots maybe") && refused("view hud"));
}

/* REST OF THE LINE: gametype, mark and option's value keep their spaces and case; quotes round them come off */
static void rest_tests(void)
{
	CHECK(ok("gametype AE SLAYER", 1) && !strcmp(cmd.rest, "AE SLAYER"));
	CHECK(ok("gametype \"AE SLAYER\"", 1) && !strcmp(cmd.rest, "AE SLAYER"));
	CHECK(refused("gametype") && refused("mark"));
	CHECK(ok("mark the ramp jump, try 2  ", 1) && !strcmp(cmd.rest, "the ramp jump, try 2"));
	CHECK(ok("dev mark x", 0) && !strcmp(cmd.rest, "x"));
	CHECK(ok("config game.fov 90", 1) && cmd.id == AE_DEV_CMD_CONFIG && !strcmp(cmd.word[0], "game.fov") &&
		!strcmp(cmd.rest, "90"));
	CHECK(ok("config hud.style \"AE Modern\"", 1) && !strcmp(cmd.rest, "AE Modern"));
	CHECK(refused("config game.fov"));
	/* (option: a GAME OPTIONS row and its value, one word each: "fall_damage halo_2") */
	CHECK(ok("option needler mcc", 1) && cmd.id == AE_DEV_CMD_OPTION && !strcmp(cmd.word[0], "needler") &&
		!strcmp(cmd.word[1], "mcc"));
	CHECK(ok("option FALL_DAMAGE Halo_2", 1) && !strcmp(cmd.word[0], "fall_damage") && !strcmp(cmd.word[1], "halo_2"));
	CHECK(refused("option needler") && refused("option needler mcc now"));
	/* (agent: a check's verdict and its reason, a message, clear) */
	CHECK(ok("agent check 2 fail no banshee made: 5 of 6 vehicles", 1) && cmd.id == AE_DEV_CMD_AGENT &&
		cmd.number == 2 && !strcmp(cmd.word[2], "fail") && !strcmp(cmd.rest, "no banshee made: 5 of 6 vehicles"));
	CHECK(ok("agent check 1 pass", 1) && !cmd.rest[0] && ok("agent check 3 WAITING", 1));
	CHECK(refused("agent check 4 pass") && refused("agent check 1 maybe") && refused("agent check") &&
		refused("agent check x pass"));
	CHECK(ok("agent say Try the ramp again: the jump did not land", 1) &&
		!strcmp(cmd.rest, "Try the ramp again: the jump did not land"));
	CHECK(refused("agent say") && ok("agent clear", 1) && refused("agent clear all") && refused("agent shout x"));
	/* (a long mark is kept to the field; a long word elsewhere is refused, not cut to another name) */
	{
		char line[512];

		memset(line, 'a', sizeof line);
		memcpy(line, "mark ", 5);
		line[sizeof line - 1] = 0;
		CHECK(ok(line, 1) && strlen(cmd.rest) == AE_DEV_CMD_REST - 1);
		memcpy(line, "give ", 5);
		CHECK(refused(line) && strstr(cmd.error, "longer"));
		memcpy(line, "config x ", 9);
		CHECK(ok(line, 1));
	}
	/* (more words than fit: refused, except for the rest-of-line commands) */
	CHECK(refused("spawn a b c d e f g h i j k"));
	CHECK(ok("mark a b c d e f g h i j k l m", 1) && !strcmp(cmd.rest, "a b c d e f g h i j k l m"));
}

static void table_tests(void)
{
	const char *names[AE_DEV_CMD_COUNT];
	long seconds;
	int id;

	for (id = AE_DEV_CMD_NONE + 1; id < AE_DEV_CMD_COUNT; id++)
	{
		CHECK(ae_dev_cmd_find(ae_dev_cmd_name(id)) == id);
		CHECK(!strncmp(ae_dev_cmd_usage(id), ae_dev_cmd_name(id), strlen(ae_dev_cmd_name(id))));
	}
	CHECK(!strcmp(ae_dev_cmd_name(0), "") && !strcmp(ae_dev_cmd_name(AE_DEV_CMD_COUNT), ""));
	CHECK(ae_dev_cmd_complete("s", names, AE_DEV_CMD_COUNT) == 3 && !strcmp(names[0], "spawn") &&
		!strcmp(names[1], "score") && !strcmp(names[2], "shot"));
	CHECK(ae_dev_cmd_complete("G", names, AE_DEV_CMD_COUNT) == 3);
	CHECK(ae_dev_cmd_complete("give", names, 1) == 1 && !strcmp(names[0], "give"));
	CHECK(ae_dev_cmd_complete("", names, AE_DEV_CMD_COUNT) == AE_DEV_CMD_COUNT - 2);
	CHECK(ae_dev_cmd_complete("he", names, AE_DEV_CMD_COUNT) == 0);
	CHECK(ae_dev_cmd_complete("zz", names, AE_DEV_CMD_COUNT) == 0);
	CHECK(ae_dev_cmd_seconds("166:39", &seconds) && seconds == 9999 && !ae_dev_cmd_seconds("166:40", &seconds));
}

/* HOST ONLY: a client never runs one, whatever its own setting; help is only words, so it answers anywhere */
static void gate_tests(void)
{
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_GIVE, 1, 0) == AE_DEV_CMD_ALLOWED);
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_GIVE, 0, 0) == AE_DEV_CMD_SETTING_OFF);
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_GIVE, 1, 1) == AE_DEV_CMD_CLIENT);
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_GIVE, 0, 1) == AE_DEV_CMD_CLIENT);
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_MARK, 1, 1) == AE_DEV_CMD_CLIENT);
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_LIST, 1, 1) == AE_DEV_CMD_CLIENT);
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_HELP, 0, 1) == AE_DEV_CMD_ALLOWED);
	CHECK(strstr(ae_dev_cmd_gate_text(AE_DEV_CMD_CLIENT), "only the host") != NULL);
	CHECK(strstr(ae_dev_cmd_gate_text(AE_DEV_CMD_SETTING_OFF), "system.dev_mode") != NULL);
	CHECK(!strcmp(ae_dev_cmd_gate_text(AE_DEV_CMD_ALLOWED), ""));
	/* (on a client the bare names are off, so "give dmr" is the script console's; "dev give dmr" is ours, refused) */
	CHECK(!ae_dev_cmd_parse("give dmr", 0, &cmd));
	CHECK(ok("dev give dmr", 0) && ae_dev_cmd_gate(cmd.id, 1, 1) == AE_DEV_CMD_CLIENT);
}

int main(void)
{
	gate_tests();
	prefix_tests();
	argument_tests();
	rest_tests();
	table_tests();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_dev_cmd_rules: all passed\n");
	return failures != 0;
}
