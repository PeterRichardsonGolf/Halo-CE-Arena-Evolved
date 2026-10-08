/*
AE_DRAW_STUB.C

The recording ae_draw.h of the unit tests (ae_draw_stub.h). Its state follows
ae_draw.c: a view (set by ae_draw_view, else the whole frame), a clip stack
(layout units, nested clips intersecting, at most 8 stored), a global alpha
(ae_draw_set_alpha; reset by ae_draw_view_full and by ae_stub_reset, the
stub's Present). Text measures as ae_draw.c measures it: AE_FONT_TITLE /
ROW / BODY with OpenCE / Overpass 900 / Overpass 750 (size an em, y the
capitals' top), the device fonts square (size wide a glyph, size tall).
*/

#include "ae_draw_stub.h"
#include "ae_font.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAXIMUM_CALLS = 8192, MAXIMUM_CLIPS = 8 };

static struct
{
	struct ae_layout layout;
	struct ae_view view;
	float clips[MAXIMUM_CLIPS][4];
	int clip_depth;
	float alpha;
	struct ae_stub_call calls[MAXIMUM_CALLS];
	int count;
	/* a call found the record full */
	int overflowed;
	unsigned int frame;
	int images;
	/* the faces' data (kept while loaded) */
	unsigned char *faces[AE_NUMBER_OF_FACES];
} stub;

/* ---------- the faces */

static unsigned char *read_file(const char *path, int *size)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data = NULL;
	long length;

	*size = 0;
	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) > 0 && fseek(file, 0, SEEK_SET) == 0)
	{
		data = malloc((size_t)length);
		if (data && fread(data, 1, (size_t)length, file) == (size_t)length)
			*size = (int)length;
		else
		{
			free(data);
			data = NULL;
		}
	}
	fclose(file);
	return data;
}

static void load_faces(void)
{
	static const char *const files[AE_NUMBER_OF_FACES] =
	{
		"port/assets/fonts/OpenCE-Regular.ttf", "port/assets/fonts/Overpass-900.ttf",
		"port/assets/fonts/Overpass-750.ttf",
	};
	int face, size;

	for (face = 0; face < AE_NUMBER_OF_FACES; face++)
	{
		if (ae_font_ready(face))
			continue;
		free(stub.faces[face]);
		stub.faces[face] = read_file(files[face], &size);
		if (!stub.faces[face] || !ae_font_load(face, stub.faces[face], size))
			printf("ae draw stub: cannot load %s (run from the repo root)\n", files[face]);
	}
}

/* the AE face of a font (-1: a device font) */
static int face_of(int font)
{
	switch (font)
	{
	case AE_FONT_TITLE: return AE_FACE_OPENCE;
	case AE_FONT_ROW: return AE_FACE_OVERPASS_900;
	case AE_FONT_BODY: return AE_FACE_OVERPASS_750;
	}
	return -1;
}

/* ---------- the stub's own calls */

void ae_stub_reset(float layout_width, float window_height)
{
	load_faces();
	stub.layout.height = (float)AE_LAYOUT_HEIGHT;
	stub.layout.scale = window_height > 0.0f ? window_height / (float)AE_LAYOUT_HEIGHT : 1.0f;
	stub.layout.width = layout_width;
	stub.layout.origin_x = stub.layout.origin_y = 0.0f;
	ae_layout_view(0.0f, 0.0f, stub.layout.width, stub.layout.height, &stub.view);
	stub.clip_depth = 0;
	stub.alpha = 1.0f;
	stub.count = 0;
	stub.overflowed = 0;
	stub.frame++;
}

int ae_stub_count(void)
{
	return stub.count;
}

int ae_stub_overflowed(void)
{
	return stub.overflowed;
}

struct ae_stub_call const *ae_stub_get(int index)
{
	return index >= 0 && index < stub.count ? &stub.calls[index] : NULL;
}

int ae_stub_find_text(const char *text, int from)
{
	int index;

	for (index = from < 0 ? 0 : from; index < stub.count; index++)
	{
		if (stub.calls[index].kind == AE_STUB_TEXT && !strcmp(stub.calls[index].text, text))
			return index;
	}
	return -1;
}

