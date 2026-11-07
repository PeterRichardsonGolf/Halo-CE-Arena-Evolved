/*
AE_DEV_CMD_RULES.H

DEV MODE's commands: one set of commands for the console, the telnet console,
TEST RUN set-up lines and scripts (ae_dev_cmd.c runs them in the game). This
part is the grammar alone: a line to a command and its arguments, checked, and
the names for completion. Pure: string and stdlib only, no engine headers.

A line is words split on spaces; text in double quotes is one word. A dev
command is a line whose first word is a command's name (taken only when the
caller allows bare names: DEV MODE on) or "dev" followed by one (always taken;
"dev" alone is help). help is "dev help" only: a bare "help" is the script
console's own. The script functions' names never clash with the bare names
(tools/test_ae_dev_cmd.py).
The command's name and its fixed words are lowercase; matching ignores case.

	give <weapon>|all              a weapon of the map (by its tag's last name)
	spawn <vehicle> [here|ahead|<spot>]
	bots <0-127> [even|against] [recruit|easy|normal|heroic|legendary|demon]
	powerup <item> [now]           now: picked up at once
	respawn [me|all]
	kill [me|all|bots]
	end round|game
	score red|blue|me <0-9999>
	time <seconds>|<m:ss>          the round time left (0 to 9999 s)
	goto <spot> [facing <spot>] | start <n> | <x> <y> <z>
	                               a spot or start faces as the map sets it, or toward the other spot
	gametype <name ...>            the rest of the line
	option <row> <value>           a GAME OPTIONS row of the game being played, as the pause menu sets it (option
	                               needler mcc, option fall_damage halo_2: words matched without case, spaces or "_");
	                               only the rows the game reads as it plays (NEEDLER, FALL DAMAGE, TIMERS), and not
	                               with other players in the game
	config <key> <value ...>       a config key for this run; the rest of the line is its value
	agent check <1-3> pass|fail|look|waiting [reason ...]
	agent say <text ...> | agent clear
	                               the live agent's verdict on a check of the TEST RUN card, its message, or neither
	mark <text ...>                a line in the log and the event stream
	shot                           a screenshot now
	help [command]                 (dev help only)
	list weapons|vehicles|items|spots|options
	view bots|spawns|items|net [on|off]
	viewmodel [<x> <y> <z> <scale>|place <right> <up> <forward> <yaw> <pitch> <roll> <scale>|probe|save|clear]
	                               the held weapon's first-person placement (offset along the gun's forward, left,
	                               up in model units, uniform scale), live; alone it prints the numbers; save keeps
	                               them in view_models.txt; clear is the shipped numbers again
	ttk [on|off]                   every kill answered with its time: the killer's first damage to the victim (this life) to the
	                               death, the hits and the headshots; alone it says whether it is on
	dummy [<n>] [here|<distance>]  <n> (1-8, one by default) bots that stand still and never shoot, <distance> (1-200 world
	                               units, 10 by default) ahead of you and facing you, or here; each heals to full 3 s after it was
	                               last hurt and respawns where it stood 1 s after it dies (a local game's bots)
	dummy clear                    the dummies leave
	hud                            the held weapon's HUD elements (the weapon HUD tag's own, HUD STYLE CE): number, kind, bitmap
	hud <i> <dx> <dy> [<scale>]    element i moved by dx, dy (HUD units, 640 x 480, right and down) and scaled, live
	hud <i> hide|show              element i not drawn, or drawn again
	hud reset [<i>]                element i (or all of the weapon's) as the tag has it
	hud save                       keeps the numbers in hud_offsets.txt
	tune                           the held weapon's numbers (ae_weapon_tune_rules.h's keys), each with its present value
	tune <key> <value>             one number changed live in the loaded tags (trigger0.rate 9, trigger0.spread 1..3,
	                               magazine0.size 30, projectile0.speed 400, projectile0.impact.damage 50, melee.damage
	                               80, weapon.zoom 2, weapon.autoaim 4): a bounds key takes a or a..b; only the numbers
	                               the engine cannot hold are refused, and not with other players in the game
	tune save                      writes the changed numbers to tune/<weapon key>.json in the config folder (a recipe's
	                               tune layer: tools/weapon_pack/apply_tune.py)
	tune reset                     the held weapon's changed numbers back as the map loaded them
	reset                          the group's start state back (TEST RUN)
	fireteam follow|regroup|attack|defend|watch|go|clear|status
	fireteam recruit [<bot>]       the host player's fireteam takes an order (attack, defend, watch and go at the host's
	                               feet) or a bot (its number in the logs, 1 up; none: the first of the team in another fireteam) from
	                               another fireteam; status logs the roles
	round end|next|live            a round mode's round (ELIMINATION: ae_round.h): end it now (the side with the most
	                               standing wins it, level is a tie), end it tied and start the next at once, or end
	                               its countdown now (live)
	zone now|small|off             the round's danger zone: its first closing starts now, it stands at its small ring
	                               now (a tenth, the last closing in its wait), or none this match
	zone look a|b                  how this machine draws the zone's edge for this run: a the energy wall, b the light
	                               curtain (game.zone_look is the setting)
	down <player> [here]           a living player goes down (dies, a revive spot where they fall); here: put beside
	                               you first. <player>: me, mate (the nearest teammate), enemy, an absolute index, or the
	                               start of a name
	revive <player>                a downed player back at their spot at once (mate: the nearest downed teammate)
	god [me|<player>|off]          that player takes no damage (me when none named; off ends it; their vehicle still does)
*/

