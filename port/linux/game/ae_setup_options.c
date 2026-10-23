/*
AE_SETUP_OPTIONS.C

The game setup spike's GAME OPTIONS model. See ae_setup_options.h. The texts are the PC editor's (tools/port_settings.py
for AE's rows, the stock screens' for the rest, as the shelved M4's ae_strings.c has them) with Halo 2's names where
the brief gives them (ACTIVE CAMO, MOTION SENSOR, FORCE EVEN TEAMS, BETRAYAL PENALTY, GRENADES ON MAP, SHIELD TYPE),
and the 0.3.6 drafts' for GRENADES ON MAP and SENSOR RANGE. Each row's values are in the order of the glue's raw
values (ae_glue_setup_options.c), which are menu_functions.c's gametype_options[] order.
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "ae_setup_options.h"

#define COUNT(list) ((short)(sizeof(list) / sizeof((list)[0])))
#define ENGINE(engine) (short)(1 << (engine))
#define ALL_BUT_SLAYER (short)(ENGINE(AE_SETUP_ENGINE_CTF) | ENGINE(AE_SETUP_ENGINE_ODDBALL) | \
	ENGINE(AE_SETUP_ENGINE_KING) | ENGINE(AE_SETUP_ENGINE_RACE))
#define TEAM_ENGINES (short)(ENGINE(AE_SETUP_ENGINE_SLAYER) | ENGINE(AE_SETUP_ENGINE_ODDBALL) | \
	ENGINE(AE_SETUP_ENGINE_KING) | ENGINE(AE_SETUP_ENGINE_RACE))

/* ---------- the texts */

static const char *const yes_no[] = { "YES", "NO" };
static const char *const on_off[] = { "ON", "OFF" };
static const char *const off_on[] = { "OFF", "ON" };

static const char *const kills_values[] = { "5", "10", "15", "25", "50", "75", "100", "150", "200", "250", "500" };
static const char *const kills_help[] = { "The kills the leading player or team needs to win, from 5 to 500." };
static const char *const captures_values[] = { "1", "3", "5", "10", "15" };
static const char *const captures_help[] = { "The flag captures a team needs to win." };
static const char *const minutes_score_values[] = { "1 MINUTE", "2 MINUTES", "5 MINUTES", "10 MINUTES", "15 MINUTES" };
static const char *const hill_help[] = { "The time on the hill a player or team needs to win." };
static const char *const ball_score_help[] = { "Minutes with the ball, or as It, to win; Juggernaut games count kills "
	"instead." };
static const char *const laps_values[] = { "1", "3", "5", "10", "15", "25" };
static const char *const laps_help[] = { "The laps a player or team needs to win." };
static const char *const time_values[] = { "NONE", "1 MINUTE", "2 MINUTES", "3 MINUTES", "4 MINUTES", "5 MINUTES",
	"6 MINUTES", "7 MINUTES", "8 MINUTES", "9 MINUTES", "10 MINUTES", "12 MINUTES", "15 MINUTES", "20 MINUTES",
	"25 MINUTES", "30 MINUTES", "45 MINUTES", "60 MINUTES" };
static const char *const time_help[] = { "The game ends when the time is up, or sooner when someone reaches the score "
	"to win. NONE: only the score ends it." };
static const char *const countdown_help[] = { "The game starts at once.",
	"A 3-2-1 countdown on a black screen starts the game, so everyone sees the map at the same time." };

static const char *const lives_values[] = { "UNLIMITED", "1", "3", "5" };
static const char *const lives_help[] = { "You respawn without limit until the game ends.", "You have one life.",
	"You have three lives.", "You have five lives." };
static const char *const respawn_values[] = { "INSTANT", "5 SECONDS", "7.5 SECONDS", "10 SECONDS", "15 SECONDS" };
static const char *const respawn_help[] = { "You respawn at once.", "You wait five seconds to respawn.",
	"You wait seven and a half seconds to respawn, as in Halo 1: NHE's objective gametypes.",
	"You wait ten seconds to respawn.", "You wait fifteen seconds to respawn." };
static const char *const growth_values[] = { "NONE", "5 SECONDS", "10 SECONDS", "15 SECONDS" };
static const char *const growth_help[] = { "Your respawn time stays at its starting value.",
	"Each death adds five seconds to your respawn time and each kill takes five off.",
	"Each death adds ten seconds to your respawn time and each kill takes ten off.",
	"Each death adds fifteen seconds to your respawn time and each kill takes fifteen off." };
static const char *const suicide_help[] = { "Ending your own life costs nothing.",
	"Your own death adds a five-second wait before you respawn.",
	"Your own death adds a ten-second wait before you respawn.",
	"Your own death adds a fifteen-second wait before you respawn." };
static const char *const shields_values[] = { "NORMAL", "NONE" };
static const char *const shields_help[] = { "Your suit has shields, which makes you harder to kill.",
	"Your suit has no shields, so everyone dies much more easily." };
static const char *const health_amount_values[] = { "50%", "100%", "150%", "200%", "300%", "400%" };
static const char *const health_amount_help[] = { "You start with half your health and half-capacity shields.",
	"You start with full health and shields.", "You start with one and a half times the health and shield strength.",
	"You start with twice the health and shield strength.", "You start with three times the health and shield strength.",
	"You start with four times the health and shield strength." };
static const char *const camo_help[] = { "All players are always invisible.",
	"All players are visible, unless they pick up active camouflage." };
static const char *const sensor_values[] = { "ALL", "FRIENDS", "ENEMIES", "NONE" };
static const char *const sensor_help[] = { "Every moving player shows on your motion sensor.",
	"Only your moving friends show on your motion sensor.",
	"Only enemies show on your motion sensor when moving, never your teammates. In a free for all, every other player.",
	"Other players never show on your motion sensor." };
static const char *const range_values[] = { "STOCK", "10 M", "15 M", "25 M" };
static const char *const range_help[] = { "The motion sensor keeps the map's own range.",
	"The motion sensor shows players within 10 m of you.", "The motion sensor shows players within 15 m of you.",
	"The motion sensor shows players within 25 m of you." };
static const char *const odd_man_help[] = { "The odd man out cannot respawn until someone else dies and takes the place.",
	"You respawn at once, without waiting for someone else to die." };
static const char *const crouch_values[] = { "STAYS DOWN", "HALO CE" };
static const char *const crouch_help[] = { "Holding crouch on a controller keeps you down, stick pushed all the way "
	"too, as on a keyboard.", "As Halo CE: on a controller, pushing the stick all the way stands you up from a crouch." };
static const char *const melee_unzoom_help[] = { "A melee while zoomed takes the zoom off and strikes, as in Halo 2.",
	"As Halo CE: no melee while zoomed; take the zoom off first." };
