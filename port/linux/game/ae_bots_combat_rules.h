/*
AE_BOTS_COMBAT_RULES.H

The bots' fight by level (ae_bots_combat.c, notes/specs/2026-10-16-bots-infinite.md area 3) as pure rules: no
engine or config calls (port/linux/tests/ae_bots_combat_rules_test.c runs them). The "combat dance"
made fallible per level, as its designers described it (ideas only; every number is ours, tuned on the build box):

- MELEE: RECRUIT strikes only at point blank, now and then, after a wait in reach and with a long wait between;
  EASY waits longer than the rest; HEROIC and LEGENDARY strike when the target's back is turned or its shields are
  low (else only at point blank), and close in to finish a fight they have won.
- GRENADES: RECRUIT rarely, and only far off; EASY when about to die (else rarely); NORMAL now and then; HEROIC and
  LEGENDARY also on first contact and when about to die, and lead the target's path.
- CONFIDENCE 0..1 from its own and its target's shields and health, the distance against its weapon's, the rounds
  in its magazine and the height between them: NORMAL and up back off along a path when it is low (RECRUIT and
  EASY never retreat cleverly), after a moment's hesitation, until their shields are back.
- A random per-bot delay on each decision (reaction, looking about, retreating): no metronome rhythm.
- Strafing in segments (halts at NORMAL and up), jumps in a fight at NORMAL and up.

The levels follow these ratios: RECRUIT ~ Recruit, EASY between Recruit and Marine, NORMAL ~
Marine, HEROIC ~ ODST, LEGENDARY ~ Spartan.

Levels: AE_BOTS_LEVEL_RECRUIT (0) .. AE_BOTS_LEVEL_DEMON (5). DEMON (owner 2026-10-20) is the LEGENDARY of before:
it fights as LEGENDARY in every rule here but shared sightings (LEGENDARY 20 units, DEMON anywhere). Vitality: a fraction (1 full; shields above 1
are an overshield). Ticks: 30 a second.
*/

#ifndef __AE_BOTS_COMBAT_RULES_H
#define __AE_BOTS_COMBAT_RULES_H

enum
{
	AE_BOTS_LEVEL_RECRUIT = 0,
	AE_BOTS_LEVEL_EASY,
	AE_BOTS_LEVEL_NORMAL,
	AE_BOTS_LEVEL_HEROIC,
	AE_BOTS_LEVEL_LEGENDARY,
	AE_BOTS_LEVEL_DEMON,
	AE_BOTS_LEVELS
};

#define AE_BOTS_COMBAT_TICKS_PER_SECOND 30

/* a number 0 <= n < 1 from two others (the bot's player and a tick): the same for the same pair, spread evenly */
float ae_bots_combat_hash01(unsigned long a, unsigned long b);

/* the ticks from seeing a target to the first shot: the skill's own times 0.85 .. 1.35 (hash01 of the sighting),
so that a bot is now quicker, now slower, never as a metronome; never quicker than 0.85 of the skill's (RECRUIT:
1.0 .. 1.5, never quicker than its own) */
long ae_bots_combat_reaction_ticks(int level, long skill_ticks, float hash01);

/* the ticks added to the look's three (0, 1 or 2) */
long ae_bots_combat_look_jitter(float hash01);

/* how sure the bot is it wins this fight, 0..1: 0.5 when even; its own vitality against its target's (shields
weigh more: they come back), less when its magazine is nearly empty, less when the target is much further than
its weapon reaches, a little more from above */
float ae_bots_combat_confidence(float own_shield, float own_body, float target_shield, float target_body,
	float distance, float weapon_range, float loaded, float height_above);

/* MELEE: whether the bot strikes now: within reach (distance, world units), the wait since the last over
(cooldown_over), random 0..1 */
int ae_bots_combat_melee(int level, float distance, int cooldown_over, int back_turned, float target_shield,
	int charging, float random);
