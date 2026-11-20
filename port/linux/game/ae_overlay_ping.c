/*
AE_OVERLAY_PING.C

PING on the overlay line (ae_overlay_ping.h).
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "interface/ui_widget.h"
#include "../src/port_config.h"
#include "ae_hud_style.h"
#include "latency_meter.h"
#include "network_distributed.h"

#include "ae_overlay_ping.h"

#include <stdio.h>
#include <string.h>

/* port_config.c's, cinematics.c's, ae_hud_text.c's, the platform's */
int ae_config_chosen(const char *name);
void draw_quad(rectangle2d *rectangle, pixel32 color);
void ae_hud_fill_quad_color(const real points[4][2], const union real_argb_color *color);
void platform_log(char const *format, ...);

/* the ping the line was last given (set by ae_overlay_ping_append, read by the drawers) */
static struct
{
	int shown;
	long ping;      /* NONE: none measured */
	int problem;
} bars;

void ae_overlay_ping_format(char *text, long size, long ping, int host, int problem)
{
	if (!text || size <= 0)
		return;
	if (problem && !host)
		snprintf(text, (size_t)size, "PING --");
	else if (ping == NONE)
		text[0] = 0;
	else
		snprintf(text, (size_t)size, "%s %ld MS", host ? "MAX PING" : "PING", ping > 999 ? 999L : ping);
}

int ae_overlay_ping_mode_of(const char *value)
{
	if (!value)
		return AE_OVERLAY_PING_OFF;
	if (!strcmp(value, "number"))
		return AE_OVERLAY_PING_NUMBER;
	if (!strcmp(value, "number_and_bars"))
		return AE_OVERLAY_PING_BARS;
	return AE_OVERLAY_PING_OFF;
}

int ae_overlay_ping_mode(void)
{
	return ae_overlay_ping_mode_of(config_string("display.ping"));
}

int ae_overlay_ping_wanted(void)
{
	return ae_overlay_ping_mode() != AE_OVERLAY_PING_OFF;
}

int ae_overlay_ping_bars_wanted(void)
{
	return ae_overlay_ping_mode() == AE_OVERLAY_PING_BARS;
}

void ae_overlay_ping_profile_loaded(const char *latency_meter)
{
	if (!latency_meter || strcmp(latency_meter, "true") || ae_config_chosen("display.ping"))
		return;
	config_write("display.ping", "number_and_bars");
	platform_log("settings: a profile had display.latency_meter on: display.ping \"number_and_bars\"");
}

void ae_overlay_ping_append(char *line, long size)
{
	short connection = game_connection();
	int host = connection == _game_connection_network_server;
	int mode = ae_overlay_ping_mode();
	char text[32];
	size_t length;

	bars.shown = 0;
	/* (the line is empty with PERFORMANCE off: the ping is then alone on it) */
	if (!line || size <= 0 || mode == AE_OVERLAY_PING_OFF)
		return;
	if ((!host && connection != _game_connection_network_client) || !game_in_progress() || !game_engine_in_play() ||
		main_menu_is_active())
	{
		return;
	}
	bars.ping = host ? distributed_slowest_client_ping() : distributed_own_ping();
	bars.problem = !host && distributed_connection_problem();
	ae_overlay_ping_format(text, (long)sizeof(text), bars.ping, host, bars.problem);
	length = strlen(line);
	if (text[0] && length + 8 + strlen(text) < (size_t)size)
	{
		/* (before a second line, if one is there) */
		char *newline = strchr(line, '\n');
		char more[48];

		bars.shown = mode == AE_OVERLAY_PING_BARS;
		/* (AE's HUD style: the bars a word after it; CLASSIC's are drawn beside the line) */
		snprintf(more, sizeof(more), "%s%s%s", length ? "  " : "", text,
			bars.shown && ae_hud_style_is_ae() ? "  \x01" : "");
		if (newline)
		{
			char rest[128];

			snprintf(rest, sizeof(rest), "%s", newline);
			snprintf(newline, (size_t)size - (size_t)(newline - line), "%s%s", more, rest);
		}
		else
			snprintf(line + length, (size_t)size - length, "%s", more);
	}
}