static const char *const mouse_aim_values[] = { "OFF", "ALLOWED" };
static const char *const mouse_aim_help[] = { "No aim assist for the mouse, for every player; controllers keep theirs.",
	"Each player's own MOUSE AIM ASSIST setting decides, unless the host blocks it." };

static const char *const team_play_help[] = { "The players play in teams.", "Every player plays for themselves." };
static const char *const friendly_fire_values[] = { "OFF", "ON", "SHIELDS ONLY", "EXPLOSIVES ONLY" };
static const char *const friendly_fire_help[] = { "Players cannot be hurt by their teammates.",
	"Players can be hurt by their teammates' weapons and explosives.",
	"Friendly fire only reduces shields; health is unaffected.", "Players can be hurt by their teammates' explosives only." };
static const char *const betrayal_help[] = { "Killing a teammate costs no respawn time.",
	"Killing a teammate adds five seconds to your next respawn.",
	"Killing a teammate adds ten seconds to your next respawn.",
	"Killing a teammate adds fifteen seconds to your next respawn." };
static const char *const even_teams_help[] = { "Players may change teams freely.",
	"Players cannot join a fuller team." };
static const char *const friends_help[] = { "A green triangle marks your teammates so you do not shoot them.",
	"Teammates are told apart only by the colour of their armour." };

static const char *const weapon_set_values[] = { "NORMAL", "PISTOLS", "RIFLES", "PLASMA WEAPONS", "SNIPER",
	"NO SNIPING", "ROCKET LAUNCHERS", "SHOTGUNS", "SHORT RANGE", "HUMAN", "COVENANT", "CLASSIC", "HEAVY WEAPONS",
	"NO GRENADES" };
static const char *const weapon_set_help[] = { "The map keeps the weapons its designers placed.",
	"Every weapon on the map becomes a pistol.", "Every weapon on the map becomes an assault rifle or plasma rifle.",
	"Every weapon on the map becomes a plasma weapon.", "The guns on the map become weapons with sniper scopes.",
	"Sniper rifles and pistols do not appear on the map.", "Every weapon on the map becomes a rocket launcher.",
	"Every weapon on the map becomes a shotgun.", "Only weapons that work at short range appear.",
	"Only human weapons appear.", "Only Covenant weapons appear.", "The flamethrower and fuel rod gun do not appear.",
	"Only the most powerful weapons appear.", "The map's weapons, without any grenades." };
static const char *const map_weapons_help[] = { "The map's weapons appear where its designers placed them.",
	"No weapons appear on the map; with a loadout of NONE and NONE you have melee and grenades only." };
static const char *const map_grenades_help[] = { "The map's grenades appear where its designers placed them.",
	"No grenades on the map: players have only the ones they spawn with." };
static const char *const equipment_values[] = { "MAP'S OWN", "GENERIC" };
static const char *const equipment_help[] = { "You start with the weapons the map designers chose for the map.",
	"You start with the generic weapons on every map." };
static const char *const loadout_values[] = { "CATEGORY", "CUSTOM" };
static const char *const loadout_help[] = { "The weapons follow the weapon set.",
	"Everyone starts with the primary and secondary weapons below." };
static const char *const loadout_weapon_values[] = { "NONE", "RANDOM", "ASSAULT RIFLE", "PISTOL", "SHOTGUN",
	"SNIPER RIFLE", "ROCKET LAUNCHER", "PLASMA PISTOL", "PLASMA RIFLE", "NEEDLER", "BATTLE RIFLE", "SMG", "SILENCED SMG",
	"CARBINE", "SILENCED AR" };
static const char *const primary_help[] = { "The weapon everyone spawns with in hand. Lists the stock weapons and "
	"the packs that are on and can start (WEAPON PACKS)." };
static const char *const secondary_help[] = { "The weapon everyone spawns with on their back. Lists the stock weapons "
	"and the packs that are on and can start (WEAPON PACKS)." };
static const char *const weapon_packs_values[] = { "NONE" };
static const char *const weapon_packs_help[] = { "Choose which weapon packs the game adds, and which can be start "
	"weapons." };
static const char *const name_values[] = { "" };
static const char *const name_help[] = { "The gametype's name, up to eleven characters. SAVE AS NEW keeps it." };
static const char *const pack_help_br[] = { "The battle rifle is not added to the map.",
	"The battle rifle pack is added to the map: the game may spawn and give it." };
static const char *const pack_help_smg[] = { "The SMG is not added to the map.",
	"The SMG pack is added to the map: the game may spawn and give it." };
static const char *const pack_help_smg_silenced[] = { "The silenced SMG is not added to the map.",
	"The silenced SMG pack is added to the map: the game may spawn and give it." };
static const char *const pack_help_carbine[] = { "The carbine is not added to the map.",
	"The carbine pack is added to the map: the game may spawn and give it." };
static const char *const pack_help_ar_silenced[] = { "The silenced assault rifle is not added to the map.",
	"The silenced assault rifle pack is added to the map: the game may spawn and give it." };
static const char *const packs_on_map_help[] = { "The map's weapons are the game's own.",
	"The map's pistols become the battle rifle and its assault rifles the SMG, where those packs are on." };
static const char *const frags_values[] = { "GAME'S OWN", "NONE", "1", "2", "3", "4" };
static const char *const frags_help[] = { "Players start with the game's own number of frag grenades.",
	"Players start with no grenades at all.", "Players start with one frag grenade.",
	"Players start with two frag grenades.", "Players start with three frag grenades.",
	"Players start with four frag grenades." };
static const char *const bottomless_help[] = { "A shot takes a round from the magazine, as in Halo CE.",
	"A shot takes nothing from the magazine and adds no heat; you still reload when you like." };
static const char *const grenades_help[] = { "Everyone has an unlimited supply of grenades.",
	"Each player carries up to four grenades of each type." };
static const char *const ammo_help[] = { "Weapons run out of ammo as in Halo CE.",
	"A reload takes nothing from your reserve, and energy weapons never run down." };

static const char *const vehicle_respawn_values[] = { "NEVER", "30 SECONDS", "60 SECONDS", "90 SECONDS", "2 MINUTES",
	"3 MINUTES", "5 MINUTES" };
static const char *const vehicle_respawn_help[] = { "How long an idle vehicle waits to respawn; NEVER: vehicles "
	"do not respawn." };
static const char *const vehicles_destructible_help[] = { "Explosives destroy vehicles, as in Halo 2; one cannot "
	"die while someone rides in it.", "As Halo CE: vehicles never die." };
static const char *const vehicle_set_values[] = { "DEFAULT", "NONE", "WARTHOGS", "GHOSTS", "SCORPIONS",
	"ROCKET WARTHOGS", "BANSHEES", "GUN TURRETS", "PC", "CUSTOM" };
