/*
AE_SPLIT_LAYOUT.C

SPLIT SCREEN's layout in the engine (ae_split_layout.h): the settings read, the screen's shape, and the rules
(ae_split_layout_rules.h) applied to the views, their dividers, their field of view and their HUD's size.
*/

#include "cseries/cseries.h"
#include "cutscene/cinematics.h"
#include "main/main.h"
#include "game/players.h"
#include "models/model_animation_definitions.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "render/render_cameras.h"

#include "ae_split_layout.h"
#include "ae_split_layout_rules.h"

const char *config_string(const char *name);
int config_boolean(const char *name);
unsigned long config_changes(void);
void platform_log(const char *format, ...);

/* the settings, read again when Settings or config.toml changes */
static struct
{
	boolean read;
	unsigned long read_at;
	int two_players;
	boolean side_bars;
	/* (the last layout logged: 0 none yet) */
	int logged;
} settings;

static void settings_read(void)
{
	if (settings.read && settings.read_at == config_changes())
		return;
	settings.read = TRUE;
	settings.read_at = config_changes();
	settings.two_players = ae_split_two_players_value(config_string("display.split_two_players"));
	settings.side_bars = config_boolean("display.split_side_bars") != 0;
}

static void rect_from(rectangle2d const *in, struct ae_split_rect *out)
{
	out->x0 = in->x0;
	out->y0 = in->y0;
	out->x1 = in->x1;
	out->y1 = in->y1;
}

static void rect_to(struct ae_split_rect const *in, rectangle2d *out)
{
	out->x0 = (short)in->x0;
	out->y0 = (short)in->y0;
	out->x1 = (short)in->x1;
	out->y1 = (short)in->y1;
}

/* whether two players sit side by side on this screen (its shape in the game's square units) */
static boolean two_side_by_side(void)
{
	rectangle2d const *screen = &rasterizer_globals.reserved04.screen_bounds;

	settings_read();
	return ae_split_side_by_side(settings.two_players, screen->x1 - screen->x0, screen->y1 - screen->y0) != 0;
}

/* whether two players top and bottom have side bars */
static boolean two_side_bars(void)
{
	settings_read();
	return settings.side_bars && !two_side_by_side();
}

/* the log's line when two players' layout changes (telemetry: the log is what a test or a report reads) */
static void layout_log(void)
{
	rectangle2d const *screen = &rasterizer_globals.reserved04.screen_bounds;
	int layout = two_side_by_side() ? 2 : two_side_bars() ? 3 : 1;

	if (settings.logged == layout)
		return;
	settings.logged = layout;
	platform_log("split screen: two players %s (screen %dx%d units)",
		layout == 2 ? "side by side" : layout == 3 ? "top and bottom, side bars" : "top and bottom",
		screen->x1 - screen->x0, screen->y1 - screen->y0);
}

void ae_split_layout_counts(long players, long *columns, long *rows)
{
	if (players != 2)
		return;
	layout_log();
	ae_split_counts((int)players, two_side_by_side(), columns, rows);
}

void ae_split_layout_view(long players, rectangle2d *pixel_bounds, rectangle2d *safe_frame_bounds)
{
	struct ae_split_rect view, safe;

	if (players != 2)
		return;
	rect_from(pixel_bounds, &view);
	if (two_side_by_side())
	{
		/* (a quarter view's insets: the view's height halved) */
		if (ae_split_safe_frame(&view, (view.y1 - view.y0) / 2, &safe))
			rect_to(&safe, safe_frame_bounds);
	}
	else if (two_side_bars() && ae_split_bars_narrow(&view))
	{
		rect_to(&view, pixel_bounds);
		if (ae_split_safe_frame(&view, view.y1 - view.y0, &safe))
			rect_to(&safe, safe_frame_bounds);
	}
}

