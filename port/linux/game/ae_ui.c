/* ae_ui.c: Arena Evolved menus, the UI core (see ae_ui.h). No engine includes.

Transitions (spec 7, ae_motion.h): pushing a screen slides it in from the right over the old one, which slides left
and fades (200 ms); pushing a popover fades and scales it in (150 ms) over the screens under it, which draw as they
are. Popping is instant (the popped screen's leave is called and it is gone; a deliberate departure from spec 7's
mirrored back slide, preflight P7: drawing a screen after its leave is more risk than the motion is worth; the owner
sees it at the design gate): a popped screen's revealed screen slides in from the left (200 ms), a popped popover
just goes. Once a slide is over, the screens under the top screen are not drawn (P7); popovers draw over the screens
under them. One transition at a time: a new push or pop finishes the last, and so does any event or pointer that
reaches a screen (the press acts on the new screen: nothing waits on motion). REDUCE MOTION makes them instant. */

#include <stddef.h>
#include <string.h>
#include "ae_motion.h"
#include "ae_sound.h"
#include "ae_ui.h"

/* key repeat timings (spec "Input, focus, motion") */
enum
{
	AE_REPEAT_DELAY_MS = 400,
	AE_REPEAT_INTERVAL_MS = 80,
	AE_REPEAT_FAST_AFTER_MS = 1000,
	AE_REPEAT_FAST_INTERVAL_MS = 40,
	/* (a seeded hold: about 24 days, within a 32-bit millisecond count) */
	AE_REPEAT_NEVER_MS = 0x7FFFFFFF
};

static struct ae_screen stack[AE_MAXIMUM_SCREENS];
static int depth;
static enum ae_device last_device = AE_DEVICE_XBOX;
static ae_ui_before_draw before_draw;

enum { TRANSITION_NONE, TRANSITION_SCREEN, TRANSITION_DIALOG };
/* the running transition: its screen (SCREEN: the one sliding in, the top screen; DIALOG: the popover), direction
(SCREEN: 1 forward, -1 back) and progress 0 -> 1 */
static struct { int kind, direction, index; struct ae_motion motion; } transition;

static void transition_start(int kind, int direction, int index, unsigned short duration_ms)
{
	transition.kind = kind;
	transition.direction = direction;
	transition.index = index;
	transition.motion.duration = 0;
	ae_motion_start(&transition.motion, 0.0f, 1.0f, duration_ms);
}

/* the transition, if one is still running (one over is forgotten) */
static int transition_running(void)
{
	if (transition.kind != TRANSITION_NONE && !ae_motion_running(&transition.motion))
		transition.kind = TRANSITION_NONE;
	return transition.kind != TRANSITION_NONE;
}

static void transition_finish(void)
{
	transition.kind = TRANSITION_NONE;
}

/* the topmost screen that isn't a popover below index (index excluded), or -1 */
static int screen_below(int index)
{
	while (--index >= 0)
		if (!stack[index].screen_class->popover)
			return index;
	return -1;
}

enum { RESET_HOOKS = 4 };
static void (*reset_hooks[RESET_HOOKS])(void);

int ae_ui_add_reset_hook(void (*hook)(void))
{
	int index;

	for (index = 0; index < RESET_HOOKS; index++)
		if (reset_hooks[index] == hook)
			return 1;
	for (index = 0; index < RESET_HOOKS; index++)
		if (!reset_hooks[index])
		{
			reset_hooks[index] = hook;
			return 1;
		}
	return 0;
}

void ae_ui_reset(void)
{
	int index;

	depth = 0;
	last_device = AE_DEVICE_XBOX;
	transition_finish();
	for (index = 0; index < RESET_HOOKS; index++)
		if (reset_hooks[index])
			reset_hooks[index]();
}

int ae_ui_depth(void)
{
	return depth;
}

int ae_ui_holds(void const *data)
{
	int index;

	for (index = 0; index < depth; index++)
		if (stack[index].data == data)
			return 1;
	return 0;
}

struct ae_screen *ae_ui_top(void)
{
	return depth ? &stack[depth - 1] : NULL;
}

enum ae_device ae_ui_last_device(void)
{
	return last_device;
}

int ae_ui_push(struct ae_screen_class const *screen_class, short owner, void *data)
{
	struct ae_screen *screen;

	if (!screen_class || depth >= AE_MAXIMUM_SCREENS)
		return 0;
	transition_finish();
	screen = &stack[depth++];
	screen->screen_class = screen_class;
	screen->owner = owner;
	screen->focus = 0;
	screen->data = data;
	if (screen_class->popover)
		transition_start(TRANSITION_DIALOG, 1, depth - 1, AE_MOTION_DIALOG_OPEN_MS);
	else
		transition_start(TRANSITION_SCREEN, 1, depth - 1, AE_MOTION_SCREEN_MS);
	if (screen_class->enter)
		screen_class->enter(screen);
	return 1;
}