static const char *const vehicle_set_help[] = { "The map's default vehicles.", "No vehicles; you travel on foot.",
	"Warthogs only.", "Ghosts only.", "Scorpion tanks only.", "Rocket warthogs only.", "Banshees only.",
	"Covenant gun turrets only.", "Every vehicle the map places, as Halo PC places them.",
	"Choose how many of each vehicle below." };
static const char *const count_values[] = { "NONE", "1", "2", "3", "4" };
static const char *const count_help[] = { "How many of this vehicle the map may place. Changing it makes the set "
	"CUSTOM." };

static const char *const death_bonus_help[] = { "Each time you respawn you run a little faster, which helps newer "
	"players keep up.", "You run at the same speed however often you die." };
static const char *const kill_penalty_help[] = { "Each time you kill another player you run a little slower, a "
	"handicap for sharp shooters.", "You run at the same speed however many players you kill." };
static const char *const kill_order_help[] = { "You only score by killing your designated target, which changes with "
	"each kill.", "Every kill counts toward your score." };
static const char *const assault_help[] = { "You must carry your own flag into the opposing base to score.",
	"You must capture the enemy flag and bring it back to your own base." };
static const char *const single_flag_values[] = { "OFF", "1 MINUTE", "2 MINUTES", "3 MINUTES", "5 MINUTES",
	"10 MINUTES" };
static const char *const single_flag_help[] = { "OFF: each team has its own flag. A time: one flag on the map, and "
	"the teams swap offense and defense on that clock." };
static const char *const must_reset_help[] = { "You cannot return a dropped flag by touching it; it returns itself "
	"after a while.", "A dropped flag returns to its base when its own team walks over it." };
static const char *const at_home_help[] = { "Your own flag must be at home for you to score.",
	"You can score even while the enemy holds your flag." };
static const char *const moving_hill_help[] = { "During the game the hill moves to a random place.",
	"The hill stays in the same place." };
static const char *const trait_values[] = { "NONE", "INVISIBLE", "EXTRA DAMAGE", "DAMAGE RESISTANT" };
static const char *const trait_with_help[] = { "No special ability while you carry the ball.",
	"You are invisible to the other players while you carry the ball.", "You do extra damage while you carry the ball.",
	"You take less damage while you carry the ball." };
static const char *const trait_without_help[] = { "No special ability while you do not carry the ball.",
	"Only the ball carrier is visible; everyone else is invisible.", "Players without the ball do extra damage.",
	"Players without the ball take less damage." };
static const char *const speed_values[] = { "NORMAL", "SLOW", "FAST" };
static const char *const speed_help[] = { "You run at the same speed with or without the ball.",
	"You run slower than the others while you carry the ball.", "You run faster than the others while you carry the ball." };
static const char *const ball_type_values[] = { "NORMAL", "REVERSE TAG", "JUGGERNAUT" };
static const char *const ball_type_help[] = { "Hold the ball as long as you can.",
	"The first player to score a kill becomes It; others become It by killing It. There is no ball.",
	"The first player to kill becomes the Juggernaut and scores for all kills. There is no ball." };
static const char *const random_start_help[] = { "The ball appears at random places on the map.",
	"The ball always appears at the same place." };
static const char *const ball_count_values[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13",
	"14", "15", "16" };
static const char *const ball_count_help[] = { "How many balls are on the map at once, from 1 to 16." };
static const char *const ball_melee_values[] = { "STOCK", "LETHAL" };
static const char *const ball_melee_help[] = { "The ball carrier's melee is as in Halo CE: it takes the shield and a "
	"little health, and kills from behind.", "The ball carrier's melee kills in one hit, shields and all, as in "
	"Halo 3." };
static const char *const race_type_values[] = { "NORMAL", "ANY ORDER", "RALLY" };
static const char *const race_type_help[] = { "You reach all the flags in order to complete a lap.",
	"You touch all the flags in any order to complete a lap.",
	"Everyone races to the same point; the first to reach it scores a lap." };
static const char *const team_scoring_values[] = { "MINIMUM", "MAXIMUM", "SUM" };
static const char *const team_scoring_help[] = { "A team is ranked by its lowest-scoring member.",
	"A team's score is its highest-scoring member's.", "A team's score is the sum of its members' scores." };
static const char *const objectives_values[] = { "MOTION SENSOR", "NAV POINTS", "NONE", "LINE OF SIGHT" };
static const char *const objectives_help[] = { "The objectives (hills, flags, balls) show on your motion sensor.",
	"The objectives have markers on screen.", "The objectives show neither on the motion sensor nor on screen.",
	"The objectives have markers on screen only while you can see them." };

static const char *const health_values[] = { "HALO CE", "REACH", "HALO 2", "HALO 3" };
static const char *const health_help[] = { "Your health only comes back with a health pack.",
	"As in Halo: Reach: health comes back 4 s after you were last hurt, up to the top of its third; shields refill "
	"in 3 s. Packs fill the rest.", "As in Halo 2: your health comes back 10 s after you were last hurt, with or "
	"without shields.", "As in Halo 3 (the same as HALO 2): your health comes back 10 s after you were last hurt." };
static const char *const fall_help[] = { "A hard landing hurts, and a long enough fall kills.",
	"Landings never hurt, from any height. Pits and leaving the map still kill." };
static const char *const spread_values[] = { "OFF", "FIRST SHOT", "FULL" };
static const char *const spread_help[] = { "The pistol and sniper rifle spread as in stock Halo.",
	"The pistol's first shot from rest, and every unzoomed sniper rifle shot, go exactly where you aim.",
	"Every pistol and sniper rifle shot goes exactly where you aim, held fire too: no spread and no bloom." };
static const char *const feedback_values[] = { "HALO CE", "ARENA EVOLVED" };
static const char *const feedback_help[] = { "Shields and hits look as in Halo CE.",
	"Shields show their state: a pop when one headshot will finish, a flicker while down, a sweep as they recharge; "
	"a blood pop on the kill. No damage change." };
static const char *const drop_values[] = { "HALO CE", "ALWAYS", "EXCEPT POWER" };
static const char *const drop_help[] = { "As Halo CE: your spawn's second weapon vanishes when you die.",
	"Both your weapons drop when you die, and stay 30 seconds.",
	"Both drop and stay 30 seconds, but a holstered rocket launcher, sniper rifle or shotgun vanishes." };
static const char *const practice_help[] = { "Weapons and powerups respawn as the map sets.",
	"Every weapon and powerup respawns every 30 seconds, for practising grenade tricks." };
static const char *const timers_values[] = { "OFF", "HUD", "HUD + WAYPOINTS", "LINE OF SIGHT" };
static const char *const timers_help[] = { "No timers: learn the spawns the hard way.",
	"When the power items (rockets, sniper, shotgun, overshield, camo) spawn, in a list for everyone.",
	"The power items' list, and waypoints over their spots from 10 seconds before they spawn, seen through walls.",
	"Waypoints over the power items' spots only while you can see them, from 10 seconds before they spawn." };
