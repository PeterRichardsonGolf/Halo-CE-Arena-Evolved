/*
AE_PANEL_MODEL.H

HUD STYLE AE's panels (spec notes/specs/2026-10-11-score-strip.md 14.15, plan
Task 11): what the ammo + grenade group, the shield + health group and the
own-buff rows SAY, from a plain snapshot of the engine's state (filled by
ae_panel_engine.c). Pure: no engine types, so the unit test
(port/linux/tests/ae_panel_model_test.c) builds it without the game. Where
things go on screen is ae_panel_layout.c's job; how they are drawn,
ae_panels.c's.

Texts are wchar_t arrays written character by character (the game builds with
-fshort-wchar: no wcs* calls).
*/

#ifndef __AE_PANEL_MODEL_H
#define __AE_PANEL_MODEL_H

#include <wchar.h>

#include "ae_pure_types.h"

enum ae_panel_weapon_kind
{
	_ae_panel_none,             /* nothing in the ammo group (no weapon, a seat with no weapon) */
	_ae_panel_rounds,           /* reserve + "x" + round glyph, the magazine as CE's bullets */
	_ae_panel_battery,          /* battery % + battery glyph, the heat bar */
	_ae_panel_battery_no_heat,  /* battery % + battery glyph, no bar (energy sword) */
	_ae_panel_tank,             /* reserve centred, no glyph, the tank as a full-width bar (flamethrower, magazine > 60) */
	_ae_panel_seat,             /* a vehicle seat: rows of bars / a shell with grey labels, no boxes, no grenades */
	NUMBER_OF_AE_PANEL_WEAPON_KINDS
};

enum ae_panel_art               /* CE's bullet art: hud_ammo_alphas 1 / 5 / 7 / 4 / 6 / 2; bar = no rounds art */
{
	_ae_art_ar,
	_ae_art_pistol,
	_ae_art_shell,
	_ae_art_needle,
	_ae_art_sniper,
	_ae_art_rocket,
	_ae_art_bar,
	NUMBER_OF_AE_ARTS
};

enum ae_panel_glyph
{
	_ae_glyph_none,
	_ae_glyph_round,            /* hud_ammo_type_icons 0, after an "x" */
	_ae_glyph_battery,          /* hud_ammo_type_icons 4 */
	_ae_glyph_frag,             /* hud_ammo_type_icons 2 */
	_ae_glyph_shield,           /* hud_unit_backgrounds 0 */
	_ae_glyph_cross,            /* the health plus sign (drawn, not art) */
	NUMBER_OF_AE_GLYPHS
};

/* what the engine reader knows about the weapon beyond its numbers: stock weapons by tag, pack weapons by their
record; NONE = unknown, the magazine's size decides */
enum ae_panel_hint
{
	_ae_hint_none = -1,
	_ae_hint_assault_rifle,
	_ae_hint_pistol,
	_ae_hint_shotgun,
	_ae_hint_needler,
	_ae_hint_sniper,
	_ae_hint_rocket,
	_ae_hint_battle_rifle,
	_ae_hint_flamethrower,
	_ae_hint_fuel_rod_rounds,   /* the community maps' rounds variant (weapons\fuel rod gun): 3 + 2 lying shells */
	NUMBER_OF_AE_HINTS
};

enum ae_panel_seat_row_kind
{
	_ae_seat_heat,              /* Warthog gunner, Ghost */
	_ae_seat_bolts,             /* Banshee's guns ("HEAT") */
	_ae_seat_bomb,              /* Banshee's fuel rod ("BOMB") */
	_ae_seat_cannon,            /* Scorpion's shell ("CANNON": one lying rocket-art shell, not a bar) */
	_ae_seat_mg,                /* Scorpion's machine gun ("MG") */
	NUMBER_OF_AE_SEAT_ROW_KINDS
};

enum
{
	AE_PANEL_MAXIMUM_ROWS = 3,
	AE_PANEL_MAXIMUM_ROUNDS = 60,   /* magazines over 60 are a bar */
	AE_PANEL_HEALTH_CELLS = 8,
	AE_PANEL_MAXIMUM_BUFFS = 2,
	AE_PANEL_MAXIMUM_SEAT_ROWS = 2,
	AE_PANEL_BUFF_RED_SECONDS = 5
};

struct ae_panel_snapshot        /* filled by ae_panel_engine.c; plain data, no engine types */
{
	boolean has_weapon, in_seat, weapon_uses_rounds, weapon_has_heat, weapon_has_battery;
	short weapon_type_hint;     /* enum ae_panel_hint */
	short rounds_loaded, rounds_per_magazine, rounds_reserve;
	real heat, battery, tank;   /* 0..1; battery = charge left (1 - CE's age) */
	boolean overheated, low_ammo_flash_on;  /* CE's own flash phase AND cutoff test */
	short grenades[2]; short armed_grenade; /* 0 frag, 1 plasma */
	real shield, health;        /* shield 0..3 (1 = full, above = overshield); health 0..1 */
	boolean shield_low, shield_down, shield_blink_on, health_low;
	real shield_trail, shield_trail_alpha;  /* the hit trail (ae_panel_trail_step): the shield before the hit, 0..1 */
	long camo_ticks;            /* 0 = none */
	real overshield_seconds;    /* projection; 0 = none */
	struct { short kind; real fill; boolean overheated; } seat_rows[AE_PANEL_MAXIMUM_SEAT_ROWS];
	short seat_row_count;
	boolean reduced_motion;
};

