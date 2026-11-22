#include <stdio.h>
#include <string.h>

#include "ae_spawner_logic_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define T AE_SPAWNER_TICKS_PER_SECOND

static struct ae_spawner_sight const none = { 0, 0, 0 };
static struct ae_spawner_sight const on_spot = { 1, 0, 0 };
static struct ae_spawner_sight const in_hand = { 1, 1, 0 };
static struct ae_spawner_sight const spent = { 1, 0, 1 };

/* STATIC: a spawn on every beat of the clock from the delay, whatever sits there; no state. */
static void test_static(void)
{
	struct ae_spawner_machine s;

	ae_spawner_logic_begin(&s, AE_SPAWNER_STATIC, 120 * T, 0);
	CHECK(ae_spawner_logic_step(&s, 0, &none) == AE_SPAWNER_DO_SPAWN);	/* tick 0: as CE */
	ae_spawner_logic_made(&s, 0, TRUE);
	CHECK(ae_spawner_logic_step(&s, 1, &on_spot) == AE_SPAWNER_DO_NOTHING);
	CHECK(ae_spawner_logic_step(&s, 120 * T - 1, &on_spot) == AE_SPAWNER_DO_NOTHING);
	CHECK(ae_spawner_logic_step(&s, 120 * T, &on_spot) == AE_SPAWNER_DO_SPAWN);	/* taken or not */
	CHECK(ae_spawner_logic_step(&s, 240 * T, &in_hand) == AE_SPAWNER_DO_SPAWN);
	CHECK(ae_spawner_logic_ticks_left(&s, 0) == 120 * T);
	CHECK(ae_spawner_logic_ticks_left(&s, 1) == 120 * T - 1);
	CHECK(ae_spawner_logic_ticks_left(&s, 120 * T - 1) == 1);
	CHECK(ae_spawner_logic_on_spot(&s, 5 * T));
	CHECK(!ae_spawner_logic_on_spot(&s, 25 * T));
	/* a delay: the first spawn at 60 s, then every 120 s */
	ae_spawner_logic_begin(&s, AE_SPAWNER_STATIC, 120 * T, 60 * T);
	CHECK(ae_spawner_logic_step(&s, 0, &none) == AE_SPAWNER_DO_NOTHING);
	CHECK(ae_spawner_logic_ticks_left(&s, 0) == 60 * T);
	CHECK(!ae_spawner_logic_on_spot(&s, 0));
	CHECK(ae_spawner_logic_step(&s, 60 * T, &none) == AE_SPAWNER_DO_SPAWN);
	CHECK(ae_spawner_logic_step(&s, 180 * T, &none) == AE_SPAWNER_DO_SPAWN);
	CHECK(ae_spawner_logic_step(&s, 120 * T, &none) == AE_SPAWNER_DO_NOTHING);
	/* the floor on the period */
	ae_spawner_logic_begin(&s, AE_SPAWNER_STATIC, 2 * T, 0);
	CHECK(s.period == 5 * T);
	ae_spawner_logic_begin(&s, 99, 10 * T, -5);	/* a bad logic reads static, a bad delay 0 */
	CHECK(s.logic == AE_SPAWNER_STATIC && s.delay == 0);
}