/* the ticks the bot waits within reach before it strikes (the melee delays: Recruit 1-3 s, Marine 0.1-0.5,
ODST and Spartan 0.1-0.3; owner 2026-10-20, "melee reaction needs to be tuned better between the levels", they were
alike from NORMAL up: RECRUIT 1-3 s, EASY 0.6-1.5, NORMAL 0.35-0.7, HEROIC 0.25-0.5, LEGENDARY 0.18-0.35, DEMON
0.1-0.3), random 0..1 */
long ae_bots_combat_melee_delay(int level, float random);
/* the ticks until it may strike again (RECRUIT 3-4.5 s, EASY 2-3, NORMAL 1.2-1.7, HEROIC 1-1.5, LEGENDARY 0.95-1.35,
DEMON 0.9-1.3), random 0..1 */
long ae_bots_combat_melee_cooldown(int level, float random);

/* INFECTION ZOMBIES (OpenCE's Infection, infection.c: a zombie holds the Oddball skull alone, its blow its one
attack; owner 10-24, "aggressive with the oddball"): whether a zombie strikes now: within reach (1.1 units, RECRUIT
0.9) every level strikes, RECRUIT now and then (40%: 2, asked again after a second); no shield or back-turned test,
as a zombie has nothing else to do. 0: not now (out of reach, or waiting); 1: strike; 2: it chose not to */
int ae_bots_combat_zombie_melee(int level, float distance, int cooldown_over, float random);
/* a zombie's wait within reach before it strikes, and until its next blow: half its level's melee delay, 0.6 of its
level's cooldown */
long ae_bots_combat_zombie_melee_delay(int level, float random);
long ae_bots_combat_zombie_melee_cooldown(int level, float random);

/* GRENADES: whether the bot throws now (asked each tick of a fight): its target's distance, whether it has a
grenade, has reacted, whether the target is new to it (first contact, the first 1.5 s), whether it is about to
die, the skill's chance a second (bots.c's table: grenade_chance), random 0..1 */
int ae_bots_combat_grenade(int level, float distance, int has_grenade, int reacted, int first_contact,
	int about_to_die, float skill_chance, float random);
/* the ticks until it may throw again, random 0..1 */
long ae_bots_combat_grenade_cooldown(int level, float random);
/* how much of the target's path a throw leads (0 none, 1 all) */
float ae_bots_combat_grenade_lead(int level);
/* about to die: no shields and under 0.6 of its health */
int ae_bots_combat_about_to_die(float own_shield, float own_body);

/* HEADSHOTS: whether the bot aims at the head: NORMAL and up, the target's shields down, a weapon that kills
with a shot to the head (a precision weapon) */
int ae_bots_combat_aim_head(int level, float target_shield, int precision_weapon);

/* RETREAT (NORMAL and up; disengage below 0.1): low confidence held for the bot's hesitation (6 to 18
ticks) starts it, with its shields under 0.3; it lasts at least 2 s (HEROIC and up 3 s: hysteresis),
ends when its shields are back (0.95) or its confidence is high again (0.6), after 7 s at most; then not again for
6 s. RETREAT ON THE SHIELD'S POP (LEGENDARY and up): its shields gone and its confidence under 0.35 start it too,
after a short hesitation (3 to 9 ticks), as a player turns away as the shield pops rather than at the last hit */
struct ae_bots_combat_retreat
{
	int active;
	long low_since;
	long started;
	long next_allowed;
};
/* a bot's state when it spawns: not retreating, no low confidence yet */
void ae_bots_combat_retreat_reset(struct ae_bots_combat_retreat *state);
/* the state's step at tick now: TRUE while retreating; *started / *ended set on the ticks it does */
int ae_bots_combat_retreat_step(struct ae_bots_combat_retreat *state, int level, float confidence, float own_shield,
	long now, float hesitation_hash01, int *started, int *ended);
/* the confidence under which a level retreats (0 for one that does not) */
float ae_bots_combat_retreat_threshold(int level);

/* CLOSING IN (HEROIC and up): whether the bot charges to finish its target with a blow: sure of the fight, the
target's shields down and under half its health, within 6 units */
int ae_bots_combat_charge(int level, float confidence, float target_shield, float target_body, float distance);