static const char *const training_help[] = { "No training aids.",
	"Waypoints over the power items as they come up, and the player spawns. For learning a map." };
static const char *const spawn_heat_help[] = { "Spawn markers show where players are likely to spawn next (with "
	"TRAINING).", "Plain spawn markers for everyone." };

static const char *const preset_values[] = { "CUSTOM", "SPLIT SCREEN" };
static const char *const preset_help[] = { "SILENT MOVEMENT and SILENT WEAPONS as set below.",
	"For players sharing speakers: SILENT MOVEMENT and SILENT WEAPONS both SILENT." };
static const char *const silent_values[] = { "NORMAL", "JUST ME", "SILENT" };
static const char *const movement_help[] = { "Footsteps, jumps and landings sound as in Halo CE.",
	"You hear your own footsteps, jumps and landings; everyone else moves silently. Only Arena Evolved players can join.",
	"Nobody's footsteps, jumps or landings make a sound, yours included. Only Arena Evolved players can join." };
static const char *const weapons_help[] = { "Readying and putting away weapons sound as in Halo CE.",
	"You hear your own weapon swaps; everyone else's are silent. Only Arena Evolved players can join.",
	"Nobody's weapon swaps make a sound, yours included. Only Arena Evolved players can join." };

static const char *const nhe_values[] = { "BY VEHICLE SET", "VANILLA", "TIMER ONLY", "NHE & TIMER", "NHE & POWERUPS",
	"TRAINING" };
static const char *const nhe_help[] = { "NHE's maps only: the vehicle set picks NHE's mode, as NHE did.",
	"NHE's maps: no timers, as retail with NHE's messages.", "NHE's maps: NHE's talking and on-screen timers.",
	"NHE's maps: NHE's talking and on-screen timers (its host teleport stays off).",
	"NHE's maps: NHE's timers and its powerup calls.",
	"NHE's maps: NHE's training: the countdown, spawn markers, randoms and waypoints." };
static const char *const nhe_extras_help[] = { "Stock teams and camera.", "As Halo 1: NHE: everyone joins red and keeps "
	"their colour between games, the dead camera stays on you, and the match clock counts up." };

/* ---------- the rows */

#define VALUES(list) list, COUNT(list)
#define ROW(label, group, engines, team_only, ae, values, helps) \
	{ label, group, engines, team_only, ae, VALUES(values), VALUES(helps) }
#define VEHICLE_COUNT(label, team_only) ROW(label, AE_SG_VEHICLES, 0, team_only, 0, count_values, count_help)
/* (a row of the WEAPON PACKS screen, or of both screens) */
#define PAGE_ROW(label, page, values, helps) \
	{ label, AE_SG_EQUIPMENT, 0, 0, 1, VALUES(values), VALUES(helps), page }

static struct ae_setup_row const rows[AE_SETUP_OPTIONS] =
{
	[AE_SO_KILLS_TO_WIN] = ROW("KILLS TO WIN", AE_SG_MATCH, ENGINE(AE_SETUP_ENGINE_SLAYER), 0, 0, kills_values, kills_help),
	[AE_SO_CAPTURES_TO_WIN] = ROW("CAPTURES TO WIN", AE_SG_MATCH, ENGINE(AE_SETUP_ENGINE_CTF), 0, 0, captures_values,
		captures_help),
	[AE_SO_HILL_SCORE] = ROW("SCORE TO WIN", AE_SG_MATCH, ENGINE(AE_SETUP_ENGINE_KING), 0, 0, minutes_score_values,
		hill_help),
	[AE_SO_BALL_SCORE] = ROW("SCORE TO WIN", AE_SG_MATCH, ENGINE(AE_SETUP_ENGINE_ODDBALL), 0, 0, minutes_score_values,
		ball_score_help),
	[AE_SO_LAPS_TO_WIN] = ROW("LAPS TO WIN", AE_SG_MATCH, ENGINE(AE_SETUP_ENGINE_RACE), 0, 0, laps_values, laps_help),
	[AE_SO_TIME_LIMIT] = ROW("TIME LIMIT", AE_SG_MATCH, 0, 0, 0, time_values, time_help),
	[AE_SO_COUNTDOWN] = ROW("PRE-GAME COUNTDOWN", AE_SG_MATCH, 0, 0, 1, off_on, countdown_help),

	[AE_SO_NAME] = ROW("NAME", AE_SG_MATCH, 0, 0, 0, name_values, name_help),

	[AE_SO_LIVES] = ROW("NUMBER OF LIVES", AE_SG_PLAYERS, 0, 0, 0, lives_values, lives_help),
	[AE_SO_RESPAWN_TIME] = ROW("RESPAWN TIME", AE_SG_PLAYERS, 0, 0, 0, respawn_values, respawn_help),
	[AE_SO_RESPAWN_GROWTH] = ROW("RESPAWN TIME GROWTH", AE_SG_PLAYERS, 0, 0, 0, growth_values, growth_help),
	[AE_SO_SUICIDE_PENALTY] = ROW("SUICIDE PENALTY", AE_SG_PLAYERS, 0, 0, 0, growth_values, suicide_help),
	[AE_SO_SHIELDS] = ROW("SHIELD TYPE", AE_SG_PLAYERS, 0, 0, 0, shields_values, shields_help),
	[AE_SO_MAXIMUM_HEALTH] = ROW("MAXIMUM HEALTH", AE_SG_PLAYERS, 0, 0, 0, health_amount_values, health_amount_help),
	[AE_SO_ACTIVE_CAMO] = ROW("ACTIVE CAMO", AE_SG_PLAYERS, 0, 0, 0, on_off, camo_help),
	[AE_SO_MOTION_SENSOR] = ROW("MOTION SENSOR", AE_SG_PLAYERS, 0, 0, 0, sensor_values, sensor_help),
	[AE_SO_SENSOR_RANGE] = ROW("SENSOR RANGE", AE_SG_PLAYERS, 0, 0, 1, range_values, range_help),
	[AE_SO_ODD_MAN_OUT] = ROW("ODD MAN OUT", AE_SG_PLAYERS, 0, 0, 0, yes_no, odd_man_help),
	[AE_SO_CONTROLLER_CROUCH] = ROW("CONTROLLER CROUCH", AE_SG_PLAYERS, 0, 0, 1, crouch_values, crouch_help),
	[AE_SO_MELEE_UNZOOMS] = ROW("MELEE UNZOOMS", AE_SG_PLAYERS, 0, 0, 1, on_off, melee_unzoom_help),
	[AE_SO_MOUSE_AIM_ASSIST] = ROW("MOUSE AIM ASSIST", AE_SG_PLAYERS, 0, 0, 1, mouse_aim_values, mouse_aim_help),

