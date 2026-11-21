/* A SERVICE RECORD box's DETAILS page (see the header). */
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "math/real_math.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_pgcr_draw.h"
#include "ae_record_pages.h"
#include "ae_screen_record.h"
#include "ae_screen_record_details.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum { DETAILS_HIT = 0x762B, LEFT = 96, TOP = 232, PANE_H = 704 };

static struct
{
	int on_stack, bot;
	struct ae_rv_table table;
	char name[96], sub[96];
	struct ae_prompt prompts[1];
	short prompt_count;
} page;

static struct ae_screen_class const page_class;

int ae_screen_record_details_open(short owner, struct ae_rv_table const *table, char const *name, int bot, char const *sub)
{
	if (page.on_stack)
		return 0;
	memset(&page, 0, sizeof(page));
	page.table = *table;
	page.bot = bot;
	snprintf(page.name, sizeof(page.name), "%s", name ? name : "");
	snprintf(page.sub, sizeof(page.sub), "%s", sub ? sub : "");
	if (!ae_ui_push(&page_class, owner, &page))
		return 0;
	page.on_stack = 1;
	platform_log("ae record: details %s", table->title);
	return 1;
}

int ae_screen_record_details_is_open(void)
{
	return page.on_stack;
}

void ae_screen_record_details_remove(void)
{
	if (page.on_stack)
		ae_ui_remove(&page);
}

static void page_leave(struct ae_screen *screen)
{
	(void)screen;
	page.on_stack = 0;
}

static int page_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	return event->action == AE_ACTION_BACK ? 0 : 1;
}

static void page_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;

	(void)screen;
	if (ae_prompts_pointer(page.prompts, page.prompt_count, pointer, DETAILS_HIT, &event))
		ae_ui_dispatch(&event);
}

static void page_draw(struct ae_screen *screen)
{
	struct ae_frame frame;
	struct ae_density density;
	struct frame_metrics f;
	struct ae_rv_table const *t = &page.table;
	float w, y, pitch = t->rows > 10 ? 36.0f : 46.0f, right = LEFT + 1728.0f - 28.0f, step = 156.0f;
	int row, column;

	(void)screen;
	ae_record_frame_begin(&f, &frame, &density);
	w = put_tracked(&f, AE_FONT_BODY, 18, 0.14f, LEFT, 96.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, "SERVICE RECORD");
	rect(&f, LEFT + w + 12.0f, 94.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
	put_tracked(&f, AE_FONT_BODY, 18, 0.14f, LEFT + w + 28.0f, 96.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, t->title);
	w = put(&f, AE_FONT_ROW, 44, LEFT, 150.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, page.name);
	if (page.bot)
		draw_outlined_tag(&f, LEFT + w + 16.0f, 150.0f, "BOT", AE_COLOR_MUTED);
	put_tracked(&f, AE_FONT_BODY, 18, 0.08f, LEFT, 196.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, page.sub);
	ae_rp_pane(&f, LEFT, TOP, 1728.0f, PANE_H);
	y = ae_rp_label(&f, LEFT + 28.0f, TOP + 28.0f, t->title, t->tail);
	for (column = 0; column < t->columns; column++)
		put_tracked(&f, AE_FONT_BODY, 13, 0.14f, right - (float)(t->columns - 1 - column) * step, TOP + 28.0f + 12.0f, AE_ALIGN_RIGHT,
			AE_COLOR_MUTED, t->head[column]);
	rect(&f, LEFT + 28.0f, y + 6.0f, 1672.0f, 1.0f, 0.0f, AE_COLOR_RULE);
	for (row = 0; row < t->rows; row++)
	{
		float cy = y + 40.0f + (float)row * pitch;

		put_tracked(&f, AE_FONT_ROW, 20, 0.04f, LEFT + 28.0f, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, t->row[row].name);
		for (column = 0; column < t->columns; column++)
			put(&f, AE_FONT_ROW, 24, right - (float)(t->columns - 1 - column) * step, cy, AE_ALIGN_RIGHT,
				strcmp(t->row[row].value[column], AE_RV_DASH) ? AE_COLOR_TITLE : AE_COLOR_MUTED, t->row[row].value[column]);
	}
	page.prompt_count = 0;
	memset(page.prompts, 0, sizeof(page.prompts));
	page.prompts[page.prompt_count].button = AE_BUTTON_B;
	page.prompts[page.prompt_count].label = "Back";
	page.prompts[page.prompt_count++].action = AE_ACTION_BACK;
	ae_widget_footer_band(&density, frame.rect.y + 1080.0f * f.m);
	ae_widget_prompts(&density, frame.rect.x + LEFT * f.m, frame.rect.y + 1020.0f * f.m, page.prompts, page.prompt_count,
		"THIS PC'S VIEW", frame.rect.x + (LEFT + 1728.0f) * f.m, -1, DETAILS_HIT);
}

static struct ae_screen_class const page_class =
{
	.name = "record_details", .leave = page_leave, .handle = page_handle, .draw = page_draw, .pointer = page_pointer,
};