/* PICKUP: made -> READY (kept on its spot); taken -> WAITING, the next one a period later. */
static void test_pickup(void)
{
	struct ae_spawner_machine s;

	ae_spawner_logic_begin(&s, AE_SPAWNER_PICKUP, 30 * T, 0);
	CHECK(s.state == AE_SPAWNER_WAITING && !ae_spawner_logic_on_spot(&s, 0));
	CHECK(ae_spawner_logic_step(&s, 0, &none) == AE_SPAWNER_DO_SPAWN);
	ae_spawner_logic_made(&s, 0, TRUE);
	CHECK(s.state == AE_SPAWNER_READY && ae_spawner_logic_on_spot(&s, 1));
	CHECK(ae_spawner_logic_step(&s, 1, &on_spot) == AE_SPAWNER_DO_KEEP);
	CHECK(ae_spawner_logic_step(&s, 90 * T, &on_spot) == AE_SPAWNER_DO_KEEP);	/* untaken: stays */
	CHECK(ae_spawner_logic_ticks_left(&s, 90 * T) == 30 * T);	/* the timers show the period while READY */
	CHECK(ae_spawner_logic_step(&s, 100 * T, &in_hand) == AE_SPAWNER_DO_LOG_TAKEN);
	CHECK(s.state == AE_SPAWNER_WAITING && !s.has_object && s.next_at == 130 * T);
	CHECK(ae_spawner_logic_ticks_left(&s, 100 * T) == 30 * T);
	CHECK(ae_spawner_logic_ticks_left(&s, 129 * T) == 1 * T);
	CHECK(ae_spawner_logic_step(&s, 129 * T, &none) == AE_SPAWNER_DO_NOTHING);
	CHECK(ae_spawner_logic_step(&s, 130 * T, &none) == AE_SPAWNER_DO_SPAWN);
	CHECK(ae_spawner_logic_step(&s, 131 * T, &none) == AE_SPAWNER_DO_NOTHING);	/* not again until made */
	ae_spawner_logic_made(&s, 131 * T, TRUE);
	/* lost untaken (purged, destroyed, ammo walked off): the count restarts */
	CHECK(ae_spawner_logic_step(&s, 140 * T, &none) == AE_SPAWNER_DO_LOG_LOST);
	CHECK(s.next_at == 170 * T && s.state == AE_SPAWNER_WAITING);
	CHECK(ae_spawner_logic_step(&s, 170 * T, &none) == AE_SPAWNER_DO_SPAWN);
	/* a spawn that made nothing (the pool unavailable): wait a period, do not try every tick */
	ae_spawner_logic_made(&s, 170 * T, FALSE);
	CHECK(s.state == AE_SPAWNER_WAITING && s.next_at == 200 * T);
	CHECK(ae_spawner_logic_step(&s, 171 * T, &none) == AE_SPAWNER_DO_NOTHING);
	CHECK(ae_spawner_logic_step(&s, 200 * T, &none) == AE_SPAWNER_DO_SPAWN);
	/* a delay: the first spawn at the delay */
	ae_spawner_logic_begin(&s, AE_SPAWNER_PICKUP, 30 * T, 10 * T);
	CHECK(ae_spawner_logic_step(&s, 0, &none) == AE_SPAWNER_DO_NOTHING);
	CHECK(ae_spawner_logic_ticks_left(&s, 0) == 10 * T);
	CHECK(ae_spawner_logic_step(&s, 10 * T, &none) == AE_SPAWNER_DO_SPAWN);
}

/* EXPIRED: taken -> RED until the weapon is out of play, then the next one a period later. */
static void test_expired(void)
{
	struct ae_spawner_machine s;

	ae_spawner_logic_begin(&s, AE_SPAWNER_EXPIRED, 30 * T, 0);
	CHECK(ae_spawner_logic_step(&s, 0, &none) == AE_SPAWNER_DO_SPAWN);
	ae_spawner_logic_made(&s, 0, TRUE);
	CHECK(ae_spawner_logic_step(&s, 5 * T, &on_spot) == AE_SPAWNER_DO_KEEP);
	CHECK(ae_spawner_logic_step(&s, 10 * T, &in_hand) == AE_SPAWNER_DO_LOG_TAKEN);
	CHECK(s.state == AE_SPAWNER_RED && s.has_object && s.taken);
	CHECK(!ae_spawner_logic_on_spot(&s, 10 * T));
	CHECK(ae_spawner_logic_step(&s, 11 * T, &in_hand) == AE_SPAWNER_DO_NOTHING);	/* red, still held */
	CHECK(s.state == AE_SPAWNER_RED);
	CHECK(ae_spawner_logic_step(&s, 40 * T, &on_spot) == AE_SPAWNER_DO_NOTHING);	/* dropped, full: still red */
	CHECK(s.state == AE_SPAWNER_RED);
	CHECK(ae_spawner_logic_step(&s, 50 * T, &spent) == AE_SPAWNER_DO_LOG_EXPIRED);	/* empty and not held */
	CHECK(s.state == AE_SPAWNER_WAITING && !s.has_object && !s.taken && s.next_at == 80 * T);
	CHECK(ae_spawner_logic_step(&s, 79 * T, &none) == AE_SPAWNER_DO_NOTHING);
	CHECK(ae_spawner_logic_step(&s, 80 * T, &none) == AE_SPAWNER_DO_SPAWN);
	ae_spawner_logic_made(&s, 80 * T, TRUE);
	/* taken, then the game purges the drop: out of play by absence */
	CHECK(ae_spawner_logic_step(&s, 90 * T, &in_hand) == AE_SPAWNER_DO_LOG_TAKEN);
	CHECK(ae_spawner_logic_step(&s, 100 * T, &none) == AE_SPAWNER_DO_LOG_EXPIRED);
	CHECK(s.next_at == 130 * T);
	/* the one the exact once: a second step after expiry logs nothing more */
	CHECK(ae_spawner_logic_step(&s, 101 * T, &none) == AE_SPAWNER_DO_NOTHING);
}

