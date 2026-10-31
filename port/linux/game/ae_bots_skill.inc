/*
AE_BOTS_SKILL.INC

Included in bots.c right after its skill table (one AE hook line), so that
Arena Evolved's RECRUIT level sits below EASY without a fifth row in the
table of OpenCE pull request #367: bots.skill = "recruit". For new players
and children (owner 2026-10-15: EASY "will clap my poor kids"): it reacts
late, sees less, starts far off its target and settles slowly, turns slowly,
fires short bursts with long pauses, throws no grenades, does not jump, takes
no lead, and roams the map more often than it heads for the nearest enemy.
Owner 2026-10-15 after play ("these bots will still destroy my kids"; "less
of strafe demons ... they can't track like me"; then "less strafing on recruit"): its aim never settles: it
wanders between 0.8 and 6 degrees off the target, so it fires and misses as
a new player does; and it sidesteps barely (0.15 as hard), three times as
long each way, so a young player's aim can stay on it. RECRUIT and EASY also aim where
their target was a moment ago (the TRACKING DELAY below).
Also here: SHOOTING SKILL and MOVEMENT SKILL (the skill each part of a bot plays at) and the declarations of
ae_bots_combat.inc's glue, which bots.c's fight and look call (the fight by level: ae_bots_combat.h).
*/

#define AE_BOTS_SKILL_RECRUIT NUMBER_OF_BOTS_SKILLS
/* DEMON (owner 2026-10-20, after the Temple 2v2 ladder: "those legendary bots are still demon level ... move the
legendary bots up to the demon tier and distribute the difficulty with the other levels"): the LEGENDARY row of
before, above a new LEGENDARY; bots.skill = "demon" */
#define AE_BOTS_SKILL_DEMON (NUMBER_OF_BOTS_SKILLS + 1)

#include "ae_bots_combat.h"

static struct bots_skill_definition const ae_bots_recruit =
	{ "recruit", 1.4f, 16.0f, 80.0f, 22.0f, 2.5f, 5.0f, 2.6f, 0.0f, 0.0f, 0.0f, 4, 24 };

/* The ladder (owner 2026-10-20, after ten games on Temple 2v2 with BR and pistol starts): NORMAL is OpenCE's row again
(a nerf to 0.55 s / 8.5 degrees made it "too easy"); HEROIC stays ("good", "a sweaty casual matchmaking game");
LEGENDARY is new, halfway from HEROIC to DEMON in every number; DEMON is OpenCE's LEGENDARY row ("the greasiest,
sweaty demons"). */
/* EASY and NORMAL closer (owner 2026-10-20 morning: "the difference between easy and normal is still quite large"; the
soak's bot-vs-bot kills a bot-minute RECRUIT 0.89, EASY 0.96, NORMAL 1.68; answer "both, a little each"): EASY a step
up from OpenCE's row (about a third of the way to NORMAL), NORMAL a step down (a third of the 0.55 s / 8.5 degree nerf
the owner found "too easy"). */
static struct bots_skill_definition const ae_bots_easy =
	{ "easy", 0.66f, 25.0f, 106.0f, 10.5f, 1.25f, 9.5f, 2.1f, 0.4f, 0.06f, 0.00f, 7, 13 };
static struct bots_skill_definition const ae_bots_normal =
	{ "normal", 0.49f, 31.0f, 118.0f, 7.5f, 0.95f, 12.5f, 1.85f, 0.65f, 0.09f, 0.05f, 9, 10 };
/* HEROIC's aim a third of the way to NORMAL's (owner 2026-10-26, SnD on Turf after the squads and wingman work: "we need
to tune down heroic accuracy a bit they are aimbots"): the first aim error 4.0 -> 5.0 degrees, settling in 0.72 s, not
0.6, firing within 1.6 times the target's size, not 1.5, and 0.75 of a moving target's lead; its reaction, sight,
turning, bursts and grenades are OpenCE's HEROIC row's as before. */
static struct bots_skill_definition const ae_bots_heroic =
	{ "heroic", 0.30f, 42.0f, 140.0f, 5.0f, 0.72f, 20.0f, 1.6f, 0.75f, 0.16f, 0.15f, 12, 6 };
