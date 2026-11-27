/*
AE_FORGE_RULES.H

What a game that is not a Forge session keeps from Forge's network: a chat line that begins with "!F" is Forge's only
inside a session that started as a Forge session, and a normal game advertises its own network version. Forge's
network file (port/forge/src/forge_network.c) asks here before it acts on a line.
*/

#ifndef __AE_FORGE_RULES_H
#define __AE_FORGE_RULES_H

/* 1 when the chat line is shaped as a Forge packet: it begins with "!F" */
int ae_forge_rules_is_forge_line(char const *text);

/* 1 when Forge acts on the line: it is shaped as a Forge packet and this game started as a Forge session
(session: 1 then). In any other game the line is ordinary chat and switches nothing on */
int ae_forge_rules_line_accepted(int session, char const *text);

/* whether a Forge session may start on this machine: on a machine joining or joined to another's game (is_client: a network client, or a client with no
server of its own) only when it joined at Forge's network version through Forge's own join (joined_at_forge_version); anywhere else when the machine itself
asked for one (local_request: its Forge menus, its console, a variant of its own named Forge). The host's variant name never reaches a client's flag */
int ae_forge_rules_session_may_start(int is_client, int joined_at_forge_version, int local_request);

/* the network version a game advertises and checks: forge_version in a Forge session, normal_version in any
other game */
long ae_forge_rules_network_version(int session, long normal_version, long forge_version);

/* whether a key belongs to Forge and not to TEST RUN's or the menus' own readings, in a Forge session: the Windows virtual key
`vk` is one of Forge's binds (forge_vks, count of them), and only while the session is on (session: 1). TEST RUN (F4 NOTE,
F5 SKIP, F1 RELOAD) and the menus (F1 BACK, F2 START) do not read such a key: Forge's binds are as Forge keeps them
(forge_controls.ini), so a rebound key is as safe as a default one */
int ae_forge_rules_key_taken(int session, int vk, int const *forge_vks, int count);

/* the other name of a vehicle tag that a map has under one name and others under another (the multiplayer and the
campaign's warthog, ghost, scorpion, banshee and turret): into `other` (size bytes); 1 when `tag` has one. Compared without
case and with either slash; the other name keeps the slash `tag` has */
int ae_forge_rules_tag_alias(char const *tag, char *other, int size);

/* whether an object marked for gametypes (Forge's flags: bit 0 CTF, 1 Slayer, 2 Oddball, 3 KOTH, 4 Race; 0 or all bits: every gametype) is
in a game of that engine (game_engine_ctf 1 .. game_engine_race 5; 0: no game running, a Forge session: everything is). A game of any other
engine (Firefight, ...) has the objects marked for no gametype only */
int ae_forge_rules_gametype_plays(unsigned flags, int engine);

/* ---------- volumes and map settings: the physics Forge's GRAVITY volumes change */

/* a vector the world's gravity moved (straight down, -1 on z, times `amount`) moved as the pull moves it: `vector` gets
(pull - down) * amount added, so what the world's gravity took off it a GRAVITY volume's pull puts back as it says. The pull is in
units of the world's gravity (a vector pointing the way things fall) */
void ae_forge_rules_gravity_correct(float vector[3], float const pull[3], float amount);

/* the ground's hold on an object in a pull of that strength: the world's gravity times the pull's length, never less than the
world's (a weak pull sinks it no deeper than the world's does) */
float ae_forge_rules_ground_gravity(float world, float const pull[3]);

/* whether a pull takes a biped off the ground it stands on: it points out of the ground's plane (the normal is the way the ground
faces) by more than a hair */
int ae_forge_rules_pull_lifts(float const pull[3], float const ground_normal[3]);

/* a host's note that a vehicle (vehicle, a datum index) is, or lately was, in a volume's pull: in one now (in_volume) it is noted
at `now` and counts; out of every one it counts for `grace` ticks more (a throw out of one still carries it up). last_vehicle and
last_time are the note; 1 when the vehicle counts */
int ae_forge_rules_gravity_grace(int in_volume, long vehicle, long now, long grace, long *last_vehicle, long *last_time);

/* a host's note that a client's unit on foot is pulled: in a volume now it is; out of every one it is not once it stands on the ground
again, and until then it was as it last was. Returns the note */
int ae_forge_rules_gravity_noted(int in_volume, int grounded, int noted);

/* DEV MODE's `forge volume <kind>`: what a volume of that kind is made as */
enum ae_forge_volume_kind
{
	AE_FORGE_VOLUME_NONE = -1,
	AE_FORGE_VOLUME_GRAVITY,
	AE_FORGE_VOLUME_KILL,
	AE_FORGE_VOLUME_DAMAGE,
	AE_FORGE_VOLUME_HURT,
	AE_FORGE_VOLUME_ONE_WAY,
	AE_FORGE_VOLUME_PATROL,
	AE_FORGE_VOLUME_KINDS
};

struct ae_forge_volume_made
{
	int kind;
	/* its box in world units, and how far above the feet its middle is */
	float size[3];
	float up;
	/* what a value is: 1 STRENGTH in hundredths of the world's gravity, 2 a percentage of health (APPLY DAMAGE once, or
	CONSTANT DAMAGE a second), 3 an AI PATROL volume's PURSUIT RANGE in world units, 0 none; and its default and range */
	int value_kind;
	float value_default;
	float value_min;
	float value_max;
};

