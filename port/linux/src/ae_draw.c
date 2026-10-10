/*
AE_DRAW.C

The AE menus' renderer (ae_draw.h): shapes, text, button glyphs and images
gathered while the game draws a frame, drawn at its Present into the back
buffer, over the game's picture.

A deliberate fork of ChupathingyCE's game-browser overlay,
port/linux/src/ui_overlay.c (added in e1df0d43; forked as of 2ec23dae, its
last change), so that upstream's overlay and AE's menus never have to agree
(the merge-isolation rule of the AE menus' design). What is the same: each
shape is a quad whose fragment shader works out a rounded rectangle's edge
(filled, or its outline) from the distance to it; text is glyphs
(posix_ui_font.c) packed into one atlas texture as they are first drawn at a
size; the button glyph tables; the GL state saved and put back.

What changed from ui_overlay.c:
- symbols ui_overlay_* -> ae_draw_*; the file is built in every build, a stub
  without HALO_GAME_BROWSER (ae_draw_available() is 0 then);
- the layout is 1080 units tall over the whole picture, as wide as its aspect
  (ae_layout.c), not the menus' 640x480;
- views (a split-screen part, drawn in a 1080-tall space of its own) and a
  clip stack, clipped per quad in the fragment shader;
- drawing is in call order: text is laid out where it was drawn among the
  shapes (the overlay drew all text over all shapes), so a dialog covers the
  text of the screen under it;
- images (RGBA textures, at most 64 live and 256 MB, 64 MB on 32-bit) drawn
  as quads with a tint;
- drawn into the back buffer's framebuffer before the screenshot is taken
  (d3d8_gl.c), not into the window after the display blit: screenshots and
  recordings show AE's screens;
- the keyboard's prompts are AE's menu keys (Esc, Page Up/Down, Q/E for
  the tabs), not the game's in-play keys the overlay showed;
- no cutouts (the game's pictures through the overlay);
- fonts: the look's own faces (ae_font.c, M2): OpenCE for AE_FONT_TITLE,
  Overpass 900 for AE_FONT_ROW, Overpass 750 for AE_FONT_BODY, sized by
  their em with y the capitals' top, and text boxes from the glyphs' own ink
  (ae_draw_text_box), never from the fonts' line metrics; a face that will
  not load falls back to the overlay's Noto Bold/Regular (as in M1: size the
  line's height, y its top). The device fonts are Kenney's prompts, as in
  the overlay;
- the glyph cache is found through a hash (the overlay scans its list);
- lines (a rotated rounded rectangle), and a global alpha for fades.
*/

#include "ae_draw.h"
#include "ae_font.h"

#ifdef HALO_GAME_BROWSER

#include "platform.h"
#include "gl.h"
#include "xgpu.h"
#include "ui_font.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (xinput_sdl.c: the device the player last used: 0 keyboard, 1 Xbox-like,
2 PlayStation, 3 Nintendo) */
int platform_input_scheme(void);
/* (d3d8_gl.c: the width of the picture the game draws, 480 lines tall) */
long halo_screen_width(void);

enum
{
	MAXIMUM_QUADS = 8192,
	MAXIMUM_VERTICES = MAXIMUM_QUADS * 6,
	ATLAS_SIZE = 2048,
	MAXIMUM_GLYPHS = 4096,
	/* (the glyph hash's slots: a power of two, twice the glyphs, so a probe always ends at an empty slot) */
	GLYPH_SLOTS = 8192,
	MAXIMUM_TEXTS = 1024,
	MAXIMUM_TEXT = 64 * 1024,
	MAXIMUM_CLIPS = 8,
	MAXIMUM_IMAGES = 64,
	MAXIMUM_IMAGE_SIZE = 4096,
	MAXIMUM_RUNS = 256,
};

enum
{
	_quad_shape,
	_quad_text,
	_quad_image,
};

/* a quad as gathered (layout units, the view applied) */
struct quad
{
	int kind;
	/* text: its entry; image: its id (a slot reused later in the frame then draws nothing) */
	int index;
	float x, y, width, height;
	float radius, thickness;
	/* a shape turned about its centre (radians, clockwise on the screen: ae_draw_line) */
	float angle;
	unsigned int top, bottom;
	/* what of the layout it may draw into: x0, y0, x1, y1 */
	float clip[4];
};

/* (kind: 0 shape, 1 glyph, 2 image) */
struct vertex
{
	float x, y;
	float u, v;
	float local_x, local_y, half_width, half_height;
	float radius, thickness;
	float clip[4];
	float kind;
	unsigned char color[4];
};

/* the face that draws a font: AE's own (ae_font.c; id an AE_FACE_*: size an em, y the capitals' top) or the
overlay's (posix_ui_font.c; id a POSIX_UI_FONT_*: size the line's height, y its top, as in M1) */
struct face
{
	int ae;
	int id;
};

struct glyph
{
	struct face face;
	int pixel_size;
	unsigned int codepoint;
	/* its place in the atlas, and from the pen on the baseline (pixels) */
	short atlas_x, atlas_y, width, height, x_offset, y_offset;
	float advance;
};

struct text
{
	struct face face;
	int align;
	/* size: in layout units; tracking: in ems, after every glyph but the last */
	float size, x, y, tracking;
	unsigned int color;
	int offset;
};

struct image
{
	int live;
	unsigned int generation;
	unsigned int last_used;
	int width, height;
	/* its size as a texture (and as pixels while they wait), counted in ae.image_bytes */
	size_t bytes;
	/* its pixels until they are uploaded at a Present */
	unsigned char *pixels;
	/* (kept when the slot is freed and reused; a free slot's is deleted at the next Present) */
	GLuint texture;
};

/* a draw call's vertices, with the image they show */
struct run
{
	int first, count;
	GLuint texture;
};

static struct
{
	int ready, failed;
	GLuint program, vertex_array, vertex_buffer, atlas;
	GLint scale_location, offset_location, atlas_location, image_location;

	unsigned int frame;
	struct ae_layout layout;
	int layout_known;
	struct ae_view view;
	float clips[MAXIMUM_CLIPS][4];
	int clip_depth;
	/* 1 - the alpha every colour is multiplied by (ae_draw_set_alpha; 0, the default, is opaque) */
	float fade;

	struct quad quads[MAXIMUM_QUADS];
	int quad_count;
	/* (text is laid out at Present, when the picture's scale is known) */
	struct text texts[MAXIMUM_TEXTS];
	int text_count;
	char text[MAXIMUM_TEXT];
	int text_used;

	struct glyph glyphs[MAXIMUM_GLYPHS];
	int glyph_count;
	/* each slot a glyph's index + 1 (0: empty), by glyph_hash; linear probing */
	unsigned short glyph_slots[GLYPH_SLOTS];
	int shelf_x, shelf_y, shelf_height;
	float last_scale;

	struct image images[MAXIMUM_IMAGES];
	unsigned int image_generation;
	size_t image_bytes;

	struct run runs[MAXIMUM_RUNS];
	int run_count;
	struct vertex vertices[MAXIMUM_VERTICES];
} ae;