/* VERTICAL VIEW (owner 2026-10-20 on HEROIC: "some wall hack stuff on them still. like snapping to you when you are
above them while they are running by"; bots.c's sight had only a horizontal field of view): half the vertical view, in
degrees, around where the bot looks (its pitch): a target further above or below it is not seen (unless close, or it
shot the bot, or it is the one it fights). A player running by looks ahead, not up: RECRUIT 22, EASY 25, NORMAL 28,
HEROIC 32, LEGENDARY 36, DEMON 40 (CE's own view is about 27 each way at 16:9). */
float ae_bots_combat_vertical_half_view(int level);
/* whether a target elevation_degrees above (negative: below) the bot's line of sight at pitch_degrees is in view */
int ae_bots_combat_in_vertical_view(int level, float elevation_degrees, float pitch_degrees);

/* STEPPING IN FOR A BLOW (owner 2026-10-20: "maybe get them to throw some more melees in"; the soak: 1 to 16 blows a
5-minute 6-bot game at any level, as a bot backs off a near target to its weapon's distance and seldom comes within
reach): whether the bot steps towards a near target rather than backing off, its shields low: EASY within 2.5 units
(shields under 0.5), NORMAL within 3.5 (under 0.8), HEROIC and up within 4 (whatever its shields: a blow is a
player's answer close up); RECRUIT never */
int ae_bots_combat_step_in(int level, float distance, float target_shield);

/* JUMPS in a fight ("vertical movement", Marine up): the ticks to the next one, 0 for a level that does
not jump in a fight (RECRUIT, EASY): NORMAL every 3-5 s, HEROIC 1-4, LEGENDARY 1-3 */
long ae_bots_combat_vertical_delay(int level, float random);

/* STRAFING (in segments): the ticks a strafe to one side lasts: RECRUIT 1-4 s (the owner's), EASY 1.5-3,
NORMAL 2-4, HEROIC 1-2, LEGENDARY 0.2-0.8; random 0..1 */
long ae_bots_combat_strafe_ticks(int level, float random);
/* a strafe's HALT (it stands, still keeping its distance): the ticks of the halt at a new strafe, 0 for none:
NORMAL 0.2 of the time for 1-2 s, HEROIC 0.1 for 0.1-0.2, LEGENDARY 0.05 for 0.05-0.1; RECRUIT and EASY never
(a CE bot that stands aims far better: the kids' levels must not get harder) */
long ae_bots_combat_halt_ticks(int level, float chance_random, float length_random);
/* the halt that starts a fight: NORMAL and HEROIC up to 0.2 s, LEGENDARY and DEMON 0.1; RECRUIT and EASY none */
long ae_bots_combat_initial_halt_ticks(int level, float random);

/* SHARED SIGHTINGS (team games): how near a teammate a bot tells where it saw an enemy (0: it tells no one):
HEROIC 10 units, LEGENDARY 20, DEMON anywhere; RECRUIT .. NORMAL know only what they see */
float ae_bots_combat_share_distance(int level);

/* LONG RANGE (owner 2026-10-20 after LEGENDARY on Temple: "a bit too good with their distance shots"; the corpus: 52% of
its shots hit at 10-15 units, 25% past 15): the least aim error, in degrees, a bot keeps on a target this far away (world
units). None within 10 units; past them, up to 15, it grows by the level's degrees every 10 units: RECRUIT 3, EASY 2.6,
NORMAL 2, HEROIC 1.6, LEGENDARY 1.2, DEMON none (the LEGENDARY of before, kept as it was); past 15 the chance below
takes over. */
#define AE_BOTS_COMBAT_LONG_RANGE 15.0f
float ae_bots_combat_range_error_floor(int level, float distance);

/* FAR SHOTS (owner 2026-10-30, Blood Gulch: a floor that kept growing stopped the hits past 17 units and the firing past
22): past 15 units a bot keeps firing, on target this share of the time and otherwise just off the body, the chance by
15-20, 20-25, 25-30 and over 30 units. HEROIC .18 .12 .09 .05; NORMAL .12 .08 .05 .03; LEGENDARY .22 .16 .12 .07, under
the 25% past 15 units that was "a bit too good". -1: not far, or DEMON, which aims as it is */
float ae_bots_combat_long_range_hit_chance(int level, float distance);

