#include <stdio.h>
#include <string.h>
#include "ae_juggernaut_rules.h"
#include "ae_hit_feedback_rules.h"
#include "ae_silent_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the rule's bit sits beside COMBAT FEEDBACK's and SILENT's in one byte: none touches the others' bits; every
gametype from before (0) plays Halo CE's rules */
static void test_byte(void)
{
	unsigned byte;

	CHECK(!ae_juggernaut_h2(0));
	CHECK(AE_JUGGERNAUT_BYTE_BIT == 0x04u);
	CHECK((AE_JUGGERNAUT_BYTE_BIT & AE_HIT_FEEDBACK_BYTE_MASK) == 0 && (AE_JUGGERNAUT_BYTE_BIT & AE_SILENT_BYTE_MASK) == 0);
	for (byte = 0; byte < 256; byte++)
	{
		unsigned char on = ae_juggernaut_byte_set((unsigned char)byte, 1);
		unsigned char off = ae_juggernaut_byte_set((unsigned char)byte, 0);

		CHECK(ae_juggernaut_h2(on) && !ae_juggernaut_h2(off));
		CHECK((on & ~AE_JUGGERNAUT_BYTE_BIT & 0xFFu) == (byte & ~AE_JUGGERNAUT_BYTE_BIT & 0xFFu));
		CHECK((off & ~AE_JUGGERNAUT_BYTE_BIT & 0xFFu) == (byte & ~AE_JUGGERNAUT_BYTE_BIT & 0xFFu));
		CHECK(ae_hit_feedback_value(on) == ae_hit_feedback_value((unsigned char)byte));
		CHECK(ae_silent_mode(on, AE_SILENT_ROLE_MOVEMENT) == ae_silent_mode((unsigned char)byte, AE_SILENT_ROLE_MOVEMENT));
		CHECK(ae_silent_mode(on, AE_SILENT_ROLE_WEAPON) == ae_silent_mode((unsigned char)byte, AE_SILENT_ROLE_WEAPON));
		/* (the others' setters keep it) */
		CHECK(ae_juggernaut_h2(ae_hit_feedback_byte_set(on, AE_HIT_FEEDBACK_ARENA)));
		CHECK(ae_juggernaut_h2(ae_hit_feedback_byte_set(on, AE_HIT_FEEDBACK_CLASSIC)));
		CHECK(ae_juggernaut_h2(ae_silent_byte_set(on, AE_SILENT_ROLE_WEAPON, AE_SILENT_SILENT)));
		CHECK(!ae_juggernaut_h2(ae_silent_byte_set(off, AE_SILENT_ROLE_MOVEMENT, AE_SILENT_SILENT)));
	}
	CHECK(!strcmp(ae_juggernaut_rules_name(1), "halo 2"));
	CHECK(!strcmp(ae_juggernaut_rules_name(0), "halo ce"));
}

static struct ae_juggernaut_kill kill(int killer_is_player, int suicide, int friendly_fire, int killer_is_juggernaut,
	int victim_is_juggernaut, int killer_alive)
{
	struct ae_juggernaut_kill k;

	k.killer_scores = -1;
	k.succession = -1;
	k = ae_juggernaut_kill_decide(killer_is_player, suicide, friendly_fire, killer_is_juggernaut,
		victim_is_juggernaut, killer_alive);
	return k;
}

/* the kill handler: only the juggernaut's kills score (+1 each); killing the juggernaut makes the killer it
and scores nothing; any other kill scores nothing and changes nothing; the juggernaut's suicide, or a death to
no player, hands it to a random player */
static void test_kills(void)
{
	struct ae_juggernaut_kill k;

	/* (the juggernaut kills a player: +1, still the juggernaut) */
	k = kill(1, 0, 0, 1, 0, 1);
	CHECK(k.killer_scores == 1 && k.succession == AE_JUGGERNAUT_KEEP);
	/* (...from beyond the grave, a grenade after he died: still his kill, still +1; he is still the juggernaut) */
	k = kill(1, 0, 0, 1, 0, 0);
	CHECK(k.killer_scores == 1 && k.succession == AE_JUGGERNAUT_KEEP);
	/* (a player kills the juggernaut: no score, the killer is the juggernaut) */
	k = kill(1, 0, 0, 0, 1, 1);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_TO_KILLER);
	/* (...a killer already dead (his grenade) cannot carry it: a random player) */
	k = kill(1, 0, 0, 0, 1, 0);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_TO_RANDOM);
	/* (a player kills a player, neither the juggernaut: nothing, and no capture when there is none) */
	k = kill(1, 0, 0, 0, 0, 1);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_KEEP);
	/* (the juggernaut's suicide: no score, a random player) */
	k = kill(1, 1, 1, 1, 1, 1);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_TO_RANDOM);
	k = kill(1, 1, 0, 1, 1, 1);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_TO_RANDOM);
	/* (the juggernaut killed by no player: a fall, a kill volume, the game itself) */
	k = kill(0, 0, 0, 0, 1, 0);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_TO_RANDOM);
	k = kill(0, 0, 1, 0, 1, 0);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_TO_RANDOM);
	/* (a player's suicide, or his death to no player: nothing) */
	k = kill(1, 1, 1, 0, 0, 1);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_KEEP);
	k = kill(0, 0, 0, 0, 0, 0);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_KEEP);
	/* (a friendly kill (a team game) by the juggernaut scores nothing; the juggernaut killed by a team mate:
	killer != victim, he takes it) */
	k = kill(1, 0, 1, 1, 0, 1);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_KEEP);
	k = kill(1, 0, 1, 0, 1, 1);
	CHECK(k.killer_scores == 0 && k.succession == AE_JUGGERNAUT_TO_KILLER);
	/* (every combination: a score only for a juggernaut's kill of someone else, never with a succession) */
	{
		int bits;

		for (bits = 0; bits < 64; bits++)
		{
			int killer_is_player = bits & 1, suicide = (bits >> 1) & 1, friendly_fire = (bits >> 2) & 1;
			int killer_is_juggernaut = (bits >> 3) & 1, victim_is_juggernaut = (bits >> 4) & 1, alive = (bits >> 5) & 1;

			k = ae_juggernaut_kill_decide(killer_is_player, suicide, friendly_fire, killer_is_juggernaut,
				victim_is_juggernaut, alive);
			CHECK(k.killer_scores == (killer_is_player && !suicide && !friendly_fire && killer_is_juggernaut &&
				!victim_is_juggernaut));
			CHECK(!k.killer_scores || k.succession == AE_JUGGERNAUT_KEEP);
			CHECK(victim_is_juggernaut ? k.succession != AE_JUGGERNAUT_KEEP : k.succession == AE_JUGGERNAUT_KEEP);
			CHECK(k.succession != AE_JUGGERNAUT_TO_KILLER || (killer_is_player && !suicide && alive));
		}
	}
}