static struct bots_skill_definition const ae_bots_legendary =
	{ "legendary", 0.24f, 48.0f, 150.0f, 3.1f, 0.48f, 25.0f, 1.4f, 0.9f, 0.19f, 0.20f, 14, 5 };
static struct bots_skill_definition const ae_bots_demon =
	{ "demon", 0.18f, 55.0f, 160.0f, 2.2f, 0.35f, 30.0f, 1.25f, 1.0f, 0.22f, 0.25f, 16, 4 };
/* the skill's definition: RECRUIT's, EASY's, NORMAL's, HEROIC's, LEGENDARY's or DEMON's (the table's for none of them) */
static struct bots_skill_definition const *ae_bots_skill_definition(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? &ae_bots_recruit : skill == AE_BOTS_SKILL_DEMON ? &ae_bots_demon :
		skill == _bots_skill_legendary ? &ae_bots_legendary : skill == _bots_skill_easy ? &ae_bots_easy :
		skill == _bots_skill_normal ? &ae_bots_normal : skill == _bots_skill_heroic ? &ae_bots_heroic : &bots_skills[skill];
}

/* the chance a roam goes towards an enemy (the place nearest a living one)
rather than anywhere: bots.c's 0.65, RECRUIT's 0.2 */
static real ae_bots_hunt_chance(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? 0.2f : 0.65f;
}

/* RECRUIT's aim: after bots.c's settling, a wander that never settles: a
random walk kept between 0.8 and 6 degrees off (owner 10-16: 1.5..8 was "patronizing") (pitch: six tenths of that);
r1, r2: two random numbers in -1..1 */
static void ae_bots_aim_floor(short skill, real *yaw_error, real *pitch_error, real r1, real r2)
{
	real const low = 0.8f * BOTS_DEGREES, high = 6.0f * BOTS_DEGREES, step = 0.6f * BOTS_DEGREES;
	real *errors[2];
	real scale[2] = { 1.0f, 0.6f };
	real random[2];
	short index;

	if (skill != AE_BOTS_SKILL_RECRUIT)
		return;
	errors[0] = yaw_error;
	errors[1] = pitch_error;
	random[0] = r1;
	random[1] = r2;
	for (index = 0; index < 2; index++)
	{
		real value = *errors[index] + random[index] * step * scale[index];
		real magnitude = (real)fabs(value);
		real sign = value < 0.0f ? -1.0f : 1.0f;

		if (magnitude < low * scale[index])
			value += sign * 0.5f * step * scale[index];
		if (magnitude > high * scale[index])
			value = sign * high * scale[index];
		*errors[index] = value;
	}
}

/* how hard it sidesteps while fighting (bots.c: full, either way): RECRUIT
0.15 (0.33 was still too hard to track, owner 10-16) */
static real ae_bots_strafe(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? 0.15f : 1.0f;
}

/* how long a sidestep lasts before it may turn, ticks (bots.c: a third of a
second to a second and a third): RECRUIT three times as long; the others
(2026-10-16, segments per level) ae_bots_combat_strafe_ticks's,
from bots.c's random length */
static int ae_bots_level_of_skill(short skill);
static long ae_bots_strafe_ticks(short skill, long ticks)
{
	real random = (real)(ticks - TICKS_PER_SECOND / 3) / (real)(TICKS_PER_SECOND - 1);

	if (skill == AE_BOTS_SKILL_RECRUIT)
		return ticks * 3;
	return ae_bots_combat_strafe_ticks(ae_bots_level_of_skill(skill), random);
}

/* TRACKING DELAY (2026-10-16: the over-correction window players win
strafe fights with): RECRUIT and EASY aim at where the target was a moment ago, not where it is, so a target that
strafes or turns back is missed for that moment, as by a person. Each target's centre is kept for the last
AE_BOTS_TRACK_TICKS ticks, by its player's absolute index. */
#define AE_BOTS_TRACK_TICKS 16
#define AE_BOTS_TRACK_PLAYERS 256

static struct { long tick; real_point3d centre; } ae_bots_track[AE_BOTS_TRACK_PLAYERS][AE_BOTS_TRACK_TICKS];