	[AE_SO_TEAM_PLAY] = ROW("TEAM PLAY", AE_SG_TEAMS, TEAM_ENGINES, 0, 0, yes_no, team_play_help),
	[AE_SO_FRIENDLY_FIRE] = ROW("FRIENDLY FIRE", AE_SG_TEAMS, 0, 1, 0, friendly_fire_values, friendly_fire_help),
	[AE_SO_BETRAYAL_PENALTY] = ROW("BETRAYAL PENALTY", AE_SG_TEAMS, 0, 1, 0, growth_values, betrayal_help),
	[AE_SO_FORCE_EVEN_TEAMS] = ROW("FORCE EVEN TEAMS", AE_SG_TEAMS, 0, 1, 0, off_on, even_teams_help),
	[AE_SO_FRIENDS_ON_SCREEN] = ROW("FRIENDS ON SCREEN", AE_SG_TEAMS, 0, 1, 0, on_off, friends_help),

	[AE_SO_WEAPON_SET] = ROW("WEAPON SET", AE_SG_EQUIPMENT, 0, 0, 0, weapon_set_values, weapon_set_help),
	[AE_SO_MAP_WEAPONS] = ROW("MAP WEAPONS", AE_SG_EQUIPMENT, 0, 0, 0, yes_no, map_weapons_help),
	[AE_SO_GRENADES_ON_MAP] = ROW("GRENADES ON MAP", AE_SG_EQUIPMENT, 0, 0, 1, yes_no, map_grenades_help),
	[AE_SO_WEAPON_PACKS] = ROW("WEAPON PACKS", AE_SG_EQUIPMENT, 0, 0, 1, weapon_packs_values, weapon_packs_help),
	[AE_SO_PACK_BATTLE_RIFLE] = PAGE_ROW("BATTLE RIFLE PACK", AE_SETUP_PAGE_PACKS, off_on, pack_help_br),
	[AE_SO_PACK_SMG] = PAGE_ROW("SMG PACK", AE_SETUP_PAGE_PACKS, off_on, pack_help_smg),
	[AE_SO_PACK_SMG_SILENCED] = PAGE_ROW("SILENCED SMG PACK", AE_SETUP_PAGE_PACKS, off_on,
		pack_help_smg_silenced),
	[AE_SO_PACK_CARBINE] = PAGE_ROW("CARBINE PACK", AE_SETUP_PAGE_PACKS, off_on, pack_help_carbine),
	[AE_SO_PACK_AR_SILENCED] = PAGE_ROW("SILENCED AR PACK", AE_SETUP_PAGE_PACKS, off_on,
		pack_help_ar_silenced),
	[AE_SO_PACKS_ON_MAP] = PAGE_ROW("PACK WEAPONS ON MAP", AE_SETUP_PAGE_PACKS, off_on, packs_on_map_help),
	[AE_SO_STARTING_EQUIPMENT] = ROW("STARTING EQUIPMENT", AE_SG_EQUIPMENT, 0, 0, 0, equipment_values, equipment_help),
	[AE_SO_STARTING_GRENADES] = ROW("STARTING GRENADES", AE_SG_EQUIPMENT, 0, 0, 1, frags_values, frags_help),
	[AE_SO_LOADOUT] = ROW("LOADOUT", AE_SG_EQUIPMENT, 0, 0, 0, loadout_values, loadout_help),
	[AE_SO_PRIMARY] = { "PRIMARY WEAPON", AE_SG_EQUIPMENT, 0, 0, 0, VALUES(loadout_weapon_values), VALUES(primary_help),
		AE_SETUP_PAGE_BOTH },
	[AE_SO_SECONDARY] = { "SECONDARY WEAPON", AE_SG_EQUIPMENT, 0, 0, 0, VALUES(loadout_weapon_values),
		VALUES(secondary_help), AE_SETUP_PAGE_BOTH },
	[AE_SO_INFINITE_GRENADES] = ROW("INFINITE GRENADES", AE_SG_EQUIPMENT, 0, 0, 0, on_off, grenades_help),
	[AE_SO_INFINITE_AMMO] = ROW("INFINITE AMMO", AE_SG_EQUIPMENT, 0, 0, 1, off_on, ammo_help),
	[AE_SO_BOTTOMLESS_CLIP] = ROW("BOTTOMLESS CLIP", AE_SG_EQUIPMENT, 0, 0, 1, off_on, bottomless_help),

	[AE_SO_VEHICLE_RESPAWN] = ROW("VEHICLE RESPAWN TIME", AE_SG_VEHICLES, 0, 0, 0, vehicle_respawn_values,
		vehicle_respawn_help),
	[AE_SO_VEHICLES_DESTRUCTIBLE] = ROW("VEHICLES DESTRUCTIBLE", AE_SG_VEHICLES, 0, 0, 1, on_off,
		vehicles_destructible_help),
	[AE_SO_VEHICLE_SET_RED] = ROW("RED VEHICLES", AE_SG_VEHICLES, 0, 0, 0, vehicle_set_values, vehicle_set_help),
	[AE_SO_VEHICLE_COUNT_RED + 0] = VEHICLE_COUNT("WARTHOGS", 0),
	[AE_SO_VEHICLE_COUNT_RED + 1] = VEHICLE_COUNT("GHOSTS", 0),
	[AE_SO_VEHICLE_COUNT_RED + 2] = VEHICLE_COUNT("SCORPIONS", 0),
	[AE_SO_VEHICLE_COUNT_RED + 3] = VEHICLE_COUNT("ROCKET WARTHOGS", 0),
	[AE_SO_VEHICLE_COUNT_RED + 4] = VEHICLE_COUNT("BANSHEES", 0),
	[AE_SO_VEHICLE_COUNT_RED + 5] = VEHICLE_COUNT("GUN TURRETS", 0),
	[AE_SO_VEHICLE_SET_BLUE] = ROW("BLUE VEHICLES", AE_SG_VEHICLES, 0, 1, 0, vehicle_set_values, vehicle_set_help),
	[AE_SO_VEHICLE_COUNT_BLUE + 0] = VEHICLE_COUNT("WARTHOGS", 1),
	[AE_SO_VEHICLE_COUNT_BLUE + 1] = VEHICLE_COUNT("GHOSTS", 1),
	[AE_SO_VEHICLE_COUNT_BLUE + 2] = VEHICLE_COUNT("SCORPIONS", 1),
	[AE_SO_VEHICLE_COUNT_BLUE + 3] = VEHICLE_COUNT("ROCKET WARTHOGS", 1),
	[AE_SO_VEHICLE_COUNT_BLUE + 4] = VEHICLE_COUNT("BANSHEES", 1),
	[AE_SO_VEHICLE_COUNT_BLUE + 5] = VEHICLE_COUNT("GUN TURRETS", 1),

