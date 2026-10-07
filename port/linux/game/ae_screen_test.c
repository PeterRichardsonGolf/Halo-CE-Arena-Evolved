/*
AE_SCREEN_TEST.C

The AE menus' test screen (ae_screen_test.h): the title "AE TEST", a line
saying what it is drawn at, a 40-item list (ROW 1..ROW 40 with values) in the
look's colours (spec "Look", direction C), a panel naming the focused row,
and a footer of button prompts for the device last used. Anyone's (owner
AE_OWNER_ANY); BACK closes it and hands control back to the game's menus.

It logs to debug.txt for the tests (tools/test_ae_menus.py): "ae menus: test
screen" when it opens, "ae menus: focus N" (the item's index from 0) when the
focus moves, "ae menus: accept N", "ae menus: button NAME" and "ae menus:
test screen closed".

In the top right corner of the whole frame (one view only) it draws a white
square at alpha 0.25 over the game's picture: the tests check that it is one
blend over the picture, not built up over frames in the back buffer.
*/

#include <stdio.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_list.h"
#include "ae_screen_test.h"
#include "ae_ui.h"

void platform_log(char const *format, ...);

/* the look's colours (0xRRGGBBAA; spec "Look", the plan's Global Constraints) */
enum
{
	COLOR_PANEL = 0x181F26D1,
	COLOR_RULE = 0xC8D6E033,
	COLOR_ROW = 0x202830BD,
	COLOR_TEXT = 0xCDD6DDFF,
	COLOR_TITLE = 0xEEF3F6FF,
	COLOR_MUTED = 0x84939FFF,
	COLOR_SELECTION = 0xE8EEF2FF,
	COLOR_SELECTION_TEXT = 0x0F171EFF,
	COLOR_ACCENT = 0x5FC8ECFF,
	COLOR_WARNING = 0xE0A84AFF,
	COLOR_SWATCH = 0xFFFFFF40
};

/* the layout, in a view's units (1080 tall) */
enum
{
	ITEMS = 40,
	ROWS = 10,
	CORNER = 2,
	PANEL_X = 80,
	PANEL_Y = 60,
	PANEL_MAXIMUM_WIDTH = 1400,
	PANEL_HEIGHT = 960,
	INSET = 40,
	LIST_TOP = 240,
	ROW_HEIGHT = 56,
	ROW_GAP = 6,
	NOTCH = 6,
	SCROLLBAR = 8,
	SIDE_WIDTH = 320,
	SIDE_HEIGHT = 220,
	FOOTER_Y = 920,
	SWATCH = 40
};

static struct
{
	struct ae_list list;
	int views;
	int initialized;
} test_screen;

static void log_focus(
	short before)
{
	if (test_screen.list.focus != before)
		platform_log("ae menus: focus %d", test_screen.list.focus);
}

/* a view's rectangle in layout units: one, two (above and below) or four (quarters) */
static void view_rectangle(
	int view,
	float rectangle[4])
{
	struct ae_layout layout;

	ae_draw_current_layout(&layout);
	rectangle[0] = 0.0f;
	rectangle[1] = 0.0f;
	rectangle[2] = layout.width;
	rectangle[3] = layout.height;
	if (test_screen.views == 2)
	{
		rectangle[1] = view ? layout.height * 0.5f : 0.0f;
		rectangle[3] = layout.height * 0.5f;
	}
	else if (test_screen.views == 4)
	{
		rectangle[0] = view % 2 ? layout.width * 0.5f : 0.0f;
		rectangle[1] = view / 2 ? layout.height * 0.5f : 0.0f;
		rectangle[2] = layout.width * 0.5f;
		rectangle[3] = layout.height * 0.5f;
	}
}

static float panel_width(
	float view_width)
{
	float width = view_width - 2.0f * PANEL_X;

	return width < PANEL_MAXIMUM_WIDTH ? width : PANEL_MAXIMUM_WIDTH;
}

static float rows_width(
	float view_width)
{
	return panel_width(view_width) - 2.0f * INSET - SIDE_WIDTH - 2.0f * SCROLLBAR - 16.0f;
}

static void enter(
	struct ae_screen *screen)
{
	if (!test_screen.initialized)
	{
		ae_list_init(&test_screen.list, ITEMS, ROWS);
		test_screen.initialized = TRUE;
	}
	/* (focus memory: the row it was left on) */
	ae_list_set_focus(&test_screen.list, screen->focus);
	platform_log("ae menus: test screen (views %d)", test_screen.views);
}

static void leave(
	struct ae_screen *screen)
{
	screen->focus = test_screen.list.focus;
	platform_log("ae menus: test screen closed");
}

static int handle(
	struct ae_screen *screen,
	struct ae_event const *event)
{
	short before = test_screen.list.focus;