/* TRIGGER: never on a tick; a fire makes the next step spawn, whatever sits there. */
static void test_trigger(void)
{
	struct ae_spawner_machine s;

	ae_spawner_logic_begin(&s, AE_SPAWNER_TRIGGER, 120 * T, 0);
	CHECK(ae_spawner_logic_step(&s, 0, &none) == AE_SPAWNER_DO_NOTHING);
	CHECK(ae_spawner_logic_step(&s, 500 * T, &none) == AE_SPAWNER_DO_NOTHING);
	CHECK(ae_spawner_logic_ticks_left(&s, 500 * T) == 120 * T);	/* the timers show the period: no countdown known */
	ae_spawner_logic_fire(&s);
	CHECK(ae_spawner_logic_step(&s, 501 * T, &none) == AE_SPAWNER_DO_SPAWN);
	ae_spawner_logic_made(&s, 501 * T, TRUE);
	CHECK(ae_spawner_logic_step(&s, 502 * T, &on_spot) == AE_SPAWNER_DO_KEEP);
	CHECK(ae_spawner_logic_step(&s, 510 * T, &in_hand) == AE_SPAWNER_DO_LOG_TAKEN);
	CHECK(s.state == AE_SPAWNER_WAITING && s.next_at == -1);	/* no count: its channel decides */
	CHECK(ae_spawner_logic_step(&s, 900 * T, &none) == AE_SPAWNER_DO_NOTHING);
	ae_spawner_logic_fire(&s);
	ae_spawner_logic_fire(&s);	/* twice is once */
	CHECK(ae_spawner_logic_step(&s, 901 * T, &none) == AE_SPAWNER_DO_SPAWN);
	ae_spawner_logic_made(&s, 901 * T, TRUE);
	ae_spawner_logic_fire(&s);	/* fired while a weapon sits there: spawn again (the channel's beat wins) */
	CHECK(ae_spawner_logic_step(&s, 902 * T, &on_spot) == AE_SPAWNER_DO_SPAWN);
	ae_spawner_logic_made(&s, 902 * T, TRUE);
	/* its weapon lost untaken (purged): no count of its own, the channel's next beat brings the next */
	CHECK(ae_spawner_logic_step(&s, 950 * T, &none) == AE_SPAWNER_DO_LOG_LOST);
	CHECK(s.state == AE_SPAWNER_WAITING && s.next_at == -1 && !s.has_object);
	CHECK(ae_spawner_logic_step(&s, 2000 * T, &none) == AE_SPAWNER_DO_NOTHING);
	/* a fire on a non-trigger spawner does nothing */
	ae_spawner_logic_begin(&s, AE_SPAWNER_PICKUP, 30 * T, 10 * T);
	ae_spawner_logic_fire(&s);
	CHECK(s.next_at == 10 * T);
}

