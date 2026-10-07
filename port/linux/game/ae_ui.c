/* ae_ui.c: Arena Evolved menus, the UI core (see ae_ui.h). No engine includes. */

#include <stddef.h>
#include "ae_ui.h"

/* key repeat timings (spec "Input, focus, motion") */
enum
{
	AE_REPEAT_DELAY_MS = 400,
	AE_REPEAT_INTERVAL_MS = 80,
	AE_REPEAT_FAST_AFTER_MS = 1000,
	AE_REPEAT_FAST_INTERVAL_MS = 40
};

static struct ae_screen stack[AE_MAXIMUM_SCREENS];
static int depth;
static enum ae_device last_device = AE_DEVICE_XBOX;

void ae_ui_reset(void)
{
	depth = 0;
	last_device = AE_DEVICE_XBOX;
}

int ae_ui_depth(void)
{
	return depth;
}

struct ae_screen *ae_ui_top(void)
{
	return depth ? &stack[depth - 1] : NULL;
}

enum ae_device ae_ui_last_device(void)
{
	return last_device;
}

int ae_ui_push(struct ae_screen_class const *class, short owner, void *data)
{
	struct ae_screen *screen;

	if (!class || depth >= AE_MAXIMUM_SCREENS)
		return 0;
	screen = &stack[depth++];
	screen->class = class;
	screen->owner = owner;
	screen->focus = 0;
	screen->data = data;
	if (class->enter)
		class->enter(screen);
	return 1;
}

void ae_ui_pop(void)
{
	struct ae_screen *screen = ae_ui_top();

	if (!screen)
		return;
	if (screen->class->leave)
		screen->class->leave(screen);
	depth--;
}

void ae_ui_dispatch(struct ae_event const *event)
{
	struct ae_screen *screen = ae_ui_top();
	int owned;

	if (!screen || !event)
		return;
	owned = screen->owner == AE_OWNER_ANY || screen->owner == event->player;
	if (!owned && !(event->action == AE_ACTION_START && screen->class->start_from_anyone))
		return;
	/* only input that reaches a screen picks the prompts' glyphs */
	last_device = (enum ae_device)event->device;
	if (screen->class->handle && screen->class->handle(screen, event))
		return;
	/* pop only the screen that saw the BACK (handle may have changed the stack) */
	if (event->action == AE_ACTION_BACK && owned && ae_ui_top() == screen)
		ae_ui_pop();
}

void ae_ui_draw(void)
{
	int index;

	for (index = 0; index < depth; index++)
		if (stack[index].class->draw)
			stack[index].class->draw(&stack[index]);
}

int ae_repeat_update(struct ae_repeat *repeat, int held, unsigned long now_ms)
{
	if (!held)
	{
		repeat->held = 0;
		return 0;
	}
	if (!repeat->held)
	{
		repeat->held = 1;
		repeat->since = now_ms;
		repeat->next = now_ms + AE_REPEAT_DELAY_MS;
		return 1;
	}
	/* times compared as offsets from the press, so a wrapping millisecond clock is fine */
	if (now_ms - repeat->since < repeat->next - repeat->since)
		return 0;
	/* one step; ticks missed in a stall are skipped, the schedule keeps its pace */
	while (repeat->next - repeat->since <= now_ms - repeat->since)
		repeat->next += repeat->next - repeat->since >= AE_REPEAT_FAST_AFTER_MS ?
			AE_REPEAT_FAST_INTERVAL_MS : AE_REPEAT_INTERVAL_MS;
	return 1;
}