/* the skill's delay, ticks: RECRUIT 0.27 s (was 0.3), EASY 0.2 s, none above */
static long ae_bots_track_delay_ticks(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? 8 : skill == _bots_skill_easy ? 6 : 0;
}

/* aim_point (the target's centre now) becomes its centre the skill's delay ago, when that tick was kept */
static void ae_bots_aim_delay(short skill, long target_player_index, real_point3d *aim_point, long now)
{
	long delay = ae_bots_track_delay_ticks(skill);
	long player = target_player_index == NONE ? -1 : DATUM_INDEX_TO_ABSOLUTE_INDEX(target_player_index);
	long slot;
	static long last_now = -1;

	if (player < 0 || player >= AE_BOTS_TRACK_PLAYERS || now < 0)
		return;
	/* (a game's ticks start over: the last game's places are dropped, so that none is taken for a tick of this one) */
	if (now < last_now)
	{
		long index;

		for (index = 0; index < AE_BOTS_TRACK_PLAYERS * AE_BOTS_TRACK_TICKS; index++)
			ae_bots_track[index / AE_BOTS_TRACK_TICKS][index % AE_BOTS_TRACK_TICKS].tick = -1;
	}
	last_now = now;
	slot = now % AE_BOTS_TRACK_TICKS;
	ae_bots_track[player][slot].tick = now;
	ae_bots_track[player][slot].centre = *aim_point;
	if (delay <= 0 || delay >= AE_BOTS_TRACK_TICKS)
		return;
	slot = (now - delay) % AE_BOTS_TRACK_TICKS;
	if (now - delay >= 0 && ae_bots_track[player][slot].tick == now - delay)
		*aim_point = ae_bots_track[player][slot].centre;
}

/* MELEE (owner 2026-10-16, the lower levels rarely melee): bots.c strikes within 1.1 units once a second at
every skill; RECRUIT only at point blank (0.8), every 3 s or more and only now and then, EASY every 2 s or more:
ae_bots_combat_rules.c's ae_bots_combat_melee, which has every level's (ae_bots_combat.inc) */

/* CROUCHING IN A FIGHT (owner 2026-10-16: crouch while moving, but not as often as a player spamming it): NORMAL and
up now and then crouch while they strafe, for a short while, then not again for a while; RECRUIT and EASY never
(the lowest levels do not crouch). Kept by the bot's player index. */
static struct { long until, next; } ae_bots_crouching[AE_BOTS_TRACK_PLAYERS];

/* the chance a second to start a crouch while fighting */
static real ae_bots_crouch_chance(short skill)
{
	return skill == _bots_skill_normal ? 0.12f : skill == _bots_skill_heroic ? 0.20f :
		skill == _bots_skill_legendary || skill == AE_BOTS_SKILL_DEMON ? 0.28f : 0.0f;
}

/* whether the fighting bot crouches this tick: a crouch lasts 0.3 to 0.6 s, then none for at least 2.5 s; random
in 0..1 */
static boolean ae_bots_crouch(short skill, long player_index, long now, real random)
{
	long player = player_index == NONE ? -1 : DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	real chance = ae_bots_crouch_chance(skill);

	if (player < 0 || player >= AE_BOTS_TRACK_PLAYERS || chance <= 0.0f)
		return FALSE;
	/* (a game's ticks start over: an old crouch and an old wait are dropped; a crouch lasts at most 0.6 s and a
	wait 3.1 s, so anything further ahead than that is the last game's) */
	if (ae_bots_crouching[player].until > now + TICKS_PER_SECOND ||
		ae_bots_crouching[player].next > now + TICKS_PER_SECOND * 10)
	{
		ae_bots_crouching[player].until = 0;
		ae_bots_crouching[player].next = 0;
	}
	if (now < ae_bots_crouching[player].until)
		return TRUE;
	if (now < ae_bots_crouching[player].next || random >= chance / TICKS_PER_SECOND)
		return FALSE;
	/* (random is under chance / TICKS_PER_SECOND here: spread back over 0..1 for the length) */
	ae_bots_crouching[player].until = now + (long)(TICKS_PER_SECOND * (0.3f + 0.3f * random * TICKS_PER_SECOND / chance));
	ae_bots_crouching[player].next = ae_bots_crouching[player].until + TICKS_PER_SECOND * 5 / 2;
	return TRUE;
}

