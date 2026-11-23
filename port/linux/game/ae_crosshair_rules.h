/*
AE_CROSSHAIR_RULES.H

CROSSHAIR v3 (SETTINGS > HUD > RETICLE; ae_crosshair.c draws it): a code-drawn crosshair as pure rules. No engine
includes (port/linux/tests/ae_crosshair_rules_test.c runs them, under the sanitizers).

A CROSSHAIR is a shape's centre, its inner and outer line sets and its frame, each with the length, thickness,
offset and opacity the player edits, plus colour and an outline. The shapes (37, curated from Kenney's Crosshair
Pack, CC0) are authored once in tools/ae_crosshair_shapes.py. A crosshair has a SHARE CODE (version 1: 23 bytes, 37
characters), read strictly: a fixed length, a character whitelist, a CRC before any field is used, every field
clamped, nothing allocated from the input, no text taken from it. The profile keeps up to 8 saved crosshairs ("NAME|code"),
the main one, and a per-weapon table of choices.
*/

#ifndef AE_CROSSHAIR_RULES_H
#define AE_CROSSHAIR_RULES_H

#include <stddef.h>

enum { AE_XH_SLOTS = 8, AE_XH_NAME = 12, AE_XH_SHAPE_NONE = 0 };
enum { AE_XH_CENTRE_NONE, AE_XH_CENTRE_DOT, AE_XH_CENTRE_PLUS };

/* the editor's ranges; lengths, thicknesses and offsets are kept in half pixels (at 1080p) */
enum
{
	AE_XH_LENGTH_MAX = 80, AE_XH_THICKNESS_MIN = 1, AE_XH_THICKNESS_MAX = 20, AE_XH_OFFSET_MAX = 80,
	AE_XH_OPACITY_MIN = 10, AE_XH_FRAME_SIZE_MIN = 50, AE_XH_FRAME_SIZE_MAX = 200, AE_XH_CENTRE_SIZE_MAX = 16,
	AE_XH_OUTLINE_THICKNESS_MAX = 8
};

struct ae_xh_lines
{
	unsigned char on, length, thickness, offset, opacity;
};

struct ae_crosshair
{
	char name[AE_XH_NAME + 1];
	unsigned char shape;      /* 1 to the library's count; 0 none (lines and centre only) */
	unsigned char centre;     /* AE_XH_CENTRE_ */
	unsigned char centre_size;
	unsigned char color[3];
	unsigned char opacity;
	struct ae_xh_lines inner, outer;
	unsigned char frame_on, frame_size, frame_thickness, frame_opacity;
	unsigned char outline_on, outline_thickness, outline_opacity;
	unsigned char movement, firing;
};

/* ---------- the shape library */
int ae_xh_shape_count(void);
const char *ae_xh_shape_name(int shape);       /* 1 based; "" outside */
int ae_xh_shape_family(int shape);             /* 0 LINES, 1 CHEVRONS, 2 RINGS, 3 SHAPES */
const char *ae_xh_family_name(int family);
int ae_xh_shape_of_name(const char *name);     /* 0 when none */

/* the colours of the editor's COLOR row (no green or red: they are the ally's and the enemy's) */
int ae_xh_palette_count(void);
const char *ae_xh_palette_name(int index);
unsigned long ae_xh_palette_rgb(int index);
/* (0.7.7) CUSTOM COLOR: "#RRGGBB" or "RRGGBB" (six hex digits, either case, spaces round it allowed) to 0xRRGGBB; 0 when it
is not one (nothing else is read). The text form is "#RRGGBB" in capitals */
int ae_xh_hex_parse(const char *text, unsigned long *rgb);
void ae_xh_hex_format(unsigned long rgb, char *text, size_t size);
/* the palette entry a crosshair's colour is, -1 for a custom colour */
int ae_xh_palette_of(const struct ae_crosshair *xh);
void ae_xh_color_set(struct ae_crosshair *xh, unsigned long rgb);

/* a new crosshair (TWIN CROSS, white, a 1 pixel outline) */
void ae_xh_default(struct ae_crosshair *xh);
/* (0.7.7) the starters a profile has before the player saves any (display.crosshair_1 to _3's defaults): PRECISE, SNAP,
OCTA, each about 22 pixels across at 1080p. index 0 to ae_xh_starter_count() - 1; 0 outside */
int ae_xh_starter_count(void);
int ae_xh_starter(int index, struct ae_crosshair *xh);
/* the shape's centre, lines and frame in place of the crosshair's; its colour, opacity, outline, name and motion stay */
void ae_xh_apply_shape(struct ae_crosshair *xh, int shape);
/* every field into its range (a name cleaned) */
void ae_xh_clamp(struct ae_crosshair *xh);
/* a typed name: A-Z 0-9 space - . only (lower case raised), at most 12, trimmed; returns its length */
int ae_xh_name_clean(const char *text, char *name, size_t size);
/* whether the shape has a frame, and the layout word its lines use ("CROSS"...) */
int ae_xh_shape_has_frame(int shape);