/* SQUADS THAT SPLIT (owner 2026-10-30, big team games: "they all keep pushing when they engage some should break off the main group"):
a squad of AE_BOTS_COMBAT_WIDE_SQUAD or more spreads out. How many of a squad flank: about a third of a wide squad, at
most 6; a smaller squad LEGENDARY and up 2, HEROIC 1, as ever. How long a flank lasts: a wide squad's flanker gets the
walk at 2.2 units a second and 5 s more, 10 to 25 s; a smaller squad's 10 s. How long a wide squad's flanker may lose
sight of its enemy, going round: 8 s, a smaller squad's 2 s. */
#define AE_BOTS_COMBAT_WIDE_SQUAD 4
long ae_bots_combat_squad_flankers(int level, long squad_size);
float ae_bots_combat_flank_seconds(long squad_size, float walk_distance);
float ae_bots_combat_flank_unseen_seconds(long squad_size);

/* CLOSE RANGE (owner 2026-10-29, Hang 'Em High: NORMAL bots' battle rifle bullets hit 58% of the time within 10 units, 16%
at 10 to 20): the least aim error, in degrees, a NORMAL bot keeps on a target within 10 units (and past 2: a blow's
reach); none for the other levels and past 10 units, where the long-range floor above takes over. */
#define AE_BOTS_COMBAT_NEAR_FLOOR 1.3f
float ae_bots_combat_near_error_floor(int level, float distance);

/* WEAPON RANGE (the corpus: a shotgun fired from 5 to 10 units, where it does almost nothing): the farthest a bot fires
a weapon, by its tag's name: a shotgun 5 units, a flamethrower 4; any other (or none) without a limit. The bot still
closes in (bots.c's distance kept) */
float ae_bots_combat_max_fire_distance(char const *weapon_name);

/* CAMO (owner 2026-10-20: "bots notice you instantly when you are using camo"): for a target in active camouflage
(camouflaged nonzero), how far it is seen (a factor on bots.c's own 0.3 of the sight), and the first aim error and the
reaction (factors); 1 for a target in sight */
float ae_bots_combat_camo_reach_scale(int camouflaged);
float ae_bots_combat_camo_error_scale(int camouflaged);
float ae_bots_combat_camo_reaction_scale(int camouflaged);

/* SPENT WEAPONS (bots.c's bots_weapon_empty, which swaps a bot off an empty weapon): a weapon of rounds is spent
when none are left, bots.c's own test (-1: left to it); an energy weapon (the plasma pistol, the plasma rifle) when
its battery is out (age 1). An energy weapon is one with no magazine or a magazine of no rounds: the stock plasma
pistol has one, every maximum 0, so bots.c took it for always empty, and a bot whose other weapon ran dry kept that
one, never fired again and backed off to the empty weapon's distance (owner 2026-10-18: Johnson "ran away and
didn't shoot"). The magazine's counts (0 for none: magazine_count) and the weapon's age 0..1 */
int ae_bots_combat_weapon_spent(int magazine_count, int rounds_loaded_maximum, int rounds_total_maximum,
	float age);

/* PRO TRAITS (owner 2026-10-19: "implement the rest of the pro traits"; the gap table pro-skills-ideas):

PRECISION PACING (NORMAL and up): with a weapon that blooms (its error grows as it fires: the pistol) and a target
past 8 units, the bot waits for the error (the trigger's 0..1) to fall under its level's limit before the next shot,
as a player paces the pistol at range: NORMAL 0.6, HEROIC 0.45, LEGENDARY 0.35, DEMON 0.25; never longer than its
level's ticks past the moment it could fire (NORMAL 6, HEROIC 8, LEGENDARY 10, DEMON 12). Up close it fires at the
weapon's rate. Nonzero: it holds the shot now. */
int ae_bots_combat_pace_holds(int level, float distance, float trigger_error, long waited_ticks);