/* a point of a view (its own units) in window pixels */
static void view_to_pixels(struct ae_view const *view, float x, float y, float *pixel_x, float *pixel_y)
{
	float layout_x, layout_y;

	ae_view_to_layout(view, x, y, &layout_x, &layout_y);
	ae_layout_to_pixels(&stub.layout, layout_x, layout_y, pixel_x, pixel_y);
}

void ae_stub_pixels(struct ae_stub_call const *call, struct ae_rect *pixels)
{
	float x0 = call->x, y0 = call->y, x1 = call->x + call->width, y1 = call->y + call->height;
	float right, bottom;

	if (call->kind == AE_STUB_TEXT)
	{
		if (call->align == AE_ALIGN_CENTER)
			x0 -= call->width * 0.5f;
		else if (call->align == AE_ALIGN_RIGHT)
			x0 -= call->width;
		x1 = x0 + call->width;
		y0 = call->y + call->top;
		y1 = call->y + call->bottom;
	}
	else if (call->kind == AE_STUB_LINE)
	{
		float half = call->thickness * 0.5f;

		x0 = (call->x < call->x1 ? call->x : call->x1) - half;
		x1 = (call->x < call->x1 ? call->x1 : call->x) + half;
		y0 = (call->y < call->y1 ? call->y : call->y1) - half;
		y1 = (call->y < call->y1 ? call->y1 : call->y) + half;
	}
	view_to_pixels(&call->view, x0, y0, &pixels->x, &pixels->y);
	view_to_pixels(&call->view, x1, y1, &right, &bottom);
	pixels->width = right - pixels->x;
	pixels->height = bottom - pixels->y;
}

float ae_stub_text_em_pixels(struct ae_stub_call const *call)
{
	return call->size * call->view.scale * stub.layout.scale;
}

/* ---------- recording */

static unsigned int faded(unsigned int rgba)
{
	float alpha;

	if (stub.alpha >= 1.0f)
		return rgba;
	alpha = (float)(rgba & 0xFFu) * stub.alpha;
	return (rgba & 0xFFFFFF00u) | (unsigned int)floorf(alpha + 0.5f);
}

/* a new call of a kind at x, y, width, height (the view's units), with the view and clip current now (NULL: no
room left) */
static struct ae_stub_call *record(int kind, float x, float y, float width, float height)
{
	struct ae_stub_call *call;

	if (stub.count >= MAXIMUM_CALLS)
	{
		stub.overflowed = 1;
		return NULL;
	}
	call = &stub.calls[stub.count++];
	memset(call, 0, sizeof(*call));
	call->kind = kind;
	call->x = x;
	call->y = y;
	call->width = width;
	call->height = height;
	call->view = stub.view;
	call->clipped = stub.clip_depth > 0;
	if (call->clipped)
	{
		float const *clip = stub.clips[(stub.clip_depth < MAXIMUM_CLIPS ? stub.clip_depth : MAXIMUM_CLIPS) - 1];

		ae_view_from_layout(&stub.view, clip[0], clip[1], &call->clip[0], &call->clip[1]);
		ae_view_from_layout(&stub.view, clip[2], clip[3], &call->clip[2], &call->clip[3]);
	}
	else
	{
		call->clip[2] = ae_view_width(&stub.view);
		call->clip[3] = (float)AE_LAYOUT_HEIGHT;
	}
	return call;
}

void ae_draw_current_layout(struct ae_layout *layout)
{
	*layout = stub.layout;
}

void ae_draw_view(float x, float y, float width, float height)
{
	ae_layout_view(x, y, width, height, &stub.view);
	stub.clip_depth = 0;
	record(AE_STUB_VIEW, x, y, width, height);
}

void ae_draw_view_full(void)
{
	ae_draw_view(0.0f, 0.0f, stub.layout.width, stub.layout.height);
	stub.alpha = 1.0f;
}

float ae_draw_view_width(void)
{
	return ae_view_width(&stub.view);
}