boolean ae_split_layout_dividers(short window_count)
{
	struct ae_split_rect screen, frame, quads[AE_SPLIT_MAXIMUM_QUADS], bars_view;
	boolean side_by_side, bars;
	int count, index;

	if (window_count != 2)
		return FALSE;
	side_by_side = two_side_by_side();
	bars = two_side_bars();
	if (!side_by_side && !bars)
		return FALSE;
	rect_from(&rasterizer_globals.reserved04.screen_bounds, &screen);
	rect_from(&rasterizer_globals.reserved04.frame_bounds, &frame);
	if (bars)
	{
		rectangle2d pixel_bounds, safe_frame_bounds;

		/* (the views as laid out this frame: either one, both have the same columns) */
		compute_window_bounds(0, 2, &pixel_bounds, &safe_frame_bounds);
		rect_from(&pixel_bounds, &bars_view);
	}
	count = ae_split_quads(&screen, &frame, side_by_side, bars ? &bars_view : NULL, quads);
	for (index = 0; index < count; index++)
	{
		rectangle2d bounds;

		rect_to(&quads[index], &bounds);
		draw_quad(&bounds, 0xFF000000);
	}
	return TRUE;
}

boolean ae_split_layout_side_bars_now(void)
{
	return main_get_window_count() == 2 && two_side_bars();
}

boolean ae_split_layout_side_by_side_now(void)
{
	return main_get_window_count() == 2 && two_side_by_side();
}

/* the side-by-side views as laid out this frame (the left one: both are half the frame, a unit apart at most): its
width, height and window's height; FALSE for any other layout */
static boolean side_by_side_view(float *width, float *height, float *window_height)
{
	rectangle2d pixel_bounds, safe_frame_bounds;

	if (!ae_split_layout_side_by_side_now())
		return FALSE;
	compute_window_bounds(0, 2, &pixel_bounds, &safe_frame_bounds);
	*width = (float)(pixel_bounds.x1 - pixel_bounds.x0);
	*height = (float)(pixel_bounds.y1 - pixel_bounds.y0);
	*window_height = (float)(safe_frame_bounds.y1 - safe_frame_bounds.y0);
	return TRUE;
}

real ae_split_layout_fov_widen(short local_player_index, real angle)
{
	float width, height, window_height;

	if (local_player_index < 0 || !(angle > 0.0f) || !side_by_side_view(&width, &height, &window_height))
		return angle;
	return ae_split_fov_widen(angle, ae_split_fov_tangent_factor(width, height, window_height));
}

real ae_split_layout_viewmodel_angle(real angle)
{
	return ae_split_layout_side_by_side_now() ? angle : 0.0f;
}

/* each local player's last band (viewmodel_projection_begin): the camera's frame and the map from the weapon's
projection to the world's (ae_split_viewmodel_map), for what the weapon's markers place */
static struct
{
	boolean valid;
	real_point3d position;
	real_vector3d forward, up, right;
	struct ae_split_view_map map;
	real_matrix4x3 nodes[MAXIMUM_NODES_PER_ANIMATION];
} bands[MAXIMUM_LOCAL_PLAYERS];

void ae_split_layout_viewmodel_bounds(short local_player_index, real weapon_angle, struct render_camera const *camera,
	real_rectangle2d *bounds)
{
	float width, height, window_height, band_height, x0, x1, y0, y1, world[4], weapon[4];
	rectangle2d const *viewport = &camera->viewport_bounds;
	boolean recorded = local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS;

	if (!side_by_side_view(&width, &height, &window_height))
	{
		if (recorded)
			bands[local_player_index].valid = FALSE;
		return;
	}
	/* (the band from the layout's view; the bounds over this viewport, the same size) */
	band_height = ae_split_viewmodel_band_height(height, ae_split_fov_upright_factor(height, window_height),
		ae_split_fov_tangent_factor(width, height, window_height));
	ae_split_viewmodel_bounds((float)(viewport->x1 - viewport->x0), (float)(viewport->y1 - viewport->y0), band_height,
		&x0, &x1, &y0, &y1);
	if (recorded && weapon_angle > 0.0f && camera->vertical_field_of_view > 0.0f)
	{
		real_vector3d *forward = &bands[local_player_index].forward, *up = &bands[local_player_index].up;

		world[0] = bounds->x0;
		world[1] = bounds->x1;
		world[2] = bounds->y0;
		world[3] = bounds->y1;
		weapon[0] = x0;
		weapon[1] = x1;
		weapon[2] = y0;
		weapon[3] = y1;
		ae_split_viewmodel_map((float)(viewport->x1 - viewport->x0) / (float)(viewport->y1 - viewport->y0),
			tanf(camera->vertical_field_of_view * 0.5f), world, tanf(weapon_angle * 0.5f), weapon,
			&bands[local_player_index].map);
		/* (the camera's frame as render_camera_build_frustum takes it: across = forward x up, up square to forward) */
		bands[local_player_index].position = camera->position;
		*forward = camera->forward;
		normalize3d(forward);
		cross_product3d(forward, &camera->up, &bands[local_player_index].right);
		normalize3d(&bands[local_player_index].right);
		cross_product3d(&bands[local_player_index].right, forward, up);
		normalize3d(up);
		bands[local_player_index].valid = TRUE;
	}
	bounds->x0 = x0;
	bounds->x1 = x1;
	bounds->y0 = y0;
	bounds->y1 = y1;
}

