/*
AE_SPLIT_LAYOUT_RULES.H

SPLIT SCREEN's layout (Settings > SPLIT SCREEN > LAYOUT; display.split_two_players, display.split_side_bars): where two
players' views go, and what each view's camera and HUD take from that. Pure rules, in the game's screen units (480
lines high, square: rasterizer_globals' screen and frame bounds), with no engine includes
(port/linux/tests/ae_split_layout_rules_test.c); ae_split_layout.c applies them to compute_window_bounds (main.c), the
dividers (interface.c), the field of view (view_fov.c) and the HUD's size (hud_item_timers.c, ae_score_strip.c).

- TWO PLAYERS: TOP AND BOTTOM is the game's own (one column, two rows). SIDE BY SIDE is two columns and one row: each
  view the screen's full height and half its width. AUTO is SIDE BY SIDE on a screen 21:9 or wider (2.2:1 and over),
  TOP AND BOTTOM on a narrower one. Three and four players are the game's own whatever the setting.
- SIDE BARS (two players top and bottom only): each view no wider than 8:3 of the game's own view (640 x 240 units, so
  1440 x 538 pixels at 1080p once the divider takes its line), centred, with black bars beside it.
- Each split view's safe frame (the HUD's and the reticle's, main.c's compute_window_split_safe_frame) is the view
  inset by 7.5% of its width and height; a side-by-side view takes the inset of a quarter view's height, so its HUD
  sits at its corners as a four-player view's does.
- A side-by-side view's camera is the middle ground the owner picked (11-11) between the game's rule (one player's
  vertical angle kept: about 57 degrees across at display.fov 85) and one player's width (what one player's 16:9
  screen shows across: 94.3 degrees): the tangent of its half-angle across is the two's geometric mean, so it
  follows display.fov. The game's angle spans a view's window (its safe frame, render_camera_build_frustum_bounds),
  which on one player's screen is the title-safe frame, 408 of 480 lines (display.fov is the horizontal angle across
  that frame); the rules count the side-by-side view's own window.
- A side-by-side view's first-person weapon keeps one player's size against the world: drawn with one player's angle
  (its own or display.viewmodel_fov) through an off-centre projection whose 16:9 band, one player's whole screen,
  sits on the view's foot, centred across, as tall as the view times one player's vertical tangent over the view's
  (its sides crop off at the view's edges, as one player's do at the screen's). What rises above the band (a swing,
  a vent) goes on drawing over the view. What the weapon's markers place (the muzzle flash, particles, lights) is
  moved by the same band, so it sits on the drawn weapon (ae_split_viewmodel_map).
- The HUD's size in a view goes by the view's share of the screen (a half 0.85, a quarter 0.75). A side-by-side view
  counts as a quarter (the four-player HUD), a view narrowed by side bars as the half it was.
*/

#ifndef AE_SPLIT_LAYOUT_RULES_H
#define AE_SPLIT_LAYOUT_RULES_H

/* display.split_two_players */
enum
{
	AE_SPLIT_TWO_AUTO,
	AE_SPLIT_TWO_TOP_AND_BOTTOM,
	AE_SPLIT_TWO_SIDE_BY_SIDE
};

/* AUTO's side-by-side screens: at least AE_SPLIT_AUTO_WIDE_NUMERATOR / AE_SPLIT_AUTO_WIDE_DENOMINATOR (2.2:1) */
#define AE_SPLIT_AUTO_WIDE_NUMERATOR 11
#define AE_SPLIT_AUTO_WIDE_DENOMINATOR 5
/* SIDE BARS: a top-and-bottom view no wider than this shape (8:3, the 640 x 240 view of a 4:3 screen) */
#define AE_SPLIT_BARS_WIDTH 8
#define AE_SPLIT_BARS_HEIGHT 3
/* the shape whose horizontal angle display.fov is */
#define AE_SPLIT_FOV_ASPECT (16.0f / 9.0f)
/* the share of one player's screen height its window (the title-safe frame, which the angle spans) takes: 408 / 480 */
#define AE_SPLIT_ONE_VIEW_WINDOW (408.0f / 480.0f)
/* the most quads ae_split_quads writes (the divider and two bars) */
#define AE_SPLIT_MAXIMUM_QUADS 3

struct ae_split_rect
{
	int x0, y0, x1, y1;
};

/* the setting's value from its text ("auto", "top_and_bottom", "side_by_side"; anything else AUTO) */
int ae_split_two_players_value(char const *text);

