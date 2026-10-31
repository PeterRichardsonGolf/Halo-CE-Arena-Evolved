/*
AE_WAYPOINT_INK.C

The item waypoints' places to a fraction of a HUD unit (ae_waypoint_ink.h): the arrow's ink read once a sprite from
its bitmap's pixels (bitmap_2d_get_pixel: any format the game reads, compressed or swizzled), and the labels' pictures
drawn as hud_draw_bitmap_direct draws them, about a centre that need not be a whole unit.
*/

#include <math.h>
#include <string.h>

#include "cseries.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmaps_sampling.h"
#include "cache/texture_cache.h"
#include "rasterizer/rasterizer.h"

#include "ae_panels.h"
#include "ae_waypoint_icons.h"
#include "ae_waypoint_ink.h"

/* bitmaps.c's and hud_draw.c's (their own enums) */
#define AE_WAYPOINT_BITMAP_TYPE_2D 0
#define AE_WAYPOINT_BITMAP_LINEAR_BIT 4
#define AE_WAYPOINT_BLEND_ALPHA_MULTIPLY_ADD 7
/* a sprite larger than this each way is not read (a waypoint arrow's is 32 or so) */
#define AE_WAYPOINT_INK_MAXIMUM_SPRITE 256
/* the sprites whose ink was read (a map has a few arrows) */
#define AE_WAYPOINT_INK_CACHED 16

static struct
{
	boolean drawn;
	struct bitmap_data const *bitmap;
	real_rectangle2d clip;
	real scale;
	real theta;
} ae_waypoint_arrow;

static struct
{
	struct bitmap_data const *bitmap;
	unsigned long pixels;	/* its pixels' address when read (a map's next bitmap may be at the same place) */
	real_rectangle2d clip;
	real middle;	/* the ink's middle from the sprite's, a fraction of its width */
} ae_waypoint_inks[AE_WAYPOINT_INK_CACHED];
static long ae_waypoint_ink_next;

void platform_log(char const *format, ...);

static unsigned char ae_waypoint_ink_weights[AE_WAYPOINT_INK_MAXIMUM_SPRITE * AE_WAYPOINT_INK_MAXIMUM_SPRITE];

void ae_waypoint_arrow_drawn(struct bitmap_data const *bitmap, real_rectangle2d const *clip, real scale, real theta)
{
	ae_waypoint_arrow.drawn = bitmap && clip;
	if (!ae_waypoint_arrow.drawn)
		return;
	ae_waypoint_arrow.bitmap = bitmap;
	ae_waypoint_arrow.clip = *clip;
	ae_waypoint_arrow.scale = scale;
	ae_waypoint_arrow.theta = theta;
}

void ae_waypoint_arrow_forget(void)
{
	ae_waypoint_arrow.drawn = FALSE;
}

void ae_waypoint_ink_forget_all(void)
{
	memset(ae_waypoint_inks, 0, sizeof(ae_waypoint_inks));
	ae_waypoint_ink_next = 0;
	ae_waypoint_arrow.drawn = FALSE;
}

/* a sprite's ink middle (a fraction of its width; 0 for a sprite with no ink) from its bitmap's pixels: FALSE while
they are not in memory */
static boolean ae_waypoint_ink_read(struct bitmap_data const *bitmap, real_rectangle2d const *clip, real *middle)
{
	struct bitmap_data *pixels = (struct bitmap_data *)bitmap;
	long x0, x1, y0, y1;
	long width, height;
	long x, y;
	int maximum = 0;
	int left, right;

	if (bitmap->type != AE_WAYPOINT_BITMAP_TYPE_2D || TEST_FLAG(bitmap->flags, AE_WAYPOINT_BITMAP_LINEAR_BIT) ||
		bitmap->width < 1 || bitmap->height < 1 || !_texture_cache_bitmap_get_hardware_format(pixels, FALSE, FALSE) ||
		!bitmap->base_address)
	{
		return FALSE;
	}
	x0 = (long)floor(clip->x0 * bitmap->width + 0.5f);
	x1 = (long)floor(clip->x1 * bitmap->width + 0.5f);
	y0 = (long)floor(clip->y0 * bitmap->height + 0.5f);
	y1 = (long)floor(clip->y1 * bitmap->height + 0.5f);
	x0 = MAX(x0, 0);
	y0 = MAX(y0, 0);
	x1 = MIN(x1, bitmap->width);
	y1 = MIN(y1, bitmap->height);
	width = x1 - x0;
	height = y1 - y0;
	*middle = 0.0f;
	if (width < 1 || height < 1 || width > AE_WAYPOINT_INK_MAXIMUM_SPRITE || height > AE_WAYPOINT_INK_MAXIMUM_SPRITE)
		return TRUE;
	/* (a pixel's weight: its alpha and its brightness, the less; an additive sprite's black is no ink) */
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			real_point2d texel;
			pixel32 pixel;
			int alpha, red, green, blue, weight;

			texel.x = ((real)(x0 + x) + 0.5f) / (real)bitmap->width;
			texel.y = ((real)(y0 + y) + 0.5f) / (real)bitmap->height;
			pixel = bitmap_2d_get_pixel(pixels, &texel, 1.0f);
			alpha = (int)((pixel >> 24) & 0xff);
			red = (int)((pixel >> 16) & 0xff);
			green = (int)((pixel >> 8) & 0xff);
			blue = (int)(pixel & 0xff);
			weight = MAX(red, MAX(green, blue));
			weight = MIN(weight, alpha);
			ae_waypoint_ink_weights[y * width + x] = (unsigned char)weight;
			maximum = MAX(maximum, weight);
		}
	}
	/* (its ink: a quarter of its strongest pixel or more) */
	if (maximum < 4 || !ae_waypoint_ink_columns(ae_waypoint_ink_weights, (int)width, (int)height, maximum / 4,
		&left, &right))
	{
		return TRUE;
	}
	*middle = ae_waypoint_ink_middle(left, right, (int)width);
	platform_log("waypoint: arrow sprite %ldx%ld at %ld,%ld of a %dx%d bitmap (format %d): ink columns %d..%d, "
		"its middle %+.2f of the sprite's width from the sprite's", width, height, x0, y0, (int)bitmap->width,
		(int)bitmap->height, (int)bitmap->format, left, right, *middle);
	return TRUE;
}