/* ---------- the device's buttons */

static const unsigned int button_glyphs[4][AE_NUMBER_OF_BUTTONS] =
{
	/* keyboard: AE's menu keys (ae_input.c), not the game's in-play keys:
	Enter, Esc, Delete, - (Y has no key: Tab steps the focus), - (START has
	none), Page Up, Page Down, Q, E (the tabs), Left, Right, F1 */
	{ 0xE05E, 0xE062, 0xE058, 0, 0, 0xE0A7, 0xE0A5, 0xE0B3, 0xE05A, 0xE020, 0xE022, 0xE067 },
	/* Xbox: A, B, X, Y, menu, LT, RT, LB, RB, -, -, view */
	{ 0xE004, 0xE006, 0xE01E, 0xE020, 0xE014, 0xE047, 0xE04D, 0xE043, 0xE049, 0, 0, 0xE01C },
	/* PlayStation: cross, circle, square, triangle, options, L2, R2, L1, R1,
	-, -, share */
	{ 0xE04B, 0xE041, 0xE051, 0xE053, 0xE009, 0xE07D, 0xE085, 0xE078, 0xE080, 0, 0, 0xE00B },
	/* Nintendo: its east and south buttons are A and B where Xbox's are B
	and A; L and R for both; plus and minus */
	{ 0xE005, 0xE007, 0xE019, 0xE017, 0xE00F, 0xE00B, 0xE013, 0xE00B, 0xE013, 0, 0, 0xE00D },
};

static const int button_fonts[4] =
{
	POSIX_UI_FONT_KEYBOARD, POSIX_UI_FONT_XBOX, POSIX_UI_FONT_PLAYSTATION, POSIX_UI_FONT_NINTENDO,
};

static const char *const button_words[AE_NUMBER_OF_BUTTONS] =
{
	"A", "B", "X", "Y", "Start", "LT", "RT", "LB", "RB", "<", ">", "Back",
};

/* a device (0 keyboard, 1 Xbox, 2 PlayStation, 3 Nintendo) from a device font */
static int device_of(int device_font)
{
	int value;

	switch (device_font)
	{
	case AE_FONT_KEYBOARD: return 0;
	case AE_FONT_XBOX: return 1;
	case AE_FONT_PLAYSTATION: return 2;
	case AE_FONT_NINTENDO: return 3;
	}
	value = platform_input_scheme();
	return value >= 0 && value < 4 ? value : 1;
}

static int posix_font(int font)
{
	switch (font)
	{
	case AE_FONT_BODY: return POSIX_UI_FONT_REGULAR;
	case AE_FONT_XBOX: return POSIX_UI_FONT_XBOX;
	case AE_FONT_PLAYSTATION: return POSIX_UI_FONT_PLAYSTATION;
	case AE_FONT_NINTENDO: return POSIX_UI_FONT_NINTENDO;
	case AE_FONT_KEYBOARD: return POSIX_UI_FONT_KEYBOARD;
	}
	/* (titles and rows, when their own faces will not load) */
	return POSIX_UI_FONT_BOLD;
}

static struct face posix_face(int posix)
{
	struct face face;

	face.ae = 0;
	face.id = posix;
	return face;
}

/* the face drawing a font: the look's own for titles, rows and body text (loaded the first time one is asked
for: measuring before the first Present agrees with drawing), else the overlay's */
static struct face face_of(int font)
{
	struct face face;
	int ae_face = -1;

	switch (font)
	{
	case AE_FONT_TITLE: ae_face = AE_FACE_OPENCE; break;
	case AE_FONT_ROW: ae_face = AE_FACE_OVERPASS_900; break;
	case AE_FONT_BODY: ae_face = AE_FACE_OVERPASS_750; break;
	}
	if (ae_face >= 0)
	{
		ae_font_embedded_load();
		if (ae_font_ready(ae_face))
		{
			face.ae = 1;
			face.id = ae_face;
			return face;
		}
	}
	return posix_face(posix_font(font));
}

/* a glyph's advance at a size (an em for AE's faces, a line's height for the overlay's), with the kerning after
the previous (0: none) */
static float advance_of(struct face face, float size, unsigned int codepoint, unsigned int previous)
{
	return face.ae ? ae_font_advance(face.id, size, codepoint, previous) :
		posix_ui_font_advance(face.id, size, codepoint, previous);
}

/* ---------- layout, views and clips */

static void layout_default(void)
{
	long screen_width = halo_screen_width();

	/* (before the first Present: the game's picture, 480 lines, its width wide) */
	ae_draw_layout(0, 0, screen_width > 0 ? (int)screen_width : 640, 480, &ae.layout);
}

static struct ae_layout const *layout(void)
{
	if (!ae.layout_known)
		layout_default();
	return &ae.layout;
}

void ae_draw_current_layout(struct ae_layout *out)
{
	*out = *layout();
}

void ae_draw_view(float x, float y, float width, float height)
{
	ae_layout_view(x, y, width, height, &ae.view);
	ae.clip_depth = 0;
}

/* the whole frame as the view (the first drawing of a frame sets it: that keeps the alpha set before it) */
static void view_full(void)
{
	ae_draw_view(0.0f, 0.0f, layout()->width, layout()->height);
}

void ae_draw_view_full(void)
{
	view_full();
	ae.fade = 0.0f;
}

static struct ae_view const *view(void)
{
	if (ae.view.height <= 0.0f)
		view_full();
	return &ae.view;
}

float ae_draw_view_width(void)
{
	return ae_view_width(view());
}

void ae_draw_current_view(struct ae_view *out)
{
	*out = *view();
}

float ae_draw_units_per_pixel(void)
{
	float pixels = layout()->scale * view()->scale;

	return pixels > 0.0f ? 1.0f / pixels : 1.0f;
}

void ae_draw_set_alpha(float alpha)
{
	ae.fade = alpha >= 1.0f ? 0.0f : alpha > 0.0f ? 1.0f - alpha : 1.0f;
}

/* a colour with the global alpha */
static unsigned int faded(unsigned int rgba)
{
	float alpha;

	if (ae.fade <= 0.0f)
		return rgba;
	alpha = (float)(rgba & 0xFFu) * (1.0f - ae.fade);
	return (rgba & 0xFFFFFF00u) | (unsigned int)floorf(alpha + 0.5f);
}

/* the clips stored (a push past the deepest stores none) */
static int clip_top(void)
{
	return ae.clip_depth < MAXIMUM_CLIPS ? ae.clip_depth : MAXIMUM_CLIPS;
}

/* the current clip (layout units): the innermost clip, else the view */
static void current_clip(float clip[4])
{
	if (ae.clip_depth > 0)
	{
		memcpy(clip, ae.clips[clip_top() - 1], sizeof(float) * 4);
		return;
	}
	clip[0] = view()->x;
	clip[1] = view()->y;
	clip[2] = view()->x + view()->width;
	clip[3] = view()->y + view()->height;
}