/* who may become the juggernaut at random: a player in the game, alive, not one already, not the one just
excluded (the juggernaut who killed himself) */
static void test_eligible(void)
{
	CHECK(ae_juggernaut_eligible(1, 1, 0, 0, 0));
	CHECK(!ae_juggernaut_eligible(0, 1, 0, 0, 0));
	CHECK(!ae_juggernaut_eligible(1, 0, 0, 0, 0));
	CHECK(!ae_juggernaut_eligible(1, 1, 1, 0, 0));
	CHECK(!ae_juggernaut_eligible(1, 1, 0, 1, 0));
	CHECK(!ae_juggernaut_eligible(1, 1, 0, 0, 1));
}

/* the pick: one of the eligible, by the host's random number; none when there is none */
static void test_pick(void)
{
	int hits[4] = { 0, 0, 0, 0 };
	unsigned random;

	CHECK(ae_juggernaut_pick(0, 12345u) == -1);
	CHECK(ae_juggernaut_pick(-3, 1u) == -1);
	CHECK(ae_juggernaut_pick(1, 0u) == 0 && ae_juggernaut_pick(1, 65535u) == 0);
	for (random = 0; random < 65536u; random++)
	{
		int pick = ae_juggernaut_pick(4, random);

		CHECK(pick >= 0 && pick < 4);
		if (pick >= 0 && pick < 4)
			hits[pick]++;
	}
	/* (each as likely: a 16-bit random number over 4) */
	CHECK(hits[0] == 16384 && hits[1] == 16384 && hits[2] == 16384 && hits[3] == 16384);
	{
		int seen[16] = { 0 }, n, all = 1;

		for (random = 0; random < 65536u; random += 7)
			seen[ae_juggernaut_pick(16, random)] = 1;
		for (n = 0; n < 16; n++)
			all &= seen[n];
		CHECK(all);
	}
}

/* the overshield on CE's: full shields over-charge to 3, the decay is given back, damage waits the stun */
static void test_shield(void)
{
	struct ae_juggernaut_shield r;

	/* (full, not hit: over-charge) */
	r = ae_juggernaut_shield_tick(1.0f, 1.0f, 0, 0, 60);
	CHECK(r.action == AE_JUGGERNAUT_SHIELD_OVERCHARGE && r.wait == 0);
	/* (over-charging already, or at 3: nothing) */
	r = ae_juggernaut_shield_tick(2.0f, 1.9667f, 1, 0, 60);
	CHECK(r.action == AE_JUGGERNAUT_SHIELD_NONE);
	r = ae_juggernaut_shield_tick(3.0f, 3.0f, 0, 0, 60);
	CHECK(r.action == AE_JUGGERNAUT_SHIELD_NONE);
	/* (CE's multiplayer decay: given back, exactly) */
	r = ae_juggernaut_shield_tick(3.0f - AE_JUGGERNAUT_SHIELD_DECAY, 3.0f, 0, 0, 60);
	CHECK(r.action == AE_JUGGERNAUT_SHIELD_GIVE_BACK && r.shield_give > 0.00074f && r.shield_give < 0.00075f);
	/* (a hit on the overshield: wait his stun, then climb back to 3) */
	r = ae_juggernaut_shield_tick(2.2f, 3.0f, 0, 0, 60);
	CHECK(r.action == AE_JUGGERNAUT_SHIELD_NONE && r.wait == 60);
	r = ae_juggernaut_shield_tick(2.2f, 2.2f, 0, 2, 60);
	CHECK(r.action == AE_JUGGERNAUT_SHIELD_NONE && r.wait == 1);
	r = ae_juggernaut_shield_tick(2.2f, 2.2f, 0, 1, 60);
	CHECK(r.action == AE_JUGGERNAUT_SHIELD_OVERCHARGE && r.wait == 0);
	/* (under 1: CE's recharge runs; at 1 again, over-charge) */
	r = ae_juggernaut_shield_tick(0.4f, 0.3f, 0, 0, 60);
	CHECK(r.action == AE_JUGGERNAUT_SHIELD_NONE);
	r = ae_juggernaut_shield_tick(0.0f, 0.5f, 0, 0, 0);
	CHECK(r.action == AE_JUGGERNAUT_SHIELD_NONE && r.wait == 1);
	r = ae_juggernaut_shield_tick(1.0f, 0.98f, 0, 0, 60);
	CHECK(r.action == AE_JUGGERNAUT_SHIELD_OVERCHARGE);
}

int main(void)
{
	test_byte();
	test_kills();
	test_eligible();
	test_pick();
	test_shield();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