/* ---------- the share code */
enum { AE_XH_CODE_OK, AE_XH_CODE_NOT_A_CODE, AE_XH_CODE_DAMAGED, AE_XH_CODE_NEWER, AE_XH_CODE_RESULTS };
enum { AE_XH_CODE_TEXT = 64, AE_XH_CODE_INPUT_MAX = 96 };
/* "AE1-0426-QXFR-..." (the name is not in it) */
void ae_xh_code_encode(const struct ae_crosshair *xh, char *text, size_t size);
/* result; *xh set only on OK (an unknown shape id becomes shape 0, *unknown_shape 1); text up to AE_XH_CODE_INPUT_MAX */
int ae_xh_code_decode(const char *text, struct ae_crosshair *xh, int *unknown_shape);
const char *ae_xh_code_why(int result);

/* ---------- the profile's strings */
/* "NAME|code" (a free slot is empty; "starter:1" to "starter:3", a slot's default, is that starter); the name is the typed
one, cleaned */
int ae_xh_slot_parse(const char *text, struct ae_crosshair *xh);
void ae_xh_slot_format(const struct ae_crosshair *xh, char *text, size_t size);

/* the per-weapon table: a weapon key (a-z 0-9 _) to a choice. AE_XH_CHOICE_MAIN is no entry */
enum { AE_XH_CHOICE_MAIN = -2, AE_XH_CHOICE_OWN = -1, AE_XH_WEAPON_ENTRIES = 28, AE_XH_WEAPON_KEY = 32 };
struct ae_xh_weapons
{
	int count;
	struct { char key[AE_XH_WEAPON_KEY]; signed char choice; } entry[AE_XH_WEAPON_ENTRIES];
};
void ae_xh_weapons_parse(const char *text, struct ae_xh_weapons *table);
void ae_xh_weapons_format(const struct ae_xh_weapons *table, char *text, size_t size);
int ae_xh_weapons_get(const struct ae_xh_weapons *table, const char *key);
/* 0 when the table is full (the choice MAIN removes an entry and always works) */
int ae_xh_weapons_set(struct ae_xh_weapons *table, const char *key, int choice);
/* a weapon's tag name ("weapons\assault rifle\assault rifle") to its key ("assault_rifle"). A multiplayer copy's "mp" in
front is dropped ("vehicles\ghost\mp_ghost gun" is "ghost_gun") and the stationary gun's two tags are one key
("gun_turret_gun"), so a vehicle gun has one key on every map */
void ae_xh_weapon_key_of_tag(const char *tag_name, char *key, size_t size);
/* the number of entries with a choice other than MAIN, and how many of them OWN */
void ae_xh_weapons_summary(const struct ae_xh_weapons *table, int *own, int *custom);

/* the main crosshair: "own" or "1" to "8"; returns AE_XH_CHOICE_OWN or the slot 0 to 7 */
int ae_xh_main_of(const char *text);