void ae_draw_clip_push(float x, float y, float width, float height)
{
	float outer[4], *clip;

	current_clip(outer);
	/* (too deep: the innermost clip again, so pops still pair) */
	if (ae.clip_depth >= MAXIMUM_CLIPS)
	{
		ae.clip_depth++;
		return;
	}
	clip = ae.clips[ae.clip_depth++];
	ae_view_to_layout(view(), x, y, &clip[0], &clip[1]);
	ae_view_to_layout(view(), x + width, y + height, &clip[2], &clip[3]);
	clip[0] = fmaxf(clip[0], outer[0]);
	clip[1] = fmaxf(clip[1], outer[1]);
	clip[2] = fmaxf(clip[0], fminf(clip[2], outer[2]));
	clip[3] = fmaxf(clip[1], fminf(clip[3], outer[3]));
}

void ae_draw_clip_pop(void)
{
	if (ae.clip_depth > 0)
		ae.clip_depth--;
}

/* ---------- gathering */

int ae_draw_available(void)
{
	/* (no Present yet, or none with GL: the dedicated server, the null renderer) */
	return !ae.failed && ae.frame > 0;
}

unsigned int ae_draw_frame(void)
{
	return ae.frame;
}

/* a quad at x, y, width, height in the view's units, its clip the current one */
static struct quad *new_quad(int kind, float x, float y, float width, float height)
{
	struct quad *quad;

	if (ae.quad_count >= MAXIMUM_QUADS)
		return NULL;
	quad = &ae.quads[ae.quad_count++];
	memset(quad, 0, sizeof(*quad));
	quad->kind = kind;
	ae_view_to_layout(view(), x, y, &quad->x, &quad->y);
	quad->width = width * view()->scale;
	quad->height = height * view()->scale;
	current_clip(quad->clip);
	return quad;
}

void ae_draw_gradient(float x, float y, float width, float height, float radius, unsigned int top, unsigned int bottom)
{
	struct quad *quad = new_quad(_quad_shape, x, y, width, height);

	if (!quad)
		return;
	quad->radius = radius * view()->scale;
	quad->top = faded(top);
	quad->bottom = faded(bottom);
}

void ae_draw_rect(float x, float y, float width, float height, float radius, unsigned int rgba)
{
	ae_draw_gradient(x, y, width, height, radius, rgba, rgba);
}

void ae_draw_outline(float x, float y, float width, float height, float radius, float thickness, unsigned int rgba)
{
	struct quad *quad = new_quad(_quad_shape, x, y, width, height);

	if (!quad)
		return;
	quad->radius = radius * view()->scale;
	quad->thickness = (thickness > 0.0f ? thickness : 1.0f) * view()->scale;
	quad->top = quad->bottom = faded(rgba);
}

void ae_draw_line(float x0, float y0, float x1, float y1, float thickness, unsigned int rgba)
{
	float dx = x1 - x0, dy = y1 - y0;
	float length = sqrtf(dx * dx + dy * dy);
	float wide = thickness > 0.0f ? thickness : 1.0f;
	struct quad *quad;

	/* (a rounded rectangle along the stroke, a half thickness past each end: its ends are round) */
	quad = new_quad(_quad_shape, (x0 + x1) * 0.5f - (length + wide) * 0.5f, (y0 + y1) * 0.5f - wide * 0.5f,
		length + wide, wide);
	if (!quad)
		return;
	quad->radius = wide * 0.5f * view()->scale;
	quad->angle = atan2f(dy, dx);
	quad->top = quad->bottom = faded(rgba);
}

/* (UTF-8 read as ae_font.c reads it: invalid bytes are U+FFFD) */
static unsigned int next_codepoint(const char **cursor)
{
	return ae_font_utf8_next(cursor);
}

/* a string's width at a size, tracking ems after every glyph but the last */
static float measure(struct face face, float size, float tracking, const char *text)
{
	const char *cursor = text;
	unsigned int codepoint, previous = 0;
	float width = 0.0f;

	if (face.ae)
		return ae_font_measure(face.id, size, tracking, text, NULL, NULL);
	while ((codepoint = next_codepoint(&cursor)) != 0)
	{
		if (previous)
			width += tracking * size;
		width += posix_ui_font_advance(face.id, size, codepoint, previous);
		previous = codepoint;
	}
	return width;
}

float ae_draw_text_tracked_width(int font, float size, float tracking_em, const char *utf8)
{
	return utf8 ? measure(face_of(font), size, tracking_em, utf8) : 0.0f;
}

float ae_draw_text_width(int font, float size, const char *utf8)
{
	return ae_draw_text_tracked_width(font, size, 0.0f, utf8);
}

float ae_draw_text_box(int font, float size, const char *utf8, float *top, float *bottom)
{
	struct face face = face_of(font);
	float padding = 2.0f * ae_draw_units_per_pixel();
	float width, ink_top, ink_bottom, cap;

	if (!utf8)
		utf8 = "";
	if (!face.ae)
	{
		/* (the overlay's Noto stands in: its line, y its top, size its height) */
		*top = -padding;
		*bottom = size + padding;
		return measure(face, size, 0.0f, utf8);
	}
	/* (the ink from the glyphs' boxes, above and below the baseline, which is the cap height below y) */
	width = ae_font_measure(face.id, size, 0.0f, utf8, &ink_top, &ink_bottom);
	cap = ae_font_cap_height(face.id, size);
	*top = cap - ink_top - padding;
	*bottom = cap + ink_bottom + padding;
	return width;
}

float ae_draw_cap_height(int font, float size)
{
	struct face face = face_of(font);

	return face.ae ? ae_font_cap_height(face.id, size) : 0.7f * size;
}

/* text of a face at x, y in the view's units; its width in them */
static float add_text(struct face face, float size, float tracking, float x, float y, int align, unsigned int color,
	const char *text)
{
	int length = (int)strlen(text);
	float width = measure(face, size, tracking, text);
	struct quad *quad;
	struct text *entry;

	if (ae.text_count >= MAXIMUM_TEXTS || ae.text_used + length + 1 > MAXIMUM_TEXT)
		return width;
	quad = new_quad(_quad_text, x, y, 0.0f, size);
	if (!quad)
		return width;
	quad->index = ae.text_count;
	entry = &ae.texts[ae.text_count++];
	entry->face = face;
	entry->align = align;
	entry->size = size * view()->scale;
	entry->tracking = tracking;
	entry->x = quad->x;
	entry->y = quad->y;
	entry->color = faded(color);
	entry->offset = ae.text_used;
	memcpy(ae.text + ae.text_used, text, (size_t)length + 1);
	ae.text_used += length + 1;
	return width;
}

float ae_draw_text_tracked(int font, float size, float tracking_em, float x, float y, int align, unsigned int rgba,
	const char *utf8)
{
	return utf8 && *utf8 ? add_text(face_of(font), size, tracking_em, x, y, align, rgba, utf8) : 0.0f;
}

float ae_draw_text(int font, float size, float x, float y, int align, unsigned int rgba, const char *utf8)
{
	return ae_draw_text_tracked(font, size, 0.0f, x, y, align, rgba, utf8);
}