#ifndef AE_DEV_CMD_RULES_H
#define AE_DEV_CMD_RULES_H

#define AE_DEV_CMD_WORDS 8
#define AE_DEV_CMD_WORD 64
#define AE_DEV_CMD_REST 192

enum ae_dev_cmd_id
{
	AE_DEV_CMD_NONE = 0,
	AE_DEV_CMD_GIVE,
	AE_DEV_CMD_SPAWN,
	AE_DEV_CMD_BOTS,
	AE_DEV_CMD_POWERUP,
	AE_DEV_CMD_RESPAWN,
	AE_DEV_CMD_KILL,
	AE_DEV_CMD_END,
	AE_DEV_CMD_SCORE,
	AE_DEV_CMD_TIME,
	AE_DEV_CMD_GOTO,
	AE_DEV_CMD_GAMETYPE,
	AE_DEV_CMD_OPTION,
	AE_DEV_CMD_MARK,
	AE_DEV_CMD_SHOT,
	AE_DEV_CMD_HELP,
	AE_DEV_CMD_LIST,
	AE_DEV_CMD_VIEW,
	AE_DEV_CMD_RESET,
	AE_DEV_CMD_CONFIG,
	AE_DEV_CMD_AGENT,
	AE_DEV_CMD_VIEWMODEL,
	AE_DEV_CMD_FIRETEAM,
	AE_DEV_CMD_ROUND,
	AE_DEV_CMD_ZONE,
	AE_DEV_CMD_DOWN,
	AE_DEV_CMD_REVIVE,
	AE_DEV_CMD_GOD,
	AE_DEV_CMD_SEAT,
	AE_DEV_CMD_TTK,
	AE_DEV_CMD_DUMMY,
	AE_DEV_CMD_HUD,
	AE_DEV_CMD_TUNE,
	AE_DEV_CMD_COUNT
};

struct ae_dev_cmd
{
	int id;                  /* enum ae_dev_cmd_id; AE_DEV_CMD_NONE when the line was not a dev command */
	int words;               /* the arguments after the command's name */
	char word[AE_DEV_CMD_WORDS][AE_DEV_CMD_WORD];
	char rest[AE_DEV_CMD_REST]; /* the line after the name (gametype, mark), after the first argument (config), after
	                             "say" or after "check <n> <verdict>" (agent) */
	long number;             /* bots' count, score's points, time's seconds, goto start's index, dummy's count, hud's element */
	float position[3];       /* goto <x> <y> <z>; dummy's distance and here (1 or 0); hud's dx, dy and scale (0: not given) */
	int has_position;
	char error[96];          /* "" or why the line cannot run (a usage line) */
};

/* parses a line. 0: not a dev command (the caller hands it to the script evaluator); 1: a dev command, in out, whose
error is "" when it can run. bare: names without "dev" are taken (DEV MODE on) */
int ae_dev_cmd_parse(const char *line, int bare, struct ae_dev_cmd *out);

/* a command's name ("give") and its usage line; "" for an id out of range */
const char *ae_dev_cmd_name(int id);
const char *ae_dev_cmd_usage(int id);
/* the command whose name is word (any case), AE_DEV_CMD_NONE when none is */
int ae_dev_cmd_find(const char *word);

/* the bare command names (not help) that start with prefix (any case), in table order, up to max into out; the number found */
int ae_dev_cmd_complete(const char *prefix, const char **out, int max);

/* who may run a command: DEV MODE's setting on this machine, and this machine a network game's client */
enum ae_dev_cmd_gate
{
	AE_DEV_CMD_ALLOWED = 0,
	AE_DEV_CMD_SETTING_OFF, /* system.dev_mode off here */
	AE_DEV_CMD_CLIENT       /* another machine hosts this game: its rules hold, never a client's */
};
/* whether the command may run here: help always; every other command only with the setting on and not as a client */
int ae_dev_cmd_gate(int id, int setting, int client);
/* the line said for a gate's refusal ("" for AE_DEV_CMD_ALLOWED) */
const char *ae_dev_cmd_gate_text(int gate);

/* seconds from "90" or "1:30": 1 and *seconds, or 0 when the text is neither (or past 9999) */
int ae_dev_cmd_seconds(const char *text, long *seconds);

#endif
