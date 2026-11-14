/*
AE_DYNAMIC_RESOLUTION_GL.C

TARGET 60 FPS in the renderer (ae_dynamic_resolution_gl.h): the drawn part's mapping, the pass that scales it up over
the view, and the frame's time.
*/

#include "xgpu.h"
#include "ae_dynamic_resolution_gl.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

/* (port/linux/game/ae_dynamic_resolution.c) the percent for the next frame, from this one's time in ms (below 0: not
known) */
int ae_dynamic_resolution_policy(float frame_ms);

/* (d3d8_gl.c) the back buffer for the pass: the framebuffer to read, its texture, the framebuffer to draw into (the
multisampled one with multisampling), its size in pixels, its pixels a unit, its format; and the renderer's state
afterwards */
int halo_screen_back_buffer_gl(GLuint *source, GLuint *texture, GLuint *destination, unsigned long *width,
	unsigned long *height, float scale[2], GLenum *format);
void halo_screen_gl_restore(void);

float ae_dynres_scale = 1.0f;
int ae_dynres_drawing;

/* the view's top left corner, in the game's units, that the drawn part starts at */
static float anchor[2];

/* ---------- while a view's 3D passes draw */

float ae_dynres_coordinate(float coordinate, int axis)
{
	return anchor[axis] + (coordinate - anchor[axis]) * ae_dynres_scale;
}

float ae_dynres_area(void)
{
	return ae_dynres_scale * ae_dynres_scale;
}

void ae_dynres_texture(int screen, unsigned long width, unsigned long height, float scale[4])
{
	if (!ae_dynres_drawing || !screen || !width || !height)
		return;
	/* (a coordinate c reads the drawn part's anchor + (c - anchor) x scale, in the texture's units over its size) */
	scale[0] *= ae_dynres_scale;
	scale[1] *= ae_dynres_scale;
	scale[2] = anchor[0] * (1.0f - ae_dynres_scale) / (float)width;
	scale[3] = anchor[1] * (1.0f - ae_dynres_scale) / (float)height;
}

void halo_screen_dynamic_resolution_begin(short x0, short y0, short x1, short y1)
{
	(void)x1;
	(void)y1;
	ae_dynres_drawing = ae_dynres_scale < 0.999f;
	anchor[0] = (float)x0;
	anchor[1] = (float)y0;
}

/* ---------- the pass that scales the drawn part up */

static struct
{
	int failed;
	GLuint program, vertex_array, sampler;
	GLint map, limits;
	/* the drawn part copied, so the pass does not read what it draws */
	GLuint copy, copy_framebuffer;
	unsigned long width, height;
	GLenum format;
} pass;

static const char vertex_source[] =
	"void main()\n"
	"{\n"
	"\tgl_Position = vec4(float((gl_VertexID & 1) << 2) - 1.0, float((gl_VertexID & 2) << 1) - 1.0, 0.0, 1.0);\n"
	"}\n";

static const char fragment_source[] =
	"uniform sampler2D color_texture;\n"
	"/* the texel a pixel reads: its place times xy plus zw, kept inside the drawn part (limits) */\n"
	"uniform vec4 map;\n"
	"uniform vec4 limits;\n"
	"out vec4 result;\n"
	"\n"
	"void main()\n"
	"{\n"
	"\tvec2 uv = clamp(gl_FragCoord.xy * map.xy + map.zw, limits.xy, limits.zw);\n"
	"\n"
	"\tresult = textureLod(color_texture, uv, 0.0);\n"
	"}\n";

static void shader_header(struct xgpu_text *text)
{
#ifdef HALO_GLES
	xgpu_text_append(text, "#version %s\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n",
		xgpu_capabilities.shading_language);
#elif defined(__APPLE__)
	xgpu_text_append(text, "#version 410 core\n");
#else
	xgpu_text_append(text, "#version 450 core\n");
#endif
}

