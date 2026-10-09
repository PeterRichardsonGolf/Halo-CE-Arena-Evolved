#include <math.h>
#include <stdio.h>
#include "ae_motion.h"
#include "ae_sound.h"
#include "ae_ui.h"

static int failures, entered, left, handled, drawn, starts;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void enter(struct ae_screen *s) { (void)s; entered++; }
static void leave(struct ae_screen *s) { (void)s; left++; }
static int handle(struct ae_screen *s, struct ae_event const *e)
{
	if (e->action == AE_ACTION_DOWN) { s->focus++; handled++; return 1; }
	if (e->action == AE_ACTION_START) { starts++; return 0; }
	return 0;
}
static void draw(struct ae_screen *s) { (void)s; drawn++; }
static int pointed;
static void point(struct ae_screen *s, struct ae_pointer const *p) { (void)s; if (p->left_clicks) pointed++; }
static struct ae_screen_class const plain = { .name = "plain", .enter = enter, .leave = leave, .handle = handle,
	.draw = draw, .pointer = point };
static struct ae_screen_class const lobby = { .name = "lobby", .enter = enter, .leave = leave, .handle = handle,
	.draw = draw, .start_from_anyone = 1 };

/* draw order: records the focus of each screen drawn */
static short order[AE_MAXIMUM_SCREENS];
static int ordered;
static void draw_order(struct ae_screen *s) { if (ordered < AE_MAXIMUM_SCREENS) order[ordered++] = s->focus; }
static struct ae_screen_class const layered = { .name = "layered", .draw = draw_order };

/* before_draw: records each call (the screen's focus, its index) between the draws */
static int befores, before_index[AE_MAXIMUM_SCREENS];
static float before_motion[3], before_motions[AE_MAXIMUM_SCREENS][3];
static void before(struct ae_screen const *s, int index, float offset_x_u, float alpha, float scale)
{
	if (befores < AE_MAXIMUM_SCREENS)
	{
		before_index[befores++] = index;
		/* (marks the order: a draw after this records the same slot) */
		if (ordered < AE_MAXIMUM_SCREENS)
			order[ordered++] = (short)(-1 - s->focus);
	}
	before_motion[0] = offset_x_u;
	before_motion[1] = alpha;
	before_motion[2] = scale;
	if (befores > 0 && befores <= AE_MAXIMUM_SCREENS)
	{
		before_motions[befores - 1][0] = offset_x_u;
		before_motions[befores - 1][1] = alpha;
		before_motions[befores - 1][2] = scale;
	}
}

/* update: the top screen's, once a frame */
static int updated;
static struct ae_screen *updated_screen;
static void update(struct ae_screen *s) { updated++; updated_screen = s; }
static struct ae_screen_class const typing = { .name = "typing", .draw = draw_order, .update = update };
static struct ae_screen_class const popover = { .name = "popover", .draw = draw_order, .popover = 1 };
/* (a draw that notes whether it drew as a closing screen's ghost) */
static int ghost_draws, plain_draws;
static void draw_ghost_note(struct ae_screen *s)
{
	(void)s;
	if (ae_ui_drawing_ghost())
		ghost_draws++;
	else
		plain_draws++;
}
static struct ae_screen_class const noting = { .name = "noting", .draw = draw_ghost_note, .handle = handle,
	.leave = leave };
static struct ae_screen_class const noting_popover = { .name = "noting popover", .draw = draw_ghost_note,
	.leave = leave, .popover = 1 };

static struct ae_event ev(short player, int action) { struct ae_event e; e.player = player; e.action = (unsigned char)action; e.device = AE_DEVICE_XBOX; e.repeat = 0; return e; }