/* the kind named (gravity, kill, damage, hurt, oneway, patrol: without case), or AE_FORGE_VOLUME_NONE */
int ae_forge_rules_volume_kind(char const *name);
/* the kind's make-up; 0 for no such kind */
int ae_forge_rules_volume_made(int kind, struct ae_forge_volume_made *made);

/* DEV MODE's `forge settings`: a team's slot (red 0, blue 1, ffa 2; -1 for no such team) and an atmosphere (default 0, clear 1,
light 2, heavy 3, flood 4; -1 for no such one) */
int ae_forge_rules_team_slot(char const *name);
int ae_forge_rules_atmosphere(char const *name);
/* a tag's name that matches what was asked for: the name asked for is a part of the tag's last name (the path's leaf), compared without
case; 1 when it matches */
int ae_forge_rules_tag_matches(char const *path, char const *asked);

/* ---------- AI: DEV MODE's `forge ai <actor> <team> [goal [seconds]]` */

/* a team as an AI spawn's TEAM row has it (the game's team numbers, and Forge's own for the players': own 0, player 1, human 2, covenant 3,
flood 4, sentinel 5, players 6 (ALL PLAYERS), red 7, blue 8); -1 for no such team */
int ae_forge_rules_ai_team(char const *name);
/* a goal (none 0, patrol 1, route 2, hunt 3: forge_ai_goal); -1 for no such goal */
int ae_forge_rules_ai_goal(char const *name);
/* the three in one number (the edit queue carries one): team below 16, goal below 16, respawn seconds 0 to 600 (0: no respawning) */
int ae_forge_rules_ai_pack(int team, int goal, int respawn_seconds);
int ae_forge_rules_ai_unpack_team(int packed);
int ae_forge_rules_ai_unpack_goal(int packed);
int ae_forge_rules_ai_unpack_respawn(int packed);
/* the number a goal's target is by: the lowest of the numbers there are (numbers, count of them) when none was asked for (asked 0);
else the one asked for. 0 when there is none to go to */
int ae_forge_rules_ai_link(int asked, int const *numbers, int count);

/* DEV MODE's `forge ready`: whether a staged Forge scene is up. 0 with no session (text says so); -1 while it is being made (text says what it
waits for: the level, the edits, the mode); 1 when the session is on, Forge is up on the level the player is in, no edit is queued and no mode
asked for is still to be taken, and (ai_pending) the AI placed has stood up: a play that begins makes the AI from its markers, which need a
few ticks to be alive and on their feet */
int ae_forge_rules_ready(int session, int in_level, int edits_queued, int modes_pending, int ai_pending, char *text, int size);

/* ---------- Firefight markers: DEV MODE's `forge ff <kind> [round [wave]]` */

enum ae_forge_ff_kind
{
	AE_FORGE_FF_NONE = -1,
	/* the spawn types of a FIREFIGHT marker, in Forge's order: enemy infantry, infantry and vehicles, a vehicle, a Ghost, a heavy vehicle */
	AE_FORGE_FF_INFANTRY,
	AE_FORGE_FF_VEHICLES,
	AE_FORGE_FF_VEHICLE,
	AE_FORGE_FF_GHOST,
	AE_FORGE_FF_HEAVY,
	/* an enemy drop point, a player vehicle pad, a player spawn, a generator, and the props: a resupply rack, a drop area */
	AE_FORGE_FF_ZONE,
	AE_FORGE_FF_PAD,
	AE_FORGE_FF_PLAYER,
	AE_FORGE_FF_GENERATOR,
	AE_FORGE_FF_RACK,
	AE_FORGE_FF_DROP,
	AE_FORGE_FF_KINDS
};

/* a kind named (infantry, vehicles, vehicle, ghost, heavy, zone, pad, player, generator, rack, drop: without case), else AE_FORGE_FF_NONE */
int ae_forge_rules_ff_kind(char const *name);
/* a round (1 to 5, or `all`: 0) as a marker's round filter; -1 for none */
int ae_forge_rules_ff_round(char const *name);
/* kind, round filter and wave (0 all, 1 to 3) in one number (the edit queue carries one) and out again */
int ae_forge_rules_ff_pack(int kind, int round, int wave);
int ae_forge_rules_ff_unpack_kind(int packed);
int ae_forge_rules_ff_unpack_round(int packed);
int ae_forge_rules_ff_unpack_wave(int packed);

/* ---------- DEV MODE's `forge host <map> [variant]` and `forge join <address>` */

/* an address as the harness joins one: a.b.c.d:port, each part a whole number (0 to 255, the port 1 to 65535), nothing else; 1 when it is one */
int ae_forge_rules_address(char const *text);
/* a map's or a variant's name as the network test takes one: letters, digits, _ and -, 1 to 40 of them */
int ae_forge_rules_net_name(char const *text);

/* DEV MODE's `forge send <line>`: a Forge line sent as this machine's own, to test what a host does with it: `!F`, a letter, and up to 61 of
letters, digits, `:`, `.` and `-`; 1 when it is one */
int ae_forge_rules_raw_line(char const *text);

#endif /* __AE_FORGE_RULES_H */
