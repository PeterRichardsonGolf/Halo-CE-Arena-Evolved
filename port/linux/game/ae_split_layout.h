/*
AE_SPLIT_LAYOUT.H

SPLIT SCREEN's layout in the engine (the rules: ae_split_layout_rules.h). Two players' views side by side or narrowed
by side bars (display.split_two_players, display.split_side_bars), read again when Settings changes them. Hooks:
main.c's compute_window_bounds (the counts and each view), interface.c's dividers, view_fov.c's angles, the HUD's
view share (hud_item_timers.c, ae_score_strip.c).
*/

#ifndef AE_SPLIT_LAYOUT_H
#define AE_SPLIT_LAYOUT_H

#include "cseries/cseries.h"
#include "math/integer_math.h"
#include "math/real_math.h"

/* compute_window_bounds: the columns and rows for this many views (two side by side: 2 x 1) */
void ae_split_layout_counts(long players, long *columns, long *rows);

/* compute_window_bounds: a view of this many (its pixel bounds and safe frame, as the game laid them out): narrowed
by side bars, or a side-by-side view's safe frame at a quarter view's insets */
void ae_split_layout_view(long players, rectangle2d *pixel_bounds, rectangle2d *safe_frame_bounds);

/* interface.c's dividers for this many views: TRUE when AE drew them (side by side's down the middle, side bars'
divider and the bars, black each frame); FALSE leaves the game's */
boolean ae_split_layout_dividers(short window_count);

/* whether the two views on screen now are narrowed by side bars (main_get_window_count is 2) */
boolean ae_split_layout_side_bars_now(void);

/* whether the two views on screen now are side by side (main_get_window_count is 2) */
boolean ae_split_layout_side_by_side_now(void);

/* a view's vertical angle (radians), widened for a side-by-side view to the middle ground between one player's
vertical angle and one player's width (ae_split_fov_tangent_factor); any other view's, and 0, unchanged */
real ae_split_layout_fov_widen(short local_player_index, real angle);

/* view_fov.c: the first-person weapon's own angle where display.fov left the view's (the view's unwidened angle) in
a side-by-side view, 0 (the view's own) elsewhere */
real ae_split_layout_viewmodel_angle(real angle);

struct render_camera;
struct object_marker;

/* view_fov.c's viewmodel_projection_begin: a side-by-side view's weapon drawn as one player's screen in the 16:9 band
on the view's foot, sized to keep one player's weapon against the world (ae_split_viewmodel_bounds), over this
camera's viewport (bounds: the world's in, the weapon's out); notes the player's band for the weapon's markers. Other
views' bounds untouched */
void ae_split_layout_viewmodel_bounds(short local_player_index, real weapon_angle, struct render_camera const *camera,
	real_rectangle2d *bounds);

/* first_person_weapons.c: the weapon's markers and nodes, as effects, particles and lights read them, moved by the
player's band so what they place sits on the drawn weapon (the weapon's own drawing is not moved); untouched outside
side by side. The node is a copy, kept until the next call for that node */
void ae_split_layout_viewmodel_markers(short local_player_index, struct object_marker *markers, short count);
real_matrix4x3 *ae_split_layout_viewmodel_node(short local_player_index, short node_index, real_matrix4x3 *node_matrix);

/* what a view's share of the screen is multiplied by for the HUD's size (ae_split_hud_share_factor): 0.5 side by
side, the width side bars took back, else 1 */
real ae_split_layout_hud_share(void);

#endif
