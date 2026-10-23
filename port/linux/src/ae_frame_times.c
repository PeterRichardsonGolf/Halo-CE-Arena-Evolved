/*
AE_FRAME_TIMES.C

(Arena Evolved) A measuring build's frame timers (ae_frame_times.h). With HALO_AE_FRAME_TIMES=1 each second logs one
line: the frames, their average length, the share of them with player 1 alive, each slot's average ms per frame and
calls per frame (slots under 0.01 ms left out), each counter's calls per frame, the GPU's ms per frame (a
GL_TIME_ELAPSED query read when ready, never waited on), and how much the settings' change counts moved. Off, every
call returns at once.
*/

/* (the platform layer's order, as in ae_draw.c: its header first, and before gl.h, which on Windows brings
windows.h) */
#include "platform.h"
#include "ae_frame_times.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HALO_SERVER) || defined(HALO_ANDROID)

int ae_ft_on(void) { return 0; }
void ae_ft_enter(int slot) { (void)slot; }
void ae_ft_leave(int slot) { (void)slot; }
void ae_ft_count(int counter) { (void)counter; }
void ae_ft_alive(int alive) { (void)alive; }
void ae_ft_frame(void) {}
void ae_ft_gpu_begin(void) {}
void ae_ft_gpu_end(void) {}

#else

#ifndef HALO_GLES
#include "gl.h"
#endif
#include <SDL3/SDL.h>

extern void (*ae_config_counter)(int what);
unsigned long config_changes(void);
unsigned long ae_config_player_value_changes(void);

static const char *const slot_names[] =
{
	"input", "events", "port_upd", "net_start", "ui_widgets", "player_ctl", "net_end", "game_time", "non_det",
	"game_render", "throttle", "present_frame",
	"frame_begin", "player_view", "win_begin", "objects", "scene", "tone_aa", "interface", "hud", "engine_hud",
	"ui_render", "win_end", "nonplayer_view", "frame_end",
	"ae_ui_process", "ae_pl_settings", "ae_ce_map", "ae_main_menu", "ae_dialog", "ae_lobby", "ae_ui_render",
	"ae_draw_present",
	"d3d_present", "vis_send", "capture", "to_window", "swap", "swap_gl", "swap_sound_wait", "limiter", "vis_read",
	"fence_wait", "flip_wait", "gl_counted"
};

static const char *const counter_names[] =
{
	"cfg_lookup", "cfg_player_hit", "cfg_changes", "cfg_player_changes", "cfg_stat", "log_lines", "ae_ui_up",
	"ae_quads", "glGetIntegerv", "glIsEnabled", "glGetError", "glReadPixels", "glFinish", "glFlush", "glGetQueryObj",
	"glClientWaitSync", "glMapBufferRange", "glGetTexImage", "glCheckFbStatus", "glGet_other", "cfg_changes_in_hud"
};

typedef char ae_ft_slot_names_match[sizeof(slot_names) / sizeof(slot_names[0]) == AE_FT_SLOTS ? 1 : -1];
typedef char ae_ft_counter_names_match[sizeof(counter_names) / sizeof(counter_names[0]) == AE_FC_COUNTERS ? 1 : -1];

static int on = -1;
static Uint64 started[AE_FT_SLOTS];
static Uint64 spent[AE_FT_SLOTS];
static unsigned long calls[AE_FT_SLOTS];
static unsigned long counts[AE_FC_COUNTERS];
static unsigned long frames, alive_frames;
static int alive_now;
static Uint64 second_start, worst_frame, last_frame_end;
static unsigned long last_config_changes, last_player_changes;

