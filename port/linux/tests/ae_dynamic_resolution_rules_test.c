#include <stdio.h>

#include "ae_dynamic_resolution_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* n frames of frame_ms; the percent after the last */
static int frames(struct ae_dynres_state *state, int on, int views, float frame_ms, int n)
{
	int percent = state->percent;

	while (n-- > 0)
		percent = ae_dynres_frame(state, on, views, frame_ms);
	return percent;
}

int main(void)
{
	struct ae_dynres_state state;
	int percent, i;

	ae_dynres_reset(&state);
	state.on = 0;
	state.views = 0;
	CHECK(state.percent == 100);
	/* (the first frame with the setting on sets it up, at 100) */
	CHECK(ae_dynres_frame(&state, 1, 2, 10.0f) == 100);
	/* a game's first frames are not counted, however slow */
	CHECK(frames(&state, 1, 2, 40.0f, AE_DYNRES_START_FRAMES) == 100);

	/* the fitting percent: the pixels go with the square, down to a step; floor and ceiling */
	CHECK(ae_dynres_fitting_percent(100, 15.0f) == 100);
	CHECK(ae_dynres_fitting_percent(100, 20.0f) == 85);   /* 100 x sqrt(0.75) = 86.6 */
	CHECK(ae_dynres_fitting_percent(100, 30.0f) == 70);   /* 70.7 */
	CHECK(ae_dynres_fitting_percent(100, 60.0f) == 50);
	CHECK(ae_dynres_fitting_percent(100, 200.0f) == 50);  /* never under the floor */
	CHECK(ae_dynres_fitting_percent(80, 10.0f) == 95);    /* 97.9: up too, if asked */
	CHECK(ae_dynres_fitting_percent(100, 0.0f) == 100);

	/* down fast: two slow frames do nothing, the third drops to what fits */
	CHECK(frames(&state, 1, 2, 20.0f, 2) == 100);
	CHECK(ae_dynres_frame(&state, 1, 2, 20.0f) == 85);
	/* settling: the next three frames are not counted, however slow */
	CHECK(frames(&state, 1, 2, 40.0f, 3) == 85);
	/* a slightly slow run: at least one step */
	CHECK(frames(&state, 1, 2, 15.5f, 3) == 80);
	/* a very slow run goes to the floor at once, and no lower */
	frames(&state, 1, 2, 10.0f, 3);
	CHECK(frames(&state, 1, 2, 100.0f, 3) == 50);
	frames(&state, 1, 2, 10.0f, 3);
	CHECK(frames(&state, 1, 2, 100.0f, 10) == 50);

	/* up slowly: 59 fast frames do nothing, the 60th one step (at the floor no change was made, so nothing settles) */
	CHECK(frames(&state, 1, 2, 10.0f, 59) == 50);
	CHECK(ae_dynres_frame(&state, 1, 2, 10.0f) == 55);
	/* a frame between 12 and 15 ms starts the run again */
	frames(&state, 1, 2, 10.0f, 3);
	frames(&state, 1, 2, 10.0f, 50);
	CHECK(ae_dynres_frame(&state, 1, 2, 13.0f) == 55);
	CHECK(frames(&state, 1, 2, 10.0f, 59) == 55);
	CHECK(ae_dynres_frame(&state, 1, 2, 10.0f) == 60);
	/* all the way back to 100 and no further */
	for (i = 0; i < 20; i++)
	{
		frames(&state, 1, 2, 10.0f, 3);
		frames(&state, 1, 2, 10.0f, 60);
	}
	CHECK(state.percent == 100);
	/* a slow frame between fast ones never drops it: three in a row do */
	CHECK(frames(&state, 1, 2, 20.0f, 2) == 100);
	CHECK(ae_dynres_frame(&state, 1, 2, 10.0f) == 100);
	CHECK(frames(&state, 1, 2, 20.0f, 2) == 100);

	/* a hitch (a load) counts neither way */
	ae_dynres_reset(&state);
	state.on = 1;
	state.views = 1;
	CHECK(frames(&state, 1, 1, 20.0f, 2) == 100);
	CHECK(ae_dynres_frame(&state, 1, 1, 900.0f) == 100);
	CHECK(frames(&state, 1, 1, 20.0f, 2) == 100);
	percent = ae_dynres_frame(&state, 1, 1, 20.0f);
	CHECK(percent == 85);

	/* the views change, or the setting goes off: back to 100 */
	CHECK(ae_dynres_frame(&state, 1, 2, 20.0f) == 100);
	CHECK(frames(&state, 1, 2, 30.0f, AE_DYNRES_START_FRAMES) == 100);
	frames(&state, 1, 2, 30.0f, 3);
	CHECK(state.percent == 70);
	CHECK(ae_dynres_frame(&state, 0, 2, 30.0f) == 100);
	CHECK(frames(&state, 0, 2, 30.0f, 10) == 100);

	/* the forced percent */
	CHECK(ae_dynres_forced(0) == 0);
	CHECK(ae_dynres_forced(-3) == 0);
	CHECK(ae_dynres_forced(50) == 50);
	CHECK(ae_dynres_forced(10) == 50);
	CHECK(ae_dynres_forced(73) == 73);
	CHECK(ae_dynres_forced(150) == 100);

	if (failures)
		return 1;
	printf("ae_dynamic_resolution_rules: ok\n");
	return 0;
}