static int pass_prepare(void)
{
	struct xgpu_text text = { 0 };
	GLuint vertex_shader, fragment_shader;

	if (pass.program || pass.failed)
		return pass.program != 0;
	pass.failed = 1;
	shader_header(&text);
	xgpu_text_append(&text, "%s", vertex_source);
	vertex_shader = xgpu_compile_shader(GL_VERTEX_SHADER, text.buffer, "dynamic resolution");
	text.length = 0;
	shader_header(&text);
	xgpu_text_append(&text, "%s", fragment_source);
	fragment_shader = vertex_shader ? xgpu_compile_shader(GL_FRAGMENT_SHADER, text.buffer, "dynamic resolution") : 0;
	free(text.buffer);
	if (!fragment_shader)
		return 0;
	pass.program = xgpu_link_program(vertex_shader, fragment_shader, "dynamic resolution");
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	if (!pass.program)
	{
		platform_log("dynamic resolution: its pass does not build: drawn at full resolution");
		return 0;
	}
	glUseProgram(pass.program);
	glUniform1i(glGetUniformLocation(pass.program, "color_texture"), 0);
	pass.map = glGetUniformLocation(pass.program, "map");
	pass.limits = glGetUniformLocation(pass.program, "limits");
	glGenSamplers(1, &pass.sampler);
	glSamplerParameteri(pass.sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(pass.sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(pass.sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(pass.sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenVertexArrays(1, &pass.vertex_array);
	pass.failed = 0;
	return 1;
}

/* the copy, the back buffer's size and format */
static void copy_fit(unsigned long width, unsigned long height, GLenum format)
{
	GLenum draw_buffer = GL_COLOR_ATTACHMENT0;

	if (pass.copy && pass.width == width && pass.height == height && pass.format == format)
		return;
	if (pass.copy)
	{
		glDeleteFramebuffers(1, &pass.copy_framebuffer);
		glDeleteTextures(1, &pass.copy);
	}
	pass.width = width;
	pass.height = height;
	pass.format = format;
	glGenTextures(1, &pass.copy);
	glBindTexture(GL_TEXTURE_2D, pass.copy);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, (GLint)format, (GLsizei)width, (GLsizei)height, 0, GL_RGBA,
		format == GL_RGBA16F ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE, NULL);
	glGenFramebuffers(1, &pass.copy_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, pass.copy_framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pass.copy, 0);
	glDrawBuffers(1, &draw_buffer);
}

/* the pixel edge of a coordinate (d3d8_gl.c's scaled_pixel) */
static GLint pixel_of(float coordinate, float scale)
{
	float value = coordinate * scale + 0.5f;
	GLint pixel = (GLint)value;

	if ((float)pixel > value)
		pixel--;
	return pixel;
}

void halo_screen_dynamic_resolution_end(short x0, short y0, short x1, short y1)
{
	GLuint source, texture, destination;
	unsigned long width, height;
	float scale[2], map[4], limits[4];
	GLint view[4], drawn[4];
	GLenum format;
	int was_drawing = ae_dynres_drawing;

	ae_dynres_drawing = 0;
	if (!was_drawing)
		return;
	/* (the pass's first build binds its program, and the back buffer's lookup may resolve and bind a framebuffer: the
	renderer's state is invalidated on every way out from here) */
	if (!pass_prepare() ||
		!halo_screen_back_buffer_gl(&source, &texture, &destination, &width, &height, scale, &format))
	{
		halo_screen_gl_restore();
		return;
	}
	(void)texture;
	view[0] = pixel_of((float)x0, scale[0]);
	view[1] = pixel_of((float)y0, scale[1]);
	view[2] = pixel_of((float)x1, scale[0]);
	view[3] = pixel_of((float)y1, scale[1]);
	drawn[0] = view[0];
	drawn[1] = view[1];
	drawn[2] = pixel_of(anchor[0] + ((float)x1 - anchor[0]) * ae_dynres_scale, scale[0]);
	drawn[3] = pixel_of(anchor[1] + ((float)y1 - anchor[1]) * ae_dynres_scale, scale[1]);
	if (view[2] <= view[0] || view[3] <= view[1] || drawn[2] <= drawn[0] || drawn[3] <= drawn[1])
	{
		halo_screen_gl_restore();
		return;
	}

	copy_fit(width, height, format);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_POLYGON_OFFSET_FILL);
#ifndef HALO_GLES
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
	glBindFramebuffer(GL_READ_FRAMEBUFFER, source);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pass.copy_framebuffer);
	glBlitFramebuffer(drawn[0], drawn[1], drawn[2], drawn[3], drawn[0], drawn[1], drawn[2], drawn[3],
		GL_COLOR_BUFFER_BIT, GL_NEAREST);

	/* (a pixel's centre p of the view reads drawn + (p - view) x drawn size / view size, over the copy's size) */
	map[0] = (float)(drawn[2] - drawn[0]) / (float)(view[2] - view[0]) / (float)width;
	map[1] = (float)(drawn[3] - drawn[1]) / (float)(view[3] - view[1]) / (float)height;
	map[2] = (float)drawn[0] / (float)width - (float)view[0] * map[0];
	map[3] = (float)drawn[1] / (float)height - (float)view[1] * map[1];
	limits[0] = ((float)drawn[0] + 0.5f) / (float)width;
	limits[1] = ((float)drawn[1] + 0.5f) / (float)height;
	limits[2] = ((float)drawn[2] - 0.5f) / (float)width;
	limits[3] = ((float)drawn[3] - 0.5f) / (float)height;

	glBindFramebuffer(GL_FRAMEBUFFER, destination);
	glViewport(view[0], view[1], view[2] - view[0], view[3] - view[1]);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glUseProgram(pass.program);
	glUniform4fv(pass.map, 1, map);
	glUniform4fv(pass.limits, 1, limits);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pass.copy);
	glBindSampler(0, pass.sampler);
	glBindVertexArray(pass.vertex_array);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindSampler(0, 0);
	halo_screen_gl_restore();
}