	(void)screen;
	switch (event->action)
	{
	case AE_ACTION_UP: ae_list_move(&test_screen.list, -1, FALSE); break;
	case AE_ACTION_DOWN: ae_list_move(&test_screen.list, 1, FALSE); break;
	case AE_ACTION_PAGE_UP: ae_list_page(&test_screen.list, -1); break;
	case AE_ACTION_PAGE_DOWN: ae_list_page(&test_screen.list, 1); break;
	case AE_ACTION_ACCEPT: platform_log("ae menus: accept %d", test_screen.list.focus); break;
	case AE_ACTION_BACK: return 0;
	case AE_ACTION_LEFT: platform_log("ae menus: button LEFT"); break;
	case AE_ACTION_RIGHT: platform_log("ae menus: button RIGHT"); break;
	case AE_ACTION_TAB_PREVIOUS: platform_log("ae menus: button TAB_PREVIOUS"); break;
	case AE_ACTION_TAB_NEXT: platform_log("ae menus: button TAB_NEXT"); break;
	case AE_ACTION_X: platform_log("ae menus: button X"); break;
	case AE_ACTION_Y: platform_log("ae menus: button Y"); break;
	case AE_ACTION_START: platform_log("ae menus: button START"); break;
	case AE_ACTION_SELECT: platform_log("ae menus: button SELECT"); break;
	default: return 0;
	}
	log_focus(before);
	return 1;
}

/* the item under a point of the first view (layout units), or -1 */
static short item_at(
	float x,
	float y)
{
	struct ae_view view;
	float rectangle[4], view_x, view_y;
	short row;

	view_rectangle(0, rectangle);
	ae_layout_view(rectangle[0], rectangle[1], rectangle[2], rectangle[3], &view);
	ae_view_from_layout(&view, x, y, &view_x, &view_y);
	if (view_x < PANEL_X + INSET || view_x >= PANEL_X + INSET + rows_width(ae_view_width(&view)) || view_y < LIST_TOP)
		return -1;
	row = (short)((view_y - LIST_TOP) / (ROW_HEIGHT + ROW_GAP));
	if (view_y - LIST_TOP - row * (ROW_HEIGHT + ROW_GAP) >= ROW_HEIGHT)
		return -1;
	return ae_list_item_at_row(&test_screen.list, row);
}

static void pointer(
	struct ae_screen *screen,
	struct ae_pointer const *pointer)
{
	short before = test_screen.list.focus;
	short item = item_at(pointer->x, pointer->y);

	(void)screen;
	if (pointer->wheel_steps)
		ae_list_scroll(&test_screen.list, (short)-pointer->wheel_steps);
	/* (hover focuses only when the pointer moves: spec "Input, focus, motion") */
	if (pointer->moved && item >= 0)
		ae_list_set_focus(&test_screen.list, item);
	if (pointer->left_clicks && item >= 0)
	{
		ae_list_set_focus(&test_screen.list, item);
		platform_log("ae menus: accept %d", item);
	}
	log_focus(before);
}

static int device_font(
	void)
{
	switch (ae_ui_last_device())
	{
	case AE_DEVICE_KEYBOARD_MOUSE: return AE_FONT_KEYBOARD;
	case AE_DEVICE_PLAYSTATION: return AE_FONT_PLAYSTATION;
	case AE_DEVICE_NINTENDO: return AE_FONT_NINTENDO;
	case AE_DEVICE_XBOX: break;
	}
	return AE_FONT_XBOX;
}

static char const *device_name(
	void)
{
	switch (ae_ui_last_device())
	{
	case AE_DEVICE_KEYBOARD_MOUSE: return "KEYBOARD AND MOUSE";
	case AE_DEVICE_PLAYSTATION: return "PLAYSTATION";
	case AE_DEVICE_NINTENDO: return "NINTENDO";
	case AE_DEVICE_XBOX: break;
	}
	return "XBOX";
}

/* a prompt: the button's glyph and its label; the x after it */
static float prompt(
	float x,
	int button,
	char const *label)
{
	x += ae_draw_button(device_font(), button, 40.0f, x, FOOTER_Y, COLOR_TITLE) + 10.0f;
	x += ae_draw_text(AE_FONT_ROW, 28.0f, x, FOOTER_Y + 6.0f, AE_ALIGN_LEFT, COLOR_TEXT, label) + 36.0f;
	return x;
}

