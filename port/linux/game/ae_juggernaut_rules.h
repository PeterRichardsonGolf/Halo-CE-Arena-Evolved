/*
AE_JUGGERNAUT_RULES.H

Arena Evolved's JUGGERNAUT rules (AE JUGGER; owner 2026-10-14), as pure
rules (no engine includes: port/linux/tests/ae_juggernaut_rules_test.c runs
them). The engine side is ae_juggernaut.c's; the kill handler is the AE
Juggernaut's (numbers and rules only).

Stock CE's Juggernaut (the oddball engine's terminator ball) scores +1 for
killing the juggernaut and +1 for each of the juggernaut's kills, and a kill
while nobody is the juggernaut scores and makes the killer it. Under these
rules, on the host:

- only the juggernaut's kills score, +1 each;
- killing the juggernaut makes the killer the juggernaut and scores nothing;
- any other kill scores nothing and changes nothing;
- the juggernaut's suicide, his death to no player (a fall, a kill volume) or
  to a killer already dead, and no juggernaut at all (the game's start, the
  juggernaut quit) give the role to a random player who is alive (the host's
  own random number; a network client shows what the host sends).

The gametype keeps the rule in bit 2 of the byte the Xbox game left as its
universal variant's pad5 (bits 0-1 are COMBAT FEEDBACK's, ae_hit_feedback_rules.h;
the high nibble SILENT's, ae_silent_rules.h). Every gametype from before has 0
there: Halo CE's rules. It plays only in a game whose ball is the terminator
(Juggernaut); the byte travels with the variant (the saved file's and the
network game's sizes are unchanged), and nothing new goes over the network.
*/

#ifndef __AE_JUGGERNAUT_RULES_H
#define __AE_JUGGERNAUT_RULES_H

#define AE_JUGGERNAUT_BYTE_BIT 0x04u

/* whether a variant's byte has AE JUGGER's rules; the byte with them on or off (its other bits kept) */
int ae_juggernaut_h2(unsigned char byte);
unsigned char ae_juggernaut_byte_set(unsigned char byte, int on);
/* "halo 2" or "halo ce", for the logs */
char const *ae_juggernaut_rules_name(int on);

/* what becomes of the role after a kill */
enum
{
	/* (as it is: the juggernaut stays the juggernaut, or there still is none) */
	AE_JUGGERNAUT_KEEP = 0,
	/* the killer is the juggernaut now */
	AE_JUGGERNAUT_TO_KILLER,
	/* the dead juggernaut's role goes to a random player who is alive (not him) */
	AE_JUGGERNAUT_TO_RANDOM,
};

struct ae_juggernaut_kill
{
	/* the killer gets +1 */
	int killer_scores;
	/* AE_JUGGERNAUT_* */
	int succession;
};

/* a kill under AE JUGGER's rules: the killer a player (not none), the killer the
victim himself, the game's friendly fire flag (a suicide has it too), whether
the killer and the victim are the juggernaut (before the kill), and whether the
killer is alive (has a body to carry it) */
struct ae_juggernaut_kill ae_juggernaut_kill_decide(int killer_is_player, int suicide, int friendly_fire,
	int killer_is_juggernaut, int victim_is_juggernaut, int killer_alive);

/* whether a player may become the juggernaut at random: in the game (not
quit), alive, not the juggernaut already, and not the one excluded (the
juggernaut who just killed himself) */
int ae_juggernaut_eligible(int present, int alive, int quit, int already_juggernaut, int excluded);

/* which of count eligible players (0..count-1) a 16-bit random number picks:
-1 when there is none */
int ae_juggernaut_pick(int count, unsigned random);

/* the juggernaut's overshield (three times its maximum shield), on CE's overshield (a
shield vitality over 1 up to 3, CE's over-charge climbing to 3, and in multiplayer a decay of
AE_JUGGERNAUT_SHIELD_DECAY a tick back to 1). Each tick on the host, from the juggernaut's shield now and a tick ago:
- a drop of no more than the decay is the decay: given back (his overshield does not wear off);
- a bigger drop is damage: the wait starts at his shield's stun ticks (at least 1);
- otherwise the wait counts down;
- over-charging already: nothing more;
- full (1, CE's normal recharge done) or a hurt overshield (over 1) whose wait ran out: over-charge to 3.
Under 1, CE's own recharge runs (its stun first). */
#define AE_JUGGERNAUT_SHIELD_DECAY 0.00074074074f
enum
{
	AE_JUGGERNAUT_SHIELD_NONE = 0,
	/* start CE's over-charge (to 3) */
	AE_JUGGERNAUT_SHIELD_OVERCHARGE,
	/* give the decay back (shield_give) */
	AE_JUGGERNAUT_SHIELD_GIVE_BACK,
};
struct ae_juggernaut_shield
{
	/* AE_JUGGERNAUT_SHIELD_* */
	int action;
	/* the vitality to give back */
	float shield_give;
	/* the wait's ticks from now */
	int wait;
};
struct ae_juggernaut_shield ae_juggernaut_shield_tick(float shield, float shield_before, int over_charging, int wait,
	int stun_ticks);

#endif /* __AE_JUGGERNAUT_RULES_H */