	[AE_SO_DEATH_BONUS] = ROW("DEATH BONUS", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_SLAYER), 0, 0, yes_no, death_bonus_help),
	[AE_SO_KILL_PENALTY] = ROW("KILL PENALTY", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_SLAYER), 0, 0, yes_no,
		kill_penalty_help),
	[AE_SO_KILL_IN_ORDER] = ROW("KILL IN ORDER", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_SLAYER), 0, 0, yes_no,
		kill_order_help),
	[AE_SO_ASSAULT] = ROW("ASSAULT", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_CTF), 0, 0, yes_no, assault_help),
	[AE_SO_SINGLE_FLAG] = ROW("SINGLE FLAG", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_CTF), 0, 0, single_flag_values,
		single_flag_help),
	[AE_SO_FLAG_MUST_RESET] = ROW("FLAG MUST RESET", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_CTF), 0, 0, yes_no,
		must_reset_help),
	[AE_SO_FLAG_AT_HOME] = ROW("FLAG AT HOME TO SCORE", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_CTF), 0, 0, yes_no,
		at_home_help),
	[AE_SO_MOVING_HILL] = ROW("MOVING HILL", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_KING), 0, 0, yes_no, moving_hill_help),
	[AE_SO_TRAIT_WITH_BALL] = ROW("TRAIT WITH BALL", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_ODDBALL), 0, 0, trait_values,
		trait_with_help),
	[AE_SO_TRAIT_WITHOUT_BALL] = ROW("TRAIT WITHOUT BALL", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_ODDBALL), 0, 0,
		trait_values, trait_without_help),
	[AE_SO_SPEED_WITH_BALL] = ROW("SPEED WITH BALL", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_ODDBALL), 0, 0, speed_values,
		speed_help),
	[AE_SO_BALL_TYPE] = ROW("BALL TYPE", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_ODDBALL), 0, 0, ball_type_values,
		ball_type_help),
	[AE_SO_RANDOM_START] = ROW("RANDOM START", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_ODDBALL), 0, 0, yes_no,
		random_start_help),
	[AE_SO_BALL_COUNT] = ROW("BALLS", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_ODDBALL), 0, 0, ball_count_values,
		ball_count_help),
	[AE_SO_BALL_MELEE] = ROW("BALL MELEE", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_ODDBALL), 0, 1, ball_melee_values,
		ball_melee_help),
	[AE_SO_RACE_TYPE] = ROW("RACE TYPE", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_RACE), 0, 0, race_type_values,
		race_type_help),
	[AE_SO_TEAM_SCORING] = ROW("TEAM SCORING", AE_SG_MODE, ENGINE(AE_SETUP_ENGINE_RACE), 1, 0, team_scoring_values,
		team_scoring_help),
	[AE_SO_OBJECTIVES] = ROW("OBJECTIVES", AE_SG_MODE, ALL_BUT_SLAYER, 0, 0, objectives_values, objectives_help),

	[AE_SO_HEALTH] = ROW("HEALTH", AE_SG_ARENA, 0, 0, 1, health_values, health_help),
	[AE_SO_FALL_DAMAGE] = ROW("FALL DAMAGE", AE_SG_ARENA, 0, 0, 1, on_off, fall_help),
	[AE_SO_NO_SPREAD] = ROW("NO SPREAD", AE_SG_ARENA, 0, 0, 1, spread_values, spread_help),
	[AE_SO_COMBAT_FEEDBACK] = ROW("COMBAT FEEDBACK", AE_SG_ARENA, 0, 0, 1, feedback_values, feedback_help),
	[AE_SO_DROP_SECONDARY] = ROW("DROP SECONDARY", AE_SG_ARENA, 0, 0, 1, drop_values, drop_help),
	[AE_SO_PRACTICE] = ROW("PRACTICE MODE", AE_SG_ARENA, 0, 0, 1, off_on, practice_help),
	[AE_SO_TIMERS] = ROW("TIMERS", AE_SG_ARENA, 0, 0, 1, timers_values, timers_help),
	[AE_SO_TRAINING] = ROW("TRAINING", AE_SG_ARENA, 0, 0, 1, off_on, training_help),
	[AE_SO_SPAWN_HEAT] = ROW("SPAWN HEAT", AE_SG_ARENA, 0, 0, 1, on_off, spawn_heat_help),

	[AE_SO_SOUND_PRESET] = ROW("SOUND PRESET", AE_SG_SOUND, 0, 0, 1, preset_values, preset_help),
	[AE_SO_SILENT_MOVEMENT] = ROW("SILENT MOVEMENT", AE_SG_SOUND, 0, 0, 1, silent_values, movement_help),
	[AE_SO_SILENT_WEAPONS] = ROW("SILENT WEAPONS", AE_SG_SOUND, 0, 0, 1, silent_values, weapons_help),

	[AE_SO_NHE_MODE] = ROW("NHE MODE", AE_SG_NHE, 0, 0, 1, nhe_values, nhe_help),
	[AE_SO_NHE_EXTRAS] = ROW("NHE EXTRAS", AE_SG_NHE, 0, 0, 1, off_on, nhe_extras_help),
};

static const char *const group_names[AE_SETUP_GROUPS] = { "MATCH", "PLAYERS", "TEAMS", "EQUIPMENT", "VEHICLES", "MODE",
	"ARENA", "SOUND", "NHE" };

/* ---------- texts */

struct ae_setup_row const *ae_setup_row(int option)
{
	return option >= 0 && option < AE_SETUP_OPTIONS && rows[option].label ? &rows[option] : NULL;
}

static char name_text[16];

void ae_setup_name_set(const char *name)
{
	size_t length = name ? strlen(name) : 0;

	if (length > sizeof(name_text) - 1)
		length = sizeof(name_text) - 1;
	if (length)
		memcpy(name_text, name, length);
	name_text[length] = 0;
}

const char *ae_setup_value_text(int option, int value_index)
{
	struct ae_setup_row const *row = ae_setup_row(option);

	if (option == AE_SO_NAME && value_index == 0)
		return name_text;

	return row && value_index >= 0 && value_index < row->value_count ? row->values[value_index] : "";
}

const char *ae_setup_help(int option, int value_index)
{
	struct ae_setup_row const *row = ae_setup_row(option);

	if (!row)
		return "";
	if (row->help_count == 1)
		return row->helps[0];
	return value_index >= 0 && value_index < row->help_count ? row->helps[value_index] : "";
}

const char *ae_setup_label(struct ae_setup_access const *access, int option)
{
	struct ae_setup_row const *row = ae_setup_row(option);

	if (!row)
		return "";
	if (option == AE_SO_VEHICLE_SET_RED && access && !ae_setup_team(access))
		return "VEHICLE SET";
	return row->label;
}

