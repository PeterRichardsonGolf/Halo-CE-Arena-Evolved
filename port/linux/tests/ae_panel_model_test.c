/* HUD STYLE AE's panel model (port/linux/game/ae_panel_model.c): what the ammo, grenade, shield and health panels
say; spec 14.15 parts 2, 3 and 5, plan Task 11 and Review Focus 6 and 7 */
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "ae_panel_model.h"

static int failures;
static int checks;
#define AE_CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } \
	while (0)
#define AE_CHECK_WSTR(a, b) do { checks++; if (wcscmp((a), (b)) != 0) { printf("FAIL %s:%d \"%ls\" != \"%ls\"\n", \
	__FILE__, __LINE__, (a), (b)); failures++; } } while (0)
#define AE_NEAR(a, b) ((a) > (b) - 0.001f && (a) < (b) + 0.001f)

/* a live player with full shield and health, a weapon of this hint and magazine */
static void base(struct ae_panel_snapshot *s, short hint, short magazine, short loaded, short reserve)
{
	memset(s, 0, sizeof(*s));
	s->has_weapon = TRUE;
	s->weapon_uses_rounds = magazine > 0;
	s->weapon_type_hint = hint;
	s->rounds_per_magazine = magazine;
	s->rounds_loaded = loaded;
	s->rounds_reserve = reserve;
	s->grenades[0] = 2; s->grenades[1] = 1; s->armed_grenade = 0;
	s->shield = 1.0f; s->health = 1.0f;
}

static void battery(struct ae_panel_snapshot *s, real charge, boolean heat)
{
	memset(s, 0, sizeof(*s));
	s->has_weapon = TRUE;
	s->weapon_has_battery = TRUE;
	s->weapon_has_heat = heat;
	s->weapon_type_hint = _ae_hint_none;
	s->battery = charge;
	s->shield = 1.0f; s->health = 1.0f;
}

static int rows_are(const struct ae_panel_model *m, short count, short r0, short r1, short r2)
{
	return m->row_count == count && (count < 1 || m->rows[0] == r0) && (count < 2 || m->rows[1] == r1) &&
		(count < 3 || m->rows[2] == r2);
}

/* one case per row of 14.15's weapon table */
static void test_weapon_table(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m;

	base(&s, _ae_hint_assault_rifle, 60, 60, 180); ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_rounds); AE_CHECK_WSTR(m.number, L"180"); AE_CHECK(m.glyph == _ae_glyph_round);
	AE_CHECK(m.art == _ae_art_ar); AE_CHECK(rows_are(&m, 3, 20, 20, 20)); AE_CHECK(!m.bar_shown); AE_CHECK(!m.lying);
	AE_CHECK(m.lit == 60);

	base(&s, _ae_hint_pistol, 12, 12, 36); ae_panel_model_build(&s, &m);
	AE_CHECK_WSTR(m.number, L"036"); AE_CHECK(m.art == _ae_art_pistol); AE_CHECK(rows_are(&m, 1, 12, 0, 0));
	AE_CHECK(m.glyph == _ae_glyph_round); AE_CHECK(!m.bar_shown);

	base(&s, _ae_hint_shotgun, 12, 12, 4); ae_panel_model_build(&s, &m);
	AE_CHECK_WSTR(m.number, L"004"); AE_CHECK(m.art == _ae_art_shell); AE_CHECK(rows_are(&m, 1, 12, 0, 0));

	base(&s, _ae_hint_needler, 20, 20, 60); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_needle); AE_CHECK(rows_are(&m, 1, 20, 0, 0));

	base(&s, _ae_hint_sniper, 4, 4, 8); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_sniper); AE_CHECK(rows_are(&m, 2, 2, 2, 0)); AE_CHECK(m.lying);
	AE_CHECK_WSTR(m.number, L"008");

	base(&s, _ae_hint_rocket, 2, 2, 0); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_rocket); AE_CHECK(rows_are(&m, 1, 2, 0, 0)); AE_CHECK(m.lying);
	AE_CHECK_WSTR(m.number, L"000"); AE_CHECK(m.glyph == _ae_glyph_round);

	base(&s, _ae_hint_battle_rifle, 36, 36, 108); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_ar); AE_CHECK(rows_are(&m, 2, 18, 18, 0)); AE_CHECK_WSTR(m.number, L"108");

	base(&s, _ae_hint_flamethrower, 100, 75, 200); ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_tank); AE_CHECK_WSTR(m.number, L"200"); AE_CHECK(m.glyph == _ae_glyph_none);
	AE_CHECK(m.row_count == 0); AE_CHECK(m.bar_shown); AE_CHECK(AE_NEAR(m.bar_fill, 0.75f)); AE_CHECK(m.art == _ae_art_bar);

	battery(&s, 0.87f, TRUE); s.heat = 0.4f; ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_battery); AE_CHECK_WSTR(m.number, L"087");
	AE_CHECK(m.glyph == _ae_glyph_battery); AE_CHECK(m.bar_shown); AE_CHECK(AE_NEAR(m.bar_fill, 0.4f));
	AE_CHECK(m.row_count == 0);

	battery(&s, 1.0f, TRUE); ae_panel_model_build(&s, &m); AE_CHECK_WSTR(m.number, L"100");
	battery(&s, 0.0f, TRUE); ae_panel_model_build(&s, &m); AE_CHECK_WSTR(m.number, L"000");
	battery(&s, 0.13f, TRUE); ae_panel_model_build(&s, &m); AE_CHECK_WSTR(m.number, L"013");

	/* the stock fuel rod (PC / Custom Edition: a battery weapon): drawn exactly as plasma */
	battery(&s, 0.5f, TRUE); ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_battery); AE_CHECK_WSTR(m.number, L"050");

	/* the community maps' rounds fuel rod: 3 + 2 lying rocket-size shells */
	base(&s, _ae_hint_fuel_rod_rounds, 5, 5, 15); ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_rounds); AE_CHECK(m.art == _ae_art_rocket); AE_CHECK(m.lying);
	AE_CHECK(rows_are(&m, 2, 3, 2, 0));

	/* energy sword (pack): battery, no bar */
	battery(&s, 0.6f, FALSE); ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_battery_no_heat); AE_CHECK_WSTR(m.number, L"060");
	AE_CHECK(m.glyph == _ae_glyph_battery); AE_CHECK(!m.bar_shown);
}