void ae_draw_current_view(struct ae_view *view)
{
	*view = stub.view;
}

float ae_draw_units_per_pixel(void)
{
	float pixels = stub.layout.scale * stub.view.scale;

	return pixels > 0.0f ? 1.0f / pixels : 1.0f;
}

void ae_draw_set_alpha(float alpha)
{
	stub.alpha = alpha >= 1.0f ? 1.0f : alpha > 0.0f ? alpha : 0.0f;
}

void ae_draw_clip_push(float x, float y, float width, float height)
{
	float outer[4], *clip;

	if (stub.clip_depth > 0)
		memcpy(outer, stub.clips[(stub.clip_depth < MAXIMUM_CLIPS ? stub.clip_depth : MAXIMUM_CLIPS) - 1],
			sizeof(outer));
	else
	{
		outer[0] = stub.view.x;
		outer[1] = stub.view.y;
		outer[2] = stub.view.x + stub.view.width;
		outer[3] = stub.view.y + stub.view.height;
	}
	/* (too deep: the innermost clip again, so pops still pair) */
	if (stub.clip_depth < MAXIMUM_CLIPS)
	{
		clip = stub.clips[stub.clip_depth];
		ae_view_to_layout(&stub.view, x, y, &clip[0], &clip[1]);
		ae_view_to_layout(&stub.view, x + width, y + height, &clip[2], &clip[3]);
		clip[0] = fmaxf(clip[0], outer[0]);
		clip[1] = fmaxf(clip[1], outer[1]);
		clip[2] = fmaxf(clip[0], fminf(clip[2], outer[2]));
		clip[3] = fmaxf(clip[1], fminf(clip[3], outer[3]));
	}
	stub.clip_depth++;
	record(AE_STUB_CLIP_PUSH, x, y, width, height);
}

void ae_draw_clip_pop(void)
{
	if (stub.clip_depth > 0)
		stub.clip_depth--;
	record(AE_STUB_CLIP_POP, 0.0f, 0.0f, 0.0f, 0.0f);
}

void ae_draw_gradient(float x, float y, float width, float height, float radius, unsigned int top, unsigned int bottom)
{
	struct ae_stub_call *call = record(AE_STUB_GRADIENT, x, y, width, height);

	if (!call)
		return;
	call->thickness = radius;
	call->rgba = faded(top);
	call->rgba_bottom = faded(bottom);
}

void ae_draw_rect(float x, float y, float width, float height, float radius, unsigned int rgba)
{
	struct ae_stub_call *call = record(AE_STUB_RECT, x, y, width, height);

	if (!call)
		return;
	/* (the radius in size: a rect has no text) */
	call->size = radius;
	call->rgba = call->rgba_bottom = faded(rgba);
}

void ae_draw_outline(float x, float y, float width, float height, float radius, float thickness, unsigned int rgba)
{
	struct ae_stub_call *call = record(AE_STUB_OUTLINE, x, y, width, height);

	if (!call)
		return;
	call->size = radius;
	call->thickness = thickness;
	call->rgba = call->rgba_bottom = faded(rgba);
}

void ae_draw_line(float x0, float y0, float x1, float y1, float thickness, unsigned int rgba)
{
	struct ae_stub_call *call = record(AE_STUB_LINE, x0, y0, x1 - x0, y1 - y0);

	if (!call)
		return;
	call->x1 = x1;
	call->y1 = y1;
	call->thickness = thickness;
	call->rgba = call->rgba_bottom = faded(rgba);
}

/* a device font's width: size a glyph, tracking ems after every glyph but the last */
static float square_width(float size, float tracking_em, const char *utf8)
{
	const char *cursor = utf8;
	int count = 0;

	while (ae_font_utf8_next(&cursor))
		count++;
	return (float)count * size + (count > 1 ? tracking_em * size * (float)(count - 1) : 0.0f);
}

float ae_draw_text_tracked_width(int font, float size, float tracking_em, const char *utf8)
{
	int face = face_of(font);

	if (!utf8)
		return 0.0f;
	if (face < 0)
		return square_width(size, tracking_em, utf8);
	return ae_font_measure(face, size, tracking_em, utf8, NULL, NULL);
}