/* ---------- the stock reticle's rows */
enum { AE_XH_RETICLE_SIZE_MIN = 50, AE_XH_RETICLE_SIZE_MAX = 200, AE_XH_RETICLE_SIZE_DEFAULT = 100 };
int ae_xh_reticle_size_clamp(long percent);
enum { AE_XH_SCALING_FOV, AE_XH_SCALING_FIXED };
int ae_xh_reticle_scaling_of(const char *text);
/* (0.7.7) FOLLOWS FOV's base: the field of view (degrees across a 4:3 view, as the game's own 70) at which a weapon's own
reticle is its stock size; it covers the same part of the world at any other. "78" (the default) or "70" (the game's own
view: the reticle as before) */
enum { AE_XH_FOV_BASE_78, AE_XH_FOV_BASE_70 };
int ae_xh_fov_base_of(const char *text);
/* the stock reticle's size at the game's own view for a base: tan(base / 2) / tan(35 degrees) (1 for 70, about 1.157 for 78) */
float ae_xh_fov_base_factor(int base);
/* (0.7.7) RETICLE MOTION, a weapon's own reticle: RIFLES (the AE rifles' growth a shot, as in 0.7.6: the default), ALL
WEAPONS (every weapon's reticle grows with its real spread and a shot's kick, the motion a drawn crosshair has), OFF */
enum { AE_XH_STOCK_MOTION_RIFLES, AE_XH_STOCK_MOTION_ALL, AE_XH_STOCK_MOTION_OFF };
int ae_xh_stock_motion_of(const char *text);
/* ALL WEAPONS: how a weapon's own reticle moves, from its own tag: its rate of fire (shots a second) and its error cone
(degrees, the trigger's lower and upper bound). A weapon whose cone opens (a span over 0.05 degrees: assault rifle, pistol,
plasma rifle) moves with the real error it has now, up to GAIN of its size (0.10 for a small cone to 0.35 for 3 degrees
and over: the AE rifles' own 35 %), never past the pixels of the real cone; any other weapon (a fixed or no cone: shotgun,
sniper, needler, launchers) kicks once a shot by GROWTH (0.15 for 6 shots a second and over, 0.18 from 2.5, 0.22 below),
settling in MS (700 / rate, 200 to 500 ms). */
struct ae_xh_motion_class
{
	int has_spread;
	float gain;
	float growth;
	int ms;
};
void ae_xh_motion_class_of(float rate, float lower_degrees, float upper_degrees, struct ae_xh_motion_class *out);
/* a weapon's own reticle's scale for that motion: kick (a shot's, 0 to 1), error (the trigger's, 0 to 1) and spread (1080p
pixels from the middle, over a nominal 24 pixel reticle); at most 1.35, the AE rifles' own growth */
float ae_xh_stock_motion_scale(struct ae_xh_motion_class const *cls, float kick, float error, float spread);
/* COLOR: ORIGINAL (the tag's), SAME AS HUD, WHITE, ICE, CYAN, PINK, YELLOW */
enum { AE_XH_TINT_ORIGINAL, AE_XH_TINT_HUD, AE_XH_TINT_WHITE, AE_XH_TINT_ICE, AE_XH_TINT_CYAN, AE_XH_TINT_PINK,
	AE_XH_TINT_YELLOW, AE_XH_TINTS };
int ae_xh_tint_of(const char *text);
const char *ae_xh_tint_text(int tint);
/* the 0xRRGGBB of a tint (ORIGINAL and SAME AS HUD give base) */
unsigned long ae_xh_tint_rgb(int tint, unsigned long base);
/* red over an enemy beats green over an ally beats the chosen colour */
unsigned long ae_xh_state_rgb(unsigned long chosen, int enemy, int ally);
/* (0.7.8) ENEMY COLOR / ALLY COLOR (HUD > RETICLE): the colours a crosshair and a weapon's own reticle take over an enemy
in range / an ally; red and green by default. Words: red, orange, yellow, green, cyan, blue, magenta, white */
enum { AE_XH_STATE_RED, AE_XH_STATE_ORANGE, AE_XH_STATE_YELLOW, AE_XH_STATE_GREEN, AE_XH_STATE_CYAN, AE_XH_STATE_BLUE,
	AE_XH_STATE_MAGENTA, AE_XH_STATE_WHITE, AE_XH_STATE_COLORS };
int ae_xh_state_color_of(const char *text, int fallback);
unsigned long ae_xh_state_color_rgb(int color);
/* the colours ae_xh_state_rgb gives (the drawing player's: set before each draw); defaults red and green */
void ae_xh_state_colors_set(int enemy, int ally);

/* ---------- the host rule (universal_variant.ball_melee bit 2, SET = CUSTOM CROSSHAIRS OFF) */
int ae_xh_allowed_of(unsigned char ball_melee);
unsigned char ae_xh_allowed_set(unsigned char ball_melee, int allowed);

/* ---------- drawing: the crosshair as pieces, in pixels from the centre (which sits on a pixel corner) */
enum { AE_XH_PIECE_RECT, AE_XH_PIECE_SEGMENT, AE_XH_PIECE_DISC };
struct ae_xh_piece
{
	unsigned char kind;
	float x0, y0, x1, y1;     /* RECT: its corners; SEGMENT: its ends; DISC: the centre in x0, y0 */
	float size;               /* SEGMENT: its thickness; DISC: the radius */
	unsigned int rgba;        /* 0xRRGGBBAA */
};
enum { AE_XH_PIECES = 1024 };
/* what moves the crosshair, in pixels at 1080p: the firing spread and kick, and the movement cue (capped at 3) */
struct ae_xh_motion { float spread, kick, move; };
/* scale: window pixels per 1080p pixel. rgb: the colour to draw in (the state's). Returns the pieces (outline first) */
int ae_xh_build(const struct ae_crosshair *xh, const struct ae_xh_motion *motion, unsigned long rgb, float scale,
	struct ae_xh_piece *pieces, int maximum);
/* the farthest the crosshair's pieces reach from the centre in 1080p pixels at rest (the box tests keep it in) */
float ae_xh_extent(const struct ae_crosshair *xh);
/* the firing spread in 1080p pixels for a trigger error angle (radians) at a vertical field of view (radians) */
float ae_xh_spread_pixels(float error_angle, float vertical_fov);

#endif