struct ae_panel_model
{
	short weapon_kind;          /* enum ae_panel_weapon_kind */
	wchar_t number[4];          /* "180", "087"; empty = no number */
	short glyph;                /* enum ae_panel_glyph: round, battery, none */
	short art, row_count, rows[AE_PANEL_MAXIMUM_ROWS], lit;  /* rounds per row, top row first; lit = rounds_loaded */
	short row_lit[AE_PANEL_MAXIMUM_ROWS];  /* lit rounds per row, in the drain order (ae_panel_model_row_lit) */
	boolean lying;              /* sniper / rocket art: lying rounds, CE's spacing, left-aligned */
	boolean rounds_red;         /* low-ammo flash ON frame */
	real bar_fill; boolean bar_shown, bar_red;
	struct { wchar_t count[3]; boolean armed, zero, shown; } grenade[2];
	real shield_fill, overshield1, overshield2; boolean shield_red, shield_hidden_blink;
	real shield_trail, shield_trail_alpha;  /* the lost chunk's end and its alpha (0: none) */
	short health_cells_lit; boolean health_red;
	struct { wchar_t name[12]; wchar_t time[6]; boolean time_red; } buff[AE_PANEL_MAXIMUM_BUFFS];
	short buff_count;           /* OVERSHIELD, then CAMO */
	struct { short kind; wchar_t label[8]; real fill; boolean red; boolean shell; } seat[AE_PANEL_MAXIMUM_SEAT_ROWS];
	short seat_count;           /* HEAT / BOMB / CANNON / MG; shell = one lying rocket-art round, not a bar */
};

void ae_panel_model_build(const struct ae_panel_snapshot *in, struct ae_panel_model *out);

/* the lit rounds of each row (out_lit[i]): CE's own order, as its meter draws a part-spent magazine (spec 14.15 part
9's check: CE wins over the spec's "by x" rule): the rounds fill from the TOP row's left, so they go out from the
BOTTOM row's right end, a row at a time */
void ae_panel_model_row_lit(const short *rows, short row_count, short lit, short out_lit[AE_PANEL_MAXIMUM_ROWS]);

/* the magazine's rows for a kind of art and a magazine size (the borrowing rule); returns the row count */
short ae_panel_model_rows_for(short art, short magazine, short rows[AE_PANEL_MAXIMUM_ROWS]);

/* seconds left for a camo timer in ticks (30 a second), rounded up */
long ae_panel_camo_seconds(long ticks);

/* CE's shield hit trail (hud_unit.c: the meter keeps the part a hit took in its flash colour for 15 ticks, then it
goes): the shield before the hit, kept AE_PANEL_TRAIL_HOLD ticks, then fading over AE_PANEL_TRAIL_FADE; an overshield's
drain (above 1) is no hit; a rising shield ends it */
#define AE_PANEL_TRAIL_HOLD 15
#define AE_PANEL_TRAIL_FADE 15
struct ae_panel_trail { real trail, last; long hit_tick; boolean active; };
void ae_panel_trail_step(struct ae_panel_trail *state, real shield, long now, real *trail, real *alpha);

/* the scripts' HUD blink (hud_blink_health / _shield / _motion_sensor: a10's "this is your health" lesson; campaign
review 2026-10-18 I1): a group's fade, 1 when not blinking; blinking it is 1 and 0.35 on the panels' 15-tick beat, in
the group's own colour (no new colour); with REDUCE MOTION a steady group dimmed to 0.35 for 0.5 s every 2 s */
#define AE_PANEL_SCRIPT_BLINK_TICKS 15
#define AE_PANEL_SCRIPT_BLINK_DIM 0.35f
#define AE_PANEL_SCRIPT_BLINK_CALM_TICKS 60
real ae_panel_script_blink_fade(boolean blinking, long tick, boolean reduced_motion);

/* a weapon HUD number shown only when zoomed (hud_weapon.c's number elements; its state is the element's state type):
TRUE when it is the zoomed view's range or elevation readout (states 6 and 7: the sniper's, kept under AE, owner
2026-10-17); FALSE for one of the weapon's own counts (rounds, heat, age: states 0 to 5), which the AE ammo group shows.
The engine does not read the flag (it draws such a number zoomed or not): an H2CE map's weapon HUD has the total
rounds with it, drawn as a second count beside the AE shield bar (0.3.4) */
#define AE_PANEL_WEAPON_NUMBER_DISTANCE 6
#define AE_PANEL_WEAPON_NUMBER_ELEVATION 7
boolean ae_panel_zoomed_number_is_readout(short state_type);

#endif /* __AE_PANEL_MODEL_H */