static void utf8_of(unsigned int codepoint, char *out)
{
	out[0] = (char)(0xE0 | (codepoint >> 12));
	out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
	out[2] = (char)(0x80 | (codepoint & 0x3F));
	out[3] = 0;
}

float ae_draw_button_width(int device_font, int button, float size)
{
	int device = device_of(device_font);
	char text[4];

	if (button < 0 || button >= AE_NUMBER_OF_BUTTONS)
		return 0.0f;
	if (!button_glyphs[device][button] || !posix_ui_font_has(button_fonts[device], button_glyphs[device][button]))
		return measure(posix_face(POSIX_UI_FONT_BOLD), size, 0.0f, button_words[button]);
	utf8_of(button_glyphs[device][button], text);
	return measure(posix_face(button_fonts[device]), size, 0.0f, text);
}

float ae_draw_button(int device_font, int button, float size, float x, float y, unsigned int rgba)
{
	int device = device_of(device_font);
	char text[4];

	if (button < 0 || button >= AE_NUMBER_OF_BUTTONS)
		return 0.0f;
	/* (a button the device's font lacks: its name) */
	if (!button_glyphs[device][button] || !posix_ui_font_has(button_fonts[device], button_glyphs[device][button]))
		return add_text(posix_face(POSIX_UI_FONT_BOLD), size, 0.0f, x, y, AE_ALIGN_LEFT, rgba, button_words[button]);
	utf8_of(button_glyphs[device][button], text);
	return add_text(posix_face(button_fonts[device]), size, 0.0f, x, y, AE_ALIGN_LEFT, rgba, text);
}

/* ---------- images */

static struct image *image_of(int id)
{
	struct image *image;
	unsigned int key;

	if (id <= 0)
		return NULL;
	key = (unsigned int)id - 1;
	image = &ae.images[key % MAXIMUM_IMAGES];
	return image->live && image->generation == key / MAXIMUM_IMAGES ? image : NULL;
}

/* the most the live images may take (map pictures are small: a 512x256 one is 512 KB) */
static size_t image_budget(void)
{
	return (size_t)(sizeof(void *) >= 8 ? 256 : 64) * 1024 * 1024;
}

/* the live image drawn longest ago (-1: none) */
static int least_recent_image(void)
{
	int slot, oldest = -1;

	for (slot = 0; slot < MAXIMUM_IMAGES; slot++)
	{
		if (ae.images[slot].live &&
			(oldest < 0 || ae.frame - ae.images[slot].last_used > ae.frame - ae.images[oldest].last_used))
		{
			oldest = slot;
		}
	}
	return oldest;
}

/* frees a slot (its ids then draw nothing; its texture goes at the next Present) */
static void image_free(int slot)
{
	struct image *image = &ae.images[slot];

	free(image->pixels);
	image->pixels = NULL;
	image->live = 0;
	ae.image_bytes -= image->bytes;
	image->bytes = 0;
}

int ae_draw_image_load(const unsigned char *rgba, int width, int height)
{
	struct image *image;
	size_t size;
	int slot;

	if (!rgba || width <= 0 || height <= 0 || width > MAXIMUM_IMAGE_SIZE || height > MAXIMUM_IMAGE_SIZE)
		return 0;
	size = (size_t)width * (size_t)height * 4;
	if (size > image_budget())
		return 0;
	/* (room: the images drawn longest ago go) */
	while (ae.image_bytes + size > image_budget() && (slot = least_recent_image()) >= 0)
		image_free(slot);
	for (slot = 0; slot < MAXIMUM_IMAGES && ae.images[slot].live; slot++)
		;
	if (slot >= MAXIMUM_IMAGES)
		image_free(slot = least_recent_image());
	image = &ae.images[slot];
	image->pixels = malloc(size);
	/* (out of memory: 0, and the images freed above to make room stay freed; their ids draw nothing, as after
	any eviction) */
	if (!image->pixels)
		return 0;
	memcpy(image->pixels, rgba, size);
	ae.image_generation = (ae.image_generation + 1) & 0xFFFFFF;
	image->generation = ae.image_generation;
	image->width = width;
	image->height = height;
	image->bytes = size;
	ae.image_bytes += size;
	image->last_used = ae.frame;
	image->live = 1;
	return (int)(image->generation * MAXIMUM_IMAGES + (unsigned int)slot) + 1;
}

void ae_draw_image(int id, float x, float y, float width, float height, unsigned int tint)
{
	struct image *image = image_of(id);
	struct quad *quad;

	if (!image)
		return;
	image->last_used = ae.frame;
	quad = new_quad(_quad_image, x, y, width, height);
	if (!quad)
		return;
	quad->index = id;
	quad->top = quad->bottom = faded(tint);
}

/* ---------- the pointer */

void ae_draw_pointer_to_layout(short menu_x, short menu_y, float *x, float *y)
{
	ae_layout_from_menu_point(menu_x, menu_y, (int)halo_screen_width(), layout()->width, x, y);
}

/* ---------- GL */

static const char vertex_source[] =
	"in vec2 position;\n"
	"in vec2 texture_coordinate;\n"
	"in vec4 shape;\n"
	"in vec2 edge;\n"
	"in vec4 color;\n"
	"in vec4 clip;\n"
	"in float kind;\n"
	"uniform vec2 scale;\n"
	"uniform vec2 offset;\n"
	"out vec2 v_texture_coordinate;\n"
	"out vec4 v_shape;\n"
	"out vec2 v_edge;\n"
	"out vec4 v_color;\n"
	"out vec4 v_clip;\n"
	"out float v_kind;\n"
	"void main()\n"
	"{\n"
	"\tv_texture_coordinate = texture_coordinate;\n"
	"\tv_shape = shape;\n"
	"\tv_edge = edge;\n"
	"\tv_color = color;\n"
	"\tv_clip = clip;\n"
	"\tv_kind = kind;\n"
	"\tgl_Position = vec4(position * scale + offset, 0.0, 1.0);\n"
	"}\n";

