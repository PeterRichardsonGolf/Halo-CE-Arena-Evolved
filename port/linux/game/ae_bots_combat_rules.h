/*
AE_BOTS_COMBAT_RULES.H

The bots' fight by level (ae_bots_combat.c, notes/specs/2026-10-16-bots-infinite.md area 3) as pure rules: no
engine or config calls (port/linux/tests/ae_bots_combat_rules_test.c runs them). Halo Infinite's "combat dance"
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

Where Infinite's own per-level tuning (the owner's game files, read 2026-10-16; local research notes, not in the
repo) gave a number, the levels follow its ratios: RECRUIT ~ Recruit, EASY between Recruit and Marine, NORMAL ~
Marine, HEROIC ~ ODST, LEGENDARY ~ Spartan.

Levels: AE_BOTS_LEVEL_RECRUIT (0) .. AE_BOTS_LEVEL_LEGENDARY (4). Vitality: a fraction (1 full; shields above 1
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
/* the ticks the bot waits within reach before it strikes (Infinite's melee delays: Recruit 1-3 s, Marine 0.1-0.5,
ODST and Spartan 0.1-0.3; EASY between), random 0..1 */
long ae_bots_combat_melee_delay(int level, float random);
/* the ticks until it may strike again, random 0..1 */
long ae_bots_combat_melee_cooldown(int level, float random);

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

/* RETREAT (NORMAL and up; Infinite's disengage below 0.1): low confidence held for the bot's hesitation (6 to 18
ticks) starts it, with its shields under 0.3; it lasts at least 2 s (HEROIC and up 3 s: Infinite's hysteresis),
ends when its shields are back (0.95) or its confidence is high again (0.6), after 7 s at most; then not again for
6 s */
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

/* JUMPS in a fight (Infinite's "vertical movement", Marine up): the ticks to the next one, 0 for a level that does
not jump in a fight (RECRUIT, EASY): NORMAL every 3-5 s, HEROIC 1-4, LEGENDARY 1-3 */
long ae_bots_combat_vertical_delay(int level, float random);

/* STRAFING (Infinite's segments): the ticks a strafe to one side lasts: RECRUIT 1-4 s (the owner's), EASY 1.5-3,
NORMAL 2-4, HEROIC 1-2, LEGENDARY 0.2-0.8; random 0..1 */
long ae_bots_combat_strafe_ticks(int level, float random);
/* a strafe's HALT (it stands, still keeping its distance): the ticks of the halt at a new strafe, 0 for none:
NORMAL 0.2 of the time for 1-2 s, HEROIC 0.1 for 0.1-0.2, LEGENDARY 0.05 for 0.05-0.1; RECRUIT and EASY never
(a CE bot that stands aims far better: the kids' levels must not get harder) */
long ae_bots_combat_halt_ticks(int level, float chance_random, float length_random);
/* the halt that starts a fight: NORMAL and HEROIC up to 0.2 s, LEGENDARY 0.1; RECRUIT and EASY none */
long ae_bots_combat_initial_halt_ticks(int level, float random);

/* SHARED SIGHTINGS (team games): how near a teammate a bot tells where it saw an enemy (0: it tells no one):
HEROIC 10 units, LEGENDARY anywhere; RECRUIT .. NORMAL know only what they see */
float ae_bots_combat_share_distance(int level);

/* SPENT WEAPONS (bots.c's bots_weapon_empty, which swaps a bot off an empty weapon): a weapon of rounds is spent
when none are left, bots.c's own test (-1: left to it); an energy weapon (the plasma pistol, the plasma rifle) when
its battery is out (age 1). An energy weapon is one with no magazine or a magazine of no rounds: the stock plasma
pistol has one, every maximum 0, so bots.c took it for always empty, and a bot whose other weapon ran dry kept that
one, never fired again and backed off to the empty weapon's distance (owner 2026-10-18: Johnson "ran away and
didn't shoot"). The magazine's counts (0 for none: magazine_count) and the weapon's age 0..1 */
int ae_bots_combat_weapon_spent(int magazine_count, int rounds_loaded_maximum, int rounds_total_maximum,
	float age);

#endif
