#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_medal_history.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_history history;

#define PI_F 3.14159265f

enum { TAG_PISTOL = 100, TAG_RIFLE = 101, TAG_FRAG = 102, TAG_MELEE = 103, TAG_SNIPER = 104 };

static void fresh(void)
{
	ae_history_reset(&history);
}

static struct ae_history_hit make_hit(int attacker, long tick, long tag, int category, int kind, int head, float shield,
	float body)
{
	struct ae_history_hit hit;

	memset(&hit, 0, sizeof(hit));
	hit.attacker = attacker;
	hit.tick = tick;
	hit.tag = tag;
	hit.category = (short)category;
	hit.kind = (short)kind;
	hit.head = (unsigned char)head;
	hit.shield = shield;
	hit.body = body;
	/* the shield and the body weigh the same here */
	hit.taken = (shield + body) / 2.0f;
	return hit;
}

static void add_hit(int victim, int attacker, long tick, long tag, int category, int kind, int head, float shield,
	float body, float before_shield, float before_body)
{
	struct ae_history_hit hit = make_hit(attacker, tick, tag, category, kind, head, shield, body);

	ae_history_hit(&history, victim, &hit, before_shield, before_body);
}

static void ring_test(void)
{
	int index;

	fresh();
	/* more shots than the ring holds: the newest stay */
	for (index = 0; index < 100; index++)
		ae_history_shot(&history, 4, 1000 + index, TAG_PISTOL, AE_MEDAL_ROLE_SNIPER, 0, 1);
	CHECK(history.players[4].shot_count == AE_HISTORY_SHOTS);
	CHECK(ae_history_no_scope(&history, 4, 1099));
	/* a player who is gone: nothing */
	ae_history_clear_player(&history, 4);
	CHECK(!ae_history_no_scope(&history, 4, 1099));
	/* bad slots and rings: nothing and no harm */
	ae_history_shot(&history, -1, 1, 1, 1, 0, 1);
	ae_history_shot(&history, 999, 1, 1, 1, 0, 1);
	ae_history_step(&history, -1, 1, 0, 0, 0, 0, 0);
	ae_history_step(&history, 999, 1, 0, 0, 0, 0, 0);
	add_hit(-1, 0, 1, 1, 1, 1, 0, 0.1f, 0.0f, 1.0f, 1.0f);
	add_hit(999, 0, 1, 1, 1, 1, 0, 0.1f, 0.0f, 1.0f, 1.0f);
	CHECK(ae_history_taken_by(&history, -1, 0) == 0.0f && ae_history_taken_by(&history, 999, 0) == 0.0f);
	CHECK(!ae_history_no_scope(&history, -1, 5) && !ae_history_turned(&history, -1, 5));
	CHECK(!ae_history_quick_draw(&history, -1, 1, 5, 1, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET));
	CHECK(!ae_history_reversal(&history, -1, 1) && !ae_history_nade_shot(&history, 1, -1, 1));
	CHECK(!ae_history_off_the_rack(&history, -1, 1, 5));
}

static void life_test(void)
{
	int index;

	fresh();
	add_hit(1, 0, 100, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 2, 110, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.25f, 0.0f, 0.5f, 1.0f);
	CHECK(history.players[1].life.count == 2);
	CHECK(history.players[1].life.start_shield == 1.0f && history.players[1].life.start_body == 1.0f);
	CHECK(fabsf(ae_history_taken_by(&history, 1, 0) - 0.25f) < 1e-6f);
	CHECK(fabsf(ae_history_taken_by(&history, 1, 2) - 0.125f) < 1e-6f);
	CHECK(ae_history_taken_by(&history, 1, 5) == 0.0f);
	/* a life ends: a new one, from its own start */
	ae_history_life_end(&history, 1);
	CHECK(history.players[1].life.count == 0 && ae_history_taken_by(&history, 1, 0) == 0.0f);
	add_hit(1, 0, 500, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.5f, 0.0f, 0.75f, 0.5f);
	CHECK(history.players[1].life.start_shield == 0.75f && history.players[1].life.start_body == 0.5f);
	/* too many blows: the life is flagged, not written past */
	ae_history_life_end(&history, 1);
	for (index = 0; index < AE_HISTORY_HITS + 5; index++)
		add_hit(1, 0, 600 + index, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.001f, 0.0f, 1.0f, 1.0f);
	CHECK(history.players[1].life.count == AE_HISTORY_HITS && history.players[1].life.overflow);
}