/* SHOOTING SKILL and MOVEMENT SKILL (queue 120; bots.shooting_skill, bots.movement_skill: ae_bots_combat.h): the
skill, bots.c's index (AE_BOTS_SKILL_RECRUIT, AE_BOTS_SKILL_DEMON or its table's), each part of a bot plays at; BOT SKILL
(the default) keeps bots.skill's */
static int ae_bots_level_of_skill(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? AE_BOTS_LEVEL_RECRUIT : skill == AE_BOTS_SKILL_DEMON ? AE_BOTS_LEVEL_DEMON :
		(int)skill + 1;
}

static short ae_bots_skill_of_level(int level)
{
	return level == AE_BOTS_LEVEL_RECRUIT ? AE_BOTS_SKILL_RECRUIT : level == AE_BOTS_LEVEL_DEMON ? AE_BOTS_SKILL_DEMON :
		(short)(level - 1);
}

/* the aim, the reaction, how far and wide it sees, grenades, blows, headshots */
static short ae_bots_shooting_skill(short skill)
{
	int level = ae_bots_combat_setting_level(FALSE);

	return level < 0 ? skill : ae_bots_skill_of_level(level);
}

/* strafing, jumping, crouching, closing in, retreating */
static short ae_bots_movement_skill(short skill)
{
	int level = ae_bots_combat_setting_level(TRUE);

	return level < 0 ? skill : ae_bots_skill_of_level(level);
}

/* the definition a bot's tick reads (bots_bot_update's tick.skill): the shooting skill's; its jump and grenade
chances 0, as the jumps in a fight and the throws are ae_bots_fight_move's and ae_bots_fight_grenade's */
static struct bots_skill_definition const *ae_bots_tick_skill(short skill)
{
	static struct bots_skill_definition definition;

	definition = *ae_bots_skill_definition(ae_bots_shooting_skill(skill));
	definition.jump_chance = 0.0f;
	definition.grenade_chance = 0.0f;
	return &definition;
}

/* the fight by level's glue (ae_bots_combat.inc, at bots.c's end), called from bots.c's AE hook lines */
static long ae_bots_fight_reaction(struct bots_bot *bot, struct bots_tick *tick, long reaction_ticks);
static long ae_bots_fight_look_jitter(struct bots_bot *bot, struct bots_tick *tick);
static void ae_bots_fight_share(struct bots_bot *bot, struct bots_tick *tick, long target);
static void ae_bots_fight_aim_point(struct bots_bot *bot, struct bots_tick *tick, real_point3d *aim_point);
static boolean ae_bots_fight_melee(struct bots_bot *bot, struct bots_tick *tick);
static long ae_bots_fight_melee_ticks(struct bots_bot *bot, struct bots_tick *tick);
static void ae_bots_fight_grenade(struct bots_bot *bot, struct bots_tick *tick, long reaction_ticks);
/* (0.7.0, the owner: "bots with a shotgun don't swap to a longer weapon") the close-range weapon out of its range: the
other one, if it has rounds and reaches further (ae_bots_combat.inc) */
static void ae_bots_range_swap(struct bots_bot *bot, struct bots_tick *tick);
static void ae_bots_fight_move(struct bots_bot *bot, struct bots_tick *tick);
static void ae_bots_crouch_jump_tick(struct bots_bot *bot, struct bots_tick *tick);
/* BOT TELLS (ae_bots_combat.inc): the look about of a bot standing at its post, in place of a steady turn; and where the
   objective would have it watch (ae_bots_objective.inc) */
static void ae_bots_look_about(struct bots_bot *bot, struct bots_tick *tick);
static boolean ae_bots_objective_hold_yaw(long bot_index, real *yaw);
/* (2026-10-20, ae_bots_combat_rules.h) CAMO: the sight of a camouflaged unit and the first aim error at it; LONG RANGE: the
aim error a far target keeps; WEAPON RANGE: whether the target is near enough for the held weapon */
static real ae_bots_fight_camo_reach(struct unit_datum const *unit);
static real ae_bots_fight_camo_error(struct player_datum const *target);
static void ae_bots_fight_range_floor(struct bots_bot *bot, struct bots_tick *tick);
static boolean ae_bots_squads_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting);
static boolean ae_bots_fight_in_range(struct bots_bot *bot, struct bots_tick *tick);
/* (2026-10-20) VERTICAL VIEW: bots.c's target scoring, a target too far above or below where the bot looks */
static boolean ae_bots_fight_in_vertical_view(struct bots_bot *bot, real_point3d const *eye, real_point3d const *point,
	real distance, boolean hurt, long player_index);