/* the GPU's time: a ring of GL_TIME_ELAPSED queries, read once their results are there */
#ifndef HALO_GLES
#define GPU_QUERIES 8
static GLuint gpu_queries[GPU_QUERIES];
static int gpu_issued[GPU_QUERIES];
static int gpu_ready = -1, gpu_active, gpu_next, gpu_read_next;
static Uint64 gpu_spent;
static unsigned long gpu_frames, gpu_dropped;
/* (loaded here, not gl.h's, so the reads are not among the counted calls) */
static __typeof__(halo_glGetQueryObjectuiv) query_uiv;
static __typeof__(&glGetQueryObjectui64v) query_ui64v;
#endif

/* (ae_config_players.h's counter: its numbers in the counters' order from AE_FC_CONFIG_LOOKUP) */
static void config_counted(int what)
{
	if (what >= 0 && what <= AE_FC_CONFIG_STAT - AE_FC_CONFIG_LOOKUP)
		counts[AE_FC_CONFIG_LOOKUP + what]++;
	if (what == AE_FC_CONFIG_CHANGES - AE_FC_CONFIG_LOOKUP && started[AE_FT_HUD])
		counts[AE_FC_CONFIG_CHANGES_IN_HUD]++;
}

int ae_ft_on(void)
{
	if (on < 0)
	{
		const char *value = getenv("HALO_AE_FRAME_TIMES");

		on = value && *value && strcmp(value, "0") ? 1 : 0;
		if (on)
			ae_config_counter = config_counted;
		if (on)
			platform_log("frame times: on (HALO_AE_FRAME_TIMES): one line a second");
	}
	return on;
}

void ae_ft_enter(int slot)
{
	if (on <= 0 && !ae_ft_on())
		return;
	if (slot >= 0 && slot < AE_FT_SLOTS)
		started[slot] = SDL_GetTicksNS();
}

void ae_ft_leave(int slot)
{
	if (on <= 0 || slot < 0 || slot >= AE_FT_SLOTS || !started[slot])
		return;
	spent[slot] += SDL_GetTicksNS() - started[slot];
	started[slot] = 0;
	calls[slot]++;
}

void ae_ft_count(int counter)
{
	if (on <= 0 && !ae_ft_on())
		return;
	if (counter >= 0 && counter < AE_FC_COUNTERS)
		counts[counter]++;
}

void ae_ft_alive(int alive)
{
	alive_now = alive;
}

#ifndef HALO_GLES
static void gpu_collect(void)
{
	while (gpu_read_next != gpu_next)
	{
		GLuint available = 0;
		GLuint64 elapsed = 0;
		int index = gpu_read_next;

		if (!gpu_issued[index])
		{
			gpu_read_next = (gpu_read_next + 1) % GPU_QUERIES;
			continue;
		}
		query_uiv(gpu_queries[index], GL_QUERY_RESULT_AVAILABLE, &available);
		if (!available)
			return;
		query_ui64v(gpu_queries[index], GL_QUERY_RESULT, &elapsed);
		gpu_issued[index] = 0;
		gpu_spent += elapsed;
		gpu_frames++;
		gpu_read_next = (gpu_read_next + 1) % GPU_QUERIES;
	}
}
#endif

void ae_ft_gpu_begin(void)
{
#ifndef HALO_GLES
	if (on <= 0)
		return;
	if (gpu_ready < 0)
	{
		query_uiv = (__typeof__(query_uiv))SDL_GL_GetProcAddress("glGetQueryObjectuiv");
		query_ui64v = (__typeof__(query_ui64v))SDL_GL_GetProcAddress("glGetQueryObjectui64v");
		gpu_ready = query_uiv && query_ui64v && glGenQueries;
		if (gpu_ready)
			glGenQueries(GPU_QUERIES, gpu_queries);
		platform_log("frame times: GPU time %s", gpu_ready ? "by GL_TIME_ELAPSED queries" : "not measured");
	}
	if (!gpu_ready || gpu_active)
		return;
	gpu_collect();
	/* (the ring full: this frame not measured, never a wait) */
	if (gpu_issued[gpu_next])
	{
		gpu_dropped++;
		return;
	}
	glBeginQuery(GL_TIME_ELAPSED, gpu_queries[gpu_next]);
	gpu_active = 1;
#endif
}