/* the unknown-weapon rule (Review Focus 6): by magazine size; never more than the layout can hold */
static void test_unknown_weapons(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m;

	base(&s, _ae_hint_none, 2, 2, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_rocket && m.lying && rows_are(&m, 1, 2, 0, 0));
	base(&s, _ae_hint_none, 1, 1, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_rocket && rows_are(&m, 1, 1, 0, 0));
	base(&s, _ae_hint_none, 4, 4, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_sniper && rows_are(&m, 2, 2, 2, 0));
	base(&s, _ae_hint_none, 5, 5, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_sniper && rows_are(&m, 2, 3, 2, 0));
	base(&s, _ae_hint_none, 3, 3, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_sniper && rows_are(&m, 2, 2, 1, 0));
	base(&s, _ae_hint_none, 6, 6, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_sniper && rows_are(&m, 2, 3, 3, 0));
	base(&s, _ae_hint_none, 7, 7, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_pistol && rows_are(&m, 1, 7, 0, 0) && !m.lying);
	base(&s, _ae_hint_none, 16, 16, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_pistol && rows_are(&m, 1, 16, 0, 0));
	base(&s, _ae_hint_none, 17, 17, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_ar && rows_are(&m, 1, 17, 0, 0));
	base(&s, _ae_hint_none, 25, 25, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_ar && rows_are(&m, 2, 13, 12, 0));
	base(&s, _ae_hint_none, 36, 36, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_ar && rows_are(&m, 2, 18, 18, 0));
	base(&s, _ae_hint_none, 60, 60, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_ar && rows_are(&m, 3, 20, 20, 20));
	base(&s, _ae_hint_none, 61, 30, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_tank && m.row_count == 0 && m.bar_shown && m.art == _ae_art_bar);
	AE_CHECK(m.bar_fill > 0.49f && m.bar_fill < 0.50f);
	base(&s, _ae_hint_none, 200, 250, 4); ae_panel_model_build(&s, &m);   /* loaded over the magazine: clamped */
	AE_CHECK(m.weapon_kind == _ae_panel_tank && AE_NEAR(m.bar_fill, 1.0f));
	/* magazine 0: a rounds weapon with no magazine (rounds straight from the reserve): the number, no rounds */
	base(&s, _ae_hint_none, 0, 0, 12); s.weapon_uses_rounds = TRUE; ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_rounds && m.row_count == 0 && !m.bar_shown); AE_CHECK_WSTR(m.number, L"012");
	/* a hint whose magazine is not the stock one: the hint's art, the magazine's real size */
	base(&s, _ae_hint_assault_rifle, 32, 32, 4); ae_panel_model_build(&s, &m);
	AE_CHECK(m.art == _ae_art_ar && rows_are(&m, 2, 16, 16, 0));
	base(&s, _ae_hint_pistol, 40, 40, 4); ae_panel_model_build(&s, &m);   /* pistol art, too many for one row */
	AE_CHECK(m.art == _ae_art_pistol && rows_are(&m, 2, 20, 20, 0));
	base(&s, _ae_hint_sniper, 12, 12, 4); ae_panel_model_build(&s, &m);   /* lying: never wider than 2 rows of 6 */
	AE_CHECK(m.art == _ae_art_sniper && rows_are(&m, 2, 6, 6, 0));
	base(&s, _ae_hint_rocket, 60, 60, 4); ae_panel_model_build(&s, &m);   /* lying rockets: past 6 they become AR */
	AE_CHECK(m.art == _ae_art_ar && rows_are(&m, 3, 20, 20, 20));
	base(&s, _ae_hint_assault_rifle, 90, 30, 4); ae_panel_model_build(&s, &m);  /* over 60 always a bar */
	AE_CHECK(m.weapon_kind == _ae_panel_tank);
	/* heat AND rounds: the magazine and the heat bar */
	base(&s, _ae_hint_none, 12, 6, 4); s.weapon_has_heat = TRUE; s.heat = 0.5f; ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_rounds && m.row_count == 1 && m.bar_shown && AE_NEAR(m.bar_fill, 0.5f));
	/* (a Custom Edition map's rifle: a little heat a round, none now: no bar, owner 10-15) */
	base(&s, _ae_hint_none, 36, 36, 4); s.weapon_has_heat = TRUE; s.heat = 0.0f; ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_rounds && !m.bar_shown);
	/* heat only (no rounds, no battery): the bar, no number */
	memset(&s, 0, sizeof(s)); s.has_weapon = TRUE; s.weapon_has_heat = TRUE; s.heat = 0.25f; s.shield = 1; s.health = 1;
	s.weapon_type_hint = _ae_hint_none; ae_panel_model_build(&s, &m);
	AE_CHECK(m.bar_shown && m.number[0] == 0 && m.glyph == _ae_glyph_none && m.row_count == 0);
	/* negative / odd numbers never crash and never show garbage */
	base(&s, _ae_hint_none, -3, -1, -5); ae_panel_model_build(&s, &m);
	AE_CHECK(m.row_count == 0); AE_CHECK_WSTR(m.number, L"000");
}

