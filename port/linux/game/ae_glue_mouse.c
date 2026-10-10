/*
AE_GLUE_MOUSE.C

The MCC mouse style's settings, read from config.toml, and the functions the
hook lines call (ae_glue_mouse.h, ae_platform.h). The numbers are
ae_mouse.c's. Classic style (the default) changes nothing: every function
here leaves what upstream computed as it was.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "units/units.h"
#include "../src/port_config.h"
#include "../src/ae_platform.h"
#include "ae_glue_mouse.h"
#include "ae_mouse.h"

void platform_log(char const *format, ...);

/* the verb's counts go in on every 30th call of the look (about two a second) */
#define TEST_PERIOD 30

static struct
{
	unsigned long read_at;
	int read;
	int style;
	float sensitivity;
	float zoom_scale;
	float vehicle_scale;
	/* debug.test_input "mouse:<dx>,<dy>" */
	int test;
	float test_x;
	float test_y;
	int test_zoom;
	unsigned long test_calls;
	/* whether the last look had the test's counts in it (the log line of its angle) */
	int test_injected;
} mouse = { (unsigned long)-1 };

/* the settings, read again when a setting has been written; style MCC with its sensitivity unset (0) takes
the classic one's seed and writes it, so the first switch keeps the feel */
static void mouse_read(void)
{
	char const *test;

	if (mouse.read && mouse.read_at == config_changes())
		return;
	mouse.read = 1;
	mouse.read_at = config_changes();
	mouse.style = ae_mouse_style_from_text(config_string("input.mouse_style"));
	mouse.sensitivity = (float)config_real("input.mouse_mcc_sensitivity");
	mouse.zoom_scale = ae_mouse_clamp_scale((float)config_real("input.mouse_zoom_scale"));
	mouse.vehicle_scale = ae_mouse_clamp_scale((float)config_real("input.mouse_vehicle_scale"));
	if (mouse.style == AE_MOUSE_MCC && !(mouse.sensitivity > 0.0f))
	{
		char text[32];

		mouse.sensitivity = ae_mouse_seed((float)config_real("input.mouse_sensitivity"));
		snprintf(text, sizeof(text), "%.1f", (double)mouse.sensitivity);
		if (config_write("input.mouse_mcc_sensitivity", text))
		{
			platform_log("mouse: MCC sensitivity %s from the classic one", text);
			mouse.read_at = config_changes();
		}
	}
	mouse.sensitivity = ae_mouse_clamp_sensitivity(mouse.sensitivity);
	test = config_string("debug.test_input");
	mouse.test = ae_mouse_test_parse(test, &mouse.test_x, &mouse.test_y, &mouse.test_zoom);
}

void ae_mouse_test_counts(float *x, float *y)
{
	mouse_read();
	mouse.test_injected = 0;
	if (!mouse.test)
		return;
	if (mouse.test_calls++ % TEST_PERIOD == TEST_PERIOD - 1)
	{
		*x += mouse.test_x;
		*y += mouse.test_y;
		mouse.test_injected = 1;
	}
}

void ae_mouse_look_override(float x, float y, int invert, float *yaw, float *pitch)
{
	mouse_read();
	if (mouse.style == AE_MOUSE_MCC)
		ae_mouse_mcc_look(mouse.sensitivity, x, y, invert, yaw, pitch);
}

void ae_mouse_test_buttons(unsigned short *buttons)
{
	mouse_read();
	/* (the right stick's click: the default zoom button; the zoom level cycles on a press, so held = one press) */
	if (mouse.test && mouse.test_zoom)
		*buttons |= 0x0080;
}

void ae_mouse_scale_look(float *yaw, float *pitch, long unit_index, short zoom_level)
{
	float factor;
	/* (as the division by the weapon's magnification above the hook is guarded) */
	int zoomed = unit_index != NONE && zoom_level != NONE;
	int in_vehicle = 0;

	if (unit_index != NONE)
	{
		struct unit_datum *unit = unit_get(unit_index);

		/* (seated in any seat: the test the seat's look rates use) */
		in_vehicle = unit->object.parent_object_index != NONE && unit->unit.parent_seat_index != NONE;
	}
	mouse_read();
	factor = ae_mouse_zoom_factor(mouse.style, mouse.zoom_scale, zoomed) *
		ae_mouse_vehicle_factor(mouse.style, mouse.vehicle_scale, in_vehicle);
	if (factor != 1.0f)
	{
		*yaw *= factor;
		*pitch *= factor;
	}
	if (mouse.test_injected)
	{
		/* (the turn this frame gives the facing, in degrees: what the automated test reads) */
		platform_log("mouse test: counts %g,%g turn yaw %.5f pitch %.5f degrees (style %s, zoomed %d, vehicle %d)",
			(double)mouse.test_x, (double)mouse.test_y, (double)*yaw * 57.29577951308232,
			(double)*pitch * 57.29577951308232, mouse.style == AE_MOUSE_MCC ? "mcc" : "classic", zoomed, in_vehicle);
		mouse.test_injected = 0;
	}
}

void ae_mouse_log_settings(void)
{
	mouse_read();
	platform_log("mouse: style %s, sensitivity %g, zoom scale %g, vehicle scale %g",
		mouse.style == AE_MOUSE_MCC ? "mcc" : "classic",
		(double)(mouse.style == AE_MOUSE_MCC ? mouse.sensitivity : (float)config_real("input.mouse_sensitivity")),
		(double)mouse.zoom_scale, (double)mouse.vehicle_scale);
}

void ae_mouse_menu_text(char const *name, char *text, unsigned int size, int default_value)
{
	char *end;
	double value;

	if (strcmp(name, "input.mouse_mcc_sensitivity"))
		return;
	value = strtod(text, &end);
	if (end != text && value > 0.0)
		return;
	snprintf(text, size, "%.1f", (double)ae_mouse_seed(default_value ? 1.0f : (float)config_real("input.mouse_sensitivity")));
}