int main(void)
{
	struct ae_event e;
	struct ae_repeat r = { 0, 0, 0 };

	/* (REDUCE MOTION until the transition tests: every push and pop is instant) */
	ae_motion_set_reduced(1);

	/* stack and focus memory */
	ae_ui_reset();
	CHECK(ae_ui_top() == NULL && ae_ui_depth() == 0);
	CHECK(ae_ui_push(&plain, 0, NULL) && ae_ui_depth() == 1 && entered == 1);
	e = ev(0, AE_ACTION_DOWN); ae_ui_dispatch(&e); ae_ui_dispatch(&e);
	CHECK(ae_ui_top()->focus == 2);
	ae_ui_push(&plain, 0, NULL);
	CHECK(ae_ui_top()->focus == 0);                         /* a new screen starts at row 0 */
	e = ev(0, AE_ACTION_BACK); ae_ui_dispatch(&e);          /* unhandled BACK pops */
	CHECK(ae_ui_depth() == 1 && left == 1 && ae_ui_top()->focus == 2);
	/* owner_filter: another player's input is dropped */
	e = ev(1, AE_ACTION_DOWN); ae_ui_dispatch(&e);
	CHECK(ae_ui_top()->focus == 2);
	e = ev(1, AE_ACTION_BACK); ae_ui_dispatch(&e);          /* another player's BACK doesn't pop */
	CHECK(ae_ui_depth() == 1 && left == 1);
	e = ev(1, AE_ACTION_START); ae_ui_dispatch(&e);         /* START too, on a screen that doesn't take it */
	CHECK(starts == 0);
	/* ... except START on a screen that takes it from anyone */
	ae_ui_push(&lobby, 0, NULL);
	handled = 0; e = ev(2, AE_ACTION_START); ae_ui_dispatch(&e);
	CHECK(starts == 1 && ae_ui_depth() == 2);               /* reached handle (returned 0, not BACK: no pop) */
	e = ev(2, AE_ACTION_DOWN); ae_ui_dispatch(&e);          /* the lobby still drops the non-owner's other input */
	CHECK(handled == 0 && starts == 1 && ae_ui_top()->focus == 0);
	e = ev(2, AE_ACTION_BACK); ae_ui_dispatch(&e);
	CHECK(ae_ui_depth() == 2);
	e = ev(0, AE_ACTION_START); ae_ui_dispatch(&e);         /* the owner's START reaches it as well */
	CHECK(starts == 2);
	/* AE_OWNER_ANY takes everyone */
	ae_ui_push(&plain, AE_OWNER_ANY, NULL);
	e = ev(3, AE_ACTION_DOWN); ae_ui_dispatch(&e);
	CHECK(ae_ui_top()->focus == 1);
	e = ev(3, AE_ACTION_BACK); ae_ui_dispatch(&e);
	CHECK(ae_ui_depth() == 2);
	CHECK(entered == 4 && left == 2);                       /* every push entered, every pop left */
	drawn = 0; ae_ui_draw();
	CHECK(drawn == 1);                                      /* the one under a finished screen isn't drawn (P7) */
	/* stack limit */
	ae_ui_reset();
	{ int i, ok = 1; for (i = 0; i < AE_MAXIMUM_SCREENS; i++) ok &= ae_ui_push(&plain, 0, NULL); CHECK(ok); }
	CHECK(!ae_ui_push(&plain, 0, NULL));
	CHECK(ae_ui_depth() == AE_MAXIMUM_SCREENS);
	/* pop on an empty stack and dispatch with no screen do nothing */
	ae_ui_reset(); ae_ui_pop(); e = ev(0, AE_ACTION_BACK); ae_ui_dispatch(&e);
	CHECK(ae_ui_depth() == 0);
	/* draw: every screen, bottom to top */
	{
		/* (a screen and two popovers over it: popovers draw over the screens under them) */
		ae_ui_push(&layered, 0, NULL); ae_ui_top()->focus = 1;
		ae_ui_push(&popover, 0, NULL); ae_ui_top()->focus = 2;
		ae_ui_push(&popover, 0, NULL); ae_ui_top()->focus = 3;
		ordered = 0; ae_ui_draw();
		CHECK(ordered == 3 && order[0] == 1 && order[1] == 2 && order[2] == 3);
		e = ev(0, AE_ACTION_BACK); ae_ui_dispatch(&e);      /* no handle: BACK pops */
		CHECK(ae_ui_depth() == 2);
	}
	/* before_draw: called before each screen's draw (no view, clip or alpha leaks between screens), with its index */
	{
		ae_ui_reset();
		ae_ui_push(&layered, 0, NULL); ae_ui_top()->focus = 1;
		ae_ui_push(&popover, 0, NULL); ae_ui_top()->focus = 2;
		ae_ui_set_before_draw(before);
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 2 && before_index[0] == 0 && before_index[1] == 1);
		CHECK(ordered == 4 && order[0] == -2 && order[1] == 1 && order[2] == -3 && order[3] == 2);
		/* (no motion yet: Task 5 fills these) */
		CHECK(before_motion[0] == 0.0f && before_motion[1] == 1.0f && before_motion[2] == 1.0f);
		CHECK(ae_ui_top()->screen_class->popover && !layered.popover && !plain.popover && plain.update == NULL);
		ae_ui_set_before_draw(NULL);
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 0 && ordered == 2);
	}
	/* update: only the top screen's, and none without a screen or a handler */
	{
		ae_ui_reset();
		updated = 0; ae_ui_update();
		CHECK(updated == 0);
		ae_ui_push(&typing, 0, NULL);
		ae_ui_push(&layered, 0, NULL);
		ae_ui_update();
		CHECK(updated == 0);
		ae_ui_pop();
		ae_ui_update();
		CHECK(updated == 1 && updated_screen == ae_ui_top());
	}
	/* transitions (spec 7): a push slides, a popover fades in; a press during one finishes it and acts on the new
	screen (Review Focus 5; the gallery's harness test was dropped, P16: this is the pin) */
	{
		ae_motion_set_reduced(0);
		ae_motion_set_now(1000);
		ae_ui_reset();
		ae_ui_set_before_draw(before);
		ae_ui_push(&plain, 0, NULL); ae_ui_top()->focus = 10;      /* A (its own slide in: over nothing) */
		ae_motion_set_now(2000);                                   /* A's slide long over */
		ae_ui_push(&plain, 0, NULL); ae_ui_top()->focus = 20;      /* B at t = 0: forward */
		ae_motion_set_now(2050);                                   /* t = .25 */
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 2 && before_index[0] == 0 && before_index[1] == 1);
		/* (A: the old screen, moving left and fading; B: coming in from the right) */
		CHECK(fabsf(before_motions[0][0] - -120.0f * ae_ease_out(.25f)) < 1e-3f &&
			fabsf(before_motions[0][1] - (1.0f - .25f / .6f)) < 1e-3f && before_motions[0][2] == 1.0f);
		CHECK(fabsf(before_motions[1][0] - 120.0f * (1.0f - ae_ease_out(.25f))) < 1e-3f &&
			fabsf(before_motions[1][1] - .25f / .8f) < 1e-3f);
		/* a DOWN at 50 ms: B's handle has it, and the slide is over */
		handled = 0; e = ev(0, AE_ACTION_DOWN); ae_ui_dispatch(&e);
		CHECK(handled == 1 && ae_ui_top()->focus == 21 && ae_ui_depth() == 2);
		ordered = befores = 0; ae_ui_draw();
		/* (B alone: the screen under a finished slide isn't drawn, P7) */
		CHECK(befores == 1 && before_index[0] == 1 && before_motions[0][0] == 0.0f && before_motions[0][1] == 1.0f &&
			before_motions[0][2] == 1.0f);
		/* a popover at 40 ms: the screen under it draws as it is, the popover fades and scales in */
		ae_ui_push(&popover, 0, NULL); ae_ui_top()->focus = 30;
		ae_motion_set_now(2090);
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 2 && before_index[0] == 1 && before_motions[0][0] == 0.0f && before_motions[0][1] == 1.0f &&
			before_motions[0][2] == 1.0f);
		CHECK(before_index[1] == 2 && before_motions[1][1] > 0.0f && before_motions[1][1] < 1.0f &&
			before_motions[1][2] > .96f && before_motions[1][2] < 1.0f);
		e = ev(0, AE_ACTION_DOWN); ae_ui_dispatch(&e);         /* (popover has no handle: no focus change) */
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 2 && before_motions[1][1] == 1.0f && before_motions[1][2] == 1.0f);
		/* a pointer that reaches the top screen finishes a transition too */
		ae_ui_pop();                                             /* (a popover's pop: its ghost fades out) */
		ae_ui_push(&plain, 0, NULL);
		ae_motion_set_now(2100);
		{
			struct ae_pointer p = { 10.0f, 20.0f, 1, 0, 0, 0, 0, 0, 0 };

			ae_ui_dispatch_pointer(&p);
		}
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 1 && before_motions[0][0] == 0.0f && before_motions[0][1] == 1.0f);
		/* BACK pops (the owner's decision over P7: a back slide mirrors the forward one: the revealed screen in from
		the left, the popped one's ghost out to the right, fading, drawn last) and asks for the back sound */
		ae_sound_reset();
		ae_motion_set_now(3000);
		e = ev(0, AE_ACTION_BACK); ae_ui_dispatch(&e);
		CHECK(ae_ui_depth() == 2 && ae_sound_take(3000) == AE_SOUND_BACK);
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 2 && before_index[0] == 1 && before_motions[0][0] == -120.0f && before_motions[0][1] == 0.0f);
		CHECK(before_index[1] == 2 && before_motions[1][0] == 0.0f && before_motions[1][1] == 1.0f);
		ae_motion_set_now(3050);
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 2 && fabsf(before_motions[1][0] - 120.0f * ae_ease_out(.25f)) < 1e-3f &&
			fabsf(before_motions[1][1] - (1.0f - .25f / .6f)) < 1e-3f);
		ae_motion_set_now(3200);
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 1 && before_motions[0][0] == 0.0f && before_motions[0][1] == 1.0f);
		/* a popover's close: its ghost fades and scales out over 100 ms, over the screen beneath, which draws as it
		is; the ghost draws knowing it is one (ae_ui_drawing_ghost: no hits) */
		ae_ui_push(&noting_popover, 0, NULL);
		ae_motion_set_now(4000);
		ae_ui_pop();
		ae_motion_set_now(4050);
		ordered = befores = ghost_draws = plain_draws = 0; ae_ui_draw();
		CHECK(befores == 2 && ghost_draws == 1 && before_index[1] == 2 && before_motions[1][1] > 0.0f &&
			before_motions[1][1] < 1.0f && before_motions[1][2] < 1.0f && before_motions[1][2] >= .96f);
		CHECK(!ae_ui_drawing_ghost() && ae_ui_drawing_alpha() == 1.0f);
		ae_motion_set_now(4100);
		ordered = befores = ghost_draws = 0; ae_ui_draw();
		CHECK(befores == 1 && ghost_draws == 0);
		/* a press during a close lands on the screen beneath, and the close is over (no ghost drawn after it) */
		ae_ui_push(&noting, 0, NULL); ae_ui_top()->focus = 40;
		ae_motion_set_now(5000);
		ae_ui_pop();
		ae_motion_set_now(5050);
		ordered = befores = ghost_draws = 0; ae_ui_draw();
		CHECK(ghost_draws == 1);
		handled = 0;
		e = ev(0, AE_ACTION_DOWN); ae_ui_dispatch(&e);
		CHECK(handled == 1 && ae_ui_top()->focus == 22);
		ordered = befores = ghost_draws = 0; ae_ui_draw();
		CHECK(befores == 1 && ghost_draws == 0 && before_motions[0][0] == 0.0f && before_motions[0][1] == 1.0f);
		/* ae_ui_remove of the top screen is instant: no ghost (its data free at once) */
		{
			static int removable;

			ae_ui_push(&noting, 0, &removable);
			ae_motion_set_now(5500);
			ae_ui_remove(&removable);
			ae_motion_set_now(5510);
			ordered = befores = ghost_draws = 0; ae_ui_draw();
			CHECK(ghost_draws == 0 && befores == 1);
		}
		/* REDUCE MOTION: every transition instant */
		ae_motion_set_reduced(1);
		ae_ui_push(&plain, 0, NULL);
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 1 && before_index[0] == 2 && before_motions[0][0] == 0.0f && before_motions[0][1] == 1.0f);
		ae_ui_push(&popover, 0, NULL);
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 2 && before_motions[1][1] == 1.0f && before_motions[1][2] == 1.0f);
		/* (closes too: no ghost) */
		ae_ui_pop();
		ae_ui_pop();
		ordered = befores = 0; ae_ui_draw();
		CHECK(befores == 1);
		ae_ui_set_before_draw(NULL);
	}
	/* last device: only input that passes the owner filter changes it */
	ae_ui_reset();
	CHECK(ae_ui_last_device() == AE_DEVICE_XBOX);
	ae_ui_push(&plain, 0, NULL);
	e = ev(1, AE_ACTION_DOWN); e.device = AE_DEVICE_PLAYSTATION; ae_ui_dispatch(&e);
	CHECK(ae_ui_last_device() == AE_DEVICE_XBOX);
	e = ev(0, AE_ACTION_DOWN); e.device = AE_DEVICE_KEYBOARD_MOUSE; ae_ui_dispatch(&e);
	CHECK(ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE);
	ae_ui_reset(); ae_ui_push(&plain, AE_OWNER_ANY, NULL);
	e = ev(2, AE_ACTION_DOWN); e.device = AE_DEVICE_NINTENDO; ae_ui_dispatch(&e);
	CHECK(ae_ui_last_device() == AE_DEVICE_NINTENDO);
	/* pointer: player 0's (or anyone's) top screen takes it; it makes the keyboard and mouse the last device */
	{
		struct ae_pointer p = { 10.0f, 20.0f, 1, 1, 0, 0, 0, 0, 0 };

		ae_ui_reset();
		ae_ui_dispatch_pointer(&p);                        /* no screen: nothing */
		ae_ui_push(&plain, 0, NULL);
		p.touch = 1;                                       /* a tap keeps the prompts */
		ae_ui_dispatch_pointer(&p);
		CHECK(pointed == 1 && ae_ui_last_device() == AE_DEVICE_XBOX);
		p.touch = 0;
		ae_ui_dispatch_pointer(&p);
		CHECK(pointed == 2 && ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE);
		ae_ui_push(&plain, 1, NULL);                       /* player 2's screen: not the mouse's */
		ae_ui_dispatch_pointer(&p);
		CHECK(pointed == 2);
		p.player = 1;                                      /* unless the mouse is player 2's */
		ae_ui_dispatch_pointer(&p);
		CHECK(pointed == 3);
		p.player = AE_PLAYER_NONE;                         /* nobody's: only anyone's screens */
		ae_ui_dispatch_pointer(&p);
		CHECK(pointed == 3);
		ae_ui_push(&lobby, AE_OWNER_ANY, NULL);            /* no pointer handler */
		ae_ui_dispatch_pointer(&p);
		CHECK(pointed == 3 && ae_ui_depth() == 3);
		ae_ui_dispatch_pointer(NULL);
	}
	/* repeat: first press steps once, then 400 ms, then every 80, 40 after 1 s; at most one step per call */
	CHECK(ae_repeat_update(&r, 1, 1000) == 1);
	CHECK(ae_repeat_update(&r, 1, 1000) == 0);
	CHECK(ae_repeat_update(&r, 1, 1399) == 0);
	CHECK(ae_repeat_update(&r, 1, 1400) == 1);
	CHECK(ae_repeat_update(&r, 1, 1479) == 0);
	CHECK(ae_repeat_update(&r, 1, 1480) == 1);
	CHECK(ae_repeat_update(&r, 1, 2000) == 1);              /* a stall: one step, the missed ticks are skipped */
	CHECK(ae_repeat_update(&r, 1, 2039) == 0);
	CHECK(ae_repeat_update(&r, 1, 2040) == 1);
	CHECK(ae_repeat_update(&r, 1, 2079) == 0);
	CHECK(ae_repeat_update(&r, 1, 2080) == 1);
	CHECK(ae_repeat_update(&r, 0, 2090) == 0 && !r.held);
	/* a controller with no player reaches only anyone's screens */
	ae_ui_reset(); ae_ui_push(&plain, 0, NULL);
	e = ev(AE_PLAYER_NONE, AE_ACTION_DOWN); ae_ui_dispatch(&e);
	CHECK(ae_ui_top()->focus == 0);
	ae_ui_push(&plain, AE_OWNER_ANY, NULL); ae_ui_dispatch(&e);
	CHECK(ae_ui_top()->focus == 1);
	/* a new press starts over */
	CHECK(ae_repeat_update(&r, 1, 3000) == 1);
	CHECK(ae_repeat_update(&r, 1, 3100) == 0);
	CHECK(ae_repeat_update(&r, 1, 3400) == 1);
	/* the millisecond clock wraps while held */
	ae_repeat_update(&r, 0, 3500);
	CHECK(ae_repeat_update(&r, 1, (unsigned long)-100) == 1);
	CHECK(ae_repeat_update(&r, 1, (unsigned long)-50) == 0);
	CHECK(ae_repeat_update(&r, 1, 299) == 0);
	CHECK(ae_repeat_update(&r, 1, 300) == 1);
	CHECK(ae_repeat_update(&r, 1, 379) == 0);
	CHECK(ae_repeat_update(&r, 1, 380) == 1);
	/* seeded as held (held when the screen opened): no step while held, however long; a new press steps */
	ae_repeat_seed(&r, 1, 5000);
	CHECK(ae_repeat_update(&r, 1, 5000) == 0);
	CHECK(ae_repeat_update(&r, 1, 5400) == 0);
	CHECK(ae_repeat_update(&r, 1, 90000) == 0);
	CHECK(ae_repeat_update(&r, 0, 90010) == 0);
	CHECK(ae_repeat_update(&r, 1, 90020) == 1);
	ae_repeat_seed(&r, 0, 100000);                          /* not held: as new */
	CHECK(!r.held && ae_repeat_update(&r, 1, 100010) == 1);
	if (failures) return 1;
	printf("ae_ui: ok\n");
	return 0;
}