real ae_waypoint_arrow_ink_x(void)
{
	struct bitmap_data const *bitmap = ae_waypoint_arrow.bitmap;
	real_rectangle2d const *clip = &ae_waypoint_arrow.clip;
	unsigned long pixels;
	real middle;
	long index;

	/* (a turned arrow, at the view's edge, has its label beside it) */
	if (!ae_waypoint_arrow.drawn || !bitmap || ae_waypoint_arrow.theta != 0.0f)
		return 0.0f;
	pixels = (unsigned long)bitmap->base_address;
	for (index = 0; index < AE_WAYPOINT_INK_CACHED; index++)
	{
		if (ae_waypoint_inks[index].bitmap == bitmap && ae_waypoint_inks[index].pixels == pixels &&
			!memcmp(&ae_waypoint_inks[index].clip, clip, sizeof(*clip)))
		{
			break;
		}
	}
	if (index == AE_WAYPOINT_INK_CACHED)
	{
		if (!pixels || !ae_waypoint_ink_read(bitmap, clip, &middle))
			return 0.0f;
		index = ae_waypoint_ink_next;
		ae_waypoint_ink_next = (ae_waypoint_ink_next + 1) % AE_WAYPOINT_INK_CACHED;
		ae_waypoint_inks[index].bitmap = bitmap;
		ae_waypoint_inks[index].pixels = pixels;
		ae_waypoint_inks[index].clip = *clip;
		ae_waypoint_inks[index].middle = middle;
	}

	return ae_waypoint_inks[index].middle * (clip->x1 - clip->x0) * (real)bitmap->width * ae_waypoint_arrow.scale;
}

void ae_waypoint_sprite_draw(struct bitmap_data const *bitmap, real_rectangle2d const *clip, real x, real y,
	real scale, pixel32 color)
{
	struct dynamic_screen_vertex vertices[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	real half_width;
	real half_height;
	short index;

	if (!bitmap || !clip)
		return;
	/* (its size as hud_draw_bitmap_internal makes it: each half a whole unit, its middle where asked) */
	half_width = (real)fast_ftol((clip->x1 - clip->x0) * (real)bitmap->width * 0.5f * scale);
	half_height = (real)fast_ftol((clip->y1 - clip->y0) * (real)bitmap->height * 0.5f * scale);
	for (index = 0; index < 4; index++)
	{
		boolean right = ((index + 1) & 2) != 0;
		boolean bottom = index > 1;

		vertices[index].position.x = right ? x + half_width : x - half_width;
		vertices[index].position.y = bottom ? y + half_height : y - half_height;
		vertices[index].texture_coordinates.x = right ? clip->x1 : clip->x0;
		vertices[index].texture_coordinates.y = bottom ? clip->y1 : clip->y0;
		vertices[index].color = color;
	}
	csmemset(&parameters, 0, sizeof(parameters));
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.map_scale[0].i = 1.0f;
	parameters.framebuffer_blend_function = AE_WAYPOINT_BLEND_ALPHA_MULTIPLY_ADD;
	parameters.map[0] = (struct bitmap_data *)bitmap;
	if (ae_panels_hiding_ce())
		return;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}