/* a point by the weapon moved by its player's band */
static void band_point(short local_player_index, real_point3d *point)
{
	real_vector3d d;
	real across, up, depth, across2, up2;

	d.i = point->x - bands[local_player_index].position.x;
	d.j = point->y - bands[local_player_index].position.y;
	d.k = point->z - bands[local_player_index].position.z;
	across = dot_product3d(&d, &bands[local_player_index].right);
	up = dot_product3d(&d, &bands[local_player_index].up);
	depth = dot_product3d(&d, &bands[local_player_index].forward);
	across2 = bands[local_player_index].map.ax * across + bands[local_player_index].map.cx * depth;
	up2 = bands[local_player_index].map.ay * up + bands[local_player_index].map.cy * depth;
	point->x = bands[local_player_index].position.x + bands[local_player_index].forward.i * depth +
		bands[local_player_index].right.i * across2 + bands[local_player_index].up.i * up2;
	point->y = bands[local_player_index].position.y + bands[local_player_index].forward.j * depth +
		bands[local_player_index].right.j * across2 + bands[local_player_index].up.j * up2;
	point->z = bands[local_player_index].position.z + bands[local_player_index].forward.k * depth +
		bands[local_player_index].right.k * across2 + bands[local_player_index].up.k * up2;
}

static boolean band_in_play(short local_player_index)
{
	return local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS && bands[local_player_index].valid &&
		ae_split_layout_side_by_side_now();
}

void ae_split_layout_viewmodel_markers(short local_player_index, struct object_marker *markers, short count)
{
	short index;

	if (!band_in_play(local_player_index) || !markers)
		return;
	/* (the places move; the directions stay: a marker's own offset from its node is a few centimetres) */
	for (index = 0; index < count; index++)
	{
		band_point(local_player_index, &markers[index].node_matrix.position);
		band_point(local_player_index, &markers[index].matrix.position);
	}
}

real_matrix4x3 *ae_split_layout_viewmodel_node(short local_player_index, short node_index, real_matrix4x3 *node_matrix)
{
	real_matrix4x3 *moved;

	if (!band_in_play(local_player_index) || !node_matrix || node_index < 0 || node_index >= MAXIMUM_NODES_PER_ANIMATION)
		return node_matrix;
	moved = &bands[local_player_index].nodes[node_index];
	*moved = *node_matrix;
	band_point(local_player_index, &moved->position);
	return moved;
}

real ae_split_layout_hud_share(void)
{
	rectangle2d const *screen = &rasterizer_globals.reserved04.screen_bounds;
	rectangle2d pixel_bounds, safe_frame_bounds;
	boolean side_by_side, bars;

	if (main_get_window_count() != 2)
		return 1.0f;
	side_by_side = two_side_by_side();
	bars = two_side_bars();
	if (!side_by_side && !bars)
		return 1.0f;
	compute_window_bounds(0, 2, &pixel_bounds, &safe_frame_bounds);
	return ae_split_hud_share_factor(side_by_side, bars, (float)(screen->x1 - screen->x0),
		(float)(pixel_bounds.x1 - pixel_bounds.x0));
}