static void draw_view(
	int view)
{
	struct ae_layout layout;
	float width = ae_draw_view_width();
	float panel = panel_width(width);
	float rows = rows_width(width);
	float side_x = PANEL_X + INSET + rows + 2.0f * SCROLLBAR + 16.0f;
	float side_width = PANEL_X + panel - INSET - side_x;
	char text[128];
	short row;

	ae_draw_current_layout(&layout);
	ae_draw_rect(PANEL_X, PANEL_Y, panel, PANEL_HEIGHT, CORNER, COLOR_PANEL);
	ae_draw_outline(PANEL_X, PANEL_Y, panel, PANEL_HEIGHT, CORNER, 2.0f, COLOR_RULE);
	ae_draw_text(AE_FONT_TITLE, 72.0f, PANEL_X + INSET, PANEL_Y + 28.0f, AE_ALIGN_LEFT, COLOR_TITLE, "AE TEST");
	sprintf(text, "LAYOUT %.0fx%.0f AT %.2f PX A UNIT - VIEW %d OF %d - %s", layout.width, layout.height,
		layout.scale, view + 1, test_screen.views, device_name());
	ae_draw_text(AE_FONT_BODY, 28.0f, PANEL_X + INSET, PANEL_Y + 112.0f, AE_ALIGN_LEFT, COLOR_MUTED, text);
	ae_draw_rect(PANEL_X + INSET, PANEL_Y + 160.0f, panel - 2.0f * INSET, 2.0f, 0.0f, COLOR_RULE);

	/* the list's rows */
	ae_draw_clip_push(PANEL_X + INSET, LIST_TOP, rows, ROWS * (ROW_HEIGHT + ROW_GAP));
	for (row = 0; row < ROWS; row++)
	{
		short item = ae_list_item_at_row(&test_screen.list, row);
		float y = LIST_TOP + row * (ROW_HEIGHT + ROW_GAP);
		boolean focused = item == test_screen.list.focus;

		if (item < 0)
			break;
		ae_draw_rect(PANEL_X + INSET, y, rows, ROW_HEIGHT, CORNER, focused ? COLOR_SELECTION : COLOR_ROW);
		if (focused)
			ae_draw_rect(PANEL_X + INSET, y, NOTCH, ROW_HEIGHT, 0.0f, COLOR_ACCENT);
		sprintf(text, "ROW %d", item + 1);
		ae_draw_text(AE_FONT_ROW, 30.0f, PANEL_X + INSET + 24.0f, y + 13.0f, AE_ALIGN_LEFT,
			focused ? COLOR_SELECTION_TEXT : COLOR_TEXT, text);
		sprintf(text, "VALUE %d", (item * 37) % 100);
		ae_draw_text(AE_FONT_BODY, 28.0f, PANEL_X + INSET + rows - 24.0f, y + 14.0f, AE_ALIGN_RIGHT,
			focused ? COLOR_SELECTION_TEXT : COLOR_MUTED, text);
	}
	ae_draw_clip_pop();

	/* the scroll bar: the window's place in the list */
	{
		float track = ROWS * (ROW_HEIGHT + ROW_GAP) - ROW_GAP;
		float x = PANEL_X + INSET + rows + SCROLLBAR;

		ae_draw_rect(x, LIST_TOP, SCROLLBAR, track, SCROLLBAR * 0.5f, COLOR_RULE);
		ae_draw_rect(x, LIST_TOP + track * test_screen.list.first / ITEMS, SCROLLBAR, track * ROWS / ITEMS,
			SCROLLBAR * 0.5f, COLOR_ACCENT);
	}

	/* the focused row */
	ae_draw_rect(side_x, LIST_TOP, side_width, SIDE_HEIGHT, CORNER, COLOR_ROW);
	ae_draw_text(AE_FONT_BODY, 26.0f, side_x + 24.0f, LIST_TOP + 20.0f, AE_ALIGN_LEFT, COLOR_MUTED, "FOCUSED");
	sprintf(text, "ROW %d", test_screen.list.focus + 1);
	ae_draw_text(AE_FONT_TITLE, 64.0f, side_x + 24.0f, LIST_TOP + 64.0f, AE_ALIGN_LEFT, COLOR_TITLE, text);
	sprintf(text, "VALUE %d", (test_screen.list.focus * 37) % 100);
	ae_draw_text(AE_FONT_BODY, 28.0f, side_x + 24.0f, LIST_TOP + 150.0f, AE_ALIGN_LEFT, COLOR_WARNING, text);

	/* the footer's prompts */
	{
		float x = PANEL_X + INSET;

		x = prompt(x, AE_BUTTON_A, "SELECT");
		x = prompt(x, AE_BUTTON_B, "BACK");
		x = prompt(x, AE_BUTTON_LEFT_TRIGGER, "PAGE UP");
		prompt(x, AE_BUTTON_RIGHT_TRIGGER, "PAGE DOWN");
	}
}

static void draw(
	struct ae_screen *screen)
{
	int view;

	(void)screen;
	for (view = 0; view < test_screen.views; view++)
	{
		float rectangle[4];

		view_rectangle(view, rectangle);
		ae_draw_view(rectangle[0], rectangle[1], rectangle[2], rectangle[3]);
		draw_view(view);
	}
	ae_draw_view_full();
	if (test_screen.views == 1)
		ae_draw_rect(ae_draw_view_width() - 20.0f - SWATCH, 20.0f, SWATCH, SWATCH, 0.0f, COLOR_SWATCH);
}

static struct ae_screen_class const test_screen_class =
{
	"test", enter, leave, handle, draw, FALSE, pointer
};

int ae_screen_test_open(
	int views)
{
	test_screen.views = views == 2 || views == 4 ? views : 1;
	return ae_ui_push(&test_screen_class, AE_OWNER_ANY, &test_screen);
}