static void test_reserve_numbers(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m;
	static const short reserves[] = { 0, 4, 36, 180, 999, 1000, 32767 };
	static const wchar_t *texts[] = { L"000", L"004", L"036", L"180", L"999", L"999", L"999" };
	int i;

	for (i = 0; i < (int)(sizeof(reserves) / sizeof(reserves[0])); i++)
	{
		base(&s, _ae_hint_assault_rifle, 60, 60, reserves[i]); ae_panel_model_build(&s, &m);
		AE_CHECK_WSTR(m.number, texts[i]);
	}
}

static void test_states(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m;

	/* low ammo: red only on the flash's ON frames (the snapshot says CE's cutoff test passed and the phase is on) */
	base(&s, _ae_hint_assault_rifle, 60, 8, 180); s.low_ammo_flash_on = TRUE; ae_panel_model_build(&s, &m);
	AE_CHECK(m.rounds_red); AE_CHECK(m.lit == 8);
	s.low_ammo_flash_on = FALSE; ae_panel_model_build(&s, &m); AE_CHECK(!m.rounds_red);
	/* empty magazine: all dim, the number unchanged, nothing red */
	base(&s, _ae_hint_assault_rifle, 60, 0, 180); s.low_ammo_flash_on = TRUE; ae_panel_model_build(&s, &m);
	AE_CHECK(m.lit == 0 && m.row_lit[0] == 0 && m.row_lit[1] == 0 && m.row_lit[2] == 0); AE_CHECK(!m.rounds_red);
	AE_CHECK_WSTR(m.number, L"180");
	/* overheated: the bar red */
	battery(&s, 0.5f, TRUE); s.heat = 1.0f; s.overheated = TRUE; ae_panel_model_build(&s, &m);
	AE_CHECK(m.bar_red && AE_NEAR(m.bar_fill, 1.0f));
	s.overheated = FALSE; ae_panel_model_build(&s, &m); AE_CHECK(!m.bar_red);
	/* a reserve of zero is not red; a loaded rocket is not red */
	base(&s, _ae_hint_rocket, 2, 2, 0); ae_panel_model_build(&s, &m); AE_CHECK(!m.rounds_red);
}