static const char fragment_source[] =
	"in vec2 v_texture_coordinate;\n"
	"in vec4 v_shape;\n"
	"in vec2 v_edge;\n"
	"in vec4 v_color;\n"
	"in vec4 v_clip;\n"
	"in float v_kind;\n"
	"uniform sampler2D atlas;\n"
	"uniform sampler2D image;\n"
	"out vec4 fragment;\n"
	"void main()\n"
	"{\n"
	"\tfloat alpha;\n"
	/* (pixels from the picture's top left: row 0 is its top) */
	"\tvec2 p = gl_FragCoord.xy;\n"
	"\tif (p.x < v_clip.x || p.y < v_clip.y || p.x >= v_clip.z || p.y >= v_clip.w)\n"
	"\t\tdiscard;\n"
	"\tif (v_kind > 1.5)\n"
	"\t{\n"
	"\t\tvec4 texel = texture(image, v_texture_coordinate);\n"
	"\t\tfragment = vec4(texel.rgb * v_color.rgb, texel.a * v_color.a);\n"
	"\t\treturn;\n"
	"\t}\n"
	"\tif (v_kind > 0.5)\n"
	"\t\talpha = texture(atlas, v_texture_coordinate).r;\n"
	"\telse\n"
	"\t{\n"
	/* the rounded rectangle's distance: shape.xy the point from its middle,
	shape.zw its half size, edge.x the corners' radius, edge.y the outline's
	thickness (0: filled) */
	"\t\tvec2 q = abs(v_shape.xy) - v_shape.zw + vec2(v_edge.x);\n"
	"\t\tfloat d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - v_edge.x;\n"
	"\t\talpha = clamp(0.5 - d, 0.0, 1.0);\n"
	"\t\tif (v_edge.y > 0.0)\n"
	"\t\t\talpha *= clamp(d + v_edge.y + 0.5, 0.0, 1.0);\n"
	"\t}\n"
	"\tfragment = vec4(v_color.rgb, v_color.a * alpha);\n"
	"}\n";

static GLuint compile(GLenum type, const char *body)
{
	char source[4096];
	GLuint shader = glCreateShader(type);
	const char *pointer = source;
	GLint status = 0;

#ifdef HALO_GLES
	snprintf(source, sizeof(source), "#version %s\nprecision highp float;\n%s", xgpu_capabilities.shading_language,
		body);
#elif defined(__APPLE__)
	snprintf(source, sizeof(source), "#version 410 core\n%s", body);
#else
	snprintf(source, sizeof(source), "#version 450 core\n%s", body);
#endif
	glShaderSource(shader, 1, &pointer, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status)
	{
		char log[2048];

		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		platform_log("arena menus: cannot compile a shader: %s", log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static void attribute(GLuint index, GLint size, GLenum type, GLboolean normalized, size_t offset)
{
	glEnableVertexAttribArray(index);
	glVertexAttribPointer(index, size, type, normalized, sizeof(struct vertex), (const void *)offset);
}

static int set_up(void)
{
	GLuint vertex_shader, fragment_shader;
	GLint status = 0;
	GLint saved_vertex_array = 0, saved_array_buffer = 0, saved_texture = 0;
	static const unsigned char zero = 0;

	if (ae.ready || ae.failed)
		return ae.ready;
	vertex_shader = compile(GL_VERTEX_SHADER, vertex_source);
	fragment_shader = compile(GL_FRAGMENT_SHADER, fragment_source);
	if (!vertex_shader || !fragment_shader)
	{
		ae.failed = 1;
		return 0;
	}
	ae.program = glCreateProgram();
	glAttachShader(ae.program, vertex_shader);
	glAttachShader(ae.program, fragment_shader);
	glBindAttribLocation(ae.program, 0, "position");
	glBindAttribLocation(ae.program, 1, "texture_coordinate");
	glBindAttribLocation(ae.program, 2, "shape");
	glBindAttribLocation(ae.program, 3, "edge");
	glBindAttribLocation(ae.program, 4, "color");
	glBindAttribLocation(ae.program, 5, "clip");
	glBindAttribLocation(ae.program, 6, "kind");
	glLinkProgram(ae.program);
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	glGetProgramiv(ae.program, GL_LINK_STATUS, &status);
	if (!status)
	{
		platform_log("arena menus: cannot link the renderer's program");
		ae.failed = 1;
		return 0;
	}
	ae.scale_location = glGetUniformLocation(ae.program, "scale");
	ae.offset_location = glGetUniformLocation(ae.program, "offset");
	ae.atlas_location = glGetUniformLocation(ae.program, "atlas");
	ae.image_location = glGetUniformLocation(ae.program, "image");

	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &saved_vertex_array);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &saved_array_buffer);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &saved_texture);
	glGenVertexArrays(1, &ae.vertex_array);
	glGenBuffers(1, &ae.vertex_buffer);
	glBindVertexArray(ae.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, ae.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(ae.vertices), NULL, GL_STREAM_DRAW);
	attribute(0, 2, GL_FLOAT, GL_FALSE, offsetof(struct vertex, x));
	attribute(1, 2, GL_FLOAT, GL_FALSE, offsetof(struct vertex, u));
	attribute(2, 4, GL_FLOAT, GL_FALSE, offsetof(struct vertex, local_x));
	attribute(3, 2, GL_FLOAT, GL_FALSE, offsetof(struct vertex, radius));
	attribute(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(struct vertex, color));
	attribute(5, 4, GL_FLOAT, GL_FALSE, offsetof(struct vertex, clip));
	attribute(6, 1, GL_FLOAT, GL_FALSE, offsetof(struct vertex, kind));
	glBindVertexArray(0);

	glGenTextures(1, &ae.atlas);
	glBindTexture(GL_TEXTURE_2D, ae.atlas);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	/* (an empty texel for space: glyphs keep away from 0, 0) */
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &zero);
	glBindVertexArray((GLuint)saved_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint)saved_array_buffer);
	glBindTexture(GL_TEXTURE_2D, (GLuint)saved_texture);
	ae.shelf_x = ae.shelf_y = 2;
	ae.ready = 1;
	return 1;
}

/* a glyph's first slot in the hash (its face, its pixel size and its code point mixed) */
static unsigned int glyph_hash(struct face face, int pixel_size, unsigned int codepoint)
{
	unsigned int hash = codepoint * 0x9E3779B1u;

	hash ^= ((unsigned int)pixel_size << 8 | (unsigned int)(face.id & 0x7F) << 1 | (unsigned int)(face.ae != 0)) *
		0x85EBCA77u;
	hash ^= hash >> 15;
	hash *= 0xC2B2AE3Du;
	hash ^= hash >> 13;
	return hash & (GLYPH_SLOTS - 1);
}

static void glyph_free(struct face face, unsigned char *bitmap)
{
	if (!bitmap)
		return;
	if (face.ae)
		ae_font_free(bitmap);
	else
		posix_ui_font_free(bitmap);
}

/* a glyph at a pixel size, packed into the atlas the first time (NULL: the
atlas is full; it starts over at the next frame) */
static struct glyph *glyph(struct face face, int pixel_size, unsigned int codepoint, int *atlas_full)
{
	struct glyph *entry;
	unsigned char *bitmap;
	unsigned int slot;
	int width, height, x_offset, y_offset;

