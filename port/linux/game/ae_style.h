/*
AE_STYLE.H

The AE menus' look (ae_style.c): the colour tokens (spec 2; this table is the
only place a colour is written), the widget metrics at FULL and VIEW density
(spec 6), the frame (spec 1: a centred 16:9 box, margins 5 % of its width),
the per-view sizing rule and its pixel floors (spec 6, owner decision 2), and
UI SCALE's steps. Pure arithmetic, no engine includes (unit test:
port/linux/tests/ae_style_test.c). Plain types only.

Colours are 0xRRGGBBAA with alpha = round(a x 255). No chrome scrims (owner
answer 5): AE_COLOR_SCRIM is drawn behind dialogs only.
*/

#ifndef __AE_STYLE_H
#define __AE_STYLE_H

#define AE_COLOR_PANEL          0x181F26D1u   /* rgba(24,31,38,.82) */
#define AE_COLOR_RULE           0xC8D6E033u   /* rgba(200,214,224,.20) */
#define AE_COLOR_ROW            0x202830BDu   /* rgba(32,40,48,.74) */
#define AE_COLOR_TEXT           0xCDD6DDFFu
#define AE_COLOR_TITLE          0xEEF3F6FFu
#define AE_COLOR_MUTED          0x84939FFFu
#define AE_COLOR_SELECTION      0xE8EEF2FFu
#define AE_COLOR_SELECTION_TEXT 0x0F171EFFu
#define AE_COLOR_ACCENT         0x5FC8ECFFu
#define AE_COLOR_WARNING        0xE0A84AFFu
#define AE_COLOR_HOVER          0xE8EEF224u   /* .14 wash */
#define AE_COLOR_POPOVER        0x10161CFFu
#define AE_COLOR_SCRIM          0x04070A9Eu   /* .62, behind dialogs only (no chrome scrims: owner answer 5) */
#define AE_COLOR_VIEW_PANEL     0x10161CF2u   /* .95 */
#define AE_COLOR_KEY_CAP        0xC9D3DAFFu
#define AE_COLOR_KEY_CAP_TEXT   0x10161CFFu
#define AE_COLOR_WELL           0x0C1218DBu   /* text field, .86 */
#define AE_COLOR_FADE           0x080C10EBu   /* list overflow fade, .92 */
#define AE_COLOR_TRACK          0xC8D6E01Fu   /* scrollbar track, rule at 12 % */
#define AE_COLOR_THUMB_MOUSE    0xB9C7D2FFu
#define AE_COLOR_ARROW_HOVER    0x5FC8EC8Cu   /* accent .55 */
#define AE_COLOR_TEXT_SELECT    0x5FC8EC6Eu   /* accent .43 */
#define AE_COLOR_PRESSED        0x5FC8EC59u   /* accent .35 */
#define AE_COLOR_DISABLED_TEXT  0x84939FABu   /* muted .67 */
#define AE_COLOR_DISABLED_BAR   0xE8EEF296u   /* selection .59 */
#define AE_COLOR_DOT            0x84939F96u   /* page dots, muted .59 */
#define AE_COLOR_CHIP_DISABLED  0xC8D6E029u   /* outline .16 */
#define AE_COLOR_CHIP_DISABLED_TEXT 0x84939F96u   /* a disabled chip's text, muted .59 (preflight P9) */
#define AE_COLOR_RING           0xE8EEF2FFu   /* the 2 u white focus ring, the selection's colour (preflight P9) */
#define AE_DISABLED_ROW_ALPHA   0.43f
/* pad face buttons (owner answer 4); Nintendo, shoulders and START monochrome AE_COLOR_KEY_CAP */
#define AE_TINT_XBOX_A 0x66CC66FFu
#define AE_TINT_XBOX_B 0xDD5555FFu
#define AE_TINT_XBOX_X 0x5599CCFFu
#define AE_TINT_XBOX_Y 0xEECC44FFu
#define AE_TINT_PS_CROSS 0x7DA7E8FFu
#define AE_TINT_PS_CIRCLE 0xE36363FFu
#define AE_TINT_PS_SQUARE 0xD88CC1FFu
#define AE_TINT_PS_TRIANGLE 0x4FC1A6FFu

struct ae_rect { float x, y, width, height; };
/* a density's sizes, in spec units (u at FULL, view u at VIEW) */
struct ae_metrics
{
	float row, gap, pad, text, minor, sub_line, chip, chip_text, glyph, card, emblem, tab_text, help_title,
		body, body_line, group, group_height, badge, footer, title, corner;
};
extern struct ae_metrics const ae_metrics_full;   /* 50 6 20 24 18 16 34 18 30 66 42 24 28 21 31 18 38 13 20 60 2 */
extern struct ae_metrics const ae_metrics_view;   /* 40 4 14 22 17 14 28 16 24 52 32 19 22 17 24 15 30 12 17 0 2 */
enum { AE_DENSITY_FULL, AE_DENSITY_VIEW };
struct ae_density
{
	short kind;
	float unit;    /* drawing units (the current ae_draw view's own) per spec unit (u or view u) */
	float pixel;   /* drawing units per window pixel */
	float s;       /* window pixels per spec unit */
	struct ae_metrics const *metrics;
};
/* FULL: drawing in a view whose own 1080 units are window_height pixels */
void ae_density_full(float window_height, float ui_scale, struct ae_density *density);
/* VIEW: a view_width x view_height pixel view, drawn in an ae_draw view of exactly that view */
void ae_density_view(float view_width, float view_height, float ui_scale, struct ae_density *density);
/* §6: clamp(min(view_height / 540, view_width / 800), 0.6, 1.6) x ui_scale */
float ae_view_scale(float view_width, float view_height, float ui_scale);
/* spec units in drawing units, never below floor_pixels window pixels (0: no floor) */
float ae_size(struct ae_density const *density, float spec_units, float floor_pixels);
/* the §6 floors (VIEW: row text 16 px, minor 14 px, glyphs 18 px, rows max(40 s, row text x 1.6); FULL: no floors) */
float ae_size_text(struct ae_density const *density);
float ae_size_minor(struct ae_density const *density);
float ae_size_glyph(struct ae_density const *density);
float ae_size_row(struct ae_density const *density);
/* §6 panel, window pixels from the view's top-left: w = min(46 % view_w, 470 s), h = 86 % view_h,
centred in the view on both axes (the owner's decision over spec 6's (3 %, 7 %) from its top-left) */
void ae_view_panel_rect(float view_width, float view_height, float ui_scale, struct ae_rect *panel);
/* the frame (spec §1) in layout units of a layout layout_width x 1080: a 16:9 box as tall, centred (the whole
width when narrower); margin 5 % of its width; unit = layout units per u (= ui_scale) */
struct ae_frame { struct ae_rect rect; float margin; float unit; };
void ae_frame_compute(float layout_width, float ui_scale, struct ae_frame *frame);
float ae_frame_x(struct ae_frame const *frame, float fraction);   /* rect.x + fraction x rect.width */
float ae_frame_y(struct ae_frame const *frame, float fraction);
/* config percent to scale: the nearest of 90 / 100 / 115 / 130 (ties up); <= 0 is 100 */
float ae_ui_scale_from_percent(int percent);
/* audio.arena_menus_volume to the menus' gain: clamped to 0..1, NaN (strtod takes "nan") 0 */
float ae_volume_from_config(double volume);

#endif