/* DORMANT does nothing; a reset starts over; a retime keeps a running count and what is on the map. */
static void test_dormant_reset_retime(void)
{
	struct ae_spawner_machine s;

	ae_spawner_logic_begin(&s, AE_SPAWNER_PICKUP, 30 * T, 0);
	s.state = AE_SPAWNER_DORMANT;
	CHECK(ae_spawner_logic_step(&s, 0, &none) == AE_SPAWNER_DO_NOTHING);
	CHECK(ae_spawner_logic_step(&s, 999 * T, &none) == AE_SPAWNER_DO_NOTHING);
	ae_spawner_logic_reset(&s);
	CHECK(s.state == AE_SPAWNER_WAITING && s.next_at == 0 && s.made_at == -1);
	CHECK(ae_spawner_logic_step(&s, 0, &none) == AE_SPAWNER_DO_SPAWN);
	ae_spawner_logic_made(&s, 0, TRUE);
	/* a set switch while the weapon sits there: it stays, the new period applies from the next count */
	ae_spawner_logic_retime(&s, AE_SPAWNER_EXPIRED, 60 * T, 0, 20 * T);
	CHECK(s.logic == AE_SPAWNER_EXPIRED && s.has_object && s.state == AE_SPAWNER_READY);
	CHECK(ae_spawner_logic_step(&s, 21 * T, &on_spot) == AE_SPAWNER_DO_KEEP);
	CHECK(ae_spawner_logic_step(&s, 22 * T, &in_hand) == AE_SPAWNER_DO_LOG_TAKEN);
	CHECK(s.state == AE_SPAWNER_RED);
	/* a switch back to pickup while RED: the count runs from now with the new period */
	ae_spawner_logic_retime(&s, AE_SPAWNER_PICKUP, 30 * T, 0, 25 * T);
	CHECK(s.state == AE_SPAWNER_WAITING && !s.has_object && s.next_at == 55 * T);
	/* a running count keeps its remaining time, capped to the new period */
	ae_spawner_logic_retime(&s, AE_SPAWNER_PICKUP, 10 * T, 0, 30 * T);
	CHECK(s.next_at == 40 * T);	/* 25 s left, capped to 10 */
	ae_spawner_logic_retime(&s, AE_SPAWNER_PICKUP, 60 * T, 0, 35 * T);
	CHECK(s.next_at == 40 * T);	/* 5 s left stays 5 */
	/* to static: the clock's next beat is within a period of now, keeping the delay's phase */
	ae_spawner_logic_begin(&s, AE_SPAWNER_PICKUP, 30 * T, 0);
	ae_spawner_logic_retime(&s, AE_SPAWNER_STATIC, 120 * T, 60 * T, 200 * T);
	CHECK(s.logic == AE_SPAWNER_STATIC && !s.has_object);
	CHECK(ae_spawner_logic_ticks_left(&s, 200 * T) <= 120 * T && ae_spawner_logic_ticks_left(&s, 200 * T) >= 1);
	CHECK(ae_spawner_logic_step(&s, 200 * T + ae_spawner_logic_ticks_left(&s, 200 * T), &none) == AE_SPAWNER_DO_SPAWN);
	/* to static before its delay: the delay stands */
	ae_spawner_logic_begin(&s, AE_SPAWNER_PICKUP, 30 * T, 0);
	ae_spawner_logic_retime(&s, AE_SPAWNER_STATIC, 120 * T, 60 * T, 10 * T);
	CHECK(s.delay == 60 * T && ae_spawner_logic_ticks_left(&s, 10 * T) == 50 * T);
	/* to trigger with nothing on the spot: no count */
	ae_spawner_logic_retime(&s, AE_SPAWNER_TRIGGER, 120 * T, 0, 10 * T);
	CHECK(s.next_at == -1 && ae_spawner_logic_step(&s, 11 * T, &none) == AE_SPAWNER_DO_NOTHING);
}