/* Review Focus 7: no weapon, a seat with no weapon: the ammo group says nothing */
static void test_no_weapon(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m;

	base(&s, _ae_hint_assault_rifle, 60, 30, 180); s.has_weapon = FALSE; ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_none); AE_CHECK(m.number[0] == 0); AE_CHECK(m.row_count == 0);
	AE_CHECK(!m.bar_shown); AE_CHECK(!m.grenade[0].shown && !m.grenade[1].shown);
	AE_CHECK(m.glyph == _ae_glyph_none);
	/* the shield group still speaks */
	AE_CHECK(AE_NEAR(m.shield_fill, 1.0f) && m.health_cells_lit == 8);

	base(&s, _ae_hint_none, 0, 0, 0); s.has_weapon = FALSE; s.in_seat = TRUE; s.seat_row_count = 0;
	ae_panel_model_build(&s, &m); AE_CHECK(m.weapon_kind == _ae_panel_none && m.seat_count == 0);

	/* a model reused across frames: nothing from the last weapon survives */
	base(&s, _ae_hint_assault_rifle, 60, 30, 180); ae_panel_model_build(&s, &m);
	s.has_weapon = FALSE; ae_panel_model_build(&s, &m);
	AE_CHECK(m.lit == 0 && m.rows[0] == 0 && m.number[0] == 0);
}

static void test_grenades(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m;

	base(&s, _ae_hint_assault_rifle, 60, 60, 180);
	s.grenades[0] = 0; s.grenades[1] = 0; ae_panel_model_build(&s, &m);
	AE_CHECK_WSTR(m.grenade[0].count, L"0"); AE_CHECK_WSTR(m.grenade[1].count, L"0");
	AE_CHECK(m.grenade[0].zero && m.grenade[1].zero); AE_CHECK(m.grenade[0].armed && !m.grenade[1].armed);
	AE_CHECK(m.grenade[0].shown && m.grenade[1].shown);

	s.grenades[0] = 2; s.grenades[1] = 1; s.armed_grenade = 0; ae_panel_model_build(&s, &m);
	AE_CHECK_WSTR(m.grenade[0].count, L"2"); AE_CHECK_WSTR(m.grenade[1].count, L"1");
	AE_CHECK(m.grenade[0].armed && !m.grenade[1].armed && !m.grenade[0].zero && !m.grenade[1].zero);

	s.armed_grenade = 1; ae_panel_model_build(&s, &m);
	AE_CHECK(!m.grenade[0].armed && m.grenade[1].armed);

	s.grenades[0] = 9; s.grenades[1] = 9; ae_panel_model_build(&s, &m);
	AE_CHECK_WSTR(m.grenade[0].count, L"9"); AE_CHECK_WSTR(m.grenade[1].count, L"9");

	s.grenades[0] = 12; s.grenades[1] = 250; ae_panel_model_build(&s, &m);   /* never more than 2 digits */
	AE_CHECK_WSTR(m.grenade[0].count, L"12"); AE_CHECK_WSTR(m.grenade[1].count, L"99");
	s.grenades[0] = -1; s.armed_grenade = 5; ae_panel_model_build(&s, &m);    /* odd values: 0, nothing armed */
	AE_CHECK_WSTR(m.grenade[0].count, L"0"); AE_CHECK(!m.grenade[0].armed && !m.grenade[1].armed);
}