/* ---------- the frame's time */

/* when the last frame had been shown (and waited for), ns; 0 before the first */
static Uint64 shown_at;

#ifndef HALO_GLES
/* the GPU's time for each frame (GL_TIME_ELAPSED from the end of one swap to the frame's presentation, before its
copy to the window, which may wait for the display): a few in flight, read when the driver has them */
#define GPU_QUERIES 4
static struct
{
	int made;
	GLuint queries[GPU_QUERIES];
	int pending[GPU_QUERIES];
	int active;
	int next;
	float last_ms;
} gpu = { 0, { 0 }, { 0 }, -1, 0, -1.0f };

static void gpu_collect(void)
{
	int i;

	for (i = 0; i < GPU_QUERIES; i++)
	{
		GLuint available = 0, nanoseconds = 0;

		if (!gpu.pending[i])
			continue;
		glGetQueryObjectuiv(gpu.queries[i], GL_QUERY_RESULT_AVAILABLE, &available);
		if (!available)
			continue;
		glGetQueryObjectuiv(gpu.queries[i], GL_QUERY_RESULT, &nanoseconds);
		gpu.pending[i] = 0;
		gpu.last_ms = (float)nanoseconds / 1.0e6f;
	}
}
#endif

void ae_dynamic_resolution_frame_drawn(void)
{
#ifndef HALO_GLES
	if (gpu.active >= 0)
	{
		glEndQuery(GL_TIME_ELAPSED);
		gpu.pending[gpu.active] = 1;
		gpu.active = -1;
	}
#endif
}

/* (port/linux/game/ae_frame_pacer.c) the time LOW LATENCY slept since the last frame was shown, ms, and no more of it
until it sleeps again: the frame's own time does not include it */
float ae_frame_pacer_take_slept_ms(void);

void ae_dynamic_resolution_swap_begin(void)
{
	Uint64 now = SDL_GetTicksNS();
	float cpu_ms = shown_at && now > shown_at ? (float)(now - shown_at) / 1.0e6f - ae_frame_pacer_take_slept_ms() : -1.0f;
	float frame_ms = cpu_ms;
	int percent;

#ifndef HALO_GLES
	ae_dynamic_resolution_frame_drawn();
	if (gpu.made)
		gpu_collect();
	if (gpu.last_ms > frame_ms)
		frame_ms = gpu.last_ms;
#endif
	percent = ae_dynamic_resolution_policy(frame_ms);
	ae_dynres_scale = (float)percent / 100.0f;
	if (!(ae_dynres_scale > 0.0f && ae_dynres_scale <= 1.0f))
		ae_dynres_scale = 1.0f;
}

/* LOW LATENCY's: the GPU's time for the last frame timed (ms; below 0 when none is), and the timing of this frame started
again, now that the frame's wait is over (what the GPU did while the game slept is not the frame's) */
float ae_dynamic_resolution_gpu_ms(void)
{
#ifndef HALO_GLES
	return gpu.made ? gpu.last_ms : -1.0f;
#else
	return -1.0f;
#endif
}

void ae_dynamic_resolution_gpu_restart(void)
{
#ifndef HALO_GLES
	if (gpu.made && gpu.active >= 0)
	{
		/* (the same query, ended unread and begun again) */
		glEndQuery(GL_TIME_ELAPSED);
		glBeginQuery(GL_TIME_ELAPSED, gpu.queries[gpu.active]);
	}
#endif
}

void ae_dynamic_resolution_swap_end(void)
{
	shown_at = SDL_GetTicksNS();
#ifndef HALO_GLES
	if (!gpu.made)
	{
		glGenQueries(GPU_QUERIES, gpu.queries);
		gpu.made = 1;
	}
	/* (the next free query; none free: this frame is not timed) */
	if (gpu.active < 0 && !gpu.pending[gpu.next])
	{
		glBeginQuery(GL_TIME_ELAPSED, gpu.queries[gpu.next]);
		gpu.active = gpu.next;
		gpu.next = (gpu.next + 1) % GPU_QUERIES;
	}
#endif
}