	/* (at most MAXIMUM_GLYPHS of the GLYPH_SLOTS are taken: the probe ends at an empty slot, where a new glyph
	goes) */
	for (slot = glyph_hash(face, pixel_size, codepoint); ae.glyph_slots[slot]; slot = (slot + 1) & (GLYPH_SLOTS - 1))
	{
		entry = &ae.glyphs[ae.glyph_slots[slot] - 1];
		if (entry->codepoint == codepoint && entry->pixel_size == pixel_size && entry->face.ae == face.ae &&
			entry->face.id == face.id)
		{
			return entry;
		}
	}
	if (ae.glyph_count >= MAXIMUM_GLYPHS)
	{
		*atlas_full = 1;
		return NULL;
	}
	if (face.ae)
		bitmap = ae_font_glyph(face.id, (float)pixel_size, codepoint, &width, &height, &x_offset, &y_offset);
	else
		bitmap = posix_ui_font_glyph(face.id, (float)pixel_size, codepoint, &width, &height, &x_offset, &y_offset);
	if (!bitmap)
		width = height = 0;
	if (width > 0 && height > 0)
	{
		if (ae.shelf_x + width + 2 > ATLAS_SIZE)
		{
			ae.shelf_x = 2;
			ae.shelf_y += ae.shelf_height + 2;
			ae.shelf_height = 0;
		}
		if (width + 4 > ATLAS_SIZE || ae.shelf_y + height + 2 > ATLAS_SIZE)
		{
			glyph_free(face, bitmap);
			*atlas_full = 1;
			return NULL;
		}
	}
	ae.glyph_slots[slot] = (unsigned short)(ae.glyph_count + 1);
	entry = &ae.glyphs[ae.glyph_count++];
	entry->face = face;
	entry->pixel_size = pixel_size;
	entry->codepoint = codepoint;
	entry->width = (short)width;
	entry->height = (short)height;
	entry->x_offset = (short)x_offset;
	entry->y_offset = (short)y_offset;
	entry->advance = advance_of(face, (float)pixel_size, codepoint, 0);
	entry->atlas_x = (short)ae.shelf_x;
	entry->atlas_y = (short)ae.shelf_y;
	if (bitmap && width > 0 && height > 0)
	{
		glBindTexture(GL_TEXTURE_2D, ae.atlas);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexSubImage2D(GL_TEXTURE_2D, 0, ae.shelf_x, ae.shelf_y, width, height, GL_RED, GL_UNSIGNED_BYTE, bitmap);
		ae.shelf_x += width + 2;
		if (height > ae.shelf_height)
			ae.shelf_height = height;
	}
	glyph_free(face, bitmap);
	return entry;
}

static void forget_glyphs(void)
{
	ae.glyph_count = 0;
	memset(ae.glyph_slots, 0, sizeof(ae.glyph_slots));
	ae.shelf_x = ae.shelf_y = 2;
	ae.shelf_height = 0;
}

static void put_vertex(struct vertex *vertex, float x, float y, float u, float v, float local_x, float local_y,
	float half_width, float half_height, float radius, float thickness, float const clip[4], float kind,
	unsigned int color)
{
	vertex->x = x;
	vertex->y = y;
	vertex->u = u;
	vertex->v = v;
	vertex->local_x = local_x;
	vertex->local_y = local_y;
	vertex->half_width = half_width;
	vertex->half_height = half_height;
	vertex->radius = radius;
	vertex->thickness = thickness;
	memcpy(vertex->clip, clip, sizeof(vertex->clip));
	vertex->kind = kind;
	vertex->color[0] = (unsigned char)(color >> 24);
	vertex->color[1] = (unsigned char)(color >> 16);
	vertex->color[2] = (unsigned char)(color >> 8);
	vertex->color[3] = (unsigned char)color;
}

/* a textured rectangle (pixels) as two triangles */
static void put_textured(int *count, float left, float top, float right, float bottom, float u0, float v0, float u1,
	float v1, float const clip[4], float kind, unsigned int color)
{
	struct vertex *v;

	if (*count + 6 > MAXIMUM_VERTICES)
		return;
	v = &ae.vertices[*count];
	put_vertex(&v[0], left, top, u0, v0, 0, 0, 0, 0, 0, 0, clip, kind, color);
	put_vertex(&v[1], right, top, u1, v0, 0, 0, 0, 0, 0, 0, clip, kind, color);
	put_vertex(&v[2], left, bottom, u0, v1, 0, 0, 0, 0, 0, 0, clip, kind, color);
	put_vertex(&v[3], right, top, u1, v0, 0, 0, 0, 0, 0, 0, clip, kind, color);
	put_vertex(&v[4], right, bottom, u1, v1, 0, 0, 0, 0, 0, 0, clip, kind, color);
	put_vertex(&v[5], left, bottom, u0, v1, 0, 0, 0, 0, 0, 0, clip, kind, color);
	*count += 6;
}

/* a shape (layout units) as two triangles in pixels, a pixel larger all round for the smooth edge; a turned shape
(ae_draw_line) has its corners turned about its centre while their local coordinates stay the upright
rectangle's, so the fragment shader's rounded rectangle is drawn turned */
static void put_shape(int *count, const struct quad *quad, float const clip[4])
{
	static const float corners[6][2] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
	struct ae_layout const *l = &ae.layout;
	struct vertex *v;
	float left, top, half_width, half_height, centre_x, centre_y, radius, thickness, cosine = 1.0f, sine = 0.0f;
	const float grow = 1.0f;
	int corner;

	if (*count + 6 > MAXIMUM_VERTICES)
		return;
	v = &ae.vertices[*count];
	ae_layout_to_pixels(l, quad->x, quad->y, &left, &top);
	half_width = quad->width * l->scale * 0.5f;
	half_height = quad->height * l->scale * 0.5f;
	centre_x = left + half_width;
	centre_y = top + half_height;
	radius = quad->radius * l->scale;
	thickness = quad->thickness > 0.0f ? fmaxf(1.0f, quad->thickness * l->scale) : 0.0f;
	if (quad->angle != 0.0f)
	{
		cosine = cosf(quad->angle);
		sine = sinf(quad->angle);
	}
	for (corner = 0; corner < 6; corner++)
	{
		float local_x = corners[corner][0] * (half_width + grow);
		float local_y = corners[corner][1] * (half_height + grow);

		put_vertex(&v[corner], centre_x + local_x * cosine - local_y * sine, centre_y + local_x * sine + local_y * cosine,
			-1.0f, 0.0f, local_x, local_y, half_width, half_height, radius, thickness, clip, 0.0f,
			corners[corner][1] < 0 ? quad->top : quad->bottom);
	}
	*count += 6;
}