static void test_shield_health(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m;
	static const real shields[] = { 0.0f, 0.2f, 1.0f, 1.4f, 2.6f, 3.0f, 3.5f, -0.1f };
	static const real fill[] = { 0.0f, 0.2f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
	static const real over1[] = { 0.0f, 0.0f, 0.0f, 0.4f, 1.0f, 1.0f, 1.0f, 0.0f };
	static const real over2[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 1.0f, 1.0f, 0.0f };
	int i;

	for (i = 0; i < (int)(sizeof(shields) / sizeof(shields[0])); i++)
	{
		base(&s, _ae_hint_assault_rifle, 60, 60, 180); s.shield = shields[i]; ae_panel_model_build(&s, &m);
		AE_CHECK(AE_NEAR(m.shield_fill, fill[i])); AE_CHECK(AE_NEAR(m.overshield1, over1[i]));
		AE_CHECK(AE_NEAR(m.overshield2, over2[i]));
	}
	/* low / down: red; the blink hides it on CE's off phase; reduced motion = steady red */
	base(&s, _ae_hint_assault_rifle, 60, 60, 180); s.shield = 0.2f; s.shield_low = TRUE; s.shield_blink_on = TRUE;
	ae_panel_model_build(&s, &m); AE_CHECK(m.shield_red && !m.shield_hidden_blink);
	s.shield_blink_on = FALSE; ae_panel_model_build(&s, &m); AE_CHECK(m.shield_red && m.shield_hidden_blink);
	s.reduced_motion = TRUE; ae_panel_model_build(&s, &m); AE_CHECK(m.shield_red && !m.shield_hidden_blink);
	s.reduced_motion = FALSE; s.shield = 0; s.shield_low = FALSE; s.shield_down = TRUE; s.shield_blink_on = TRUE;
	ae_panel_model_build(&s, &m); AE_CHECK(m.shield_red && !m.shield_hidden_blink);
	s.shield = 1; s.shield_down = FALSE; ae_panel_model_build(&s, &m); AE_CHECK(!m.shield_red && !m.shield_hidden_blink);

	base(&s, _ae_hint_assault_rifle, 60, 60, 180);
	s.health = 0.0f; ae_panel_model_build(&s, &m); AE_CHECK(m.health_cells_lit == 0);
	s.health = 0.2f; s.health_low = TRUE; ae_panel_model_build(&s, &m); AE_CHECK(m.health_cells_lit == 2);
	AE_CHECK(m.health_red);
	s.health = 1.0f; s.health_low = FALSE; ae_panel_model_build(&s, &m); AE_CHECK(m.health_cells_lit == 8);
	AE_CHECK(!m.health_red);
	s.health = 0.125f; ae_panel_model_build(&s, &m); AE_CHECK(m.health_cells_lit == 1);
	s.health = 0.126f; ae_panel_model_build(&s, &m); AE_CHECK(m.health_cells_lit == 2);
	s.health = 0.001f; ae_panel_model_build(&s, &m); AE_CHECK(m.health_cells_lit == 1);
	s.health = 1.7f; ae_panel_model_build(&s, &m); AE_CHECK(m.health_cells_lit == 8);
}

static void test_buffs(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m;

	base(&s, _ae_hint_assault_rifle, 60, 60, 180); ae_panel_model_build(&s, &m); AE_CHECK(m.buff_count == 0);

	s.camo_ticks = 37 * 30; ae_panel_model_build(&s, &m);
	AE_CHECK(m.buff_count == 1); AE_CHECK_WSTR(m.buff[0].name, L"CAMO"); AE_CHECK_WSTR(m.buff[0].time, L"0:37");
	AE_CHECK(!m.buff[0].time_red);
	s.camo_ticks = 36 * 30 + 1; ae_panel_model_build(&s, &m); AE_CHECK_WSTR(m.buff[0].time, L"0:37");  /* rounded up */

	s.overshield_seconds = 91.2f; s.camo_ticks = 61 * 30; ae_panel_model_build(&s, &m);
	AE_CHECK(m.buff_count == 2); AE_CHECK_WSTR(m.buff[0].name, L"OVERSHIELD"); AE_CHECK_WSTR(m.buff[0].time, L"1:32");
	AE_CHECK_WSTR(m.buff[1].name, L"CAMO"); AE_CHECK_WSTR(m.buff[1].time, L"1:01");

	s.overshield_seconds = 4.2f; s.camo_ticks = 5 * 30; ae_panel_model_build(&s, &m);
	AE_CHECK(m.buff[0].time_red && m.buff[1].time_red); AE_CHECK_WSTR(m.buff[0].time, L"0:05");
	AE_CHECK_WSTR(m.buff[1].time, L"0:05");
	s.camo_ticks = 5 * 30 + 1; ae_panel_model_build(&s, &m); AE_CHECK(!m.buff[1].time_red);  /* 0:06 */

	s.overshield_seconds = 0; s.camo_ticks = 99999 * 30; ae_panel_model_build(&s, &m);   /* never past 9:59 */
	AE_CHECK(m.buff_count == 1); AE_CHECK_WSTR(m.buff[0].time, L"9:59");
	s.camo_ticks = -4; s.overshield_seconds = -1; ae_panel_model_build(&s, &m); AE_CHECK(m.buff_count == 0);

	AE_CHECK(ae_panel_camo_seconds(0) == 0 && ae_panel_camo_seconds(1) == 1 && ae_panel_camo_seconds(30) == 1 &&
		ae_panel_camo_seconds(31) == 2);
}

static void test_seats(void)
{
	struct ae_panel_snapshot s; struct ae_panel_model m;

	memset(&s, 0, sizeof(s)); s.shield = 1; s.health = 1; s.in_seat = TRUE; s.weapon_type_hint = _ae_hint_none;
	s.grenades[0] = 2;
	s.seat_row_count = 1; s.seat_rows[0].kind = _ae_seat_heat; s.seat_rows[0].fill = 0.3f;
	ae_panel_model_build(&s, &m);
	AE_CHECK(m.weapon_kind == _ae_panel_seat && m.seat_count == 1); AE_CHECK_WSTR(m.seat[0].label, L"HEAT");
	AE_CHECK(AE_NEAR(m.seat[0].fill, 0.3f) && !m.seat[0].red && !m.seat[0].shell);
	AE_CHECK(!m.grenade[0].shown && m.number[0] == 0 && m.row_count == 0 && !m.bar_shown);

	s.seat_row_count = 2; s.seat_rows[0].kind = _ae_seat_bolts; s.seat_rows[1].kind = _ae_seat_bomb;
	s.seat_rows[1].fill = 1.0f; s.seat_rows[0].overheated = TRUE; ae_panel_model_build(&s, &m);
	AE_CHECK(m.seat_count == 2); AE_CHECK_WSTR(m.seat[0].label, L"HEAT"); AE_CHECK_WSTR(m.seat[1].label, L"BOMB");
	AE_CHECK(m.seat[0].red && !m.seat[1].red);

	s.seat_rows[0].kind = _ae_seat_cannon; s.seat_rows[1].kind = _ae_seat_mg; s.seat_rows[0].overheated = FALSE;
	ae_panel_model_build(&s, &m);
	AE_CHECK_WSTR(m.seat[0].label, L"CANNON"); AE_CHECK(m.seat[0].shell); AE_CHECK_WSTR(m.seat[1].label, L"MG");
	AE_CHECK(!m.seat[1].shell);

	s.seat_row_count = 7; ae_panel_model_build(&s, &m); AE_CHECK(m.seat_count == 2);   /* never past the array */
	s.seat_row_count = 1; s.seat_rows[0].kind = 99; ae_panel_model_build(&s, &m);      /* an odd kind: HEAT */
	AE_CHECK_WSTR(m.seat[0].label, L"HEAT");
}

/* CE's order (its part-spent assault rifle, seen in the CLASSIC frames): the top row fills first, the bottom row
empties first, from its right */
static void test_row_lit(void)
{
	short ar[3] = { 20, 20, 20 }, sniper[3] = { 2, 2, 0 }, br[3] = { 18, 18, 0 }, odd[3] = { 13, 12, 0 };
	short out[3];

	ae_panel_model_row_lit(ar, 3, 9, out); AE_CHECK(out[0] == 9 && out[1] == 0 && out[2] == 0);
	ae_panel_model_row_lit(ar, 3, 60, out); AE_CHECK(out[0] == 20 && out[1] == 20 && out[2] == 20);
	ae_panel_model_row_lit(ar, 3, 59, out); AE_CHECK(out[0] == 20 && out[1] == 20 && out[2] == 19);
	ae_panel_model_row_lit(ar, 3, 33, out); AE_CHECK(out[0] == 20 && out[1] == 13 && out[2] == 0);
	ae_panel_model_row_lit(ar, 3, 0, out); AE_CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0);
	ae_panel_model_row_lit(ar, 3, 1, out); AE_CHECK(out[0] == 1 && out[1] == 0 && out[2] == 0);
	ae_panel_model_row_lit(sniper, 2, 3, out); AE_CHECK(out[0] == 2 && out[1] == 1);
	ae_panel_model_row_lit(br, 2, 27, out); AE_CHECK(out[0] == 18 && out[1] == 9);
	ae_panel_model_row_lit(odd, 2, 24, out); AE_CHECK(out[0] == 13 && out[1] == 11);
	ae_panel_model_row_lit(ar, 3, 99, out); AE_CHECK(out[0] == 20 && out[1] == 20 && out[2] == 20);
	ae_panel_model_row_lit(ar, 3, -2, out); AE_CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0);
	ae_panel_model_row_lit(ar, 0, 5, out); AE_CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0);

	/* the model uses the same order */
	{
		struct ae_panel_snapshot s; struct ae_panel_model m;
		base(&s, _ae_hint_battle_rifle, 36, 27, 108); ae_panel_model_build(&s, &m);
		AE_CHECK(m.row_lit[0] == 18 && m.row_lit[1] == 9 && m.lit == 27);
	}
}