/* DESCOPE REFLEX (NORMAL and up): a weapon whose shots are true only zoomed (the sniper rifle: its trigger's "use
error when unzoomed") is zoomed at a target past 6 units. A hit knocks the zoom out (CE's unit_unzoom); the bot zooms
again after its level's moment (NORMAL 0.6 .. 0.8 s, HEROIC 0.4 .. 0.5, LEGENDARY 0.25 .. 0.35, DEMON 0.15 .. 0.22)
and, past 10 units, does not fire before. RECRUIT and EASY never zoom (as before). */
int ae_bots_combat_zooms(int level, float distance);
long ae_bots_combat_rescope_ticks(int level, float random);
int ae_bots_combat_descoped_holds_fire(float distance);

/* SCOPED RIFLE (NORMAL and up): a rifle that bots zoom at range though its shots are as true unzoomed (the packs'
battle rifle and carbine: ae_bots_combat_scoped_rifle) is zoomed at a target past half its aim assist's reach unzoomed, where that aim assist
starts to fade (aim_assist.c's attenuation: whole to half the reach, none at it); zoomed, the reach is the zoom's
times longer, as for a player who scopes it. The bot's aim error is about a degree at the octagon's 13 units, which the
carbine's 17 units of reach no longer bend onto the target (NORMAL hit 4% of its shots past 14 units; the battle
rifle's 20 units, 2 to 10%); a hit's
descope and the moment before it zooms again are the DESCOPE REFLEX's, but it fires unzoomed meanwhile. RECRUIT and
EASY never zoom. Nonzero: zoom now. */
int ae_bots_combat_rifle_zooms(int level, float distance, float autoaim_distance);

/* HIGH GROUND (LEGENDARY and up): a roaming place's credit for its height over the reference (the enemy's when it
hunts, its own else), in units of distance: LEGENDARY 1.5 a unit up to 4.5, DEMON 2 a unit up to 6; none below or
for the other levels */
float ae_bots_combat_high_ground(int level, float height_above);

/* GRENADE AT THE LAST-SEEN SPOT (LEGENDARY and up): a target lost from sight 0.6 to 3 s ago, its last-seen spot 5 to
20 units off and in the bot's line of sight: once a lost contact, LEGENDARY half the time, DEMON 0.75 (random 0..1) */
int ae_bots_combat_prenade(int level, float seconds_unseen, float distance, int has_grenade, float random);

/* FRIENDLY FIRE: a teammate between the muzzle and target, within a body-width cone about the shot, blocks it.
The grenade test keeps teammates outside the landing spot's blast-sized safety radius. */
int ae_bots_combat_teammate_blocks(float shooter_x, float shooter_y, float shooter_z,
	float teammate_x, float teammate_y, float teammate_z, float target_x, float target_y, float target_z);
int ae_bots_combat_grenade_near_teammate(float landing_x, float landing_y, float landing_z,
	float teammate_x, float teammate_y, float teammate_z);

/* CROUCH JUMP: a bot crouches at the top of a jump and holds it to the landing (a ledge, a jump spot, a dodge in a
fight): in the air, between 0.1 and 2 s after the press, no longer rising (vertical velocity, units a tick, at most
0.004). Nonzero: crouch now. Whether the level may is the caller's. */
int ae_bots_combat_crouch_jump(int airborne, long ticks_since_jump, float vertical_velocity);

/* THREATS (owner 2026-10-30: a bot on a warthog's gun fired at others, not at the enemy gunner): who comes first among the enemies a
bot can see. How many units nearer a target counts: an enemy on a mounted gun, the most for a bot on a gun itself; then the
one carrying the objective; then one aiming at this bot, a power weapon, a driver. None for RECRUIT, full from HEROIC. */
struct ae_bots_threat_view
{
	int level;
	int on_gun;
	int self_on_gun;
	int carries_objective;
	int aiming_at_me;
	int power_weapon;
	int driving;
};
float ae_bots_combat_threat_score(struct ae_bots_threat_view const *view);
/* an enemy on a gun within this is noticed out of view, NORMAL and up */
#define AE_BOTS_COMBAT_THREAT_NOTICE 40.0f

#endif