void ae_ui_pop(void)
{
	struct ae_screen *screen = ae_ui_top();

	int revealed;

	if (!screen)
		return;
	transition_finish();
	if (screen->screen_class->leave)
		screen->screen_class->leave(screen);
	depth--;
	/* (gone at once; a screen's pop slides the screen it reveals in from the left, a popover's has nothing to move:
	P7) */
	revealed = screen_below(depth);
	if (!screen->screen_class->popover && revealed >= 0)
		transition_start(TRANSITION_SCREEN, -1, revealed, AE_MOTION_SCREEN_MS);
}

int ae_ui_remove(void const *data)
{
	int index;

	for (index = depth - 1; index >= 0; index--)
		if (stack[index].data == data)
			break;
	if (index < 0)
		return 0;
	if (index == depth - 1)
	{
		ae_ui_pop();
		return 1;
	}
	/* (under others: gone at once, nothing moves) */
	transition_finish();
	if (stack[index].screen_class->leave)
		stack[index].screen_class->leave(&stack[index]);
	memmove(&stack[index], &stack[index + 1], sizeof(stack[0]) * (size_t)(depth - index - 1));
	depth--;
	return 1;
}

void ae_ui_dispatch(struct ae_event const *event)
{
	struct ae_screen *screen = ae_ui_top();
	int owned;

	if (!screen || !event)
		return;
	owned = screen->owner == AE_OWNER_ANY || screen->owner == event->player;
	if (!owned && !(event->action == AE_ACTION_START && screen->screen_class->start_from_anyone))
		return;
	/* only input that reaches a screen picks the prompts' glyphs; it lands on the new screen: the transition ends */
	last_device = (enum ae_device)event->device;
	transition_finish();
	if (screen->screen_class->handle && screen->screen_class->handle(screen, event))
		return;
	/* pop only the screen that saw the BACK (handle may have changed the stack) */
	if (event->action == AE_ACTION_BACK && owned && ae_ui_top() == screen)
	{
		ae_sound_request(AE_SOUND_BACK, 0);
		ae_ui_pop();
	}
}

void ae_ui_dispatch_pointer(struct ae_pointer const *pointer)
{
	struct ae_screen *screen = ae_ui_top();

	if (!screen || !pointer || (screen->owner != AE_OWNER_ANY && screen->owner != pointer->player))
		return;
	transition_finish();
	/* (a touch keeps the prompts: a touchscreen has no keys to show) */
	if (!pointer->touch && (pointer->moved || pointer->left_clicks || pointer->right_clicks || pointer->wheel_steps))
		last_device = AE_DEVICE_KEYBOARD_MOUSE;
	if (screen->screen_class->pointer)
		screen->screen_class->pointer(screen, pointer);
}

void ae_ui_set_before_draw(ae_ui_before_draw before)
{
	before_draw = before;
}

void ae_ui_draw(void)
{
	/* the top screen (and the popovers over it draw too); during a forward slide, the old screen and its popovers
	under it */
	int base = screen_below(depth), under = -1, index;
	int running = transition_running();
	float t = running ? ae_motion_progress(&transition.motion) : 1.0f;
	float old_x = 0.0f, old_alpha = 1.0f, new_x = 0.0f, new_alpha = 1.0f, scrim, scale = 1.0f, alpha = 1.0f;

	if (base < 0)
		base = 0;
	if (running && transition.kind == TRANSITION_SCREEN)
	{
		ae_motion_screen(t, transition.direction, &old_x, &old_alpha, &new_x, &new_alpha);
		if (transition.direction > 0 && transition.index == base && base > 0)
		{
			under = screen_below(base);
			if (under < 0)
				under = 0;
		}
	}
	else if (running && transition.kind == TRANSITION_DIALOG)
		ae_motion_dialog(t, 1, &scrim, &scale, &alpha);
	for (index = 0; index < depth; index++)
	{
		float offset_x_u = 0.0f, screen_alpha = 1.0f, screen_scale = 1.0f;

		if (index < base)
		{
			/* (under the top screen: drawn only as the old screen of a forward slide) */
			if (under < 0 || index < under)
				continue;
			offset_x_u = old_x;
			screen_alpha = old_alpha;
		}
		else if (running && transition.index == index)
		{
			if (transition.kind == TRANSITION_SCREEN)
			{
				offset_x_u = new_x;
				screen_alpha = new_alpha;
			}
			else
			{
				screen_alpha = alpha;
				screen_scale = scale;
			}
		}
		if (!stack[index].screen_class->draw)
			continue;
		if (before_draw)
			before_draw(&stack[index], index, offset_x_u, screen_alpha, screen_scale);
		stack[index].screen_class->draw(&stack[index]);
	}
}

void ae_ui_update(void)
{
	struct ae_screen *screen = ae_ui_top();

	if (screen && screen->screen_class->update)
		screen->screen_class->update(screen);
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

void ae_repeat_seed(struct ae_repeat *repeat, int held, unsigned long now_ms)
{
	repeat->held = held != 0;
	repeat->since = now_ms;
	/* (as far ahead as offsets from the press reach: a step only after a new press) */
	repeat->next = now_ms + (held ? AE_REPEAT_NEVER_MS : AE_REPEAT_DELAY_MS);
}
