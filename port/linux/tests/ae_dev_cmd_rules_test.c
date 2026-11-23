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
	CHECK(ok("give rocket to bot", 1) && cmd.words == 3 && !strcmp(cmd.word[0], "rocket") && !strcmp(cmd.word[2], "bot"));
	CHECK(ok("give rocket to Jenkins", 1) && !strcmp(cmd.word[2], "jenkins"));
	CHECK(refused("give all to bot") && refused("give rocket for bot") && refused("give rocket to") &&
		refused("give rocket to bot now"));
	/* (1.0 rc2 goto bot) */
	CHECK(ok("goto bot 2.8 5.9 0.1", 1) && cmd.id == AE_DEV_CMD_GOTO && cmd.has_position && cmd.position[0] > 2.7f &&
		cmd.position[2] < 0.2f);
	CHECK(refused("goto bot 1 2") && refused("goto bot a b c"));
	/* (1.0 BOT NAV) */
	CHECK(ok("nav dump", 1) && cmd.id == AE_DEV_CMD_NAV && !strcmp(cmd.word[0], "dump"));
	CHECK(ok("nav status", 1) && cmd.id == AE_DEV_CMD_NAV);
	/* (1.0.x campaign_score status|on|off|reset, add <enemy> <style>) */
	CHECK(ok("campaign_score status", 1) && cmd.id == AE_DEV_CMD_CAMPAIGN_SCORE && cmd.words == 1 && !strcmp(cmd.word[0], "status"));
	CHECK(ok("campaign_score on", 1) && ok("campaign_score OFF", 1) && ok("campaign_score reset", 1));
	CHECK(ok("campaign_score add elite headshot+melee", 1) && cmd.words == 3 && !strcmp(cmd.word[1], "elite") &&
		!strcmp(cmd.word[2], "headshot+melee"));
	CHECK(refused("campaign_score") && refused("campaign_score add") && refused("campaign_score add elite") &&
		refused("campaign_score status now") && refused("campaign_score kill") && refused("campaign_score add a b c"));
	/* (1.0 rc2 item respawn) */
	CHECK(ok("item respawn rockets in 15", 1) && cmd.id == AE_DEV_CMD_ITEM && cmd.number == 15 && !strcmp(cmd.word[1], "rockets"));
	CHECK(refused("item") && refused("item respawn rockets") && refused("item respawn rockets in 0") && refused("item respawn rockets in x") &&
		refused("item respawn rockets 15") && refused("item take rockets in 15") && refused("item respawn rockets in 3601"));
	/* (rc2 spawner) */
	CHECK(ok("spawner wait 20", 1) && cmd.id == AE_DEV_CMD_SPAWNER && cmd.number == 20 && !strcmp(cmd.word[0], "wait"));
	CHECK(ok("spawner red 600", 1) && cmd.number == 600 && ok("spawner ready", 1));
	CHECK(refused("spawner") && refused("spawner wait") && refused("spawner wait 0") && refused("spawner wait 601") &&
		refused("spawner ready 5") && refused("spawner red x") && refused("spawner taken 5"));
	CHECK(ok("ammo 50", 1) && cmd.id == AE_DEV_CMD_AMMO && cmd.number == 50 && ok("ammo 0", 1) && ok("ammo 100", 1) &&
		refused("ammo") && refused("ammo 101") && refused("ammo x") && refused("ammo 5 5"));
	CHECK(ok("weaponset btb", 1) && ok("weaponset auto", 1));
	/* (rc2) pgcr_demo: a made-up post-game report's shape, game kind and DETAILS page */
	CHECK(ok("pgcr_demo 4v4", 1) && cmd.id == AE_DEV_CMD_PGCR_DEMO && !strcmp(cmd.word[0], "4v4"));
	CHECK(ok("pgcr_demo ffa8 flag", 1) && ok("pgcr_demo 8v8 details", 1) && ok("pgcr_demo 4v4 bomb details", 1) &&
		ok("pgcr_demo 4v4 details race", 1) && ok("pgcr_demo ffa8 hill", 1));
	CHECK(!ok("pgcr_demo", 1) && !ok("pgcr_demo big", 1) && !ok("pgcr_demo 4v4 chess", 1) && !ok("pgcr_demo 4v4 flag ball", 1) &&
		!ok("pgcr_demo 4v4 details details", 1) && !ok("pgcr_demo 4v4 flag details extra", 1));
	CHECK(refused("nav") && refused("nav dump now") && refused("nav show now") && refused("nav show all off") &&
		refused("nav dump all"));
	/* (1.0 BOT NAV) nav show: number 2 all (the default), 1 routes, 0 off */
	CHECK(ok("nav show", 1) && cmd.id == AE_DEV_CMD_NAV && cmd.number == 2 && ok("nav show all", 1) && cmd.number == 2);
	CHECK(ok("nav show routes", 1) && cmd.number == 1 && ok("nav show OFF", 1) && cmd.number == 0);
	CHECK(refused("give") && !strncmp(cmd.error, "usage: give", 11));
	CHECK(refused("give a b"));
	CHECK(ok("spawn ghost", 1) && ok("spawn warthog ahead", 1) && ok("spawn hog bay", 1));
	CHECK(refused("spawn") && refused("spawn a b c"));
	CHECK(ok("bots 8", 1) && cmd.number == 8);
	CHECK(ok("bots 0", 1) && cmd.number == 0 && ok("bots 127", 1) && cmd.number == 127);
	CHECK(refused("bots 128") && refused("bots -1") && refused("bots x") && refused("bots"));
	CHECK(ok("bots 4 against legendary", 1) && ok("bots 4 demon even", 1) && ok("bots 4 heroic", 1) && ok("bots 4 elite", 1));
	CHECK(refused("bots 4 even against") && refused("bots 4 easy hard") && refused("bots 4 red"));
	CHECK(ok("powerup camo", 1) && ok("powerup overshield now", 1) && refused("powerup camo later now") && refused("powerup camo now me x"));
	CHECK(ok("powerup overshield enemy", 1) && !strcmp(cmd.word[1], "enemy") && ok("powerup overshield now mate", 1) &&
		cmd.words == 3 && !strcmp(cmd.word[2], "mate"));
	/* (where the item goes: at the feet, lying ahead, or on a player) */
	{
		const char *who = "x";

		CHECK(ok("powerup frag_grenade", 1) && ae_dev_cmd_powerup_place(&cmd, &who) == AE_DEV_POWERUP_FEET && !who);
		CHECK(ok("powerup frag_grenade now", 1) && ae_dev_cmd_powerup_place(&cmd, &who) == AE_DEV_POWERUP_FEET && !who);
		CHECK(ok("powerup frag_grenade ahead", 1) && ae_dev_cmd_powerup_place(&cmd, &who) == AE_DEV_POWERUP_AHEAD && !who);
		CHECK(ok("powerup frag_grenade AHEAD", 1) && ae_dev_cmd_powerup_place(&cmd, NULL) == AE_DEV_POWERUP_AHEAD);
		CHECK(ok("powerup overshield enemy", 1) && ae_dev_cmd_powerup_place(&cmd, &who) == AE_DEV_POWERUP_PLAYER &&
			who && !strcmp(who, "enemy"));
		CHECK(ok("powerup overshield now dummy", 1) && ae_dev_cmd_powerup_place(&cmd, &who) == AE_DEV_POWERUP_PLAYER &&
			who && !strcmp(who, "dummy"));
		CHECK(refused("powerup frag_grenade now ahead") && refused("powerup frag_grenade ahead me"));
	}
	CHECK(ok("respawn", 1) && ok("respawn all", 1) && refused("respawn bots"));
	CHECK(ok("kill", 1) && ok("kill bots", 1) && refused("kill red"));
	/* (0.9.4: a kill with a killer: the nearest bot, or a player by name) */
	CHECK(ok("kill me by bot", 1) && cmd.words == 3 && !strcmp(cmd.word[2], "bot"));
	CHECK(ok("kill me by Jenkins", 1) && !strcmp(cmd.word[2], "jenkins"));
	/* (rc2) camera third|first [view 1-4]; number: the view from 0 */
	CHECK(ok("camera third", 1) && cmd.id == AE_DEV_CMD_CAMERA && cmd.number == 0 && !strcmp(cmd.word[0], "third"));
	CHECK(ok("camera first 2", 1) && cmd.number == 1 && !strcmp(cmd.word[0], "first"));
	CHECK(refused("camera") && refused("camera second") && refused("camera third 0") && refused("camera third 5") &&
		refused("camera third 1 2"));
	/* (rc2) pending kills: after once, every a number of times (default 2) */
	CHECK(ok("kill me by bot after 12", 1) && cmd.seed == 12 && cmd.times == 1 && cmd.words == 5);
	CHECK(ok("kill me by bot every 12", 1) && cmd.seed == 12 && cmd.number == 12 && cmd.times == 2);
	CHECK(ok("kill me by bot every 8 times 5", 1) && cmd.number == 8 && cmd.times == 5);
	CHECK(refused("kill me by bot after 0") && refused("kill me by bot after 601") && refused("kill me by bot after") &&
		refused("kill me by bot later 5") && refused("kill me by bot every 5 times 1") &&
		refused("kill me by bot every 5 times 21") && refused("kill me by bot every 5 once 3") &&
		refused("kill all by bot after 5") && refused("kill me by bot after 5 times 3"));
	CHECK(refused("kill all by bot") && refused("kill me with bot") && refused("kill me by") &&
		refused("kill me by bot now"));
	/* (rc1: kill dummy [part] [kind] [side], any order, each once) */
	CHECK(ok("kill dummy", 1) && cmd.words == 1 && ok("kill dummy legs", 1) && ok("kill dummy legs melee back", 1) &&
		cmd.words == 4 && ok("kill dummy back blast head", 1));
	CHECK(refused("kill dummy legs legs") && refused("kill dummy knee") && refused("kill dummy legs bullet front extra") &&
		refused("kill dummy bullet melee"));
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
	/* (dummy reach: 4 walkers by default, 1 to 4) */
	CHECK(ok("dummy reach", 1) && !strcmp(cmd.word[0], "reach") && cmd.number == 4);
	CHECK(ok("dummy reach 2", 1) && cmd.number == 2);
	CHECK(!ok("dummy reach 5", 1) && !ok("dummy reach 2 more", 1) && !ok("dummy reach x", 1));
	/* (botrecord: the made-up store, its numbers, the state) */
	CHECK(ok("botrecord", 1) && cmd.id == AE_DEV_CMD_BOTRECORD && cmd.number == 0);
	CHECK(ok("botrecord demo seed 4 bots 12", 1) && cmd.seed == 4 && cmd.position[0] == 12.0f);
	CHECK(ok("botrecord demo", 1) && cmd.seed == 0 && cmd.position[0] == 0.0f);
	CHECK(ok("botrecord demo off", 1) && ok("botrecord forget", 1) && ok("botrecord forget player 2", 1) && cmd.number == 1);
	CHECK(ok("botrecord set Rook7 games 23 vs 6-8 player 3", 1) && cmd.number == 2);
	CHECK(ok("botrecord set Rook7 games 23 wins 12 with 9", 1) && cmd.number == 0);
	CHECK(!ok("botrecord demo bots 61", 1) && !ok("botrecord demo seed", 1) && !ok("botrecord set Rook7", 1) &&
		!ok("botrecord set Rook7 colour 3", 1) && !ok("botrecord set Rook7 vs 6", 1) && !ok("botrecord set Rook7 vs -8", 1) &&
		!ok("botrecord set Rook7 games", 1) && !ok("botrecord forget now", 1) && !ok("botrecord dance", 1) &&
		!ok("botrecord forget player 5", 1));
	CHECK(ok("menu bots", 1) && cmd.id == AE_DEV_CMD_MENU && cmd.number == 0 && cmd.seed == 0);
	CHECK(ok("menu bots player 2 focus Rook7 sort record", 1) && cmd.number == 1 && cmd.seed == 2);
	CHECK(ok("menu bots sort last", 1) && cmd.seed == 1);
	CHECK(ok("menu records", 1) && cmd.number == 0 && ok("menu records player 2", 1) && cmd.number == 1);
	CHECK(!ok("menu bots sort name", 1) && !ok("menu bots player 5", 1) && !ok("menu bots focus", 1) &&
		!ok("menu bots colour red", 1));
	/* (rc2) menu music [song|playlist] [mcc|standin|none]: MENU MUSIC and its screens, the songs' source */
	CHECK(ok("menu music", 1) && cmd.id == AE_DEV_CMD_MENU && cmd.number == 0 && cmd.seed == 0);
	CHECK(ok("menu music song", 1) && cmd.number == 1 && cmd.seed == 0);
	CHECK(ok("menu music playlist standin", 1) && cmd.number == 2 && cmd.seed == 2);
	CHECK(ok("menu music none", 1) && cmd.number == 0 && cmd.seed == 3);
	CHECK(ok("menu music song mcc", 1) && cmd.number == 1 && cmd.seed == 1);
	CHECK(!ok("menu music loud", 1) && !ok("menu music song 3", 1));
	/* (0.9.4 the TAB menu's pages) */
	CHECK(ok("menu tab", 1) && !ok("menu tab now", 1));
	CHECK(ok("menu credits", 1) && cmd.id == AE_DEV_CMD_MENU && !strcmp(cmd.word[0], "credits"));
	CHECK(ok("menu network", 1) && ok("menu network reached", 1) && ok("menu network unreached", 1) &&
		ok("menu network unknown", 1) && !strcmp(cmd.word[1], "unknown"));
	CHECK(!ok("menu credits now", 1) && !ok("menu network offline", 1) && !ok("menu network reached now", 1));
	CHECK(ok("botrecord page Rook7", 1) && ok("botrecord page Rook7 game player 2", 1) && cmd.number == 1);
	CHECK(!ok("botrecord page", 1) && !ok("botrecord page Rook7 now", 1));
	/* (rc2: the record's three pages, and the new numbers) */
	CHECK(ok("botrecord page Rook7 game page 2", 1) && cmd.position[1] == 1.0f && ok("botrecord page Rook7 page 3 player 2", 1) &&
		cmd.position[1] == 2.0f && cmd.number == 1 && ok("botrecord page Rook7", 1) && cmd.position[1] == 0.0f);
	CHECK(ok("botrecord page #1 game page 2", 1) && !strcmp(cmd.word[1], "#1") && ok("botrecord page #12", 1));
	CHECK(ok("botrecord page Rook7 page 4", 1) && cmd.position[1] == 3.0f && !ok("botrecord page Rook7 page 8", 1) && !ok("botrecord page Rook7 page 0", 1) && !ok("botrecord page Rook7 page", 1) &&
		!ok("botrecord page Rook7 game game", 1) && !ok("botrecord page Rook7 game page x", 1));
	CHECK(ok("botrecord set Rook7 plants 4 defuses 2", 1) && ok("botrecord set Rook7 counted 3 counted_v3 1 seconds 900", 1) &&
		ok("botrecord set Rook7 caps 2 returns 3 grabs 4 carrier 2", 1) && ok("botrecord set Rook7 hill 90 ball 60 laps 7", 1) && !ok("botrecord set Rook7 plants -1", 1) &&
		!ok("botrecord set Rook7 plants", 1));
	CHECK(ok("social carnage team-slayer seed 4 players 2 cursor Rook7", 1) && cmd.seed == 4 && cmd.position[0] == 2.0f &&
		cmd.position[1] == 7.0f && !strcmp(cmd.word[7], "rook7"));
	CHECK(ok("social carnage free-for-all", 1) && cmd.position[0] == 1.0f && cmd.position[1] == 0.0f);
	CHECK(!ok("social carnage", 1) && !ok("social carnage team-slayer players 5", 1) &&
		!ok("social carnage team-slayer cursor", 1) && !ok("social carnage team-slayer colour red", 1));
	/* (botstyle: a bot's style and favourite weapon, one bot or all back, the list) */
	CHECK(ok("botstyle 2 pusher shotgun", 1) && cmd.id == AE_DEV_CMD_BOTSTYLE && cmd.number == 2 &&
		cmd.position[0] == 1.0f && cmd.position[1] == 3.0f);
	CHECK(ok("botstyle 1 ANCHOR", 1) && cmd.position[0] == 2.0f && cmd.position[1] == 0.0f);
	CHECK(ok("botstyle 3 clear", 1) && cmd.number == 3 && ok("botstyle clear", 1) && ok("botstyle", 1));
	CHECK(!ok("botstyle 2 camper", 1) && !ok("botstyle 2 pusher bfg", 1) && !ok("botstyle 0 pusher", 1) &&
		!ok("botstyle 2", 1) && !ok("botstyle 2 pusher shotgun more", 1));
	/* (botskill: a bot's own pair, one bot or all back, the list) */
	CHECK(ok("botskill 2 recruit legendary", 1) && cmd.id == AE_DEV_CMD_BOTSKILL && cmd.number == 2 &&
		cmd.position[0] == 1.0f && cmd.position[1] == 6.0f);
	CHECK(ok("botskill 1 DEMON easy", 1) && cmd.position[0] == 7.0f && cmd.position[1] == 2.0f);
	CHECK(ok("botskill 3 clear", 1) && cmd.number == 3 && cmd.words == 2);
	CHECK(ok("botskill clear", 1) && cmd.number == 0 && ok("botskill", 1) && cmd.words == 0);
	CHECK(!ok("botskill 0 easy easy", 1) && !ok("botskill 2 easy", 1) && !ok("botskill 2 easy legend", 1) &&
		!ok("botskill 2 easy easy easy", 1) && !ok("botskill x easy easy", 1));
	/* (dummy run: x y z to x y z, then perhaps loop; nothing else) */
	CHECK(ok("dummy run 27.8 13.5 -0.2 to 11.4 18.5 -0.2", 1) && !strcmp(cmd.word[0], "run") && cmd.has_position &&
		cmd.has_goal && cmd.position[0] == 27.8f && cmd.position[2] == -0.2f && cmd.goal[0] == 11.4f && cmd.goal[1] == 18.5f &&
		cmd.times == 0);
	CHECK(ok("dummy run 1 2 3 to 4 5 6 loop", 1) && cmd.times == 1 && cmd.goal[2] == 6.0f);
	CHECK(ok("DUMMY RUN 1 2 3 TO 4 5 6 LOOP", 1) && cmd.times == 1);
	CHECK(refused("dummy run") && refused("dummy run 1 2 3") && refused("dummy run 1 2 3 to 4 5") &&
		refused("dummy run 1 2 3 at 4 5 6") && refused("dummy run here to 4 5 6") && refused("dummy run 1 2 3 to 4 5 6 twice") &&
		refused("dummy run 1 2 x to 4 5 6") && refused("dummy run 1 2 3 to 4 5 6 loop loop"));
	/* (dummy walk: here or x y z, then perhaps to x y z; nothing else after) */
	CHECK(ok("dummy walk here", 1) && !strcmp(cmd.word[0], "walk") && !cmd.has_position && !cmd.has_goal);
	CHECK(ok("dummy walk -4.7 10.7 -2.8", 1) && cmd.has_position && cmd.position[0] == -4.7f && cmd.position[1] == 10.7f &&
		cmd.position[2] == -2.8f && !cmd.has_goal);
	CHECK(ok("dummy walk 1 2 3 to 4 5 6", 1) && cmd.has_position && cmd.has_goal && cmd.goal[0] == 4.0f && cmd.goal[2] == 6.0f);
	CHECK(ok("dummy walk here to 85 -128 0.5", 1) && !cmd.has_position && cmd.has_goal && cmd.goal[1] == -128.0f);
	CHECK(refused("dummy walk") && refused("dummy walk 1 2") && refused("dummy walk 1 2 3 to 4 5") &&
		refused("dummy walk 1 2 3 at 4 5 6") && refused("dummy walk here 4") && refused("dummy walk here to x 5 6") &&
		refused("dummy walk 1 2 3 4") && refused("dummy walk here mate"));
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
	/* (0.8.1) record_medals <0-118> [<seed>] */
	CHECK(ok("record_medals 36", 1) && cmd.id == AE_DEV_CMD_RECORD_MEDALS && cmd.number == 36 && cmd.seed == 0);
	CHECK(ok("dev record_medals 104 7", 0) && cmd.id == AE_DEV_CMD_RECORD_MEDALS && cmd.number == 104 && cmd.seed == 7);
	/* (rc2: page <1-3> and counted <n>, after the seed or without one) */
	CHECK(ok("record_medals 36 7 page 2", 1) && cmd.number == 36 && cmd.seed == 7 && cmd.position[0] == 1.0f && cmd.position[1] == -1.0f);
	CHECK(ok("record_medals 36 page 3", 1) && cmd.seed == 0 && cmd.position[0] == 2.0f);
	/* (pages 4-7: the detail window over the record) */
	CHECK(ok("record_medals 36 page 7", 1) && cmd.position[0] == 6.0f && !ok("record_medals 36 page 8", 1));
	CHECK(ok("record_medals 36 7 counted 3", 1) && cmd.position[1] == 3.0f && cmd.position[0] == 0.0f);
	CHECK(ok("record_medals 36 page 1 counted 0", 1) && cmd.position[0] == 0.0f && cmd.position[1] == 0.0f);
	CHECK(ok("record_medals 36 7", 1) && cmd.position[0] == 0.0f && cmd.position[1] == -1.0f);
	/* (kinds <1-7>: the games over that many game kinds; 0 the usual three) */
	CHECK(ok("record_medals 104 7 page 4 kinds 7", 1) && cmd.position[0] == 3.0f && cmd.position[2] == 7.0f);
	CHECK(ok("record_medals 104 kinds 5", 1) && cmd.seed == 0 && cmd.position[2] == 5.0f && cmd.position[1] == -1.0f);
	CHECK(ok("record_medals 36 7", 1) && cmd.position[2] == 0.0f);
	CHECK(!ok("record_medals 36 kinds 0", 1) && !ok("record_medals 36 kinds 8", 1) && !ok("record_medals 36 kinds", 1));
	CHECK(ok("record_medals 104 7 page 4 kinds 7 ring other", 1) && cmd.position[2] == 7.0f && ok("record_medals 104 ring SLAYER", 1));
	CHECK(!ok("record_medals 104 ring", 1) && !ok("record_medals 104 ring capture", 1));
	CHECK(!ok("record_medals 36 page 8", 1) && !ok("record_medals 36 page 0", 1) && !ok("record_medals 36 page", 1) &&
		!ok("record_medals 36 7 page", 1) && !ok("record_medals 36 7 counted -1", 1) && !ok("record_medals 36 7 size 3", 1) &&
		!ok("record_medals 36 page 2 7", 1));
	CHECK(ok("record_medals 0", 1) && cmd.number == 0 && ok("record_medals 118 99999999", 1) && cmd.seed == 99999999UL &&
		!ok("record_medals 118 999999999", 1) && !ok("record_medals 36 4294967296", 1));
	/* (menus phase 1) lobby_fill <1-4>: the party at that size; motd_reset, motd_offline: no arguments */
	CHECK(ok("lobby_fill 4", 1) && cmd.id == AE_DEV_CMD_LOBBY_FILL && cmd.number == 4);
	CHECK(ok("dev lobby_fill 1", 0) && cmd.number == 1);
	CHECK(!ok("lobby_fill 0", 1) && !ok("lobby_fill 5", 1) && !ok("lobby_fill", 1) && !ok("lobby_fill 2 3", 1));
	CHECK(ok("motd_reset", 1) && cmd.id == AE_DEV_CMD_MOTD_RESET && !ok("motd_reset now", 1));
	CHECK(ok("motd_offline", 1) && cmd.id == AE_DEV_CMD_MOTD_OFFLINE && !ok("motd_offline 1", 1));
	/* (0.9.3) menu game_options <filter> [change <option>=<value> ...]: up to three changes */
	CHECK(ok("menu game_options all", 1) && cmd.id == AE_DEV_CMD_MENU && cmd.words == 2 && !strcmp(cmd.word[1], "all"));
	CHECK(ok("dev menu game_options changed change kills_to_win=25 change time_limit=10 change respawn_time=5", 0) &&
		cmd.words == 8 && !strcmp(cmd.word[3], "kills_to_win=25") && !strcmp(cmd.word[7], "respawn_time=5"));
	CHECK(ok("menu GAME_OPTIONS ALL CHANGE time_limit=10", 1) && cmd.words == 4);
	CHECK(refused("menu") && refused("menu game_options") &&
		refused("menu game_options all change") && refused("menu game_options all change kills_to_win") &&
		refused("menu game_options all kills_to_win=25") && refused("menu game_options all set a=1"));
	/* (0.9.0) hitch <50-5000>: a client may; join_lan: no arguments */
	CHECK(ok("hitch 1000", 1) && cmd.id == AE_DEV_CMD_HITCH && cmd.number == 1000);
	CHECK(!ok("hitch 10", 1) && !ok("hitch 9000", 1) && !ok("hitch", 1) && !ok("hitch 100 2", 1));
	CHECK(ok("hitch 1000 after 40", 1) && cmd.number == 1000 && cmd.seed == 40);
	CHECK(ok("hitch 1000", 1) && cmd.seed == 0);
	CHECK(!ok("hitch 1000 after 0", 1) && !ok("hitch 1000 later 5", 1) && !ok("hitch 1000 after", 1));
	CHECK(ok("join_lan", 1) && cmd.id == AE_DEV_CMD_JOIN_LAN && !ok("join_lan now", 1));
	/* (0.9.3) menu settings find [<text>]: the text is the rest of the line after "find" */
	CHECK(ok("menu settings find zoom", 1) && cmd.id == AE_DEV_CMD_MENU && !strcmp(cmd.rest, "zoom"));
	CHECK(ok("menu settings find Zoom Hold", 1) && !strcmp(cmd.rest, "Zoom Hold"));
	CHECK(ok("menu settings find", 1) && cmd.rest[0] == 0);
	CHECK(ok("menu settings find \"v sync\"", 1) && !strcmp(cmd.rest, "v sync"));
	CHECK(ok("menu settings player 2 find inv", 1) && cmd.number == 1 && !strcmp(cmd.rest, "inv"));
	CHECK(ok("menu settings find inv", 1) && cmd.number == 0);
	CHECK(ok("menu settings player 3 find", 1) && cmd.number == 2 && cmd.rest[0] == 0);
	CHECK(!ok("menu settings player 5 find x", 1) && !ok("menu settings player find x", 1));
	CHECK(!ok("menu settings", 1) && !ok("menu", 1) && !ok("menu video find x", 1) && !ok("menu settings look x", 1));
	/* (0.9.3) social: exp, reset, match, mode, roster, pending, maps; number = the local player (0-3), seed = the amount */
	CHECK(ok("social exp team-slayer 37", 1) && cmd.id == AE_DEV_CMD_SOCIAL && cmd.number == 0 && cmd.seed == 37 &&
		!strcmp(cmd.word[1], "team-slayer"));
	CHECK(ok("social exp all 99999 player 2", 1) && cmd.number == 1 && cmd.seed == 99999 && ok("dev social exp doubles 0", 0));
	CHECK(refused("social exp team-slayer") && refused("social exp team-slayer 100000") && refused("social exp team-slayer x") &&
		refused("social exp team-slayer 5 player 5") && refused("social exp team-slayer 5 player"));
	/* (1.0) archive: the community map archive's library without its screen */
	CHECK(ok("archive status", 1) && cmd.id == AE_DEV_CMD_ARCHIVE && ok("archive cancel", 1) && ok("archive list", 1) &&
		ok("archive list alpha", 1) && cmd.words == 2 && ok("archive get 3", 1) && cmd.number == 3 &&
		ok("archive get 20000", 1) && ok("dev archive status", 0) && ok("ARCHIVE List Alpha", 1));
	CHECK(refused("archive") && refused("archive get") && refused("archive get 0") && refused("archive get 20001") &&
		refused("archive get x") && refused("archive get 1 2") && refused("archive status now") &&
		refused("archive list a b") && refused("archive fetch 1") && refused("archive cancel 1"));
	CHECK(!ae_dev_cmd_parse("archive status", 0, &cmd) && ae_dev_cmd_find("archive") == AE_DEV_CMD_ARCHIVE &&
		strstr(ae_dev_cmd_usage(AE_DEV_CMD_ARCHIVE), "get <row>"));
	CHECK(ok("social calibration me alot", 1) && cmd.seed == 3 && cmd.number == 0 && ok("social calibration kid new player 2", 1) &&
		cmd.seed == 1 && cmd.number == 1 && ok("social calibration me unset", 1) && cmd.seed == 0);
	CHECK(refused("social calibration me") && refused("social calibration me lots") && refused("social calibration"));
	CHECK(ok("social reset", 1) && ok("social reset all", 1) && ok("social reset doubles", 1) && ok("social reset player 3", 1) &&
		cmd.number == 2 && ok("social reset doubles player 2", 1) && cmd.number == 1 && refused("social reset a b c"));
	CHECK(ok("social match team-slayer 1600", 1) && cmd.seed == 1600 && ok("social match team-slayer 4999 player 4", 1) &&
		cmd.number == 3 && ok("social match team-slayer tier 9", 1) && cmd.seed == 9 && ok("social match doubles tier 19 player 2", 1));
	CHECK(refused("social match team-slayer 5000") && refused("social match team-slayer tier 20") &&
		refused("social match team-slayer tier 0") && refused("social match team-slayer") && refused("social match team-slayer level 4"));
	CHECK(ok("social mode doubles heroic", 1) && ok("social mode team-slayer AUTO", 1) && ok("social mode doubles demon", 1) &&
		refused("social mode doubles") && refused("social mode doubles harder") && refused("social mode doubles heroic player 2"));
	CHECK(ok("social roster team-slayer", 1) && cmd.seed == 0 && ok("social roster team-slayer seed 7", 1) && cmd.seed == 7 &&
		refused("social roster") && refused("social roster team-slayer seed") && refused("social roster team-slayer seed 100000000") &&
		refused("social roster team-slayer 7"));
	CHECK(ok("social pending team-slayer", 1) && ok("social pending team-slayer player 2", 1) && cmd.number == 1 &&
		refused("social pending") && refused("social pending a b") && ok("social pending team-slayer crashed", 1) &&
		ok("social pending team-slayer crashed player 2", 1) && cmd.number == 1 && refused("social pending team-slayer crashed x"));
	/* (0.9.3 the screens: start, postgame, menu matchmaking / career) */
	CHECK(ok("social start team-slayer", 1) && ok("social start team-slayer map warren@smp seed 7 now", 1) && cmd.seed == 7 &&
		ok("social start doubles now", 1) && refused("social start") && refused("social start doubles seed") &&
		refused("social start doubles seed x") && refused("social start doubles map") && refused("social start doubles foo"));
	/* (the map vote scene: vote, votes <a,b,c>, left <0-10>) */
	CHECK(ok("social start team-objective vote", 1) && ok("social start team-objective vote seed 7 votes 3,2,1 left 4", 1) &&
		cmd.seed == 7 && ok("social start team-objective vote left 0", 1) && ok("social start team-objective vote left 10", 1) &&
		ok("social start team-objective votes 0,0,0", 1) && refused("social start team-objective vote left") &&
		ok("social start team-objective vote left 20", 1) && refused("social start team-objective vote left 21") && refused("social start team-objective vote left x") &&
		refused("social start team-objective vote votes") && refused("social start team-objective vote votes 1,2") &&
		refused("social start team-objective vote votes 1,2,3,4") && refused("social start team-objective vote votes a,b,c") &&
		refused("social start team-objective vote votes 1;2;3") && refused("social start team-objective vote votes 17,0,0") &&
		ok("social start team-objective vote party 2", 1) && ok("social start team-objective vote seed 3 party 4 left 5", 1) &&
		cmd.seed == 3 && refused("social start team-objective vote party") && refused("social start team-objective vote party 0") &&
		refused("social start team-objective vote party 5"));
	/* (rc5: the post-game lobby scene, the staged party, the recent-maps memory) */
	CHECK(ok("social start team-slayer vote postgame", 1) && ok("social start team-slayer vote postgame online still bots 2 left 12", 1) &&
		ok("social start team-slayer postgame bots 0 seed 4", 1) && refused("social start team-slayer postgame bots 8") &&
		refused("social start team-slayer postgame bots") && refused("social start team-slayer postgame left 21"));
	CHECK(ok("social start team-slayer map prisoner hold 10", 1));
	CHECK(ok("social start team-slayer hold 0", 1));
	CHECK(ok("social start team-slayer map prisoner seed 7 hold starting", 1));
	CHECK(refused("social start team-slayer hold soon"));
	CHECK(refused("social start team-slayer hold"));
	CHECK(refused("social start team-slayer hold 31"));
	CHECK(refused("social start team-slayer hold x"));
	CHECK(ok("menu tournament focus team-slayer party", 1) && ok("menu tournament party", 1) && refused("menu tournament parties"));
	CHECK(ok("social party online", 1) && ok("social party offline", 1) && ok("social party clear", 1) && refused("social party") &&
		refused("social party offline online"));
	CHECK(ok("social recent show team-slayer", 1) && ok("social recent clear all", 1) && ok("social recent set team-slayer a@smp,b", 1) &&
		refused("social recent set team-slayer") && refused("social recent show") && refused("social recent bogus team-slayer") &&
		refused("social recent clear a b"));
	{
		int votes[3] = { -1, -1, -1 };

		CHECK(ae_dev_cmd_votes("3,2,1", votes) && votes[0] == 3 && votes[1] == 2 && votes[2] == 1);
		CHECK(ae_dev_cmd_votes("0,0,16", NULL) && !ae_dev_cmd_votes("", NULL) && !ae_dev_cmd_votes("1,2,", NULL) &&
			!ae_dev_cmd_votes(",1,2", NULL) && !ae_dev_cmd_votes("1,2,3 ", NULL));
	}
	CHECK(ok("social postgame win", 1) && ok("social postgame win doubles exp 39 player 2", 1) && cmd.number == 1 &&
		cmd.position[2] == 39.0f && ok("social postgame rankup team-slayer exp 39", 1) &&
		ok("social postgame place 5 8 free-for-all exp 52", 1) && cmd.position[0] == 4.0f && cmd.position[1] == 8.0f &&
		ok("social postgame tie", 1) && ok("social postgame loss doubles", 1) && cmd.position[2] == -1.0f);
	CHECK(refused("social postgame") && refused("social postgame draw") && refused("social postgame place 5") &&
		refused("social postgame place 9 8") && refused("social postgame win doubles exp") && refused("social postgame win a b"));
	CHECK(ok("menu matchmaking", 1) && ok("menu matchmaking social", 1) && ok("menu matchmaking online", 1) &&
		ok("menu matchmaking social focus doubles", 1) && ok("menu matchmaking focus doubles", 1) && refused("menu matchmaking focus") &&
		refused("menu matchmaking social x"));
	CHECK(ok("menu tournament", 1) && ok("menu tournament focus doubles", 1) && refused("menu tournament focus") &&
		refused("menu tournament x"));
	CHECK(ok("menu tournament filter comp", 1) && ok("menu tournament filter casual focus doubles", 1) &&
		refused("menu tournament filter") && refused("menu tournament filter loud")&& refused("menu tournament filter comp x"));
	CHECK(ok("menu profiles", 1) && ok("menu profiles edit", 1) && refused("menu profiles new") && refused("menu profiles edit 2"));
	CHECK(ok("menu profiles edit colours", 1) && ok("menu profiles edit plain", 1) && refused("menu profiles colours") &&
		refused("menu profiles edit colours 2"));
	CHECK(ok("menu career", 1) && ok("menu career player 2", 1) && cmd.number == 1 && ok("menu career focus doubles", 1) &&
		ok("menu career player 3 focus doubles", 1) && refused("menu career player") && refused("menu career player 5"));
	CHECK(ok("menu campaign", 1) && ok("menu campaign seat", 1) && ok("menu campaign seat error", 1) && ok("menu campaign error", 1) &&
		refused("menu campaign seat seat error") && refused("menu campaign players"));
	CHECK(ok("menu campaign players 3", 1) && cmd.number == 3 && ok("menu campaign players 4 error", 1) && cmd.number == 4 &&
		ok("menu campaign error players 2", 1) && refused("menu campaign players 5") && refused("menu campaign players 1") &&
		refused("menu campaign seat players 3") && refused("menu campaign players 2 players 3"));
	/* (the custom campaign scenes) */
	CHECK(ok("menu campaign workshop", 1) && ok("menu campaign workshop demo", 1) && ok("menu campaign workshop fault", 1) &&
		ok("menu campaign workshop none", 1) && ok("menu campaign workshop saved", 1) &&
		ok("menu campaign workshop damaged", 1) && ok("menu campaign players 2 workshop", 1) && cmd.number == 2 &&
		ok("menu campaign workshop demo error", 1) && refused("menu campaign demo") && refused("menu campaign workshop workshop") &&
		refused("menu campaign workshop other"));
	CHECK(ok("menu hostvote", 1) && cmd.number == 0 && ok("menu hostvote now", 1) && cmd.number == 2 &&
		ok("menu hostvote lan join", 1) && cmd.number == 5 && ok("menu hostvote now lan", 1) && cmd.number == 3 &&
		refused("menu hostvote now join") && refused("menu hostvote x") && refused("menu hostvote lan lan"));
	CHECK(ok("menu coop", 1) && cmd.number == 1 && ok("menu coop players 3", 1) && cmd.number == 3 &&
		refused("menu coop players 5") && refused("menu coop players 0") && refused("menu coop players") && refused("menu coop x"));
	CHECK(ok("menu lan", 1) && refused("menu lan x"));
	/* (1.0 menu maps [<page> [<state>]]: MAPS & MODS staged; number = the page, seed = its state) */
	CHECK(ok("menu maps", 1) && cmd.number == 0 && cmd.seed == 0);
	CHECK(ok("menu maps browse", 1) && cmd.number == 2 && ok("menu maps mods restart", 1) && cmd.number == 4 && cmd.seed == 19);
	CHECK(ok("menu maps installed delete", 1) && cmd.number == 0 && cmd.seed == 5 && ok("menu MAPS Required Found", 1));
	CHECK(refused("menu maps dance") && refused("menu maps mods delete") && refused("menu maps browse consent now") &&
		refused("menu maps workshop offline"));
	/* (1.0 menu steam <state> [<percent>]: STEAM ACCOUNT staged with no Steam traffic; first-start: the first-start page) */
	CHECK(ok("menu steam signed-out", 1) && ok("menu steam typing", 1) && ok("menu steam guard-code", 1) &&
		ok("menu steam guard-app", 1) && ok("menu steam missing", 1) && ok("menu steam ready", 1) &&
		ok("menu steam workshop", 1) && ok("menu steam wrong-password", 1) && ok("menu steam not-owned", 1) &&
		ok("menu steam logout", 1) && ok("menu steam first-start", 1) && ok("menu steam WORKSHOP", 1) &&
		ok("menu steam open", 1) && refused("menu steam open 50"));
	CHECK(ok("menu steam downloading", 1) && cmd.number == 0 && ok("menu steam downloading 62", 1) && cmd.number == 62 &&
		refused("menu steam downloading 0") && refused("menu steam downloading 101") && refused("menu steam ready 50") &&
		refused("menu steam") && refused("menu steam dance") && refused("menu steam downloading x"));
	/* (0.9.4 playlists: the staged set; menu playlist: the PLAYLISTS board, a playlist's games when named) */
	CHECK(ok("playlists demo", 1) && ok("playlists forget", 1) && ok("playlists real", 1) && refused("playlists") &&
		refused("playlists demo now") && refused("playlists dance"));
	CHECK(ok("menu playlist", 1) && ok("menu playlist edit 4V4 ARENA", 1) && ok("menu playlist edit FFA", 1) &&
		refused("menu playlist edit") && refused("menu playlist x"));
	CHECK(ok("menu browser", 1) && ok("menu gametype", 1) && refused("menu gametype x") && refused("menu browser x"));
	CHECK(ok("social maps hide smp", 1) && !strcmp(cmd.word[2], "smp") && ok("social maps hide prisoner", 1) &&
		ok("social maps hide none", 1) && refused("social maps") && refused("social maps show smp") && refused("social maps hide"));
	CHECK(refused("social") && refused("social rank") && !ae_dev_cmd_parse("social exp a 1", 0, &cmd));
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_HITCH, 1, 1) == AE_DEV_CMD_ALLOWED);
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_HITCH, 0, 1) == AE_DEV_CMD_SETTING_OFF);
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_GOTO, 1, 1) == AE_DEV_CMD_CLIENT);
	/* (a replay's playback: replay and help run, reset does nothing, every other command is refused) */
	CHECK(ae_dev_cmd_during_replay(AE_DEV_CMD_REPLAY) == AE_DEV_CMD_REPLAY_ALLOWED);
	CHECK(ae_dev_cmd_during_replay(AE_DEV_CMD_HELP) == AE_DEV_CMD_REPLAY_ALLOWED);
	CHECK(ae_dev_cmd_during_replay(AE_DEV_CMD_RESET) == AE_DEV_CMD_REPLAY_NOTHING);
	CHECK(ae_dev_cmd_during_replay(AE_DEV_CMD_GOTO) == AE_DEV_CMD_REPLAY_REFUSED);
	CHECK(ae_dev_cmd_during_replay(AE_DEV_CMD_HITCH) == AE_DEV_CMD_REPLAY_REFUSED);
	CHECK(strstr(ae_dev_cmd_replay_refusal(), "only replay commands") != NULL);
	CHECK(refused("record_medals") && refused("record_medals 119") && refused("record_medals -1") &&
		refused("record_medals many") && refused("record_medals 3 1000000000") && refused("record_medals 3 4 5"));
	CHECK(ok("round live", 1) && ok("zone small", 1) && refused("zone") && refused("zone later"));
	CHECK(ok("down mate here", 1) && cmd.id == AE_DEV_CMD_DOWN && !strcmp(cmd.word[0], "mate") && cmd.words == 2);
	CHECK(ok("down Johnson", 1) && !strcmp(cmd.word[0], "johnson") && cmd.words == 1);
	CHECK(refused("down") && refused("down mate there") && refused("down a b c"));
	/* god [me|<player>|off] */
	CHECK(ok("god", 1) && cmd.id == AE_DEV_CMD_GOD && cmd.words == 0);
	CHECK(ok("god me", 1) && ok("god Johnson", 1) && !strcmp(cmd.word[0], "johnson") && ok("god off", 1));
	CHECK(refused("god a b") && !ae_dev_cmd_parse("god", 0, &cmd));
	/* netext status: the network capability names, this machine's and each machine's told */
	CHECK(ok("netext status", 1) && cmd.id == AE_DEV_CMD_NETEXT && ok("NETEXT STATUS", 1));
	CHECK(refused("netext") && refused("netext status now") && refused("netext list"));
	/* unseen [me|<player>|off] */
	CHECK(ok("unseen", 1) && cmd.id == AE_DEV_CMD_UNSEEN && cmd.words == 0);
	/* split join|leave <2-4>: number is the controller, 1-3 */
	CHECK(ok("split join 2", 1) && cmd.id == AE_DEV_CMD_SPLIT && cmd.number == 1 && !strcmp(cmd.word[0], "join"));
	CHECK(ok("split leave 4", 1) && cmd.id == AE_DEV_CMD_SPLIT && cmd.number == 3 && !strcmp(cmd.word[0], "leave"));
	CHECK(ok("split pad 3", 1) && cmd.number == 2 && ok("split card 2", 1) && cmd.number == 1);
	CHECK(refused("split join 1") && refused("split join 5") && refused("split join") && refused("split add 2") &&
		refused("split join 2 3"));
	CHECK(ok("randomizer status", 1) && cmd.id == AE_DEV_CMD_RANDOMIZER);
	CHECK(ok("randomizer on", 1) && ok("randomizer off", 1) && ok("randomizer reroll", 1) && ok("randomizer save", 1) && ok("randomizer pack", 1));
	CHECK(ok("randomizer seed 77", 1) && cmd.number == 77);
	CHECK(refused("randomizer seed") && refused("randomizer seed x"));
	CHECK(refused("randomizer") && refused("randomizer status now"));
	/* profile_move list | sample [<name> [<games>]] | export <name> | preview|import <file> <name> [merge|replace] | undo <name> */
	CHECK(ok("profile_move", 1) && cmd.id == AE_DEV_CMD_PROFILE_MOVE && cmd.words == 0 && ok("profile_move list", 1) &&
		ok("profile_move sample", 1) && ok("profile_move sample kit", 1) && ok("profile_move sample kit 80", 1) && cmd.number == 80);
	CHECK(ok("profile_move export goat", 1) && ok("profile_move undo goat", 1) && ok("profile_move import a.aeprofile goat", 1) &&
		ok("profile_move import a.aeprofile goat replace", 1) && ok("profile_move preview a.aeprofile goat merge", 1));
	CHECK(refused("profile_move export") && refused("profile_move sample kit 0") && refused("profile_move sample kit x") &&
		refused("profile_move import a.aeprofile goat swap") && refused("profile_move import a") && refused("profile_move list now"));
	CHECK(ok("menu profiles progress export", 1) && ok("menu profiles progress files", 1) && ok("menu profiles progress import", 1) &&
		refused("menu profiles progress") && refused("menu profiles progress sideways"));
	CHECK(ok("unseen me", 1) && ok("unseen Johnson", 1) && !strcmp(cmd.word[0], "johnson") && ok("unseen off", 1));
	CHECK(refused("unseen a b") && !ae_dev_cmd_parse("unseen", 0, &cmd));
	/* mods list | on <name> | off <name> | order <name> <1-256> */
	CHECK(ok("mods list", 1) && cmd.id == AE_DEV_CMD_MODS && cmd.words == 1);
	CHECK(ok("mods on hd_audio", 1) && cmd.words == 2 && !strcmp(cmd.word[1], "hd_audio") && ok("mods off german", 1));
	CHECK(ok("mods order hd_audio 1", 1) && cmd.number == 1 && ok("dev mods order \"my mod\" 256", 0) && cmd.number == 256);
	CHECK(refused("mods") && refused("mods on") && refused("mods order hd_audio") && refused("mods order hd_audio 0") &&
		refused("mods order hd_audio 257") && refused("mods shuffle x") && refused("mods list now"));
	/* maps list [<source>:] [<text>] | missing | scan | delete <map>[@<pack>] | move <map>[@<pack>] <folder>|. */
	CHECK(ok("maps list", 1) && cmd.id == AE_DEV_CMD_MAPS && !strcmp(cmd.rest, "list"));
	CHECK(ok("maps list ce: Blood Gulch", 1) && !strcmp(cmd.rest, "list ce: Blood Gulch"));
	CHECK(ok("maps missing", 1) && ok("maps scan", 1));
	CHECK(ok("maps delete hugeass", 1) && ok("maps delete lockout@h2demakes", 1));
	CHECK(ok("maps move hugeass H2 Demakes", 1) && !strcmp(cmd.rest, "move hugeass H2 Demakes"));
	CHECK(ok("maps move hugeass .", 1) && ok("dev maps list", 0));
	CHECK(refused("maps") && refused("maps frob") && refused("maps missing now") && refused("maps scan x"));
	CHECK(refused("maps delete") && refused("maps delete a b") && refused("maps move hugeass"));
	CHECK(!ae_dev_cmd_parse("maps list", 0, &cmd));
	/* replay list|status|info <n>|play <n>|eyes <player>|free|pause|speed <25-400>|seek <s>|skip ahead|back|stop */
	CHECK(ok("replay list", 1) && cmd.id == AE_DEV_CMD_REPLAY && cmd.words == 1 && ok("replay status", 1));
	CHECK(ok("replay free", 1) && ok("replay pause", 1) && ok("replay stop", 1) && ok("replay Stop", 1));
	CHECK(ok("replay info 1", 1) && cmd.number == 1 && ok("replay play 500", 1) && cmd.number == 500);
	CHECK(refused("replay play 0") && refused("replay play 501") && refused("replay play") && refused("replay play x"));
	CHECK(refused("replay info") && refused("replay play 1 2") && refused("replay list now") && refused("replay"));
	CHECK(ok("replay eyes Johnson", 1) && !strcmp(cmd.word[1], "johnson") && refused("replay eyes"));
	CHECK(ok("replay speed 25", 1) && cmd.number == 25 && ok("replay speed 400", 1) && refused("replay speed 24"));
	CHECK(refused("replay speed 401") && ok("replay seek 0", 1) && cmd.number == 0 && ok("replay seek 86400", 1));
	CHECK(refused("replay seek 86401") && refused("replay seek -1") && ok("replay skip ahead", 1) && ok("replay skip back", 1));
	CHECK(refused("replay skip") && refused("replay skip up") && refused("replay rewind") && !ae_dev_cmd_parse("replay list", 0, &cmd));
	/* inject status|maps|open <level>|list <category>|take <category> <number|name>|close|on|off|clear */
	CHECK(ok("inject status", 1) && cmd.id == AE_DEV_CMD_INJECT && ok("inject maps", 1) && ok("inject close", 1));
	CHECK(ok("inject on", 1) && ok("inject off", 1) && ok("inject clear", 1));
	CHECK(ok("inject open tsce_multiplayerv1", 1) && !strcmp(cmd.word[1], "tsce_multiplayerv1"));
	CHECK(ok("inject open 1@ws3477809506", 1) && ok("inject list 4", 1) && cmd.number == 4);
	CHECK(ok("inject take 4 0", 1) && cmd.number == 4 && !strcmp(cmd.word[2], "0"));
	CHECK(ok("inject take 0 brute_plasma_rifle", 1) && cmd.number == 0 && !strcmp(cmd.word[2], "brute_plasma_rifle"));
	CHECK(refused("inject") && refused("inject open") && refused("inject list 9") && refused("inject list x"));
	CHECK(refused("inject take 4") && refused("inject take 9 0") && refused("inject take x 0") && refused("inject on now"));
	CHECK(ae_dev_cmd_gate(AE_DEV_CMD_GOD, 0, 0) == AE_DEV_CMD_SETTING_OFF && ae_dev_cmd_gate(AE_DEV_CMD_GOD, 1, 1) == AE_DEV_CMD_CLIENT &&
		ae_dev_cmd_gate(AE_DEV_CMD_GOD, 1, 0) == AE_DEV_CMD_ALLOWED);
	/* biped [list|use <1-4>|off|check <1-4>] */
	CHECK(ok("biped", 1) && cmd.id == AE_DEV_CMD_BIPED && cmd.words == 0 && ok("biped list", 1) && ok("biped off", 1));
	CHECK(ok("biped use 2", 1) && cmd.number == 2 && ok("biped check 4", 1) && cmd.number == 4);
	CHECK(refused("biped use") && refused("biped use 0") && refused("biped use 5") && refused("biped check") &&
		refused("biped list 1") && refused("biped paint") && refused("biped use 1 2"));
	/* seat [driver|gunner|passenger] */
	CHECK(ok("seat", 1) && cmd.id == AE_DEV_CMD_SEAT && cmd.words == 0 && ok("seat gunner", 1) && ok("seat Passenger", 1));
	CHECK(refused("seat pilot") && refused("seat driver gunner me"));
	CHECK(ok("seat driver enemy", 1) && cmd.words == 2 && !strcmp(cmd.word[1], "enemy") && ok("seat gunner 3", 1) && ok("seat passenger mate", 1));
	/* (dummy ... away: facing the way you face, after the distance; with mate, away first) */
	CHECK(ok("dummy 1 1.0 away", 1) && cmd.number == 1 && cmd.position[0] == 1.0f && cmd.away == 1 && cmd.position[2] == 0.0f);
	CHECK(ok("dummy 2 6 away mate", 1) && cmd.away == 1 && cmd.position[2] == 1.0f && cmd.position[0] == 6.0f);
	CHECK(ok("dummy here away", 1) && cmd.position[1] == 1.0f && cmd.away == 1 && ok("dummy 2 6", 1) && cmd.away == 0);
	CHECK(refused("dummy away 3") && refused("dummy 2 away 6"));
	/* ride [warthog|ghost|scorpion] [circle|line] */
	CHECK(ok("ride", 1) && cmd.id == AE_DEV_CMD_RIDE && cmd.words == 0);
	CHECK(ok("ride ghost", 1) && cmd.words == 1 && ok("ride scorpion circle", 1) && !strcmp(cmd.word[1], "circle"));
	CHECK(ok("ride warthog line", 1) && refused("ride banshee") && refused("ride ghost stop") && refused("ride ghost line now"));
	CHECK(ok("powerup over_shield now dummy", 1) && cmd.words == 3);
	CHECK(ok("dummy 2 6 mate", 1) && cmd.number == 2 && cmd.position[2] == 1.0f && cmd.position[0] == 6.0f && ok("dummy mate", 1) &&
		cmd.position[2] == 1.0f && ok("dummy here mate", 1) && ok("dummy 3", 1) && cmd.position[2] == 0.0f && refused("dummy mate 3"));
	CHECK(ok("goto 1 2 3 facing 90", 1) && cmd.has_position && cmd.number == 90 && refused("goto 1 2 3 facing"));
	/* flag ours|theirs here|ahead|stand|carry|home */
	CHECK(ok("flag ours here", 1) && cmd.id == AE_DEV_CMD_FLAG && cmd.words == 2 && ok("flag Theirs ahead", 1) &&
		ok("flag theirs carry", 1) && ok("flag theirs stand", 1) && ok("flag ours home", 1) && ok("flag ours carry", 1));
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
	CHECK(ae_dev_cmd_complete("s", names, AE_DEV_CMD_COUNT) == 7 && !strcmp(names[0], "spawn") &&
		!strcmp(names[1], "score") && !strcmp(names[2], "shot") && !strcmp(names[3], "seat") && !strcmp(names[4], "social") &&
		!strcmp(names[5], "split") && !strcmp(names[6], "spawner"));
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

static void seat_tests(void)
{
	/* (a warthog: the driver's seat, the gun's, a passenger's) */
	CHECK(ae_dev_cmd_seat_wanted("driver", 1, 0));
	CHECK(!ae_dev_cmd_seat_wanted("gunner", 1, 0));
	CHECK(ae_dev_cmd_seat_wanted("gunner", 0, 1));
	CHECK(ae_dev_cmd_seat_wanted("passenger", 0, 0));
	CHECK(!ae_dev_cmd_seat_wanted("passenger", 0, 1));
	/* (a ghost's one seat drives and fires: the driver's, never the gunner's) */
	CHECK(ae_dev_cmd_seat_wanted("driver", 1, 1));
	CHECK(!ae_dev_cmd_seat_wanted("gunner", 1, 1));
	CHECK(!ae_dev_cmd_seat_wanted("passenger", 1, 1));
}

int main(void)
{
	seat_tests();
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