float ae_draw_text_width(int font, float size, const char *utf8)
{
	return ae_draw_text_tracked_width(font, size, 0.0f, utf8);
}

float ae_draw_text_box(int font, float size, const char *utf8, float *top, float *bottom)
{
	int face = face_of(font);
	float padding = 2.0f * ae_draw_units_per_pixel();
	float width, ink_top, ink_bottom, cap;

	if (!utf8)
		utf8 = "";
	if (face < 0)
	{
		*top = -padding;
		*bottom = size + padding;
		return square_width(size, 0.0f, utf8);
	}
	width = ae_font_measure(face, size, 0.0f, utf8, &ink_top, &ink_bottom);
	cap = ae_font_cap_height(face, size);
	*top = cap - ink_top - padding;
	*bottom = cap + ink_bottom + padding;
	return width;
}

float ae_draw_text_tracked(int font, float size, float tracking_em, float x, float y, int align, unsigned int rgba,
	const char *utf8)
{
	struct ae_stub_call *call;
	float width, top, bottom;

	if (!utf8 || !*utf8)
		return 0.0f;
	width = ae_draw_text_tracked_width(font, size, tracking_em, utf8);
	ae_draw_text_box(font, size, utf8, &top, &bottom);
	call = record(AE_STUB_TEXT, x, y, width, bottom - top);
	if (call)
	{
		call->font = font;
		call->align = align;
		call->size = size;
		call->thickness = tracking_em;
		call->top = top;
		call->bottom = bottom;
		call->rgba = call->rgba_bottom = faded(rgba);
		snprintf(call->text, sizeof(call->text), "%s", utf8);
	}
	return width;
}

float ae_draw_text(int font, float size, float x, float y, int align, unsigned int rgba, const char *utf8)
{
	return ae_draw_text_tracked(font, size, 0.0f, x, y, align, rgba, utf8);
}

float ae_draw_button_width(int device_font, int button, float size)
{
	(void)device_font;
	return button >= 0 && button < AE_NUMBER_OF_BUTTONS ? size : 0.0f;
}

float ae_draw_button(int device_font, int button, float size, float x, float y, unsigned int rgba)
{
	struct ae_stub_call *call;

	if (button < 0 || button >= AE_NUMBER_OF_BUTTONS)
		return 0.0f;
	call = record(AE_STUB_BUTTON, x, y, size, size);
	if (call)
	{
		call->font = device_font;
		call->align = button;
		call->size = size;
		call->rgba = call->rgba_bottom = faded(rgba);
	}
	return size;
}

int ae_draw_image_load(const unsigned char *rgba, int width, int height)
{
	if (!rgba || width <= 0 || height <= 0)
		return 0;
	return ++stub.images;
}

void ae_draw_image(int id, float x, float y, float width, float height, unsigned int tint)
{
	struct ae_stub_call *call;

	if (id <= 0)
		return;
	call = record(AE_STUB_IMAGE, x, y, width, height);
	if (call)
	{
		call->align = id;
		call->rgba = call->rgba_bottom = faded(tint);
	}
}

void ae_draw_pointer_to_layout(short menu_x, short menu_y, float *x, float *y)
{
	int screen_width = (int)floorf(stub.layout.width * stub.layout.scale + 0.5f);

	ae_layout_from_menu_point(menu_x, menu_y, screen_width, stub.layout.width, x, y);
}

int ae_draw_available(void)
{
	return 1;
}

unsigned int ae_draw_frame(void)
{
	return stub.frame;
}

void ae_draw_present(unsigned int framebuffer, int width, int height)
{
	/* (a new frame, the picture's: as ae_draw_layout lays it out) */
	(void)framebuffer;
	if (width > 0 && height > 0)
		ae_stub_reset((float)width * (float)AE_LAYOUT_HEIGHT / (float)height, (float)height);
	else
		ae_stub_reset(stub.layout.width, stub.layout.scale * (float)AE_LAYOUT_HEIGHT);
}