void ae_ft_gpu_end(void)
{
#ifndef HALO_GLES
	if (on <= 0 || !gpu_active)
		return;
	glEndQuery(GL_TIME_ELAPSED);
	gpu_active = 0;
	gpu_issued[gpu_next] = 1;
	gpu_next = (gpu_next + 1) % GPU_QUERIES;
#endif
}

void ae_ft_frame(void)
{
	Uint64 now;
	char line[2000];
	size_t used = 0;
	int index;
	double per;

	if (on <= 0)
		return;
	now = SDL_GetTicksNS();
	if (!second_start)
	{
		second_start = now;
		last_frame_end = now;
		last_config_changes = config_changes();
		last_player_changes = ae_config_player_value_changes();
		memset(spent, 0, sizeof(spent));
		memset(calls, 0, sizeof(calls));
		memset(counts, 0, sizeof(counts));
		return;
	}
	frames++;
	alive_frames += alive_now ? 1 : 0;
	if (now - last_frame_end > worst_frame)
		worst_frame = now - last_frame_end;
	last_frame_end = now;
	if (now - second_start < 1000000000ull)
		return;

	per = 1.0 / (double)frames;
	used += (size_t)snprintf(line + used, sizeof(line) - used,
		"frame times: %lu fps %.2f ms worst %.1f ms alive %.2f |", frames,
		(double)(now - second_start) / 1e6 * per, (double)worst_frame / 1e6, (double)alive_frames * per);
	for (index = 0; index < AE_FT_SLOTS && used < sizeof(line) - 64; index++)
	{
		double ms = (double)spent[index] / 1e6 * per;

		if (ms >= 0.01)
			used += (size_t)snprintf(line + used, sizeof(line) - used, " %s %.2f/%.1f", slot_names[index], ms,
				(double)calls[index] * per);
	}
	{
		/* (the frame outside the main loop's timed steps, input to present) */
		Uint64 timed = 0;

		for (index = AE_FT_INPUT; index <= AE_FT_PRESENT_FRAME; index++)
			timed += spent[index];
		if (used < sizeof(line) - 64)
			used += (size_t)snprintf(line + used, sizeof(line) - used, " untimed %.2f",
				((double)(now - second_start) - (double)timed) / 1e6 * per);
	}
	platform_log("%s", line);
	used = (size_t)snprintf(line, sizeof(line), "frame counts: per frame:");
	for (index = 0; index < AE_FC_COUNTERS && used < sizeof(line) - 64; index++)
	{
		if (counts[index])
			used += (size_t)snprintf(line + used, sizeof(line) - used, " %s %.1f", counter_names[index],
				(double)counts[index] * per);
	}
#ifndef HALO_GLES
	if (gpu_ready > 0 && used < sizeof(line) - 64)
		used += (size_t)snprintf(line + used, sizeof(line) - used, " | gpu %.2f ms (%lu read, %lu skipped)",
			gpu_frames ? (double)gpu_spent / 1e6 / (double)gpu_frames : 0.0, gpu_frames, gpu_dropped);
	gpu_spent = 0;
	gpu_frames = 0;
	gpu_dropped = 0;
#endif
	{
		unsigned long config = config_changes(), players = ae_config_player_value_changes();

		if (used < sizeof(line) - 64)
			snprintf(line + used, sizeof(line) - used, " | changes cfg(all) +%lu players +%lu",
				config - last_config_changes, players - last_player_changes);
		last_config_changes = config;
		last_player_changes = players;
	}
	platform_log("%s", line);
	memset(spent, 0, sizeof(spent));
	memset(calls, 0, sizeof(calls));
	memset(counts, 0, sizeof(counts));
	frames = 0;
	alive_frames = 0;
	worst_frame = 0;
	second_start = now;
}

#endif