const char *ae_setup_group_name(struct ae_setup_access const *access, int group)
{
	if (group < 0 || group >= AE_SETUP_GROUPS)
		return "";
	if (group == AE_SG_MODE && access)
	{
		switch (access->engine)
		{
		case AE_SETUP_ENGINE_CTF: return "CAPTURE THE FLAG";
		case AE_SETUP_ENGINE_SLAYER: return "SLAYER";
		case AE_SETUP_ENGINE_ODDBALL: return "ODDBALL";
		case AE_SETUP_ENGINE_KING: return "KING OF THE HILL";
		case AE_SETUP_ENGINE_RACE: return "RACE";
		}
	}
	return group_names[group];
}

/* ---------- what shows: derived from the access every time, never kept (as M4's S1-23) */

static int value_is(struct ae_setup_access const *access, int option, int value_index)
{
	return access->get(access->context, option) == value_index;
}

int ae_setup_team(struct ae_setup_access const *access)
{
	if (!access)
		return 0;
	if (access->engine == AE_SETUP_ENGINE_CTF)
		return 1;
	return value_is(access, AE_SO_TEAM_PLAY, AE_SETUP_TEAM_PLAY_YES);
}

int ae_setup_applies(struct ae_setup_access const *access, int option)
{
	struct ae_setup_row const *row = ae_setup_row(option);

	if (!row || !access || access->get(access->context, option) < 0)
		return 0;
	return !row->engines || (access->engine > 0 && access->engine < 16 && (row->engines & ENGINE(access->engine)));
}

static int vehicle_count_side(int option)
{
	if (option >= AE_SO_VEHICLE_COUNT_RED && option < AE_SO_VEHICLE_COUNT_RED + AE_SETUP_VEHICLE_KINDS)
		return 0;
	if (option >= AE_SO_VEHICLE_COUNT_BLUE && option < AE_SO_VEHICLE_COUNT_BLUE + AE_SETUP_VEHICLE_KINDS)
		return 1;
	return -1;
}

int ae_setup_visible(struct ae_setup_access const *access, int option)
{
	struct ae_setup_row const *row = ae_setup_row(option);
	int side = vehicle_count_side(option);

	if (!ae_setup_applies(access, option))
		return 0;
	if (row->team_only && !ae_setup_team(access))
		return 0;
	if (side >= 0)
		return value_is(access, side ? AE_SO_VEHICLE_SET_BLUE : AE_SO_VEHICLE_SET_RED, AE_SETUP_VEHICLE_SET_CUSTOM);
	if (option == AE_SO_WEAPON_SET)
		return !value_is(access, AE_SO_LOADOUT, AE_SETUP_LOADOUT_CUSTOM) || access->ae_rules;
	if (option == AE_SO_PRIMARY || option == AE_SO_SECONDARY)
		return value_is(access, AE_SO_LOADOUT, AE_SETUP_LOADOUT_CUSTOM);
	/* (WEAPON PACKS: while the gametype has a pack row to open it on) */
	if (option == AE_SO_WEAPON_PACKS)
	{
		int slot;

		for (slot = 1; slot <= AE_SETUP_PACKS; slot++)
		{
			if (ae_setup_applies(access, AE_SO_PACK_FIRST + slot - 1))
				return 1;
		}
		return ae_setup_applies(access, AE_SO_PACKS_ON_MAP);
	}
	return 1;
}

/* ---------- the list */

short ae_setup_entries(struct ae_setup_access const *access, short *entries, short maximum)
{
	short written = 0;
	int group, option;

	if (!access || !entries || maximum <= 0)
		return 0;
	for (group = 0; group < AE_SETUP_GROUPS; group++)
	{
		int headed = 0;

		for (option = 0; option < AE_SETUP_OPTIONS; option++)
		{
			if (rows[option].group != group || rows[option].page == AE_SETUP_PAGE_PACKS ||
				!ae_setup_visible(access, option))
				continue;
			if (!headed)
			{
				/* (a header with no room for a row under it is left out) */
				if (written + 2 > maximum)
					return written;
				entries[written++] = (short)(-1 - group);
				headed = 1;
			}
			else if (written + 1 > maximum)
				return written;
			entries[written++] = (short)option;
		}
	}
	return written;
}

void ae_setup_groups(struct ae_setup_access const *access, short const *entries, short count,
	struct ae_setup_group_info *groups)
{
	short index;
	int group, option;

	for (group = 0; group < AE_SETUP_GROUPS; group++)
	{
		groups[group].header = -1;
		groups[group].rows = 0;
		groups[group].changed = 0;
	}
	for (index = 0; index < count; index++)
	{
		if (ae_setup_entry_is_header(entries[index]))
			groups[ae_setup_entry_group(entries[index])].header = index;
		else
			groups[rows[entries[index]].group].rows++;
	}
	if (!access)
		return;
	for (option = 0; option < AE_SETUP_OPTIONS; option++)
	{
		if (ae_setup_applies(access, option) && access->changed(access->context, option))
			groups[rows[option].group].changed++;
	}
}

short ae_setup_group_at(short const *entries, short count, short index)
{
	if (index < 0 || index >= count)
		return -1;
	for (; index >= 0; index--)
	{
		if (ae_setup_entry_is_header(entries[index]))
			return (short)ae_setup_entry_group(entries[index]);
	}
	return -1;
}

short ae_setup_group_first_row(short const *entries, short count, int group)
{
	short index;

	for (index = 0; index + 1 < count; index++)
	{
		if (entries[index] == -1 - group)
			return (short)(index + 1);
	}
	return -1;
}

short ae_setup_group_step(short const *entries, short count, short index, short direction)
{
	short headers[AE_SETUP_GROUPS], shown = 0, at = -1, k, current = ae_setup_group_at(entries, count, index);

	for (k = 0; k < count && shown < AE_SETUP_GROUPS; k++)
	{
		if (!ae_setup_entry_is_header(entries[k]))
			continue;
		if (ae_setup_entry_group(entries[k]) == current)
			at = shown;
		headers[shown++] = k;
	}
	if (shown < 2 || at < 0)
		return index;
	at = (short)((at + (direction > 0 ? 1 : shown - 1)) % shown);
	return (short)(headers[at] + 1);
}

short ae_setup_row_step(short const *entries, short count, short index, short direction)
{
	short next = index;

	do
		next = (short)(next + (direction < 0 ? -1 : 1));
	while (next >= 0 && next < count && ae_setup_entry_is_header(entries[next]));
	return next >= 0 && next < count ? next : index;
}

short ae_setup_changes(struct ae_setup_access const *access)
{
	short changes = 0;
	int option;

	if (!access)
		return 0;
	for (option = 0; option < AE_SETUP_OPTIONS; option++)
		changes = (short)(changes + (ae_setup_applies(access, option) && access->changed(access->context, option)));
	return changes;
}

/* ---------- the WEAPON PACKS screen */