/* a text's glyphs as textured rectangles in pixels */
static void put_text(int *count, const struct text *entry, float const clip[4], int *atlas_full)
{
	struct ae_layout const *l = &ae.layout;
	const char *text = ae.text + entry->offset;
	const char *cursor = text;
	struct face face = entry->face;
	float size = entry->size * l->scale;
	int pixel_size = (int)floorf(size + 0.5f);
	float ascent, descent, pen_x, pen_y, width, tracking = entry->tracking * (float)pixel_size;
	unsigned int codepoint, previous = 0;

	if (pixel_size < 4)
		return;
	ae_layout_to_pixels(l, entry->x, entry->y, &pen_x, &pen_y);
	if (face.ae)
	{
		/* (y the capitals' top: the baseline the cap height below, at the unrounded size, as ae_draw_text_box
		has it) */
		pen_y += ae_font_cap_height(face.id, size);
	}
	else
	{
		if (!posix_ui_font_metrics(face.id, (float)pixel_size, &ascent, &descent))
			return;
		/* (y the line's top: the baseline an ascent below, centred in the size) */
		pen_y += ((float)pixel_size - (ascent + descent)) * 0.5f + ascent;
	}
	width = measure(face, (float)pixel_size, entry->tracking, text);
	if (entry->align == AE_ALIGN_CENTER)
		pen_x -= width * 0.5f;
	else if (entry->align == AE_ALIGN_RIGHT)
		pen_x -= width;
	pen_x = floorf(pen_x + 0.5f);
	pen_y = floorf(pen_y + 0.5f);
	while ((codepoint = next_codepoint(&cursor)) != 0)
	{
		struct glyph *g;

		if (previous)
			pen_x += advance_of(face, (float)pixel_size, codepoint, previous) -
				advance_of(face, (float)pixel_size, codepoint, 0) + tracking;
		g = glyph(face, pixel_size, codepoint, atlas_full);
		if (!g)
			return;
		if (g->width > 0)
		{
			put_textured(count, pen_x + g->x_offset, pen_y + g->y_offset, pen_x + g->x_offset + g->width,
				pen_y + g->y_offset + g->height, (float)g->atlas_x / ATLAS_SIZE, (float)g->atlas_y / ATLAS_SIZE,
				(float)(g->atlas_x + g->width) / ATLAS_SIZE, (float)(g->atlas_y + g->height) / ATLAS_SIZE, clip, 1.0f,
				entry->color);
		}
		pen_x += g->advance;
		previous = codepoint;
	}
}

/* an image's texture, its pixels uploaded the first time it is drawn (0: none) */
static GLuint image_texture(struct image *image)
{
	if (!image->pixels)
		return image->texture;
	if (!image->texture)
		glGenTextures(1, &image->texture);
	glBindTexture(GL_TEXTURE_2D, image->texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->width, image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	free(image->pixels);
	image->pixels = NULL;
	return image->texture;
}

/* the vertices from first on show this texture (0: whichever the run before shows) */
static void run_texture(int first, GLuint texture)
{
	struct run *run = ae.run_count ? &ae.runs[ae.run_count - 1] : NULL;

	if (run && (!texture || run->texture == texture || !run->texture))
	{
		if (texture)
			run->texture = texture;
		return;
	}
	if (ae.run_count >= MAXIMUM_RUNS)
		return;
	run = &ae.runs[ae.run_count++];
	run->first = first;
	run->count = 0;
	run->texture = texture;
}

/* the frame's quads as vertices, in the order they were drawn */
static int build_vertices(int *atlas_full)
{
	struct ae_layout const *l = &ae.layout;
	int index, count = 0;

	ae.run_count = 0;
	for (index = 0; index < ae.quad_count; index++)
	{
		const struct quad *quad = &ae.quads[index];
		float clip[4];

		ae_layout_to_pixels(l, quad->clip[0], quad->clip[1], &clip[0], &clip[1]);
		ae_layout_to_pixels(l, quad->clip[2], quad->clip[3], &clip[2], &clip[3]);
		if (clip[2] <= clip[0] || clip[3] <= clip[1])
			continue;
		switch (quad->kind)
		{
		case _quad_shape:
			run_texture(count, 0);
			put_shape(&count, quad, clip);
			break;
		case _quad_text:
			run_texture(count, 0);
			put_text(&count, &ae.texts[quad->index], clip, atlas_full);
			break;
		case _quad_image:
		{
			struct image *image = image_of(quad->index);
			GLuint texture = image ? image_texture(image) : 0;
			float left, top;

			if (!texture)
				break;
			/* (a run of its own when the image changes; none left: not drawn) */
			run_texture(count, texture);
			if (!ae.run_count || ae.runs[ae.run_count - 1].texture != texture)
				break;
			ae_layout_to_pixels(l, quad->x, quad->y, &left, &top);
			put_textured(&count, left, top, left + quad->width * l->scale, top + quad->height * l->scale, 0.0f, 0.0f,
				1.0f, 1.0f, clip, 2.0f, quad->top);
			break;
		}
		}
		if (ae.run_count)
			ae.runs[ae.run_count - 1].count = count - ae.runs[ae.run_count - 1].first;
	}
	return count;
}

/* (at every Present, drawn or not: GL's context is current) */
static void frame_done(void)
{
	int index;

	/* the textures of freed images */
	for (index = 0; index < MAXIMUM_IMAGES; index++)
	{
		if (!ae.images[index].live && ae.images[index].texture)
		{
			glDeleteTextures(1, &ae.images[index].texture);
			ae.images[index].texture = 0;
		}
	}
	ae.quad_count = ae.text_count = ae.text_used = 0;
	ae.clip_depth = 0;
	ae.view.height = 0.0f;
	ae.fade = 0.0f;
	ae.frame++;
}

void ae_draw_present(unsigned int framebuffer, int width, int height)
{
	int index, count, atlas_full = 0;
	GLint saved_vertex_array = 0, saved_array_buffer = 0, saved_program = 0, saved_active = 0;
	GLint saved_textures[2] = { 0, 0 }, saved_samplers[2] = { 0, 0 }, saved_draw_framebuffer = 0;
	GLint saved_viewport[4] = { 0, 0, 0, 0 };
	GLboolean saved_blend, saved_depth, saved_stencil, saved_cull, saved_scissor;
	GLint saved_blend_rgb_source = 0, saved_blend_rgb_destination = 0, saved_blend_alpha_source = 0,
		saved_blend_alpha_destination = 0, saved_blend_equation = 0;

	/* (the look's faces: once, from the game's data) */
	ae_font_embedded_load();
	if (width <= 0 || height <= 0)
	{
		frame_done();
		return;
	}
	ae_draw_layout(0, 0, width, height, &ae.layout);
	ae.layout_known = 1;
	if (!ae.quad_count || !set_up())
	{
		frame_done();
		return;
	}
	if (ae.layout.scale != ae.last_scale)
	{
		/* (other sizes: the glyphs packed again) */
		forget_glyphs();
		ae.last_scale = ae.layout.scale;
	}

	/* (the game's renderer keeps its bindings across frames: they are put
	back after; it binds a sampler to each unit, which would filter the atlas
	as it filters its textures) */
	saved_blend = glIsEnabled(GL_BLEND);
	saved_depth = glIsEnabled(GL_DEPTH_TEST);
	saved_stencil = glIsEnabled(GL_STENCIL_TEST);
	saved_cull = glIsEnabled(GL_CULL_FACE);
	saved_scissor = glIsEnabled(GL_SCISSOR_TEST);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &saved_vertex_array);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &saved_array_buffer);
	glGetIntegerv(GL_CURRENT_PROGRAM, &saved_program);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &saved_draw_framebuffer);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &saved_active);
	for (index = 1; index >= 0; index--)
	{
		glActiveTexture(GL_TEXTURE0 + (GLenum)index);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &saved_textures[index]);
		glGetIntegerv(GL_SAMPLER_BINDING, &saved_samplers[index]);
		glBindSampler((GLuint)index, 0);
	}
	glGetIntegerv(GL_VIEWPORT, saved_viewport);
	glGetIntegerv(GL_BLEND_SRC_RGB, &saved_blend_rgb_source);
	glGetIntegerv(GL_BLEND_DST_RGB, &saved_blend_rgb_destination);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &saved_blend_alpha_source);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &saved_blend_alpha_destination);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &saved_blend_equation);