/* The two rule sets where they differ. (a) a static POWER pad: ae always spawns on its clock; reference skips a beat
that comes under 15 s after its weapon was taken (the timers show the beat after, the skipped beat has no "on spot"). */
static void test_rules_static_skip(void)
{
	struct ae_spawner_machine s;
	int rules;

	for (rules = 0; rules < 2; rules++)
	{
		ae_spawner_logic_begin(&s, AE_SPAWNER_STATIC, 120 * T, 0);
		CHECK(s.rules == 0 && s.taken_at == -1 && s.skipped_at == -1);
		if (rules) s.rules = AE_SPAWNER_RULE_STATIC_SKIP;
		CHECK(ae_spawner_logic_step(&s, 0, &none) == AE_SPAWNER_DO_SPAWN);
		ae_spawner_logic_made(&s, 0, TRUE);
		/* taken 2 s before the beat */
		ae_spawner_logic_static_taken(&s, 118 * T);
		CHECK(s.taken_at == 118 * T);
		if (!rules)
		{
			CHECK(ae_spawner_logic_ticks_left(&s, 118 * T) == 2 * T);
			CHECK(ae_spawner_logic_step(&s, 120 * T, &none) == AE_SPAWNER_DO_SPAWN);
			CHECK(ae_spawner_logic_on_spot(&s, 121 * T));
			continue;
		}
		CHECK(ae_spawner_logic_ticks_left(&s, 118 * T) == 120 * T);	/* the beat after: capped to the period */
		CHECK(ae_spawner_logic_step(&s, 120 * T, &none) == AE_SPAWNER_DO_LOG_SKIPPED);
		CHECK(s.skipped_at == 120 * T && s.state == AE_SPAWNER_WAITING);
		CHECK(!ae_spawner_logic_on_spot(&s, 121 * T));	/* nothing came: no "on spot" window */
		CHECK(ae_spawner_logic_ticks_left(&s, 121 * T) == 119 * T);
		CHECK(ae_spawner_logic_step(&s, 240 * T, &none) == AE_SPAWNER_DO_SPAWN);	/* the next beat spawns */
		CHECK(ae_spawner_logic_on_spot(&s, 241 * T));
		/* taken 15 s or more before the beat: it spawns */
		ae_spawner_logic_static_taken(&s, 345 * T);
		CHECK(ae_spawner_logic_ticks_left(&s, 345 * T) == 15 * T);
		CHECK(ae_spawner_logic_step(&s, 360 * T, &none) == AE_SPAWNER_DO_SPAWN);
		/* 14 s before: skipped */
		ae_spawner_logic_static_taken(&s, 466 * T);
		CHECK(ae_spawner_logic_step(&s, 480 * T, &none) == AE_SPAWNER_DO_LOG_SKIPPED);
		/* a reset forgets the pickup and keeps the rule */
		ae_spawner_logic_reset(&s);
		CHECK(s.taken_at == -1 && s.skipped_at == -1 && s.rules == AE_SPAWNER_RULE_STATIC_SKIP);
		/* a pickup of a pickup spawner's weapon is not a static's */
		ae_spawner_logic_begin(&s, AE_SPAWNER_PICKUP, 30 * T, 0);
		ae_spawner_logic_static_taken(&s, 10 * T);
		CHECK(s.taken_at == -1);
	}
}

/* (b) an expired rack's weapon out of play: ae = gone, or empty and not held; reference = gone, or empty even held.
(c) a RED POWER pad shows red under ae, never under reference; a RED rack shows red under both. */
static void test_rules_out_of_play_and_red(void)
{
	int reference;

	for (reference = 0; reference < 2; reference++)
	{
		CHECK(ae_spawner_out_of_play(0, 0, 0, reference));	/* gone */
		CHECK(ae_spawner_out_of_play(0, 1, 1, reference));
		CHECK(!ae_spawner_out_of_play(1, 0, 0, reference));	/* lying with ammo */
		CHECK(!ae_spawner_out_of_play(1, 1, 0, reference));	/* held with ammo */
		CHECK(ae_spawner_out_of_play(1, 0, 1, reference));	/* lying empty */
		CHECK(ae_spawner_shown_red(1, 0, reference));	/* a rack */
		CHECK(!ae_spawner_shown_red(0, 0, reference) && !ae_spawner_shown_red(0, 1, reference));
	}
	CHECK(!ae_spawner_out_of_play(1, 1, 1, 0));	/* ae: held empty stays in play */
	CHECK(ae_spawner_out_of_play(1, 1, 1, 1));	/* reference: held empty is gone */
	CHECK(ae_spawner_shown_red(1, 1, 0));	/* ae: a taken POWER pad is red */
	CHECK(!ae_spawner_shown_red(1, 1, 1));	/* reference: never */
}

int main(void)
{
	test_static();
	test_pickup();
	test_expired();
	test_trigger();
	test_dormant_reset_retime();
	test_rules_static_skip();
	test_rules_out_of_play_and_red();
	printf("%s: %d checks, %d failures\n", __FILE__, checks, failures);
	return failures ? 1 : 0;
}