static const char *const section_names[] = { "PACKS", "START WEAPONS" };

const char *ae_setup_section_name(struct ae_setup_access const *access, int section)
{
	if (section >= AE_SETUP_SECTION_PACKS && section < AE_SETUP_SECTIONS_END)
		return section_names[section - AE_SETUP_SECTION_PACKS];
	return ae_setup_group_name(access, section);
}

short ae_setup_packs_entries(struct ae_setup_access const *access, short *entries, short maximum)
{
	static const short list[] = { -1 - AE_SETUP_SECTION_PACKS, AE_SO_PACK_BATTLE_RIFLE, AE_SO_PACK_SMG,
		AE_SO_PACK_SMG_SILENCED, AE_SO_PACK_CARBINE, AE_SO_PACK_AR_SILENCED, AE_SO_PACKS_ON_MAP,
		-1 - AE_SETUP_SECTION_START, AE_SO_PRIMARY, AE_SO_SECONDARY };
	short written = 0, index, header = -1;

	if (!access || !entries || maximum <= 0)
		return 0;
	for (index = 0; index < COUNT(list); index++)
	{
		/* (a section's header is written with its first row, when both have room) */
		if (ae_setup_entry_is_header(list[index]))
		{
			header = list[index];
			continue;
		}
		if (!ae_setup_applies(access, list[index]))
			continue;
		if (header != -1)
		{
			if (written + 2 > maximum)
				break;
			entries[written++] = header;
			header = -1;
		}
		else if (written + 1 > maximum)
			break;
		entries[written++] = list[index];
	}
	return written;
}

int ae_setup_pack_on(struct ae_setup_access const *access, int slot)
{
	return access && slot >= 1 && slot <= AE_SETUP_PACKS && value_is(access, AE_SO_PACK_FIRST + slot - 1, 1);
}

int ae_setup_pack_source(struct ae_setup_access const *access, int slot)
{
	int below = 0, other;

	if (!ae_setup_pack_on(access, slot))
		return 0;
	for (other = 1; other < slot; other++)
		below += ae_setup_pack_on(access, other);
	return below < 2 ? below + 1 : 0;
}

/* the packs' names as the summary and the help say them */
static const char *const pack_names[AE_SETUP_PACKS] = { "BATTLE RIFLE", "SMG", "SILENCED SMG", "CARBINE",
	"SILENCED AR" };
static const char *const pack_lower[AE_SETUP_PACKS] = { "battle rifle", "SMG", "silenced SMG", "carbine",
	"silenced AR" };
static const char *const pack_facts[AE_SETUP_PACKS] = {
	"Battle rifle: three-round bursts, 3x zoom.",
	"SMG: fast and close range, 60 rounds a magazine.",
	"Silenced SMG: quiet, tight grouping; it loses damage past 9 units.",
	"Carbine: single shots, 3x zoom, 18 rounds a magazine.",
	"Silenced AR: a quiet, steady rifle for range." };
enum { SUMMARY_NAMES_MAXIMUM = 14 };

void ae_setup_packs_summary(struct ae_setup_access const *access, char *text, int size)
{
	int slot, on = 0;
	size_t length = 0;
	char names[96];

	if (!text || size <= 0)
		return;
	names[0] = 0;
	for (slot = 1; slot <= AE_SETUP_PACKS; slot++)
	{
		if (!ae_setup_pack_on(access, slot))
			continue;
		length += (size_t)snprintf(names + length, sizeof(names) - length, "%s%s", on ? ", " : "",
			pack_names[slot - 1]);
		if (length >= sizeof(names))
			length = sizeof(names) - 1;
		on++;
	}
	if (!on)
		snprintf(text, (size_t)size, "NONE");
	else if (strlen(names) > SUMMARY_NAMES_MAXIMUM)
		snprintf(text, (size_t)size, "%d ON", on);
	else
		snprintf(text, (size_t)size, "%s", names);
}

const char *ae_setup_pack_help(struct ae_setup_access const *access, int option, int installed, char *text, int size)
{
	int slot = option - AE_SO_PACK_FIRST + 1, other, first = 0, second = 0;

	if (!text || size <= 0 || slot < 1 || slot > AE_SETUP_PACKS)
		return NULL;
	if (!ae_setup_pack_on(access, slot) && !installed)
	{
		snprintf(text, (size_t)size, "Not installed on this PC. Copy its .aepack into the packs folder next to the "
			"game and restart.");
		return text;
	}
	if (!ae_setup_pack_on(access, slot))
	{
		snprintf(text, (size_t)size, "%s A or left / right turns the pack on.", pack_facts[slot - 1]);
		return text;
	}
	if (ae_setup_pack_source(access, slot))
	{
		snprintf(text, (size_t)size, "%s On the map; it can also be a start weapon.", pack_facts[slot - 1]);
		return text;
	}
	/* (on the map only: the two packs on before it hold the start weapons' two places) */
	for (other = 1; other < slot; other++)
	{
		if (!ae_setup_pack_on(access, other))
			continue;
		if (!first)
			first = other;
		else if (!second)
			second = other;
	}
	snprintf(text, (size_t)size, "The %s pack is on the map, but only the first two packs on can be start weapons "
		"(%s and %s). Turn one of those off to start with the %s.", pack_lower[slot - 1],
		first ? pack_lower[first - 1] : "?", second ? pack_lower[second - 1] : "?", pack_lower[slot - 1]);
	return text;
}

int ae_setup_value_offered(struct ae_setup_access const *access, int option, int value_index)
{
	struct ae_setup_row const *row = ae_setup_row(option);

	if (!row || value_index < 0 || value_index >= row->value_count)
		return 0;
	if ((option != AE_SO_PRIMARY && option != AE_SO_SECONDARY) || value_index < AE_SETUP_WEAPON_STOCK || !access)
		return 1;
	if (access->get(access->context, option) == value_index)
		return 1;
	return ae_setup_pack_source(access, value_index - AE_SETUP_WEAPON_PACK_FIRST + 1) != 0;
}

short ae_setup_value_step(struct ae_setup_access const *access, int option, short current, short direction)
{
	struct ae_setup_row const *row = ae_setup_row(option);
	short next = current;

	if (!row)
		return current;
	for (;;)
	{
		next = (short)(next + (direction < 0 ? -1 : 1));
		if (next < 0 || next >= row->value_count)
			return current;
		if (ae_setup_value_offered(access, option, next))
			return next;
	}
}

short ae_setup_offered_values(struct ae_setup_access const *access, int option, short *indexes, short maximum)
{
	struct ae_setup_row const *row = ae_setup_row(option);
	short count = 0, index;

	if (!row || !indexes)
		return 0;
	for (index = 0; index < row->value_count && count < maximum; index++)
	{
		if (ae_setup_value_offered(access, option, index))
			indexes[count++] = index;
	}
	return count;
}
