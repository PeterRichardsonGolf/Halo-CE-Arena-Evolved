/*
ARENA_GAMETYPE_NAMES.C

port: Halo CE: Arena Evolved, the seeded gametypes' names: each one's stored
name (game_variant's, at most 11 characters: what every machine, listing and
saved file has) and the longer display name AE machines show in their lists,
lobby and scoreboard, its set (AE, AE BR, AE COMP, OCTAGON, Halo 1: NHE's, TRAINING), its
place in the set, its playlist folder (the categories: AE FFA, AE 2V2, AE 4V4, AE BTB, AE ACTION SACK,
AE PRACTICE, NHE), and a description for a panel.

The table holds every gametype of the final sets, seeded or not yet (one not
seeded is simply never on disk). Old stored names (aliases) report their
successor's set, so a kept old file sorts beside it, and show their own
stored name. Lookups are by stored name, letters' case aside, so a player's
edited copy of a seeded gametype gets its display name too (a renamed one
does not).

Pure: no file system, no engine state (tools/arena_gametype_names_check.c
checks it on its own).
*/

/* ---------- headers */

#include "cseries/cseries.h"
#include "saved games/arena_gametypes.h"
#include "game/game_engine.h"

/* ---------- constants */

enum
{
	/* a stored name's characters (game_variant's 12 with the NUL) */
	ARENA_STORED_NAME_MAXIMUM = 11,
	/* a display name's */
	ARENA_DISPLAY_NAME_MAXIMUM = 31,
};

/* ---------- structures */

struct arena_gametype_name
{
	char const *stored_name;
	char const *display_name;
	short set;
	short game_engine;
	boolean team;
	boolean special;
	char const *description;
};

struct arena_gametype_alias
{
	char const *stored_name;
	/* (the table's stored name it became) */
	char const *successor;
};

/* ---------- globals */

