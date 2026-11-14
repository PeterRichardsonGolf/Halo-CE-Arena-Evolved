/*
AE_FRAME_PACER.C

LOW LATENCY: the settings, the frames' times, the rules each frame, the log, the performance line (ae_frame_pacer.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"

#include "ae_frame_pacer.h"
#include "ae_frame_pacer_rules.h"

const char *config_string(const char *name);
int config_boolean(const char *name);
unsigned long config_changes(void);
void platform_log(const char *format, ...);
int halo_interpolation_enabled(void);
/* (source/main/main.c, source/interface/ui_widget.c) a map is loaded (the main menu's too); the main menu is up */
boolean game_in_progress(void);
boolean main_menu_is_active(void);
/* (port/linux/src/ae_frame_pacer_gl.c) whether this build can wait; the clock; a sleep to a time, the last part of it
spinning; the GPU's timing of this frame started again */
int ae_frame_pacer_platform(void);
unsigned long long ae_frame_pacer_ticks_ns(void);
void ae_frame_pacer_sleep_until(unsigned long long until_ns, unsigned long long spin_ns);
void ae_frame_pacer_restart_gpu_timing(void);

#define BOOST_SPIN_NS 500000ull
/* (the log's lines for late frames: one a few seconds, with the count) */
#define LATE_LOG_MS 5000ull

enum { MODE_OFF, MODE_ON, MODE_BOOST };

static struct
{
	boolean read;
	unsigned long read_at;
	int mode, vsync, interpolation;
	struct ae_pacer_state state;
	/* a game is running with every setting right: the rules run */
	boolean armed;
	/* BOOST works now */
	boolean boosting;
	/* when the frame's wait ended (ns), and the sleep to take off the frame's time (ms) */
	unsigned long long frame_start_ns;
	float slept_ms;
	/* the log */
	boolean learned_logged, needs_logged, unfocused_logged, stall_logged;
	unsigned long late_logged;
	unsigned long long late_logged_at_ns;
} pacer;

static void settings_read(void)
{
	const char *mode;

	if (pacer.read && pacer.read_at == config_changes())
		return;
	pacer.read = TRUE;
	pacer.read_at = config_changes();
	mode = config_string("display.low_latency");
	pacer.mode = !strcmp(mode, "boost") ? MODE_BOOST : !strcmp(mode, "on") ? MODE_ON : MODE_OFF;
	pacer.vsync = config_boolean("display.vsync");
	pacer.interpolation = halo_interpolation_enabled();
	pacer.needs_logged = FALSE;
}

/* the rules stop, everything forgotten: a game ended, or a setting no longer holds */
static void disarm(void)
{
	if (pacer.armed)
	{
		pacer.armed = FALSE;
		ae_pacer_reset(&pacer.state);
		pacer.boosting = FALSE;
		pacer.learned_logged = FALSE;
		pacer.unfocused_logged = FALSE;
		pacer.late_logged = 0;
		pacer.stall_logged = FALSE;
		pacer.slept_ms = 0.0f;
		platform_log("low latency: stopped");
	}
}

static void log_event(int event, unsigned long long now_ns)
{
	float period = ae_pacer_period_ms(&pacer.state);

	switch (event)
	{
	case AE_PACER_EVENT_LATE:
		/* (a game that misses often logs one line a few seconds) */
		if (pacer.state.late > pacer.late_logged &&
			(!pacer.late_logged_at_ns || now_ns - pacer.late_logged_at_ns >= LATE_LOG_MS * 1000000ull))
		{
			platform_log("low latency: %lu frame%s missed the blank (%lu in all; the wait now leaves %.1f ms)",
				pacer.state.late - pacer.late_logged, pacer.state.late - pacer.late_logged == 1 ? "" : "s",
				pacer.state.late, (double)pacer.state.margin_ms);
			pacer.late_logged = pacer.state.late;
			pacer.late_logged_at_ns = now_ns;
		}
		break;
	case AE_PACER_EVENT_STALL:
		/* (a GPU-bound game's full driver queue does this every frame: said once a game) */
		if (!pacer.stall_logged)
			platform_log("low latency: showing a frame took over 1.5 refreshes (the graphics driver held it); no "
				"wait after such a frame");
		pacer.stall_logged = TRUE;
		break;
	case AE_PACER_EVENT_TOO_SLOW:
		platform_log("low latency: not waiting: the frames are over %.0f ms apart (under 20 a second), too slow to "
			"pace; it waits again once they are faster", (double)AE_PACER_PERIOD_MAX_MS);
		break;
	case AE_PACER_EVENT_NOT_A_DISPLAY:
		platform_log("low latency: off for this game: the frames are shown %.1f ms apart, which no display's refresh "
			"is, so V-SYNC is not holding them (a driver or window system that does not wait for the display)",
			(double)(ae_pacer_period_ms(&pacer.state)));
		/* (the rules keep it given up until this game ends) */
		pacer.boosting = FALSE;
		break;
	case AE_PACER_EVENT_OVER_BUDGET:
		platform_log("low latency: not waiting for now: a frame takes %.1f ms of the %.1f ms between blanks",
			(double)(ae_pacer_cost_ms(&pacer.state) + pacer.state.margin_ms), (double)period);
		break;
	case AE_PACER_EVENT_FAST_AGAIN:
		platform_log("low latency: waiting again");
		break;
	default:
		break;
	}
}