/* (0.3.6, ae_bots_combat.inc) LOOK DOWN: bots_find_target's line of sight, when the enemy's middle is hidden, to its
head */
struct bots_bot;
static boolean ae_bots_fight_head_in_sight(struct bots_bot *bot, long unit_index, real_point3d const *eye,
	long target_unit_index);
/* (2026-10-20, ae_bots_combat.inc) PRO TRAITS: the grenade at the last-seen spot (the walk's), high ground when roaming */
static void ae_bots_fight_prenade(struct bots_bot *bot, struct bots_tick *tick);
static real ae_bots_roam_height(struct bots_bot *bot, real_point3d const *point, real_point3d const *enemy);
static short ae_bots_roam_higher(struct bots_bot *bot, real_point3d const *position, short index);
/* (2026-10-20, ae_bots_power.inc) POWER ITEMS: bots_choose_goal's first ask; a map's start forgets the last one's */
static boolean ae_bots_power_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting);
static void ae_bots_power_reset(void);
/* (2026-10-20, ae_bots_team.inc) the bots as a team: WINGMAN and THE NUMBERS GAME (bots_choose_goal's ask after the
power items), OUTNUMBERED (the fight's confidence), a wingman's human, a map's start */
static boolean ae_bots_team_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting);
static real ae_bots_team_outnumbered(struct bots_bot *bot, struct bots_tick *tick, int level);
static long ae_bots_team_wingman_of(struct bots_bot const *bot);
static boolean ae_bots_team_free_for_human(struct bots_bot *bot, struct player_datum *player);
static boolean ae_bots_team_leash(struct bots_bot *bot, struct bots_tick *tick, real *direction);
static void ae_bots_team_hear(struct bots_bot *bot, long player_index, real_point3d const *position, long now);
static void ae_bots_team_reset(void);
/* (2026-10-24, ae_bots_revenge.inc) REVENGE: bots_choose_goal's ask after the objectives, the nemesis's target score,
the fight's tilt, a map's start */
static boolean ae_bots_revenge_goal(struct bots_bot *bot, long player_index, real_point3d const *position, boolean fighting);
static real ae_bots_revenge_score(struct bots_bot const *bot, long player_index);
static real ae_bots_fight_threat_score(struct bots_bot *bot, struct player_datum *player, struct player_datum *other, long player_index, real distance);
static boolean ae_bots_fight_threat_noticed(struct player_datum const *other, real distance);
static boolean ae_bots_item_refills(struct unit_datum *unit, long item_index);
static void ae_bots_vehicle_threat_note(struct bots_bot *bot, struct bots_tick *tick, long target);
static real ae_bots_revenge_tilt(struct bots_bot const *bot, int level);
static void ae_bots_revenge_reset(void);
/* (ae_bots_travel.inc, bots.c's hooks) a goal's stuck reports counted; its goal given up, a roam goal again elsewhere; a
moving machine learnt in the way that its rays closed nothing of: the walkable surfaces about it closed (their count) */
struct collision_bsp;
static void ae_bots_travel_goal_stuck_count(struct bots_bot *bot, long now);
static void ae_bots_travel_repick(struct bots_bot *bot, real_point3d const *position);
static long ae_bots_travel_close_device(long object_index, struct collision_bsp const *bsp, real block_radius);
/* (ae_bots_travel.inc) whether a point is within radius of a teleporter pad (a spot a bot waits on must not be one) */
static boolean ae_bots_travel_near_pad(real_point3d const *point, real radius);
/* (ae_bots_falls.inc: bots.c's bots_bot_update after the unstick) the bot does not walk off a lethal edge, runs from its
own grenade, holds a throw into a wall */
struct bots_bot;
struct bots_tick;
static void ae_bots_falls_guard(struct bots_bot *bot, struct bots_tick *tick);