static void test_hit_trail(void)
{
	struct ae_panel_trail state = { 0 };
	real trail, alpha;

	/* full, then a hit to 0.6: the trail holds 1.0 for 15 ticks, fades over 15, then goes */
	ae_panel_trail_step(&state, 1.0f, 0, &trail, &alpha);
	AE_CHECK(alpha == 0.0f && AE_NEAR(trail, 1.0f));
	ae_panel_trail_step(&state, 0.6f, 1, &trail, &alpha);
	AE_CHECK(AE_NEAR(trail, 1.0f) && alpha == 1.0f);
	ae_panel_trail_step(&state, 0.6f, 15, &trail, &alpha);
	AE_CHECK(AE_NEAR(trail, 1.0f) && alpha == 1.0f);
	ae_panel_trail_step(&state, 0.6f, 23, &trail, &alpha);
	AE_CHECK(alpha > 0.0f && alpha < 1.0f);
	ae_panel_trail_step(&state, 0.6f, 31, &trail, &alpha);
	AE_CHECK(alpha == 0.0f && AE_NEAR(trail, 0.6f));
	/* a rising shield ends a trail at once */
	ae_panel_trail_step(&state, 0.3f, 40, &trail, &alpha);
	AE_CHECK(alpha == 1.0f && AE_NEAR(trail, 0.6f));
	ae_panel_trail_step(&state, 0.35f, 41, &trail, &alpha);
	AE_CHECK(alpha == 0.0f);
	/* an overshield's drain (above 1) is no hit */
	memset(&state, 0, sizeof(state));
	ae_panel_trail_step(&state, 2.0f, 50, &trail, &alpha);
	ae_panel_trail_step(&state, 1.9f, 51, &trail, &alpha);
	AE_CHECK(alpha == 0.0f);
}