void ae_frame_pacer_wait(void)
{
	unsigned long long now, wait;
	boolean running;

	if (!ae_frame_pacer_platform())
		return;
	settings_read();
	if (pacer.mode == MODE_OFF)
	{
		disarm();
		return;
	}
	running = game_in_progress() && !main_menu_is_active();
	if (!running)
	{
		disarm();
		return;
	}
	if (!pacer.vsync || !pacer.interpolation)
	{
		if (!pacer.needs_logged)
		{
			platform_log("low latency: %s needs V-SYNC and SMOOTH MOTION on; not waiting", pacer.mode == MODE_BOOST ?
				"BOOST" : "ON");
			pacer.needs_logged = TRUE;
		}
		disarm();
		return;
	}
	now = ae_frame_pacer_ticks_ns();
	if (!pacer.armed)
	{
		pacer.armed = TRUE;
		/* (the margin from AE_PACER_MARGIN_MS, not the static's 0, the first game too) */
		ae_pacer_reset(&pacer.state);
		platform_log("low latency: %s: the game waits before it reads input so that a frame ends just before the "
			"next blank", pacer.mode == MODE_BOOST ? "BOOST" : "ON");
	}
	log_event(ae_pacer_take_event(&pacer.state), now);
	pacer.boosting = pacer.mode == MODE_BOOST && !pacer.state.gave_up;
	if (!pacer.learned_logged && ae_pacer_period_ms(&pacer.state) > 0.0f)
	{
		pacer.learned_logged = TRUE;
		platform_log("low latency: the swaps are %.2f ms apart (%.2f Hz)", (double)ae_pacer_period_ms(&pacer.state),
			1000.0 / (double)ae_pacer_period_ms(&pacer.state));
	}
	wait = ae_pacer_wait(&pacer.state, now);
	log_event(ae_pacer_take_event(&pacer.state), now);
	if (wait)
	{
		unsigned long long woke;

		ae_frame_pacer_sleep_until(now + wait, pacer.boosting ? BOOST_SPIN_NS : 0);
		woke = ae_frame_pacer_ticks_ns();
		pacer.slept_ms += (float)((double)(woke - now) / 1.0e6);
		now = woke;
		/* (what the GPU did while the game slept is not the frame's) */
		ae_frame_pacer_restart_gpu_timing();
	}
	pacer.frame_start_ns = now;
}

void ae_frame_pacer_note_frame(unsigned long long now_ns, float gpu_ms)
{
	if (!pacer.armed || !pacer.frame_start_ns || now_ns <= pacer.frame_start_ns)
		return;
	ae_pacer_frame(&pacer.state, (float)((double)(now_ns - pacer.frame_start_ns) / 1.0e6), gpu_ms);
}

void ae_frame_pacer_note_swap(unsigned long long now_ns, unsigned long long swap_ns, int real_vsync)
{
	if (!pacer.armed)
		return;
	if (!real_vsync)
	{
		/* (an unfocused window's swaps do not wait: the phase is learnt again) */
		if (!pacer.unfocused_logged)
		{
			platform_log("low latency: the window is not focused, so its swaps do not wait for the display; "
				"not waiting until it is");
			pacer.unfocused_logged = TRUE;
		}
		ae_pacer_forget(&pacer.state);
		pacer.state.last_return_ns = 0;
		return;
	}
	ae_pacer_swap(&pacer.state, now_ns, swap_ns);
}

int ae_frame_pacer_boosting(void)
{
	return pacer.boosting;
}

float ae_frame_pacer_take_slept_ms(void)
{
	float slept = pacer.slept_ms;

	pacer.slept_ms = 0.0f;
	return slept;
}

void ae_frame_pacer_append(char *line, long size, long level)
{
	char field[24];
	long length, field_length;

	if (!pacer.armed || level != 3 || !line || size <= 0 || !ae_frame_pacer_platform())
		return;
	snprintf(field, sizeof(field), "  LATE %lu", pacer.state.late);
	field_length = (long)strlen(field);
	length = (long)strlen(line);
	if (length + field_length >= size)
		return;
	memcpy(line + length, field, (size_t)field_length + 1);
}