#ifndef HALO_GL_NO_CLIP_CONTROL
	/* (the renderer's clip space is D3D's, y down (d3d8_gl.c's
	glClipControl); this one is GL's, as on macOS and Android, which have no
	glClipControl) */
	if (glClipControl)
		glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
#endif

	/* the vertices (packing new glyphs binds the atlas; new images their textures) */
	count = build_vertices(&atlas_full);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)framebuffer);
	glViewport(0, 0, width, height);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(ae.program);
	/* (pixel row 0, the picture's top, is the framebuffer's first row: GL's y = -1) */
	glUniform2f(ae.scale_location, 2.0f / (float)width, 2.0f / (float)height);
	glUniform2f(ae.offset_location, -1.0f, -1.0f);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ae.atlas);
	glUniform1i(ae.atlas_location, 0);
	glUniform1i(ae.image_location, 1);
	glBindVertexArray(ae.vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, ae.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(ae.vertices), NULL, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)count * (GLsizeiptr)sizeof(struct vertex), ae.vertices);
	for (index = 0; index < ae.run_count; index++)
	{
		if (ae.runs[index].count <= 0)
			continue;
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, ae.runs[index].texture);
		glDrawArrays(GL_TRIANGLES, ae.runs[index].first, ae.runs[index].count);
	}

	glBindVertexArray((GLuint)saved_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint)saved_array_buffer);
	glUseProgram((GLuint)saved_program);
	for (index = 1; index >= 0; index--)
	{
		glActiveTexture(GL_TEXTURE0 + (GLenum)index);
		glBindTexture(GL_TEXTURE_2D, (GLuint)saved_textures[index]);
		glBindSampler((GLuint)index, (GLuint)saved_samplers[index]);
	}
	glActiveTexture((GLenum)saved_active);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)saved_draw_framebuffer);
	glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
#ifndef HALO_GL_NO_CLIP_CONTROL
	if (glClipControl)
		glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
#endif
	glBlendFuncSeparate((GLenum)saved_blend_rgb_source, (GLenum)saved_blend_rgb_destination,
		(GLenum)saved_blend_alpha_source, (GLenum)saved_blend_alpha_destination);
	glBlendEquation((GLenum)saved_blend_equation);
	(saved_blend ? glEnable : glDisable)(GL_BLEND);
	(saved_depth ? glEnable : glDisable)(GL_DEPTH_TEST);
	(saved_stencil ? glEnable : glDisable)(GL_STENCIL_TEST);
	(saved_cull ? glEnable : glDisable)(GL_CULL_FACE);
	(saved_scissor ? glEnable : glDisable)(GL_SCISSOR_TEST);

	if (atlas_full)
		forget_glyphs();
	frame_done();
}

#else

/* ---------- without the game browser (configure.py --no-game-browser): no
fonts, so nothing draws and ae_draw_available() is 0 */

void ae_draw_current_layout(struct ae_layout *layout) { ae_draw_layout(0, 0, 0, 0, layout); }
void ae_draw_view(float x, float y, float width, float height) { (void)x; (void)y; (void)width; (void)height; }
void ae_draw_view_full(void) { }
float ae_draw_view_width(void) { return 0.0f; }
void ae_draw_clip_push(float x, float y, float width, float height) { (void)x; (void)y; (void)width; (void)height; }
void ae_draw_clip_pop(void) { }
void ae_draw_rect(float x, float y, float width, float height, float radius, unsigned int rgba)
{
	(void)x; (void)y; (void)width; (void)height; (void)radius; (void)rgba;
}
void ae_draw_gradient(float x, float y, float width, float height, float radius, unsigned int top, unsigned int bottom)
{
	(void)x; (void)y; (void)width; (void)height; (void)radius; (void)top; (void)bottom;
}
void ae_draw_outline(float x, float y, float width, float height, float radius, float thickness, unsigned int rgba)
{
	(void)x; (void)y; (void)width; (void)height; (void)radius; (void)thickness; (void)rgba;
}
float ae_draw_text(int font, float size, float x, float y, int align, unsigned int rgba, const char *utf8)
{
	(void)font; (void)size; (void)x; (void)y; (void)align; (void)rgba; (void)utf8;
	return 0.0f;
}
float ae_draw_text_width(int font, float size, const char *utf8) { (void)font; (void)size; (void)utf8; return 0.0f; }
float ae_draw_text_tracked(int font, float size, float tracking_em, float x, float y, int align, unsigned int rgba,
	const char *utf8)
{
	(void)font; (void)size; (void)tracking_em; (void)x; (void)y; (void)align; (void)rgba; (void)utf8;
	return 0.0f;
}
float ae_draw_text_tracked_width(int font, float size, float tracking_em, const char *utf8)
{
	(void)font; (void)size; (void)tracking_em; (void)utf8;
	return 0.0f;
}
float ae_draw_text_box(int font, float size, const char *utf8, float *top, float *bottom)
{
	(void)font; (void)size; (void)utf8;
	*top = *bottom = 0.0f;
	return 0.0f;
}
float ae_draw_cap_height(int font, float size) { (void)font; return 0.7f * size; }
void ae_draw_line(float x0, float y0, float x1, float y1, float thickness, unsigned int rgba)
{
	(void)x0; (void)y0; (void)x1; (void)y1; (void)thickness; (void)rgba;
}
void ae_draw_set_alpha(float alpha) { (void)alpha; }
void ae_draw_current_view(struct ae_view *view)
{
	view->x = view->y = view->width = view->height = 0.0f;
	view->scale = 1.0f;
}
float ae_draw_units_per_pixel(void) { return 1.0f; }
float ae_draw_button(int device_font, int button, float size, float x, float y, unsigned int rgba)
{
	(void)device_font; (void)button; (void)size; (void)x; (void)y; (void)rgba;
	return 0.0f;
}
float ae_draw_button_width(int device_font, int button, float size)
{
	(void)device_font; (void)button; (void)size;
	return 0.0f;
}
int ae_draw_image_load(const unsigned char *rgba, int width, int height) { (void)rgba; (void)width; (void)height; return 0; }
void ae_draw_image(int id, float x, float y, float width, float height, unsigned int tint)
{
	(void)id; (void)x; (void)y; (void)width; (void)height; (void)tint;
}
void ae_draw_pointer_to_layout(short menu_x, short menu_y, float *x, float *y)
{
	(void)menu_x; (void)menu_y;
	*x = *y = 0.0f;
}
int ae_draw_available(void) { return 0; }
unsigned int ae_draw_frame(void) { return 0; }
void ae_draw_present(unsigned int framebuffer, int width, int height) { (void)framebuffer; (void)width; (void)height; }

#endif