/* whether two players sit side by side for this value on a screen of this size (any units, the same both ways) */
int ae_split_side_by_side(int value, int screen_width, int screen_height);

/* the views' columns and rows for this many players (the game's compute_subframe_counts gave columns and rows):
two players side by side are two columns and one row; anything else is left as it was */
void ae_split_counts(int players, int side_by_side, long *columns, long *rows);

/* SIDE BARS: a view narrowed to AE_SPLIT_BARS_WIDTH:AE_SPLIT_BARS_HEIGHT of its height, centred where it was. Returns
nonzero when it was narrowed (a view no wider than that stays) */
int ae_split_bars_narrow(struct ae_split_rect *view);

/* a split view's safe frame: the view inset by 7.5% of its width at the sides and of inset_height at the top and foot
(the view's own height, or a quarter view's for side by side), rounded as main.c's compute_window_split_safe_frame.
Returns zero, the frame untouched, for a view too small for it */
int ae_split_safe_frame(struct ae_split_rect const *view, int inset_height, struct ae_split_rect *safe);

/* the black quads over the whole screen after the views are drawn: the divider (a line 2 units thick at the frame's
middle: across for top and bottom, down for side by side) and, with side bars, the bars beside the narrowed views
(view: either narrowed view). Returns how many it wrote (at most AE_SPLIT_MAXIMUM_QUADS) */
int ae_split_quads(struct ae_split_rect const *screen, struct ae_split_rect const *frame, int side_by_side,
	struct ae_split_rect const *bars_view, struct ae_split_rect *quads);

/* the factor that keeps one player's vertical angle in a view (the game's rule): the window's height /
(AE_SPLIT_ONE_VIEW_WINDOW x the view's height) */
float ae_split_fov_upright_factor(float view_height, float window_height);

/* how much a side-by-side view's tangent of half its vertical angle grows: the geometric mean of the factor that keeps
one player's vertical angle (ae_split_fov_upright_factor) and the one that shows across what one player's 16:9 screen
does ((16:9 / AE_SPLIT_ONE_VIEW_WINDOW) x the window's height / the view's width); 1 for no view */
float ae_split_fov_tangent_factor(float view_width, float view_height, float window_height);

/* an angle (radians, the full vertical angle) widened by that factor: 2 atan(factor tan(angle / 2)); 0 stays 0 */
float ae_split_fov_widen(float angle, float factor);

/* the first-person weapon's band height in a view of that factor: the view's height x upright / factor (one player's
vertical tangent over the view's), so the weapon keeps one player's size against the world */
float ae_split_viewmodel_band_height(float view_height, float upright, float factor);

/* the band in a view (its units, y down): band_height high and band_height x 16 / 9 wide, centred across, on the
view's foot (wider than the view: its sides beyond the view's edges) */
void ae_split_viewmodel_band(float view_width, float view_height, float band_height, struct ae_split_rect *band);

/* the frustum bounds (render_camera_build_frustum's: the tangents the view shows over the camera's own, x across
times the view's height / width, y up) that draw the weapon with one player's angle into that band: one player's
screen's tangents (16:9 / AE_SPLIT_ONE_VIEW_WINDOW across, 1 / AE_SPLIT_ONE_VIEW_WINDOW up) over the band, the view
taking as much more as it is larger */
void ae_split_viewmodel_bounds(float view_width, float view_height, float band_height, float *x0, float *x1, float *y0,
	float *y1);

/* how a point by the first-person weapon moves so the world's projection puts it where the weapon's projection
draws it (its markers' effects: the muzzle flash, particles, lights), in the camera's frame (across to the right, up,
depth forward): across' = ax x across + cx x depth, up' = ay x up + cy x depth, depth kept. Each projection is its
tangent of half the vertical angle and its frustum bounds (render_camera_build_frustum's); aspect is the viewport's
width / height */
struct ae_split_view_map
{
	float ax, cx, ay, cy;
};
void ae_split_viewmodel_map(float aspect, float world_tangent, float const world_bounds[4], float weapon_tangent,
	float const weapon_bounds[4], struct ae_split_view_map *map);

/* what the HUD's view share is multiplied by: a side-by-side view counts as a quarter (half its share), a view side
bars narrowed as the whole width it had (screen width over the view's); 1 otherwise */
float ae_split_hud_share_factor(int side_by_side, int bars, float screen_width, float view_width);

#endif