/* the sets in the lists' order, each in its own order */
static struct arena_gametype_name const arena_gametype_names[] =
{
	/* AE casual: free for all, slayer first, then the ball, the hill and the race */
	{ "AE FFA SLAY", "FFA AE SLAYER", _arena_gametype_set_ae, game_engine_slayer, FALSE, FALSE,
		"Free for all slayer on AE's rules: HALO 2 health, no fall damage, NHE's no spread, a countdown, "
		"both weapons dropped, power item timers and waypoints." },
	{ "AE ELIM", "FFA AE ELIMINATION", _arena_gametype_set_ae, game_engine_slayer, FALSE, FALSE,
		"Free for all Elimination on AE's rules: one life each, last standing; power item timers and waypoints, 15 minutes." },
	{ "AE PHANTOMS", "FFA AE PHANTOMS", _arena_gametype_set_ae, game_engine_slayer, FALSE, FALSE,
		"Free for all Phantoms (everyone invisible) on AE's rules, with power item timers and waypoints; 15 minutes." },
	{ "AE ENDURE", "FFA AE ENDURANCE", _arena_gametype_set_ae, game_engine_slayer, FALSE, FALSE,
		"Free for all Endurance (5 lives, respawns slow down) on AE's rules, with power item timers and waypoints; 15 minutes." },
	{ "AE FFA ROCK", "FFA AE ROCKETS", _arena_gametype_set_ae, game_engine_slayer, FALSE, FALSE,
		"Free for all slayer with rocket launchers on AE's rules, with power item timers and waypoints; 15 minutes." },
	{ "AE FFA SNPR", "FFA AE SNIPERS", _arena_gametype_set_ae, game_engine_slayer, FALSE, FALSE,
		"Free for all slayer with sniper rifles and pistols on AE's rules, with power item timers and waypoints; 15 minutes." },
	{ "AE FFA BALL", "FFA AE ODDBALL", _arena_gametype_set_ae, game_engine_oddball, FALSE, FALSE,
		"Free for all Oddball on AE's rules, with power item timers and waypoints and the motion tracker. "
		"The ball carrier's melee kills in one hit." },
	{ "AE JUGGER", "FFA AE JUGGERNAUT", _arena_gametype_set_ae, game_engine_oddball, FALSE, FALSE,
		"Free for all Juggernaut on AE's rules; the carrier does extra damage. Power item timers and waypoints; 15 minutes." },
	{ "AE REV TAG", "FFA AE REVERSE TAG", _arena_gametype_set_ae, game_engine_oddball, FALSE, FALSE,
		"Free for all Reverse Tag on AE's rules: score by not holding the ball. Power item timers and waypoints; 15 minutes." },
	{ "AE ACCUM", "FFA AE ACCUMULATION", _arena_gametype_set_ae, game_engine_oddball, FALSE, FALSE,
		"Free for all Accumulation (many balls) on AE's rules, with power item timers and waypoints; 15 minutes." },
	{ "AE STALKER", "FFA AE STALKER", _arena_gametype_set_ae, game_engine_oddball, FALSE, FALSE,
		"Free for all Stalker on AE's rules: the carrier is invisible and the others resist damage. Power item timers and waypoints; 15 minutes." },
	{ "AE FFA KING", "FFA AE KING", _arena_gametype_set_ae, game_engine_king, FALSE, FALSE,
		"Free for all King of the Hill on AE's rules, with power item timers and waypoints; 15 minutes." },
	{ "AE CRAZYKNG", "FFA AE CRAZY KING", _arena_gametype_set_ae, game_engine_king, FALSE, FALSE,
		"Free for all Crazy King (the hill moves) on AE's rules, with power item timers and waypoints; 15 minutes." },
	{ "AE RACE", "FFA AE RACE", _arena_gametype_set_ae, game_engine_race, FALSE, FALSE,
		"Free for all Race on AE's rules, in the map's vehicles; needs a map with race flags. Power item timers and waypoints." },
	{ "AE RALLY", "FFA AE RALLY", _arena_gametype_set_ae, game_engine_race, FALSE, FALSE,
		"Free for all Rally (flag rally) on AE's rules, in the map's vehicles; needs a map with race flags." },
	{ "AE PRACTICE", "FFA AE PRACTICE", _arena_gametype_set_ae, game_engine_slayer, FALSE, FALSE,
		"Free for all practice: every weapon and powerup respawns every 30 seconds; 500 kills to win, no time limit, "
		"the map's vehicles." },
	/* AE casual: teams */
	{ "AE TEAM SLY", "TEAM AE SLAYER", _arena_gametype_set_ae, game_engine_slayer, TRUE, FALSE,
		"Team slayer on AE's rules, with power item timers and waypoints and the motion tracker." },
	{ "AE CTF", "TEAM AE CTF", _arena_gametype_set_ae, game_engine_ctf, TRUE, FALSE,
		"Capture the flag on AE's rules, with power item timers and waypoints and the motion tracker." },
	{ "AE KING", "TEAM AE KING", _arena_gametype_set_ae, game_engine_king, TRUE, FALSE,
		"Team King of the Hill on AE's rules, with power item timers and waypoints and the motion tracker." },
	{ "AE TEAM OB", "TEAM AE ODDBALL", _arena_gametype_set_ae, game_engine_oddball, TRUE, FALSE,
		"Team Oddball on AE's rules, with power item timers and waypoints and the motion tracker. "
		"The ball carrier's melee kills in one hit." },
	{ "AE 2V2 SLY", "TEAM AE 2V2 SLAYER", _arena_gametype_set_ae, game_engine_slayer, TRUE, FALSE,
		"Team slayer for two against two (split screen too), to 25 kills, on AE's rules." },
	{ "AE 2V2 CTF", "TEAM AE 2V2 CTF", _arena_gametype_set_ae, game_engine_ctf, TRUE, FALSE,
		"Capture the flag for two against two (split screen too), on AE's rules." },
	{ "AE 2V2 KING", "TEAM AE 2V2 KING", _arena_gametype_set_ae, game_engine_king, TRUE, FALSE,
		"Team King of the Hill for two against two (split screen too), on AE's rules." },
	{ "AE 2V2 BALL", "TEAM AE 2V2 ODDBALL", _arena_gametype_set_ae, game_engine_oddball, TRUE, FALSE,
		"Team Oddball for two against two (split screen too), on AE's rules. The ball carrier's melee kills in "
		"one hit." },
	/* AE casual's special team modes */
	{ "AE SNIPERS", "TEAM AE SNIPERS", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with sniper rifles and pistols, sniper ammo on the map, no motion tracker." },
	{ "AE SHOTSNIP", "TEAM AE SHOTTY SNIPERS", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with a shotgun and a sniper rifle each and no weapons on the map, no motion tracker." },
	{ "AE SWAT", "TEAM AE SWAT", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with pistols, no shields and classic health: a headshot kills. No grenades, no motion tracker." },
	{ "AE ROCKETS", "TEAM AE ROCKETS", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with rocket launchers." },
	{ "AE SHOTGUNS", "TEAM AE SHOTGUNS", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with shotguns and pistols." },
	{ "AE HEAVIES", "TEAM AE HEAVIES", _arena_gametype_set_ae, game_engine_slayer, TRUE, TRUE,
		"Team slayer with rocket launchers, assault rifles, the heavy weapons and the map's vehicles, to 75 kills." },
	/* AE GRIFBALL and AE BOMB (revision 24; owner, 2026-10-23): the Bomb game type on AE's rules (seeded only by the
	builds with Bomb, arena_gametypes.c) */
	{ "AE GRIFBALL", "TEAM AE GRIFBALL", _arena_gametype_set_ae, game_engine_bomb, TRUE, TRUE,
		"Grifball: carry the bomb into the other team's goal. Melee only, one hit kills, no shields; rounds of 3 "
		"minutes, first to 3." },
	{ "AE BOMB", "TEAM AE BOMB", _arena_gametype_set_ae, game_engine_bomb, TRUE, FALSE,
		"Carry the bomb into the other team's goal and arm it, on AE's rules: pistol and assault rifle, the motion "
		"tracker, first to 3 rounds." },
	/* AE SND (revision 26; owner, 2026-10-26): AE BOMB with the SEARCH AND DESTROY bit (ae_bomb_snd_rules.h) */
	{ "AE SND", "TEAM AE SEARCH AND DESTROY", _arena_gametype_set_ae, game_engine_bomb, TRUE, FALSE,
		"Search and Destroy: one life a round; attackers plant the bomb at the defenders' goal, defenders stop or "
		"defuse it. Sides swap every 3 rounds, first to 4." },
	/* AE BTB (revision 22; owner, 2026-10-22): big team games, 8 against 8, on AE's rules, with the map's vehicles */
	{ "AE BTB SLY", "TEAM AE BTB SLAYER", _arena_gametype_set_ae, game_engine_slayer, TRUE, FALSE,
		"Big team slayer (8 against 8) on AE's rules: 100 kills, 20 minutes, the map's vehicles, power item timers "
		"and waypoints." },
	{ "AE BTB CTF", "TEAM AE BTB CTF", _arena_gametype_set_ae, game_engine_ctf, TRUE, FALSE,
		"Big team capture the flag (8 against 8) on AE's rules: 3 captures, 20 minutes, the map's vehicles." },
	{ "AE BTB KING", "TEAM AE BTB KING", _arena_gametype_set_ae, game_engine_king, TRUE, FALSE,
		"Big team King of the Hill (8 against 8) on AE's rules: 5 minutes in the hill, 20 minutes, the map's "
		"vehicles." },
	{ "AE BTB BALL", "TEAM AE BTB ODDBALL", _arena_gametype_set_ae, game_engine_oddball, TRUE, FALSE,
		"Big team Oddball (8 against 8) on AE's rules: 5 minutes with the ball, 20 minutes, the map's vehicles. The "
		"ball carrier's melee kills in one hit." },
	{ "AE BTB HVY", "TEAM AE BTB HEAVIES", _arena_gametype_set_ae, game_engine_slayer, TRUE, FALSE,
		"Big team slayer (8 against 8) with the heavy vehicles: rocket warthogs and scorpions for both teams where "
		"the map has them. 100 kills, 20 minutes." },
	/* AE BR (revision 11): AE casual's and AE COMP's gametypes with the battle rifle in hand; free for all first */
	{ "AE BR FFA", "FFA AE BR SLAYER", _arena_gametype_set_ae_br, game_engine_slayer, FALSE, FALSE,
		"Free for all slayer on AE's rules with power item timers and waypoints. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR ELIM", "FFA AE BR ELIMINATION", _arena_gametype_set_ae_br, game_engine_slayer, FALSE, FALSE,
		"Free for all Elimination on AE's rules: one life each, last standing. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR PHANT", "FFA AE BR PHANTOMS", _arena_gametype_set_ae_br, game_engine_slayer, FALSE, FALSE,
		"Free for all Phantoms (everyone invisible) on AE's rules. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR ENDUR", "FFA AE BR ENDURANCE", _arena_gametype_set_ae_br, game_engine_slayer, FALSE, FALSE,
		"Free for all Endurance (5 lives, respawns slow down) on AE's rules. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR FBALL", "FFA AE BR ODDBALL", _arena_gametype_set_ae_br, game_engine_oddball, FALSE, FALSE,
		"Free for all Oddball on AE's rules; the ball carrier's melee kills in one hit. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR JUGG", "FFA AE BR JUGGERNAUT", _arena_gametype_set_ae_br, game_engine_oddball, FALSE, FALSE,
		"Free for all Juggernaut on AE's rules (to 15). Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR RTAG", "FFA AE BR REVERSE TAG", _arena_gametype_set_ae_br, game_engine_oddball, FALSE, FALSE,
		"Free for all Reverse Tag on AE's rules: score by not holding the ball. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR ACCUM", "FFA AE BR ACCUMULATION", _arena_gametype_set_ae_br, game_engine_oddball, FALSE, FALSE,
		"Free for all Accumulation (many balls) on AE's rules. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR STALK", "FFA AE BR STALKER", _arena_gametype_set_ae_br, game_engine_oddball, FALSE, FALSE,
		"Free for all Stalker on AE's rules: the carrier is invisible. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR FKING", "FFA AE BR KING", _arena_gametype_set_ae_br, game_engine_king, FALSE, FALSE,
		"Free for all King of the Hill on AE's rules. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR CKING", "FFA AE BR CRAZY KING", _arena_gametype_set_ae_br, game_engine_king, FALSE, FALSE,
		"Free for all Crazy King (the hill moves) on AE's rules. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR PRAC", "FFA AE BR PRACTICE", _arena_gametype_set_ae_br, game_engine_slayer, FALSE, FALSE,
		"Free for all practice: every weapon and powerup every 30 seconds, 500 kills. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR C FFA", "FFA AE BR COMP SLAYER", _arena_gametype_set_ae_br, game_engine_slayer, FALSE, FALSE,
		"Competitive free for all (AE COMP FFA's rules): 5 second respawns, 2 frags. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR TS", "TEAM AE BR SLAYER", _arena_gametype_set_ae_br, game_engine_slayer, TRUE, FALSE,
		"Team slayer on AE's rules with power item timers and waypoints. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR CTF", "TEAM AE BR CTF", _arena_gametype_set_ae_br, game_engine_ctf, TRUE, FALSE,
		"Capture the flag on AE's rules with power item timers and waypoints. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR KING", "TEAM AE BR KING", _arena_gametype_set_ae_br, game_engine_king, TRUE, FALSE,
		"Team King of the Hill on AE's rules. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR TM OB", "TEAM AE BR ODDBALL", _arena_gametype_set_ae_br, game_engine_oddball, TRUE, FALSE,
		"Team Oddball on AE's rules; the ball carrier's melee kills in one hit. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR 2V2 S", "TEAM AE BR 2V2 SLAYER", _arena_gametype_set_ae_br, game_engine_slayer, TRUE, FALSE,
		"Team slayer for two against two, to 25 kills, on AE's rules. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR 2V2 C", "TEAM AE BR 2V2 CTF", _arena_gametype_set_ae_br, game_engine_ctf, TRUE, FALSE,
		"Capture the flag for two against two on AE's rules. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR 2V2 K", "TEAM AE BR 2V2 KING", _arena_gametype_set_ae_br, game_engine_king, TRUE, FALSE,
		"Team King of the Hill for two against two on AE's rules. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR 2V2 B", "TEAM AE BR 2V2 ODDBALL", _arena_gametype_set_ae_br, game_engine_oddball, TRUE, FALSE,
		"Team Oddball for two against two on AE's rules. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR C TS", "TEAM AE BR COMP SLAYER", _arena_gametype_set_ae_br, game_engine_slayer, TRUE, FALSE,
		"Competitive team slayer (AE COMP TS's rules): 5 second respawns, 2 frags. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR C CTF", "TEAM AE BR COMP CTF", _arena_gametype_set_ae_br, game_engine_ctf, TRUE, FALSE,
		"Competitive capture the flag (AE COMP CTF's rules); flags in line of sight. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR C KOH", "TEAM AE BR COMP KING", _arena_gametype_set_ae_br, game_engine_king, TRUE, FALSE,
		"Competitive team King (AE COMP KOH's rules); the hill in line of sight. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	{ "AE BR C OB", "TEAM AE BR COMP ODDBALL", _arena_gametype_set_ae_br, game_engine_oddball, TRUE, FALSE,
		"Competitive team Oddball (AE COMP OB's rules); lethal ball melee. Battle rifle in hand, assault rifle second (the battle rifle pack needed)." },
	/* (revision 22: the AE BTB set's and the AE COMP 2V2 set's) */
	{ "AE BR BTB S", "TEAM AE BR BTB SLAYER", _arena_gametype_set_ae_br, game_engine_slayer, TRUE, FALSE,
		"Big team slayer (8 against 8), 100 kills, the map's vehicles. Battle rifle in hand, assault rifle second (the "
		"battle rifle pack needed)." },
	{ "AE BR BTB C", "TEAM AE BR BTB CTF", _arena_gametype_set_ae_br, game_engine_ctf, TRUE, FALSE,
		"Big team capture the flag (8 against 8), the map's vehicles. Battle rifle in hand, assault rifle second (the "
		"battle rifle pack needed)." },
	{ "AE BR BTB K", "TEAM AE BR BTB KING", _arena_gametype_set_ae_br, game_engine_king, TRUE, FALSE,
		"Big team King of the Hill (8 against 8), the map's vehicles. Battle rifle in hand, assault rifle second (the "
		"battle rifle pack needed)." },
	{ "AE BR BTB B", "TEAM AE BR BTB ODDBALL", _arena_gametype_set_ae_br, game_engine_oddball, TRUE, FALSE,
		"Big team Oddball (8 against 8), the map's vehicles. Battle rifle in hand, assault rifle second (the battle "
		"rifle pack needed)." },
	{ "AE BR BTB H", "TEAM AE BR BTB HEAVIES", _arena_gametype_set_ae_br, game_engine_slayer, TRUE, FALSE,
		"Big team slayer with rocket warthogs and scorpions. Battle rifle in hand, assault rifle second (the battle "
		"rifle pack needed)." },
	{ "AE BR C2 TS", "TEAM AE BR COMP 2V2 SLAYER", _arena_gametype_set_ae_br, game_engine_slayer, TRUE, FALSE,
		"Competitive 2v2 slayer (AE C2V2 TS's rules): 25 kills, 5 second respawns. Battle rifle in hand, assault "
		"rifle second (the battle rifle pack needed)." },
	{ "AE BR C2CTF", "TEAM AE BR COMP 2V2 CTF", _arena_gametype_set_ae_br, game_engine_ctf, TRUE, FALSE,
		"Competitive 2v2 capture the flag (AE C2V2 CTF's rules); flags in line of sight. Battle rifle in hand, "
		"assault rifle second (the battle rifle pack needed)." },
	{ "AE BR C2KOH", "TEAM AE BR COMP 2V2 KING", _arena_gametype_set_ae_br, game_engine_king, TRUE, FALSE,
		"Competitive 2v2 King (AE C2V2 KOH's rules); the hill in line of sight. Battle rifle in hand, assault rifle "
		"second (the battle rifle pack needed)." },
	{ "AE BR C2 OB", "TEAM AE BR COMP 2V2 ODDBALL", _arena_gametype_set_ae_br, game_engine_oddball, TRUE, FALSE,
		"Competitive 2v2 Oddball (AE C2V2 OB's rules); lethal ball melee. Battle rifle in hand, assault rifle second "
		"(the battle rifle pack needed)." },
	/* AE COMP */
	{ "AE COMP FFA", "FFA AE COMP SLAYER", _arena_gametype_set_ae_comp, game_engine_slayer, FALSE, FALSE,
		"Competitive free for all: 5 second respawns, 2 frag grenades, no motion tracker, no vehicles, power item "
		"waypoints only in line of sight, 25 kills." },
	{ "AE COMP TS", "TEAM AE COMP SLAYER", _arena_gametype_set_ae_comp, game_engine_slayer, TRUE, FALSE,
		"Competitive team slayer: 5 second respawns, 2 frag grenades, no motion tracker, no vehicles, power item "
		"waypoints only in line of sight, 50 kills." },
	{ "AE COMP CTF", "TEAM AE COMP CTF", _arena_gametype_set_ae_comp, game_engine_ctf, TRUE, FALSE,
		"Competitive capture the flag: 3 captures, 5 second respawns, 2 frag grenades, no motion tracker, no "
		"vehicles; the flags show only in line of sight." },
	{ "AE COMP KOH", "TEAM AE COMP KING", _arena_gametype_set_ae_comp, game_engine_king, TRUE, FALSE,
		"Competitive team King of the Hill: 5 minutes in the hill, 5 second respawns, 2 frags, no motion tracker, "
		"no vehicles; the hill shows only in line of sight." },
	{ "AE COMP OB", "TEAM AE COMP ODDBALL", _arena_gametype_set_ae_comp, game_engine_oddball, TRUE, FALSE,
		"Competitive team Oddball: 5 minutes with the ball, 5 second respawns, 2 frags, no motion tracker or "
		"vehicles, the ball in line of sight only, lethal ball melee." },
	/* AE COMP 2V2 (revision 22; owner, 2026-10-22): AE COMP's team gametypes for two against two */
	{ "AE C2V2 TS", "TEAM AE COMP 2V2 SLAYER", _arena_gametype_set_ae_comp, game_engine_slayer, TRUE, FALSE,
		"Competitive 2v2 slayer: 25 kills, 5 second respawns, 2 frag grenades, no motion tracker, no vehicles, power "
		"item waypoints only in line of sight." },
	{ "AE C2V2 CTF", "TEAM AE COMP 2V2 CTF", _arena_gametype_set_ae_comp, game_engine_ctf, TRUE, FALSE,
		"Competitive 2v2 capture the flag: 3 captures, 5 second respawns, 2 frag grenades, no motion tracker, no "
		"vehicles; the flags show only in line of sight." },
	{ "AE C2V2 KOH", "TEAM AE COMP 2V2 KING", _arena_gametype_set_ae_comp, game_engine_king, TRUE, FALSE,
		"Competitive 2v2 King of the Hill: 5 minutes in the hill, 5 second respawns, 2 frags, no motion tracker, no "
		"vehicles; the hill shows only in line of sight." },
	{ "AE C2V2 OB", "TEAM AE COMP 2V2 ODDBALL", _arena_gametype_set_ae_comp, game_engine_oddball, TRUE, FALSE,
		"Competitive 2v2 Oddball: 5 minutes with the ball, 5 second respawns, 2 frags, no motion tracker or vehicles, "
		"the ball in line of sight only, lethal ball melee." },

	/* OCTAGON (revision 16; owner, 2026-10-20): 1v1 practice drills (Octagon), free for all slayer on AE's casual
	rules with infinite ammo, bottomless clips, no grenades and instant respawns */
	{ "AE OCT BR", "FFA AE OCTAGON BR", _arena_gametype_set_octagon, game_engine_slayer, FALSE, FALSE,
		"1v1 drill: the battle rifle alone, infinite ammo and no reloads, no grenades, instant respawns; AE's rules "
		"(the battle rifle pack needed)." },
	{ "AE OCT BRSN", "FFA AE OCTAGON BR SNIPE", _arena_gametype_set_octagon, game_engine_slayer, FALSE, FALSE,
		"1v1 drill: battle rifle and sniper rifle, infinite ammo and no reloads, no grenades, instant respawns; AE's "
		"rules (the battle rifle pack needed)." },
	{ "AE OCT MAG", "FFA AE OCTAGON MAG", _arena_gametype_set_octagon, game_engine_slayer, FALSE, FALSE,
		"1v1 drill: the magnum alone, infinite ammo and no reloads, no grenades, instant respawns; AE's rules, 25 kills, "
		"15 minutes." },
	{ "AE OCT MGSN", "FFA AE OCTAGON MAG SNIPE", _arena_gametype_set_octagon, game_engine_slayer, FALSE, FALSE,
		"1v1 drill: magnum and sniper rifle, infinite ammo and no reloads, no grenades, instant respawns; AE's rules, "
		"25 kills, 15 minutes." },
	/* (revision 17; owner, 2026-10-20: "would be nice to have some team octagon"): the same drills as team slayer on
	AE's casual team rules (AE TEAM SLY's), to warm up with a team */
	{ "AE OCT TBR", "TEAM AE OCTAGON BR", _arena_gametype_set_octagon, game_engine_slayer, TRUE, FALSE,
		"Team drill: the battle rifle alone, infinite ammo and no reloads, no grenades, instant respawns; AE's rules "
		"(the battle rifle pack needed)." },
	{ "AE OCT TBSN", "TEAM AE OCTAGON BR SNIPE", _arena_gametype_set_octagon, game_engine_slayer, TRUE, FALSE,
		"Team drill: battle rifle and sniper rifle, infinite ammo and no reloads, no grenades, instant respawns; AE's "
		"rules (the battle rifle pack needed)." },
	{ "AE OCT TMAG", "TEAM AE OCTAGON MAG", _arena_gametype_set_octagon, game_engine_slayer, TRUE, FALSE,
		"Team drill: the magnum alone, infinite ammo and no reloads, no grenades, instant respawns; AE's rules, 50 "
		"kills, 15 minutes." },
	{ "AE OCT TMSN", "TEAM AE OCTAGON MAG SNIPE", _arena_gametype_set_octagon, game_engine_slayer, TRUE, FALSE,
		"Team drill: magnum and sniper rifle, infinite ammo and no reloads, no grenades, instant respawns; AE's rules, "
		"50 kills, 15 minutes." },

	/* the casual set's values with Halo 1: NHE's own modes */
	{ "NHE TS VAN", "TEAM NHE VANILLA", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"Team slayer on AE's rules with power item timers and waypoints. On Halo 1: NHE's maps, NHE's VANILLA mode "
		"(no NHE timers, no countdown)." },
	{ "NHE TS POW", "TEAM NHE POWERUPS", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"Team slayer on AE's rules with power item timers and waypoints. On Halo 1: NHE's maps, NHE & POWERUPS." },
	/* Halo 1: NHE's 23 gametypes, in its own order (010 to 230) */
	{ "TS 50", "TEAM NHE SLAYER 50", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"NHE's TS 50: 50 kills, 5 second respawns, no motion tracker. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "TS 100", "TEAM NHE SLAYER 100", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"NHE's TS 100: 100 kills, 5 second respawns, a 10 second suicide penalty, no motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "TS ON-OFF", "TEAM NHE SLAYER ON-OFF", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"NHE's TS ON-OFF: 50 kills, 5 second respawns, no motion tracker. On Halo 1: NHE's maps, TIMER ONLY." },
	{ "TS TRAINING", "TEAM NHE TRAINING", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"NHE's TS TRAINING: 50 kills, 5 second respawns, no motion tracker. On Halo 1: NHE's maps, TRAINING." },
	{ "TS PRACTICE", "TEAM NHE PRACTICE", _arena_gametype_set_nhe, game_engine_race, TRUE, FALSE,
		"NHE's TS PRACTICE: instant respawns, infinite grenades, every weapon and powerup every 30 seconds. "
		"On Halo 1: NHE's maps, TRAINING." },
	{ "TS SNIPERS", "TEAM NHE SNIPERS", _arena_gametype_set_nhe, game_engine_slayer, TRUE, FALSE,
		"NHE's TS SNIPERS: invisible players with sniper rifles, 50 kills, 5 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "FFA 50 NR", "FFA NHE SLAYER 50", _arena_gametype_set_nhe, game_engine_slayer, FALSE, FALSE,
		"NHE's FFA 50: 50 kills, 5 second respawns, no motion tracker. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "FFA 50 R", "FFA NHE SLAYER 50 RADAR", _arena_gametype_set_nhe, game_engine_slayer, FALSE, FALSE,
		"NHE's FFA 50 with the motion tracker: 50 kills, 5 second respawns. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "1 V 1 NR", "FFA NHE 1V1", _arena_gametype_set_nhe, game_engine_slayer, FALSE, FALSE,
		"NHE's 1 V 1: 15 kills, 5 second respawns, no motion tracker. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "1 V 1 R", "FFA NHE 1V1 RADAR", _arena_gametype_set_nhe, game_engine_slayer, FALSE, FALSE,
		"NHE's 1 V 1 with the motion tracker: 15 kills, 5 second respawns. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "KOTH 5M 7S", "TEAM NHE KING 7.5S", _arena_gametype_set_nhe, game_engine_king, TRUE, FALSE,
		"NHE's King of the Hill: 5 minutes in a moving hill, 7.5 second respawns, a 10 second suicide penalty. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "KOTH 5M 10S", "TEAM NHE KING 10S", _arena_gametype_set_nhe, game_engine_king, TRUE, FALSE,
		"NHE's King of the Hill: 5 minutes in a moving hill, 10 second respawns. On Halo 1: NHE's maps, NHE & TIMER." },
	{ "BALL 5M 7S", "TEAM NHE ODDBALL 7.5S", _arena_gametype_set_nhe, game_engine_oddball, TRUE, FALSE,
		"NHE's Oddball: 5 minutes with the ball (slow while you carry it), 7.5 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "BALL 5M 10S", "TEAM NHE ODDBALL 10S", _arena_gametype_set_nhe, game_engine_oddball, TRUE, FALSE,
		"NHE's Oddball: 5 minutes with the ball (slow while you carry it), 10 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 3C 7S", "TEAM NHE CTF 3 7.5S", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF: 3 captures, your flag at home to score, 7.5 second respawns, no motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 3C 7S R", "TEAM NHE CTF 3 7.5S RADAR", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF with the motion tracker: 3 captures, your flag at home to score, 7.5 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 3C 10S", "TEAM NHE CTF 3 10S", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF: 3 captures, your flag at home to score, 10 second respawns, no motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 3 10S R", "TEAM NHE CTF 3 10S RADAR", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF with the motion tracker: 3 captures, your flag at home to score, 10 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 5C 7S", "TEAM NHE CTF 5 7.5S", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF: 5 captures, your flag at home to score, 7.5 second respawns, no motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 5C 7S R", "TEAM NHE CTF 5 7.5S RADAR", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF with the motion tracker: 5 captures, your flag at home to score, 7.5 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 5C 10S", "TEAM NHE CTF 5 10S", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF: 5 captures, your flag at home to score, 10 second respawns, no motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF 5 10S R", "TEAM NHE CTF 5 10S RADAR", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF with the motion tracker: 5 captures, your flag at home to score, 10 second respawns. "
		"On Halo 1: NHE's maps, NHE & TIMER." },
	{ "CTF WIZARD", "TEAM NHE CTF WIZARD", _arena_gametype_set_nhe, game_engine_ctf, TRUE, FALSE,
		"NHE's CTF WIZARD: pistols, 5 captures, 10 second respawns, the motion tracker. "
		"On Halo 1: NHE's maps, NHE & TIMER." },

	/* TRAINING */
	{ "AE TRAINING", "FFA AE TRAINING", _arena_gametype_set_training, game_engine_slayer, FALSE, FALSE,
		"Free for all training: every item's spawn and the players' spawns marked, with timers; 500 kills to win, "
		"no time limit." },
};

/* old stored names: the seeds of before, kept by players or not yet moved */
static struct arena_gametype_alias const arena_gametype_aliases[] =
{
	{ "AE SLAYER", "AE FFA SLAY" },
	{ "AE ODDBALL", "AE FFA BALL" },
	{ "AE PRO FFA", "AE COMP FFA" },
	{ "AE PRO TS", "AE COMP TS" },
	{ "AE PRO CTF", "AE COMP CTF" },
	{ "AE PRO KING", "AE COMP KOH" },
	{ "AE PRO BALL", "AE COMP OB" },
	{ "NHE 1V1", "1 V 1 NR" },
	{ "NHE 2V2 TS", "TS 50" },
	{ "NHE CTF", "CTF 3C 7S" },
	{ "PRACTICE", "TS PRACTICE" },
	{ "AE VANILLA", "NHE TS VAN" },
	{ "AE POWERUPS", "NHE TS POW" },
	/* (a file kept under the names of before revision 6: the same two) */
	{ "NHE VANILLA", "NHE TS VAN" },
	{ "NHE POWERUP", "NHE TS POW" },
	{ "NHE TRAIN", "AE TRAINING" },
};

static char const *const arena_gametype_set_names[NUMBER_OF_ARENA_GAMETYPE_SETS] =
{
	"", "AE", "AE BR", "AE COMP", "OCTAGON", "NHE", "TRAINING"
};

/* the lists' categories: the playlist folders (owner 2026-10-22; "by play style, BR inside" before, 2026-10-20):
each folder's gametypes in the lists' order, a gametype's battle rifle version right after it; Halo 1: NHE's set
is its own folder, in its table order (arena_gametype_category_fill). STOCK (Halo CE's built-in gametypes) has no
rows: no seed is in it */
struct arena_gametype_category_row
{
	short category;
	char const *stored_name;
};

static struct arena_gametype_category_row const arena_gametype_category_rows[] =
{
	/* AE FFA: slayer, king, oddball, the competitive slayer, then snipers and rockets */
	{ _arena_gametype_category_ffa, "AE FFA SLAY" }, { _arena_gametype_category_ffa, "AE BR FFA" },
	{ _arena_gametype_category_ffa, "AE FFA KING" }, { _arena_gametype_category_ffa, "AE BR FKING" },
	{ _arena_gametype_category_ffa, "AE FFA BALL" }, { _arena_gametype_category_ffa, "AE BR FBALL" },
	{ _arena_gametype_category_ffa, "AE COMP FFA" }, { _arena_gametype_category_ffa, "AE BR C FFA" },
	{ _arena_gametype_category_ffa, "AE FFA SNPR" }, { _arena_gametype_category_ffa, "AE FFA ROCK" },
	/* AE 2V2: the casual 2v2s, then the competitive ones */
	{ _arena_gametype_category_2v2, "AE 2V2 SLY" }, { _arena_gametype_category_2v2, "AE BR 2V2 S" },
	{ _arena_gametype_category_2v2, "AE 2V2 CTF" }, { _arena_gametype_category_2v2, "AE BR 2V2 C" },
	{ _arena_gametype_category_2v2, "AE 2V2 KING" }, { _arena_gametype_category_2v2, "AE BR 2V2 K" },
	{ _arena_gametype_category_2v2, "AE 2V2 BALL" }, { _arena_gametype_category_2v2, "AE BR 2V2 B" },
	{ _arena_gametype_category_2v2, "AE C2V2 TS" }, { _arena_gametype_category_2v2, "AE BR C2 TS" },
	{ _arena_gametype_category_2v2, "AE C2V2 CTF" }, { _arena_gametype_category_2v2, "AE BR C2CTF" },
	{ _arena_gametype_category_2v2, "AE C2V2 KOH" }, { _arena_gametype_category_2v2, "AE BR C2KOH" },
	{ _arena_gametype_category_2v2, "AE C2V2 OB" }, { _arena_gametype_category_2v2, "AE BR C2 OB" },
	/* AE 4V4: the casual team games, then the competitive ones */
	{ _arena_gametype_category_4v4, "AE TEAM SLY" }, { _arena_gametype_category_4v4, "AE BR TS" },
	{ _arena_gametype_category_4v4, "AE CTF" }, { _arena_gametype_category_4v4, "AE BR CTF" },
	{ _arena_gametype_category_4v4, "AE KING" }, { _arena_gametype_category_4v4, "AE BR KING" },
	{ _arena_gametype_category_4v4, "AE TEAM OB" }, { _arena_gametype_category_4v4, "AE BR TM OB" },
	{ _arena_gametype_category_4v4, "AE COMP TS" }, { _arena_gametype_category_4v4, "AE BR C TS" },
	{ _arena_gametype_category_4v4, "AE COMP CTF" }, { _arena_gametype_category_4v4, "AE BR C CTF" },
	{ _arena_gametype_category_4v4, "AE COMP KOH" }, { _arena_gametype_category_4v4, "AE BR C KOH" },
	{ _arena_gametype_category_4v4, "AE COMP OB" }, { _arena_gametype_category_4v4, "AE BR C OB" },
	/* AE BTB (revision 22) */
	{ _arena_gametype_category_btb, "AE BTB SLY" }, { _arena_gametype_category_btb, "AE BR BTB S" },
	{ _arena_gametype_category_btb, "AE BTB CTF" }, { _arena_gametype_category_btb, "AE BR BTB C" },
	{ _arena_gametype_category_btb, "AE BTB KING" }, { _arena_gametype_category_btb, "AE BR BTB K" },
	{ _arena_gametype_category_btb, "AE BTB BALL" }, { _arena_gametype_category_btb, "AE BR BTB B" },
	{ _arena_gametype_category_btb, "AE BTB HVY" }, { _arena_gametype_category_btb, "AE BR BTB H" },
	/* AE OBJECTIVE: the round modes */
	{ _arena_gametype_category_objective, "AE SND" }, { _arena_gametype_category_objective, "AE BOMB" },
	/* AE ACTION SACK: the party modes, then the weapon modes */
	{ _arena_gametype_category_action_sack, "AE PHANTOMS" }, { _arena_gametype_category_action_sack, "AE BR PHANT" },
	{ _arena_gametype_category_action_sack, "AE JUGGER" }, { _arena_gametype_category_action_sack, "AE BR JUGG" },
	{ _arena_gametype_category_action_sack, "AE REV TAG" }, { _arena_gametype_category_action_sack, "AE BR RTAG" },
	{ _arena_gametype_category_action_sack, "AE ACCUM" }, { _arena_gametype_category_action_sack, "AE BR ACCUM" },
	{ _arena_gametype_category_action_sack, "AE STALKER" }, { _arena_gametype_category_action_sack, "AE BR STALK" },
	{ _arena_gametype_category_action_sack, "AE CRAZYKNG" }, { _arena_gametype_category_action_sack, "AE BR CKING" },
	{ _arena_gametype_category_action_sack, "AE ELIM" }, { _arena_gametype_category_action_sack, "AE BR ELIM" },
	{ _arena_gametype_category_action_sack, "AE ENDURE" }, { _arena_gametype_category_action_sack, "AE BR ENDUR" },
	{ _arena_gametype_category_action_sack, "AE RACE" }, { _arena_gametype_category_action_sack, "AE RALLY" },
	{ _arena_gametype_category_action_sack, "AE GRIFBALL" },
	{ _arena_gametype_category_action_sack, "AE SWAT" }, { _arena_gametype_category_action_sack, "AE SNIPERS" },
	{ _arena_gametype_category_action_sack, "AE SHOTSNIP" }, { _arena_gametype_category_action_sack, "AE ROCKETS" },
	{ _arena_gametype_category_action_sack, "AE SHOTGUNS" }, { _arena_gametype_category_action_sack, "AE HEAVIES" },
	/* AE PRACTICE: the practice modes, then the OCTAGON drills */
	{ _arena_gametype_category_practice, "AE PRACTICE" }, { _arena_gametype_category_practice, "AE BR PRAC" },
	{ _arena_gametype_category_practice, "AE TRAINING" }, { _arena_gametype_category_practice, "AE OCT BR" },
	{ _arena_gametype_category_practice, "AE OCT BRSN" }, { _arena_gametype_category_practice, "AE OCT MAG" },
	{ _arena_gametype_category_practice, "AE OCT MGSN" },
	{ _arena_gametype_category_practice, "AE OCT TBR" }, { _arena_gametype_category_practice, "AE OCT TBSN" },
	{ _arena_gametype_category_practice, "AE OCT TMAG" }, { _arena_gametype_category_practice, "AE OCT TMSN" },
};

static char const *const arena_gametype_category_names[NUMBER_OF_ARENA_GAMETYPE_CATEGORIES] =
{
	"", "AE FFA", "AE 2V2", "AE 4V4", "AE BTB", "AE OBJECTIVE", "AE ACTION SACK", "AE PRACTICE", "NHE", "STOCK"
};

/* ---------- private code */

/* a stored name (wide, NUL-ended or 11 characters at most) and an ASCII
one, letters' case aside */
static boolean arena_gametype_name_equal(
	wchar_t const *stored_name,
	char const *name)
{
	short index;

	for (index = 0; index <= ARENA_STORED_NAME_MAXIMUM; index++)
	{
		wchar_t a = index < ARENA_STORED_NAME_MAXIMUM ? stored_name[index] : 0;
		wchar_t b = (wchar_t)(unsigned char)name[index];

		if (a >= 'a' && a <= 'z')
			a = (wchar_t)(a - 'a' + 'A');
		if (b >= 'a' && b <= 'z')
			b = (wchar_t)(b - 'a' + 'A');
		if (a != b)
			return FALSE;
		if (!a)
			return TRUE;
	}
	return FALSE;
}

/* two of the table's ASCII names, exactly (no C library: the check links
this file alone) */
static boolean arena_gametype_ascii_equal(
	char const *a,
	char const *b)
{
	for (; *a == *b; a++, b++)
	{
		if (!*a)
			return TRUE;
	}
	return FALSE;
}

/* the table's entry of that stored name, else NONE */
static short arena_gametype_name_index(
	wchar_t const *stored_name)
{
	short index;

	for (index = 0; index < (short)NUMBEROF(arena_gametype_names); index++)
	{
		if (arena_gametype_name_equal(stored_name, arena_gametype_names[index].stored_name))
			return index;
	}
	return NONE;
}

/* the entry's place among its set's */
static short arena_gametype_name_order(
	short name_index)
{
	short order = 0;
	short index;

	for (index = 0; index < name_index; index++)
	{
		if (arena_gametype_names[index].set == arena_gametype_names[name_index].set)
			order++;
	}
	return order;
}

/* the entry's category and place in it: its row in the categories' table, else
by its set (NHE's in its table order; one of AE's left out of the table at
its category's end, the names check fails on it) */
static void arena_gametype_category_fill(
	short name_index,
	struct arena_gametype_info *info)
{
	struct arena_gametype_name const *name = &arena_gametype_names[name_index];
	short row;
	short order = 0;

	for (row = 0; row < (short)NUMBEROF(arena_gametype_category_rows); row++)
	{
		struct arena_gametype_category_row const *category_row = &arena_gametype_category_rows[row];

		if (row > 0 && category_row->category != arena_gametype_category_rows[row - 1].category)
			order = 0;
		if (arena_gametype_ascii_equal(category_row->stored_name, name->stored_name))
		{
			info->category = category_row->category;
			info->category_order = order;
			return;
		}
		order++;
	}
	switch (name->set)
	{
		case _arena_gametype_set_nhe:
			info->category = _arena_gametype_category_nhe;
			break;
		case _arena_gametype_set_octagon:
		case _arena_gametype_set_training:
			info->category = _arena_gametype_category_practice;
			break;
		default:
			info->category = name->team ? _arena_gametype_category_4v4 : _arena_gametype_category_ffa;
			break;
	}
	info->category_order = (short)(NUMBEROF(arena_gametype_category_rows) + info->order);
}

static void arena_gametype_info_fill(
	short name_index,
	struct arena_gametype_info *info)
{
	struct arena_gametype_name const *name = &arena_gametype_names[name_index];

	info->set = name->set;
	info->order = arena_gametype_name_order(name_index);
	info->team = name->team;
	info->special = name->special;
	info->game_engine = name->game_engine;
	info->display_name = name->display_name;
	info->description = name->description;
	arena_gametype_category_fill(name_index, info);
}

/* ---------- public code */

short arena_gametype_list_block(
	boolean known,
	struct arena_gametype_info const *info)
{
	if (!known)
		return 10;
	/* (AE FFA 1 .. NHE 8; STOCK 9 is no seed's) */
	return info->category > _arena_gametype_category_none && info->category < NUMBER_OF_ARENA_GAMETYPE_CATEGORIES ?
		info->category : 10;
}

boolean arena_gametype_has_ae_rules(
	wchar_t const *stored_name)
{
	short name_index = stored_name ? arena_gametype_name_index(stored_name) : NONE;
	short set;

	if (name_index == NONE)
		return FALSE;
	set = arena_gametype_names[name_index].set;
	return set == _arena_gametype_set_ae || set == _arena_gametype_set_ae_br || set == _arena_gametype_set_ae_comp ||
		set == _arena_gametype_set_octagon || set == _arena_gametype_set_training;
}

boolean arena_gametype_info(
	wchar_t const *stored_name,
	struct arena_gametype_info *info)
{
	short name_index;
	short index;

	if (!stored_name)
		return FALSE;
	name_index = arena_gametype_name_index(stored_name);
	if (name_index != NONE)
	{
		if (info)
			arena_gametype_info_fill(name_index, info);
		return TRUE;
	}
	for (index = 0; index < (short)NUMBEROF(arena_gametype_aliases); index++)
	{
		struct arena_gametype_alias const *alias = &arena_gametype_aliases[index];
		short successor;

		if (!arena_gametype_name_equal(stored_name, alias->stored_name))
			continue;
		successor = NONE;
		for (name_index = 0; name_index < (short)NUMBEROF(arena_gametype_names); name_index++)
		{
			if (arena_gametype_ascii_equal(arena_gametype_names[name_index].stored_name, alias->successor))
				successor = name_index;
		}
		if (successor == NONE)
			return FALSE;
		if (info)
		{
			/* (its successor's place, its own name) */
			arena_gametype_info_fill(successor, info);
			info->special = FALSE;
			info->display_name = alias->stored_name;
			info->description = NULL;
		}
		return TRUE;
	}
	return FALSE;
}

void arena_gametype_display_name(
	wchar_t const *stored_name,
	wchar_t *display_name,
	short size)
{
	short name_index;
	short index;

	if (!display_name || size <= 0)
		return;
	display_name[0] = 0;
	if (!stored_name)
		return;
	name_index = arena_gametype_name_index(stored_name);
	if (name_index != NONE)
	{
		char const *name = arena_gametype_names[name_index].display_name;

		for (index = 0; index < size - 1 && name[index]; index++)
			display_name[index] = (wchar_t)(unsigned char)name[index];
		display_name[index] = 0;
		return;
	}
	/* (the stored name as it is, an alias's too) */
	for (index = 0; index < size - 1 && index < ARENA_STORED_NAME_MAXIMUM && stored_name[index]; index++)
		display_name[index] = stored_name[index];
	display_name[index] = 0;
}

char const *arena_gametype_set_name(
	short set)
{
	return set >= 0 && set < NUMBER_OF_ARENA_GAMETYPE_SETS ? arena_gametype_set_names[set] : "";
}

char const *arena_gametype_category_name(
	short category)
{
	return category >= 0 && category < NUMBER_OF_ARENA_GAMETYPE_CATEGORIES ? arena_gametype_category_names[category] : "";
}

short arena_gametype_catalog_count(
	void)
{
	return (short)NUMBEROF(arena_gametype_names);
}

char const *arena_gametype_catalog_stored_name(
	short index)
{
	return index >= 0 && index < (short)NUMBEROF(arena_gametype_names) ? arena_gametype_names[index].stored_name :
		NULL;
}