static void test_script_blink(void)
{
	/* not blinking: full */
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(FALSE, 20, FALSE), 1.0f));
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(FALSE, 20, TRUE), 1.0f));
	/* blinking: 1 then 0.35 on the 15-tick beat */
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, 0, FALSE), 1.0f));
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, 14, FALSE), 1.0f));
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, 15, FALSE), 0.35f));
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, 29, FALSE), 0.35f));
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, 30, FALSE), 1.0f));
	/* REDUCE MOTION: dimmed 0.5 s of every 2 s */
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, 0, TRUE), 0.35f));
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, 14, TRUE), 0.35f));
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, 15, TRUE), 1.0f));
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, 59, TRUE), 1.0f));
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, 60, TRUE), 0.35f));
	/* a time before the game's start reads as its start */
	AE_CHECK(AE_NEAR(ae_panel_script_blink_fade(TRUE, -5, FALSE), 1.0f));
}

static void test_zoomed_numbers(void)
{
	short state;

	/* the sniper's range and elevation readouts stay */
	AE_CHECK(ae_panel_zoomed_number_is_readout(6));
	AE_CHECK(ae_panel_zoomed_number_is_readout(7));
	/* the weapon's own counts are the AE ammo group's (an H2CE map's total rounds flagged "only when zoomed") */
	for (state = 0; state < 6; state++)
		AE_CHECK(!ae_panel_zoomed_number_is_readout(state));
	AE_CHECK(!ae_panel_zoomed_number_is_readout(-1));
	AE_CHECK(!ae_panel_zoomed_number_is_readout(8));
}

int main(void)
{
	test_weapon_table();
	test_unknown_weapons();
	test_reserve_numbers();
	test_states();
	test_no_weapon();
	test_grenades();
	test_shield_health();
	test_buffs();
	test_seats();
	test_row_lit();
	test_hit_trail();
	test_script_blink();
	test_zoomed_numbers();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