/* the bars a round trip earns (4 to 1; none while it is not measured or the host is not heard), and their colour as
AE's HUD draws it: the browser's ping colours, green below 80 ms, pale below 150, the team red's light ink after */
static int bars_lit(float *red, float *green, float *blue)
{
	if (bars.problem || bars.ping == NONE)
	{
		*red = 236.0f / 255.0f; *green = 128.0f / 255.0f; *blue = 116.0f / 255.0f;
		return 0;
	}
	if (bars.ping < 80)
	{
		*red = 0.43f; *green = 0.86f; *blue = 0.51f;
		return 4;
	}
	if (bars.ping < 150)
	{
		*red = 0.85f; *green = 0.92f; *blue = 1.0f;
		return 3;
	}
	*red = 236.0f / 255.0f; *green = 128.0f / 255.0f; *blue = 116.0f / 255.0f;
	return bars.ping < 250 ? 2 : 1;
}

float ae_overlay_ping_bars_hud(float x, float baseline, float text_size, int draw)
{
	float width = text_size * AE_OVERLAY_PING_BAR_WIDTH, gap = text_size * AE_OVERLAY_PING_BAR_GAP;
	float height = text_size * AE_OVERLAY_PING_BAR_HEIGHT;
	float red, green, blue;
	int lit, index;

	if (!bars.shown)
		return 0.0f;
	lit = bars_lit(&red, &green, &blue);
	if (draw)
	{
		for (index = 0; index < 4; index++)
		{
			real points[4][2];
			real_argb_color color;
			float top = baseline - height * (float)(index + 1) / 4.0f;
			float left = x + (width + gap) * (float)index;

			color.alpha = 1.0f;
			if (index < lit)
			{
				color.red = red; color.green = green; color.blue = blue;
			}
			else
			{
				/* (the bars not earned: dim, as the meter's were) */
				color.red = color.green = color.blue = 0.35f;
				color.alpha = 0.6f;
			}
			points[0][0] = left;          points[0][1] = top;
			points[1][0] = left + width;  points[1][1] = top;
			points[2][0] = left + width;  points[2][1] = baseline;
			points[3][0] = left;          points[3][1] = baseline;
			ae_hud_fill_quad_color(points, &color);
		}
	}
	return width * 4.0f + gap * 3.0f;
}

static pixel32 pixel_of(real_argb_color const *color)
{
	return ((pixel32)(PIN(color->alpha, 0.0f, 1.0f) * 255.0f + 0.5f) << 24) |
		((pixel32)(PIN(color->red, 0.0f, 1.0f) * 255.0f + 0.5f) << 16) |
		((pixel32)(PIN(color->green, 0.0f, 1.0f) * 255.0f + 0.5f) << 8) |
		(pixel32)(PIN(color->blue, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void ae_overlay_ping_bars_classic(const union rectangle2d *extent, int drawn, int right)
{
	enum { BAR_WIDTH = 4, BAR_GAP = 1, GAP = 4 };
	real_argb_color color;
	short lit = 0, index, height, left;

	if (!bars.shown || !drawn || !extent || ae_hud_style_is_ae())
		return;
	if (!bars.problem && bars.ping != NONE)
		lit = latency_meter_color(bars.ping, &color);
	else
	{
		color.alpha = 1.0f; color.red = 1.0f; color.green = 0.3f; color.blue = 0.3f;
	}
	height = (short)MAX(extent->y1 - extent->y0 - 4, 4);
	left = right ? (short)(extent->x0 - GAP - 4 * (BAR_WIDTH + BAR_GAP)) : (short)(extent->x1 + GAP);
	for (index = 0; index < 4; index++)
	{
		rectangle2d bar;
		real_argb_color bar_color = color;

		if (index >= lit)
		{
			bar_color.red = bar_color.green = bar_color.blue = 0.25f;
			bar_color.alpha = 0.6f;
		}
		bar.x0 = (short)(left + index * (BAR_WIDTH + BAR_GAP));
		bar.x1 = (short)(bar.x0 + BAR_WIDTH);
		bar.y1 = (short)(extent->y1 - 2);
		bar.y0 = (short)(bar.y1 - height * (index + 1) / 4);
		draw_quad(&bar, pixel_of(&bar_color));
	}
}
