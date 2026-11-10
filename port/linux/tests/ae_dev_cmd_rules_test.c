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
	CHECK(ok("powerup camo", 1) && ok("powerup overshield now", 1) && refused("powerup camo later now") && refused("powerup camo now me x"));
	CHECK(ok("powerup overshield enemy", 1) && !strcmp(cmd.word[1], "enemy") && ok("powerup overshield now mate", 1) &&
		cmd.words == 3 && !strcmp(cmd.word[2], "mate"));
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
	/* (0.7.9 view bots: a bot's number pins it, aim unpins, enemy and own pick the team; the other views on or off only) */
	CHECK(ok("view bots 4", 1) && cmd.number == 4 && ok("view bots 127", 1) && cmd.number == 127);
	CHECK(ok("view bots enemy", 1) && cmd.number == 0 && ok("view bots own", 1) && ok("view bots aim", 1) && ok("view bots off", 1));
	CHECK(refused("view bots 0") && refused("view bots 128") && refused("view bots 4x") && refused("view bots -1"));
	CHECK(refused("view net 4") && refused("view items enemy") && refused("view bots 4 on"));
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
	/* (viewmodel: the held weapon's placement: four numbers, save, clear, or alone) */
	CHECK(ok("viewmodel", 1) && cmd.id == AE_DEV_CMD_VIEWMODEL && cmd.words == 0);
	CHECK(ok("viewmodel 1.5 0 -0.25 0.78", 1) && cmd.words == 4 && cmd.position[0] == 1.5f && cmd.position[2] == -0.25f &&
		!strcmp(cmd.word[3], "0.78"));
	CHECK(ok("viewmodel place 1 -0.5 2 3 -4 5 0.9", 1) && cmd.words == 8 && ok("viewmodel probe", 1) &&
		refused("viewmodel place 1 2 3") && refused("viewmodel place 1 2 3 4 5 6 x") && refused("viewmodel probe now"));
	CHECK(ok("viewmodel save", 1) && ok("viewmodel CLEAR", 1) && refused("viewmodel 1 2 3") &&
		refused("viewmodel 1 2 3 x") && refused("viewmodel keep") && refused("viewmodel save now"));
	/* (ttk: on, off, or alone) */
	CHECK(ok("ttk", 1) && cmd.id == AE_DEV_CMD_TTK && cmd.words == 0 && ok("ttk on", 1) && !strcmp(cmd.word[0], "on") &&
		ok("TTK Off", 1) && refused("ttk maybe") && refused("ttk on off"));
	/* (dummy: a count of 1 to 8, here or a distance of 1 to 200, clear; the defaults 1 and 10) */
	CHECK(ok("dummy", 1) && cmd.id == AE_DEV_CMD_DUMMY && cmd.number == 1 && cmd.position[0] == 10.0f && cmd.position[1] == 0.0f);
	CHECK(ok("dummy 3", 1) && cmd.number == 3 && cmd.position[0] == 10.0f);
	CHECK(ok("dummy here", 1) && cmd.number == 1 && cmd.position[1] == 1.0f);
	CHECK(ok("dummy 2 here", 1) && cmd.number == 2 && cmd.position[1] == 1.0f);
	CHECK(ok("dummy 2 25.5", 1) && cmd.number == 2 && cmd.position[0] == 25.5f && cmd.position[1] == 0.0f);
	CHECK(ok("dummy 8 200", 1) && cmd.number == 8 && ok("dummy clear", 1) && !strcmp(cmd.word[0], "clear"));
	CHECK(refused("dummy 0") && refused("dummy 9") && refused("dummy 2 0.5") && refused("dummy 2 201") &&
		refused("dummy 2 here now") && refused("dummy there") && refused("dummy clear 2") && refused("dummy 2 3 4"));
	/* (hud: alone lists; an element's move, scale, hide or show; reset; save) */
	CHECK(ok("hud", 1) && cmd.id == AE_DEV_CMD_HUD && cmd.words == 0);
	CHECK(ok("hud 4 -12 6", 1) && cmd.number == 4 && cmd.position[0] == -12.0f && cmd.position[1] == 6.0f &&
		cmd.position[2] == 0.0f);
	CHECK(ok("hud 31 1.5 -2 0.75", 1) && cmd.number == 31 && cmd.position[2] == 0.75f);
	CHECK(ok("hud 2 hide", 1) && ok("hud 2 SHOW", 1) && ok("hud save", 1) && ok("hud reset", 1) && cmd.number == -1 &&
		ok("hud reset 7", 1) && cmd.number == 7);
	CHECK(refused("hud 32 1 1") && refused("hud 4 1") && refused("hud 4 1 x") && refused("hud 4 1 2 3 4") &&
		refused("hud x hide") && refused("hud 4 flip") && refused("hud reset x") && refused("hud reset 1 2") &&
		refused("hud save now") && refused("hud list"));
	/* (tune: alone lists; a key and its value, or a pair for a bounds key; save; reset) */
	CHECK(ok("tune", 1) && cmd.id == AE_DEV_CMD_TUNE && cmd.words == 0);
	CHECK(ok("tune trigger0.rate 9", 1) && !strcmp(cmd.word[0], "trigger0.rate") && !strcmp(cmd.word[1], "9"));
	CHECK(ok("tune trigger1.rate 6..9", 1) && ok("TUNE Magazine0.Size 30", 1) && ok("tune melee.damage 5000", 1) &&
		ok("tune projectile0.speed 1e6", 1) && ok("tune save", 1) && ok("tune RESET", 1));
	CHECK(refused("tune trigger0.rate") && refused("tune nonsense 1") && refused("tune trigger0.rate abc") &&
		refused("tune magazine0.size 1..2") && refused("tune trigger0.rate 1 2") && refused("tune save now") &&
		refused("tune reset all") && refused("tune trigger.rate 5") && !strncmp(cmd.error, "usage: tune", 11));
	/* (call: the wheel's NEED A RIDE, for the ride scene) */
	CHECK(ok("call ride", 1) && cmd.id == AE_DEV_CMD_CALL && !strcmp(cmd.word[0], "ride"));
	CHECK(refused("call") && refused("call taxi") && refused("call ride now") && !strncmp(cmd.error, "usage: call", 11));
	/* (fireteam: the host player's squad takes an order, or a bot) */
	CHECK(ok("fireteam follow", 1) && cmd.id == AE_DEV_CMD_FIRETEAM && !strcmp(cmd.word[0], "follow"));
	CHECK(ok("FIRETEAM Defend", 1) && ok("fireteam status", 1) && ok("fireteam clear", 1));
	CHECK(ok("fireteam recruit 7", 1) && cmd.number == 7 && !strcmp(cmd.word[0], "recruit"));
	CHECK(ok("fireteam recruit", 1) && cmd.number == 0);
	CHECK(refused("fireteam") && refused("fireteam recruit 0") && refused("fireteam recruit 1 2") && refused("fireteam recruit x") &&
		refused("fireteam dance") && refused("fireteam follow now"));
	/* (agent: a check's verdict and its reason, a message, clear) */
	CHECK(ok("agent check 2 fail no banshee made: 5 of 6 vehicles", 1) && cmd.id == AE_DEV_CMD_AGENT &&
		cmd.number == 2 && !strcmp(cmd.word[2], "fail") && !strcmp(cmd.rest, "no banshee made: 5 of 6 vehicles"));
	CHECK(ok("agent check 1 pass", 1) && !cmd.rest[0] && ok("agent check 3 WAITING", 1));
	CHECK(refused("agent check 4 pass") && refused("agent check 1 maybe") && refused("agent check") &&
		refused("agent check x pass"));
	CHECK(ok("agent say Try the ramp again: the jump did not land", 1) &&
		!strcmp(cmd.rest, "Try the ramp again: the jump did not land"));
	CHECK(refused("agent say") && ok("agent clear", 1) && refused("agent clear all") && refused("agent shout x"));
	/* (the round modes' commands: round end|next, zone now|off, down <player> [here], revive <player>) */
	CHECK(ok("round end", 1) && cmd.id == AE_DEV_CMD_ROUND && !strcmp(cmd.word[0], "end"));
	CHECK(ok("dev round NEXT", 0) && !strcmp(cmd.word[0], "next"));
	CHECK(refused("round") && refused("round over") && refused("round end now"));
	CHECK(ok("zone now", 1) && cmd.id == AE_DEV_CMD_ZONE && ok("zone off", 1));
	CHECK(ok("roundcard won", 1) && cmd.id == AE_DEV_CMD_ROUNDCARD && !strcmp(cmd.word[0], "won"));
	CHECK(ok("roundcard LOST bomb", 1) && cmd.id == AE_DEV_CMD_ROUNDCARD && cmd.words == 2 && !strcmp(cmd.word[1], "bomb"));
	CHECK(ok("dev roundcard tied", 0) && cmd.id == AE_DEV_CMD_ROUNDCARD);
	CHECK(refused("roundcard") && refused("roundcard maybe") && refused("roundcard won a b"));
	CHECK(ok("round live", 1) && ok("zone small", 1) && refused("zone") && refused("zone later"));
	CHECK(ok("down mate here", 1) && cmd.id == AE_DEV_CMD_DOWN && !strcmp(cmd.word[0], "mate") && cmd.words == 2);
	CHECK(ok("down Johnson", 1) && !strcmp(cmd.word[0], "johnson") && cmd.words == 1);
	CHECK(refused("down") && refused("down mate there") && refused("down a b c"));
	/* god [me|<player>|off] */
	CHECK(ok("god", 1) && cmd.id == AE_DEV_CMD_GOD && cmd.words == 0);
	CHECK(ok("god me", 1) && ok("god Johnson", 1) && !strcmp(cmd.word[0], "johnson") && ok("god off", 1));
	CHECK(refused("god a b") && !ae_dev_cmd_parse("god", 0, &cmd));
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_GOD, 0, 0) == AE_DEV_CMD_SETTING_OFF && ae_dev_cmd_gate(AE_DEV_CMD_GOD, 1, 1) == AE_DEV_CMD_CLIENT &&
		ae_dev_cmd_gate(AE_DEV_CMD_GOD, 1, 0) == AE_DEV_CMD_ALLOWED);
	/* seat [driver|gunner|passenger] */
	CHECK(ok("seat", 1) && cmd.id == AE_DEV_CMD_SEAT && cmd.words == 0 && ok("seat gunner", 1) && ok("seat Passenger", 1));
	CHECK(refused("seat pilot") && refused("seat driver gunner me"));
	CHECK(ok("seat driver enemy", 1) && cmd.words == 2 && !strcmp(cmd.word[1], "enemy") && ok("seat gunner 3", 1) && ok("seat passenger mate", 1));
	CHECK(ok("powerup over_shield now dummy", 1) && cmd.words == 3);
	CHECK(ok("dummy 2 6 mate", 1) && cmd.number == 2 && cmd.position[2] == 1.0f && cmd.position[0] == 6.0f && ok("dummy mate", 1) &&
		cmd.position[2] == 1.0f && ok("dummy here mate", 1) && ok("dummy 3", 1) && cmd.position[2] == 0.0f && refused("dummy mate 3"));
	CHECK(ok("goto 1 2 3 facing 90", 1) && cmd.has_position && cmd.number == 90 && refused("goto 1 2 3 facing"));
	/* flag ours|theirs here|ahead|stand|carry|home */
	CHECK(ok("flag ours here", 1) && cmd.id == AE_DEV_CMD_FLAG && cmd.words == 2 && ok("flag Theirs ahead", 1) &&
		ok("flag theirs carry", 1) && ok("flag theirs stand", 1) && ok("flag ours home", 1));
	CHECK(refused("flag") && refused("flag ours") && refused("flag mine here") && refused("flag ours away") &&
		refused("flag ours here now"));
	CHECK(ok("revive 3", 1) && cmd.id == AE_DEV_CMD_REVIVE && refused("revive") && refused("revive a b"));
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
	CHECK(ae_dev_cmd_complete("s", names, AE_DEV_CMD_COUNT) == 4 && !strcmp(names[0], "spawn") &&
		!strcmp(names[1], "score") && !strcmp(names[2], "shot") && !strcmp(names[3], "seat"));
	CHECK(ae_dev_cmd_complete("G", names, AE_DEV_CMD_COUNT) == 4);
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