static void no_scope_test(void)
{
	fresh();
	ae_history_shot(&history, 0, 1000, TAG_SNIPER, AE_MEDAL_ROLE_SNIPER, 0, 1);
	CHECK(ae_history_no_scope(&history, 0, 1003));
	CHECK(ae_history_no_scope(&history, 0, 1060));
	CHECK(!ae_history_no_scope(&history, 0, 1061));
	/* a shot after the kill is another shot */
	CHECK(!ae_history_no_scope(&history, 0, 999));
	/* zoomed */
	fresh();
	ae_history_shot(&history, 0, 1000, TAG_SNIPER, AE_MEDAL_ROLE_SNIPER, 1, 1);
	CHECK(!ae_history_no_scope(&history, 0, 1003));
	/* the latest sniper shot decides */
	ae_history_shot(&history, 0, 1030, TAG_SNIPER, AE_MEDAL_ROLE_SNIPER, 0, 1);
	CHECK(ae_history_no_scope(&history, 0, 1033));
	ae_history_shot(&history, 0, 1040, TAG_SNIPER, AE_MEDAL_ROLE_SNIPER, 1, 1);
	CHECK(!ae_history_no_scope(&history, 0, 1043));
	/* another weapon's shot is none */
	fresh();
	ae_history_shot(&history, 0, 1000, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	CHECK(!ae_history_no_scope(&history, 0, 1003));
	CHECK(!ae_history_no_scope(&history, 1, 1003));
}

static void quick_draw_test(void)
{
	/* the killer's rifle hurt the victim, then the pistol finished: a quick draw */
	fresh();
	add_hit(1, 0, 100, TAG_RIFLE, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.6f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.4f, 1.0f, 0.4f, 1.0f);
	CHECK(ae_history_quick_draw(&history, 0, 1, 120, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET));
	CHECK(ae_history_quick_draw(&history, 0, 1, 120, TAG_PISTOL, AE_MEDAL_ROLE_PLASMA_PISTOL, AE_MEDAL_CATEGORY_PLASMA));
	/* the other weapon's last damage over 1.5 s ago */
	CHECK(ae_history_quick_draw(&history, 0, 1, 145, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET));
	CHECK(!ae_history_quick_draw(&history, 0, 1, 146, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET));
	/* not a pistol, a melee blow */
	CHECK(!ae_history_quick_draw(&history, 0, 1, 120, TAG_PISTOL, AE_MEDAL_ROLE_ASSAULT_RIFLE, AE_MEDAL_CATEGORY_BULLET));
	CHECK(!ae_history_quick_draw(&history, 0, 1, 120, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_MELEE));
	/* only the pistol's damage: no other weapon */
	fresh();
	add_hit(1, 0, 100, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.6f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.4f, 1.0f, 0.4f, 1.0f);
	CHECK(!ae_history_quick_draw(&history, 0, 1, 120, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET));
	/* the other weapon was a melee blow: not a weapon drawn */
	fresh();
	add_hit(1, 0, 100, TAG_MELEE, AE_MEDAL_CATEGORY_MELEE, AE_MEDAL_WEAPON_OTHER, 0, 0.6f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.4f, 1.0f, 0.4f, 1.0f);
	CHECK(!ae_history_quick_draw(&history, 0, 1, 120, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET));
	/* someone else did a tenth: the killer's 90 percent is short of 95 */
	fresh();
	add_hit(1, 0, 100, TAG_RIFLE, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.6f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 2, 105, TAG_RIFLE, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.2f, 0.0f, 0.4f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.2f, 1.0f, 0.2f, 1.0f);
	CHECK(!ae_history_quick_draw(&history, 0, 1, 120, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET));
}

static void reversal_test(void)
{
	/* player 0 kills 1. 1 struck 0 first at tick 100 (a third of 0's vitality), 0 struck back at 120, and finished */
	fresh();
	add_hit(0, 1, 100, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	add_hit(0, 1, 140, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.3f, 0.0f, 0.5f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 160, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.5f, 1.0f, 0.5f, 1.0f);
	CHECK(ae_history_reversal(&history, 0, 1));
	/* the victim struck first by exactly 0.9 s, and by more */
	fresh();
	add_hit(0, 1, 93, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(ae_history_reversal(&history, 0, 1));
	fresh();
	add_hit(0, 1, 92, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(!ae_history_reversal(&history, 0, 1));
	/* the killer struck first: not a reversal */
	fresh();
	add_hit(0, 1, 125, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(!ae_history_reversal(&history, 0, 1));
	/* the victim had done under a quarter by the time they were struck (the second blow came after) */
	fresh();
	add_hit(0, 1, 100, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.4f, 0.0f, 1.0f, 1.0f);
	add_hit(0, 1, 130, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.6f, 0.0f, 0.6f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(!ae_history_reversal(&history, 0, 1));
	/* exactly a quarter by then is enough; under it is not */
	fresh();
	add_hit(0, 1, 100, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(ae_history_reversal(&history, 0, 1));
	fresh();
	add_hit(0, 1, 100, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.4f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(!ae_history_reversal(&history, 0, 1));
	/* the killer did under 95 percent: another hand finished some */
	fresh();
	add_hit(0, 1, 100, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.8f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 2, 125, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.2f, 1.0f, 0.2f, 1.0f);
	CHECK(!ae_history_reversal(&history, 0, 1));
	/* the victim never struck the killer */
	fresh();
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(!ae_history_reversal(&history, 0, 1));
}

static void nade_shot_test(void)
{
	/* a grenade at tick 100, the gun at 130 and 140 to the head */
	fresh();
	add_hit(1, 0, 100, TAG_FRAG, AE_MEDAL_CATEGORY_GRENADE, AE_MEDAL_WEAPON_FRAG_GRENADE, 0, 0.7f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 130, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.3f, 0.0f, 0.3f, 1.0f);
	add_hit(1, 0, 140, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.0f, 1.0f, 0.0f, 1.0f);
	CHECK(ae_history_nade_shot(&history, 0, 1, 1));
	CHECK(!ae_history_nade_shot(&history, 0, 1, 0));
	/* the gap: the grenade's last damage to the gun's first, 2 s at most */
	fresh();
	add_hit(1, 0, 100, TAG_FRAG, AE_MEDAL_CATEGORY_GRENADE, AE_MEDAL_WEAPON_FRAG_GRENADE, 0, 0.7f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 160, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.3f, 1.0f, 0.3f, 1.0f);
	CHECK(ae_history_nade_shot(&history, 0, 1, 1));
	fresh();
	add_hit(1, 0, 100, TAG_FRAG, AE_MEDAL_CATEGORY_GRENADE, AE_MEDAL_WEAPON_FRAG_GRENADE, 0, 0.7f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 161, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.3f, 1.0f, 0.3f, 1.0f);
	CHECK(!ae_history_nade_shot(&history, 0, 1, 1));
	/* the gun first, the grenade after: no */
	fresh();
	add_hit(1, 0, 100, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.3f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 110, TAG_FRAG, AE_MEDAL_CATEGORY_GRENADE, AE_MEDAL_WEAPON_FRAG_GRENADE, 0, 0.7f, 0.0f, 0.7f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.0f, 1.0f, 0.0f, 1.0f);
	CHECK(!ae_history_nade_shot(&history, 0, 1, 1));
	/* two guns: no */
	fresh();
	add_hit(1, 0, 100, TAG_FRAG, AE_MEDAL_CATEGORY_GRENADE, AE_MEDAL_WEAPON_FRAG_GRENADE, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 110, TAG_RIFLE, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.2f, 0.0f, 0.5f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.3f, 1.0f, 0.3f, 1.0f);
	CHECK(!ae_history_nade_shot(&history, 0, 1, 1));
	/* no gun at all, no grenade at all, a melee blow in it */
	fresh();
	add_hit(1, 0, 100, TAG_FRAG, AE_MEDAL_CATEGORY_GRENADE, AE_MEDAL_WEAPON_FRAG_GRENADE, 1, 1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(!ae_history_nade_shot(&history, 0, 1, 1));
	fresh();
	add_hit(1, 0, 100, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(!ae_history_nade_shot(&history, 0, 1, 1));
	fresh();
	add_hit(1, 0, 100, TAG_FRAG, AE_MEDAL_CATEGORY_GRENADE, AE_MEDAL_WEAPON_FRAG_GRENADE, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 110, TAG_MELEE, AE_MEDAL_CATEGORY_MELEE, AE_MEDAL_WEAPON_OTHER, 0, 0.2f, 0.0f, 0.5f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.3f, 1.0f, 0.3f, 1.0f);
	CHECK(!ae_history_nade_shot(&history, 0, 1, 1));
	/* the killer did under 95 percent */
	fresh();
	add_hit(1, 2, 90, TAG_RIFLE, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.2f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 100, TAG_FRAG, AE_MEDAL_CATEGORY_GRENADE, AE_MEDAL_WEAPON_FRAG_GRENADE, 0, 0.5f, 0.0f, 0.8f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.3f, 1.0f, 0.3f, 1.0f);
	CHECK(!ae_history_nade_shot(&history, 0, 1, 1));
}

static void feed_turn(int player, long first, int count, float radians_a_tick)
{
	int index;

	for (index = 0; index < count; index++)
	{
		float yaw = radians_a_tick * (float)index;

		/* (the yaw as the game gives it: wrapped to -pi..pi) */
		yaw = fmodf(yaw + PI_F, 2.0f * PI_F);
		if (yaw < 0.0f)
			yaw += 2.0f * PI_F;
		yaw -= PI_F;
		ae_history_step(&history, player, first + index, yaw, 0, 0, 0, 0);
	}
}

static void turned_test(void)
{
	float full_in_window = 2.0f * PI_F / (float)AE_HISTORY_TURN_TICKS;

	/* a full turn over the 2.5 s window: either way */
	fresh();
	feed_turn(0, 1000, 76, full_in_window);
	CHECK(ae_history_turned(&history, 0, 1075));
	/* it starts over after one */
	CHECK(!ae_history_turned(&history, 0, 1075));
	fresh();
	feed_turn(0, 1000, 76, -full_in_window);
	CHECK(ae_history_turned(&history, 0, 1075));
	/* three quarters is 270 degrees, short of 288 */
	fresh();
	feed_turn(0, 1000, 76, 0.75f * full_in_window);
	CHECK(!ae_history_turned(&history, 0, 1075));
	/* 288 degrees and a little over: yes */
	fresh();
	feed_turn(0, 1000, 76, 0.82f * full_in_window);
	CHECK(ae_history_turned(&history, 0, 1075));
	/* a turn there and back adds to nothing */
	fresh();
	feed_turn(0, 1000, 38, full_in_window);
	feed_turn(0, 1038, 38, -full_in_window);
	CHECK(!ae_history_turned(&history, 0, 1075));
	/* a turn that ended before the window began is none */
	fresh();
	feed_turn(0, 1000, 76, full_in_window);
	ae_history_clear_player(&history, 0);
	feed_turn(0, 1000, 76, full_in_window);
	CHECK(!ae_history_turned(&history, 0, 1200));
	/* no samples: none */
	fresh();
	CHECK(!ae_history_turned(&history, 0, 1000));
}

/* the leap: a killer walks in from the front, takes off, passes over the victim and lands behind */
static void feed_leap(int player, long takeoff, float over_x, float over_y, float height_over)
{
	long tick;

	/* the victim at the origin, facing +x. On the ground in front, from x = 3 */
	for (tick = takeoff - 20; tick < takeoff; tick++)
		ae_history_step(&history, player, tick, 0, 3.0f, 0.0f, 0.0f, 0);
	for (tick = takeoff; tick < takeoff + 9; tick++)
	{
		float t = (float)(tick - takeoff) / 8.0f;

		/* from x = 3 to x = -3 over 9 samples; the highest at the middle, over the victim at t = 0.5 */
		ae_history_step(&history, player, tick, 0, 3.0f - 6.0f * t + over_x, over_y,
			height_over * 4.0f * t * (1.0f - t) + 0.01f, 1);
	}
	for (tick = takeoff + 9; tick < takeoff + 16; tick++)
		ae_history_step(&history, player, tick, 0, -3.0f, 0.0f, 0.0f, 0);
}

static void ninja_test(void)
{
	/* lands at takeoff + 9 = 1009; the kill 3 ticks after */
	fresh();
	feed_leap(0, 1000, 0.0f, 0.0f, 1.0f);
	CHECK(ae_history_ninja(&history, 0, 1012, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	/* the kill 15 ticks after the landing, and 16 */
	CHECK(ae_history_ninja(&history, 0, 1024, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	CHECK(!ae_history_ninja(&history, 0, 1025, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	/* it passed too far to the side (0.2 over 0.16) */
	fresh();
	feed_leap(0, 1000, 0.0f, 0.2f, 1.0f);
	CHECK(!ae_history_ninja(&history, 0, 1012, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	/* too high over (the arc peaks 1.4 over, past the 1.3 band) */
	fresh();
	feed_leap(0, 1000, 0.0f, 0.0f, 1.4f);
	CHECK(!ae_history_ninja(&history, 0, 1012, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	/* the victim faces the other way: the leap began behind them */
	fresh();
	feed_leap(0, 1000, 0.0f, 0.0f, 1.0f);
	CHECK(!ae_history_ninja(&history, 0, 1012, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f));
	/* still in the air at the kill: not landed */
	fresh();
	feed_leap(0, 1000, 0.0f, 0.0f, 1.0f);
	CHECK(!ae_history_ninja(&history, 0, 1004, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	/* never in the air */
	fresh();
	{
		long tick;

		for (tick = 1000; tick < 1030; tick++)
			ae_history_step(&history, 0, tick, 0, 3.0f, 0.0f, 0.0f, 0);
	}
	CHECK(!ae_history_ninja(&history, 0, 1030, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	/* a victim who is above or below the whole leap */
	fresh();
	feed_leap(0, 1000, 0.0f, 0.0f, 1.0f);
	CHECK(!ae_history_ninja(&history, 0, 1012, 0.0f, 0.0f, 3.0f, 1.0f, 0.0f));
	CHECK(!ae_history_ninja(&history, 0, 1012, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
}

static void rack_test(void)
{
	fresh();
	ae_history_spawned(&history, 777);
	ae_history_picked(&history, 0, 777, 1000);
	CHECK(ae_history_off_the_rack(&history, 0, 777, 1000));
	CHECK(ae_history_off_the_rack(&history, 0, 777, 1082));
	CHECK(!ae_history_off_the_rack(&history, 0, 777, 1083));
	/* another weapon, another player, before the pickup */
	CHECK(!ae_history_off_the_rack(&history, 0, 778, 1010));
	CHECK(!ae_history_off_the_rack(&history, 1, 777, 1010));
	CHECK(!ae_history_off_the_rack(&history, 0, 777, 999));
	/* a weapon the map's spawner did not make (a dropped one) */
	ae_history_picked(&history, 1, 555, 1000);
	CHECK(!ae_history_off_the_rack(&history, 1, 555, 1010));
	/* a weapon taken from a spawner and dropped, then taken again by another, is not from the spawner the second time */
	ae_history_picked(&history, 2, 777, 1500);
	CHECK(!ae_history_off_the_rack(&history, 2, 777, 1510));
	/* the spawner makes the same object index again: from the spawner again */
	ae_history_spawned(&history, 777);
	ae_history_picked(&history, 2, 777, 2000);
	CHECK(ae_history_off_the_rack(&history, 2, 777, 2010));
	/* many spawns push the oldest out */
	{
		long weapon;

		fresh();
		for (weapon = 1; weapon <= AE_HISTORY_SPAWNED + 1; weapon++)
			ae_history_spawned(&history, weapon);
		ae_history_picked(&history, 0, 1, 100);
		CHECK(!ae_history_off_the_rack(&history, 0, 1, 110));
		ae_history_picked(&history, 0, AE_HISTORY_SPAWNED + 1, 100);
		CHECK(ae_history_off_the_rack(&history, 0, AE_HISTORY_SPAWNED + 1, 110));
	}
	/* more pickups than the ring holds: the newest stay */
	fresh();
	{
		long weapon;

		for (weapon = 1; weapon <= 10; weapon++)
		{
			ae_history_spawned(&history, weapon);
			ae_history_picked(&history, 0, weapon, 100 + weapon);
		}
		CHECK(ae_history_off_the_rack(&history, 0, 10, 120));
		CHECK(!ae_history_off_the_rack(&history, 0, 1, 120));
	}
}

static void required_test(void)
{
	struct ae_perfect_damage pistol = { 25.0f, 75.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	struct ae_perfect_damage carbine = { 15.2f, 75.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	struct ae_perfect_damage damage;

	/* the pistol: the third round breaks the shield with a hair left over (the floats), and the head takes it */
	CHECK(ae_perfect_required_hits(&pistol) == 3);
	/* the carbine: 5 rounds of 15.2 are 76 against 75 */
	CHECK(ae_perfect_required_hits(&carbine) == 5);
	/* a round of exactly the shield empties it and the next one goes through to the body */
	damage = pistol;
	damage.damage = 75.0f;
	CHECK(ae_perfect_required_hits(&damage) == 2);
	damage.damage = 76.0f;
	CHECK(ae_perfect_required_hits(&damage) == 1);
	damage.damage = 100.0f;
	CHECK(ae_perfect_required_hits(&damage) == 1);
	/* a modifier for the shield's material, and the head's multiplier */
	damage = pistol;
	damage.shield_modifier = 2.0f;
	CHECK(ae_perfect_required_hits(&damage) == 2);
	damage = pistol;
	damage.shield_multiplier = 0.5f;
	CHECK(ae_perfect_required_hits(&damage) == 6);
	/* what leaks to the body does not make a clean count; no body damage on the head: no kill */
	damage = pistol;
	damage.leak = 0.1f;
	CHECK(ae_perfect_required_hits(&damage) == 0);
	damage = pistol;
	damage.body_multiplier = 0.0f;
	CHECK(ae_perfect_required_hits(&damage) == 0);
	damage = pistol;
	damage.damage = 0.0f;
	CHECK(ae_perfect_required_hits(&damage) == 0);
	damage.damage = -3.0f;
	CHECK(ae_perfect_required_hits(&damage) == 0);
	damage = pistol;
	damage.shield_max = 0.0f;
	CHECK(ae_perfect_required_hits(&damage) == 0);
	damage = pistol;
	damage.damage = NAN;
	CHECK(ae_perfect_required_hits(&damage) == 0);
	damage = pistol;
	damage.shield_max = INFINITY;
	CHECK(ae_perfect_required_hits(&damage) == 0);
	/* a round too weak to ever break a shield in the rounds counted */
	damage = pistol;
	damage.damage = 0.001f;
	CHECK(ae_perfect_required_hits(&damage) == 0);
	CHECK(ae_perfect_required_hits(NULL) == 0);
}

/* a pistol kill: shots at 100, 110, 120, hits two ticks later, the third to the head */
static struct ae_perfect pistol_kill(void)
{
	struct ae_perfect perfect;

	memset(&perfect, 0, sizeof(perfect));
	perfect.killer = 0;
	perfect.victim = 1;
	perfect.tick = 122;
	perfect.required = 3;
	perfect.fastest_ticks = 20;
	perfect.weapon = TAG_PISTOL;
	perfect.flight_ticks = AE_HISTORY_PERFECT_FLIGHT_TICKS;
	perfect.multiplier_ok = 1;
	perfect.head_kill = 1;
	return perfect;
}

static void pistol_shots(void)
{
	ae_history_shot(&history, 0, 100, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	ae_history_shot(&history, 0, 110, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	ae_history_shot(&history, 0, 120, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
}

/* the pistol's rounds and one more at the tick, the rounds in the order the game fires them */
static void pistol_shots_and(long extra)
{
	long ticks[4] = { 100, 110, 120, extra };
	int index, other;

	for (index = 0; index < 4; index++)
		for (other = index + 1; other < 4; other++)
			if (ticks[other] < ticks[index])
			{
				long swap = ticks[index];

				ticks[index] = ticks[other];
				ticks[other] = swap;
			}
	for (index = 0; index < 4; index++)
		ae_history_shot(&history, 0, ticks[index], TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
}

static void pistol_hits(void)
{
	add_hit(1, 0, 102, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 112, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 0.66666666f, 1.0f);
	add_hit(1, 0, 122, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.33333326f, 1.0f, 0.33333326f, 1.0f);
}

static void perfect_test(void)
{
	struct ae_perfect perfect = pistol_kill();
	int hits = -1, shots = -1;

	fresh();
	pistol_shots();
	pistol_hits();
	CHECK(ae_history_perfect(&history, &perfect, &hits, &shots));
	CHECK(hits == 3 && shots == 3);
	CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));

	/* the same with the shots and hits in the log of a different life: nothing */
	CHECK(!ae_history_perfect(&history, NULL, NULL, NULL));

	/* a miss between the rounds */
	fresh();
	pistol_shots_and(105);
	pistol_hits();
	CHECK(!ae_history_perfect(&history, &perfect, &hits, &shots));
	CHECK(hits == 3 && shots == 4);
	/* a miss before the first round, as far back as the window goes: still a miss; further back: no part of it */
	fresh();
	pistol_shots_and(94);
	pistol_hits();
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	fresh();
	pistol_shots_and(90);
	pistol_hits();
	CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));
	/* a round fired after the killing one, while its hit travelled: not a miss */
	fresh();
	pistol_shots_and(121);
	pistol_hits();
	CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));
	/* a miss no hit could have come from (the hit at 122 is from a round fired 8 ticks before it or less) */
	fresh();
	pistol_shots_and(113);
	pistol_hits();
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	/* a round that could be the one that hit: the record cannot say otherwise */
	fresh();
	pistol_shots_and(116);
	pistol_hits();
	CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));
	/* a player on another machine: their rounds come to the host later, a slower hit is allowed */
	fresh();
	pistol_shots_and(113);
	pistol_hits();
	perfect.flight_ticks = AE_HISTORY_PERFECT_FLIGHT_TICKS + AE_HISTORY_PERFECT_REMOTE_TICKS;
	CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));
	perfect = pistol_kill();
	/* several rounds after the killing one */
	fresh();
	pistol_shots();
	ae_history_shot(&history, 0, 121, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	ae_history_shot(&history, 0, 121, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	pistol_hits();
	CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));
	/* fewer rounds than hits: the record is short, no perfect on a guess */
	fresh();
	ae_history_shot(&history, 0, 100, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	ae_history_shot(&history, 0, 120, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	pistol_hits();
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	/* a round of many pellets */
	fresh();
	pistol_shots();
	ae_history_shot(&history, 0, 121, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 8);
	pistol_hits();
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	/* a round of another weapon in the window */
	fresh();
	pistol_shots();
	ae_history_shot(&history, 0, 121, TAG_RIFLE, AE_MEDAL_ROLE_ASSAULT_RIFLE, 0, 1);
	pistol_hits();
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));

	/* too slow: the fastest time 20 ticks, half again and 15 ticks over is 45 */
	fresh();
	ae_history_shot(&history, 0, 100, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	ae_history_shot(&history, 0, 123, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	ae_history_shot(&history, 0, 146, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	add_hit(1, 0, 102, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 125, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 0.66666666f, 1.0f);
	add_hit(1, 0, 147, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.33333326f, 1.0f, 0.33333326f, 1.0f);
	perfect.tick = 147;
	/* 45 ticks from the first damage: the edge, in */
	CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));
	perfect.tick = 148;
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	perfect = pistol_kill();

	/* the victim started short of full: shield, body, or charged past it */
	fresh();
	pistol_shots();
	add_hit(1, 0, 102, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 0.99f, 1.0f);
	add_hit(1, 0, 112, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 0.66f, 1.0f);
	add_hit(1, 0, 122, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.33333326f, 1.0f, 0.33f, 1.0f);
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	fresh();
	pistol_shots();
	add_hit(1, 0, 102, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 1.0f, 0.99f);
	add_hit(1, 0, 112, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 0.66666666f, 0.99f);
	add_hit(1, 0, 122, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.33333326f, 1.0f, 0.33333326f, 0.99f);
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	fresh();
	pistol_shots();
	add_hit(1, 0, 102, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 1.5f, 1.0f);
	add_hit(1, 0, 112, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 1.1f, 1.0f);
	add_hit(1, 0, 122, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.33333326f, 1.0f, 0.5f, 1.0f);
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));

	/* chip damage from another hand: under a tenth of the whole is forgiven (it comes first, so the start is not full:
	the chip's own start counts) */
	{
		/* a chip of 0.05 of the whole, between the pistol's rounds */
		struct ae_history_hit chip = make_hit(2, 105, TAG_RIFLE, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.0f, 0.0f);

		chip.taken = 0.05f;
		fresh();
		pistol_shots();
		add_hit(1, 0, 102, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 1.0f, 1.0f);
		ae_history_hit(&history, 1, &chip, 0.66666666f, 1.0f);
		add_hit(1, 0, 112, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 0.66666666f, 1.0f);
		add_hit(1, 0, 122, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.33333326f, 1.0f, 0.33333326f, 1.0f);
		CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));
		chip.taken = 0.10f;
		fresh();
		pistol_shots();
		add_hit(1, 0, 102, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 1.0f, 1.0f);
		ae_history_hit(&history, 1, &chip, 0.66666666f, 1.0f);
		add_hit(1, 0, 112, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 0.66666666f, 1.0f);
		add_hit(1, 0, 122, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.33333326f, 1.0f, 0.33333326f, 1.0f);
		CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	}

	/* two damages by the killer: the rounds are of two kinds */
	fresh();
	pistol_shots();
	add_hit(1, 0, 102, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 112, TAG_RIFLE, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 0.66666666f, 1.0f);
	add_hit(1, 0, 122, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.33333326f, 1.0f, 0.33333326f, 1.0f);
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	/* a hit fewer than the count asks (a round that took more): the count is the rule's */
	fresh();
	ae_history_shot(&history, 0, 100, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	ae_history_shot(&history, 0, 110, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, 0, 1);
	add_hit(1, 0, 102, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.6f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 112, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.4f, 1.0f, 0.4f, 1.0f);
	perfect.tick = 112;
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	perfect = pistol_kill();
	/* a hit more than asked */
	fresh();
	pistol_shots_and(90);
	add_hit(1, 0, 92, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.1f, 0.0f, 1.0f, 1.0f);
	pistol_hits();
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));

	/* the other flags: not a head kill, a sniper's, the multiplier, no count, no time */
	fresh();
	pistol_shots();
	pistol_hits();
	perfect.head_kill = 0;
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	perfect = pistol_kill();
	perfect.sniper = 1;
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	perfect = pistol_kill();
	perfect.multiplier_ok = 0;
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	perfect = pistol_kill();
	perfect.required = 0;
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	perfect = pistol_kill();
	perfect.fastest_ticks = -1;
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	perfect = pistol_kill();
	/* the killing blow was not to the head */
	fresh();
	pistol_shots();
	add_hit(1, 0, 102, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 112, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333334f, 0.0f, 0.66666666f, 1.0f);
	add_hit(1, 0, 122, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.33333326f, 1.0f, 0.33333326f, 1.0f);
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	/* a victim with no log (a record lost) */
	fresh();
	pistol_shots();
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	/* a log that overflowed */
	fresh();
	pistol_shots();
	pistol_hits();
	history.players[1].life.overflow = 1;
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	/* the killer's slot is out of range */
	fresh();
	perfect.killer = -1;
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
	perfect.killer = 0;
	perfect.victim = 999;
	CHECK(!ae_history_perfect(&history, &perfect, NULL, NULL));
}

/* the battle rifle: 4 bursts of 3, every second tick a round, a burst every 14 ticks; the 12th round is the kill */
static void burst_test(void)
{
	struct ae_perfect perfect;
	int burst, round, round_count = 0;
	long first_round = 1000;

	memset(&perfect, 0, sizeof(perfect));
	perfect.killer = 0;
	perfect.victim = 1;
	perfect.required = 12;
	perfect.fastest_ticks = 3 * 14 + 4;
	perfect.weapon = TAG_RIFLE;
	perfect.multiplier_ok = 1;
	perfect.head_kill = 1;
	perfect.flight_ticks = AE_HISTORY_PERFECT_FLIGHT_TICKS;
	fresh();
	for (burst = 0; burst < 4; burst++)
	{
		for (round = 0; round < 3; round++)
		{
			long tick = first_round + burst * 14 + round * 2;

			round_count++;
			ae_history_shot(&history, 0, tick, TAG_RIFLE, AE_MEDAL_ROLE_BATTLE_RIFLE, 0, 1);
			add_hit(1, 0, tick + 2, TAG_RIFLE, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, round_count == 12,
				round_count < 12 ? 0.08f : 0.1f, round_count == 12 ? 1.0f : 0.0f, round_count == 1 ? 1.0f : 0.5f, 1.0f);
			perfect.tick = tick + 2;
		}
	}
	CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));
	/* a 13th round fired in the tail, after the kill's: the burst finishing */
	ae_history_shot(&history, 0, perfect.tick - 1, TAG_RIFLE, AE_MEDAL_ROLE_BATTLE_RIFLE, 0, 1);
	CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));
	/* the killing round the first of a burst, the other two coming after it */
	fresh();
	round_count = 0;
	for (burst = 0; burst < 4; burst++)
	{
		for (round = 0; round < 3; round++)
		{
			long tick = first_round + burst * 14 + round * 2;

			if (burst == 3 && round > 0)
			{
				ae_history_shot(&history, 0, tick, TAG_RIFLE, AE_MEDAL_ROLE_BATTLE_RIFLE, 0, 1);
				continue;
			}
			round_count++;
			ae_history_shot(&history, 0, tick, TAG_RIFLE, AE_MEDAL_ROLE_BATTLE_RIFLE, 0, 1);
			add_hit(1, 0, tick + 2, TAG_RIFLE, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, round_count == 10,
				0.08f, round_count == 10 ? 1.0f : 0.0f, round_count == 1 ? 1.0f : 0.5f, 1.0f);
			perfect.tick = tick + 2;
		}
	}
	perfect.required = 10;
	perfect.fastest_ticks = 3 * 14;
	/* (the two rounds after the killing one sit inside the burst tail) */
	CHECK(ae_history_perfect(&history, &perfect, NULL, NULL));
}

static void overflow_and_gap_test(void)
{
	int index;

	/* a life log that lost blows is read by no question */
	fresh();
	add_hit(1, 0, 100, TAG_RIFLE, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.6f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 120, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.4f, 1.0f, 0.4f, 1.0f);
	add_hit(0, 1, 95, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 0, 0.5f, 0.0f, 1.0f, 1.0f);
	CHECK(ae_history_quick_draw(&history, 0, 1, 120, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET));
	CHECK(ae_history_reversal(&history, 0, 1));
	history.players[1].life.overflow = 1;
	CHECK(!ae_history_quick_draw(&history, 0, 1, 120, TAG_PISTOL, AE_MEDAL_ROLE_PISTOL, AE_MEDAL_CATEGORY_BULLET));
	CHECK(!ae_history_reversal(&history, 0, 1));
	CHECK(ae_history_taken_by(&history, 1, 0) == 0.0f);
	history.players[1].life.overflow = 0;
	history.players[0].life.overflow = 1;
	CHECK(!ae_history_reversal(&history, 0, 1));
	fresh();
	add_hit(1, 0, 100, TAG_FRAG, AE_MEDAL_CATEGORY_GRENADE, AE_MEDAL_WEAPON_FRAG_GRENADE, 0, 0.7f, 0.0f, 1.0f, 1.0f);
	add_hit(1, 0, 140, TAG_PISTOL, AE_MEDAL_CATEGORY_BULLET, AE_MEDAL_WEAPON_OTHER, 1, 0.3f, 1.0f, 0.3f, 1.0f);
	CHECK(ae_history_nade_shot(&history, 0, 1, 1));
	history.players[1].life.overflow = 1;
	CHECK(!ae_history_nade_shot(&history, 0, 1, 1));

	/* the steps of a past life are cleared, and a gap in the samples is no leap */
	fresh();
	feed_leap(0, 1000, 0.0f, 0.0f, 1.0f);
	CHECK(ae_history_ninja(&history, 0, 1012, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	ae_history_steps_clear(&history, 0);
	CHECK(!ae_history_ninja(&history, 0, 1012, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	CHECK(!ae_history_turned(&history, 0, 1012));
	ae_history_steps_clear(&history, -1);
	ae_history_steps_clear(&history, 999);
	fresh();
	for (index = 0; index < 20; index++)
		ae_history_step(&history, 0, 900 + index, 0, 3.0f, 0.0f, 0.0f, 0);
	/* (died in the air; the new life's first steps are on the ground, long after) */
	for (index = 0; index < 9; index++)
		ae_history_step(&history, 0, 920 + index, 0, 3.0f - 0.75f * index, 0.0f, 0.5f, 1);
	for (index = 0; index < 8; index++)
		ae_history_step(&history, 0, 1100 + index, 0, -3.0f, 0.0f, 0.0f, 0);
	CHECK(!ae_history_ninja(&history, 0, 1105, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
}

int main(void)
{
	ring_test();
	life_test();
	no_scope_test();
	quick_draw_test();
	reversal_test();
	nade_shot_test();
	turned_test();
	ninja_test();
	rack_test();
	required_test();
	perfect_test();
	burst_test();
	overflow_and_gap_test();
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_medal_history_test: ok\n");
	return 0;
}
