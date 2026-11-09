/*
FOG_SCREEN_CAMERAS.C (test)

The real previous camera bookkeeping of _rasterizer_environment_fog_screen_begin
(rasterizer_xbox_environment_fog.c): each split-screen window keeps last
frame's camera in its own slot of previous_camera_matrix[MAXIMUM_WINDOWS], the
first fog screen frame of the run filling every slot. Inside the block a local
pointer of the same name stands for the window's slot, and upstream's
"&previous_camera_matrix[i]" there indexed the pointer, not the array: a first
fog screen frame drawn in window 2 wrote slots 1 to 4 (one past the array, over
the next global: the model shader's ambient reflection tint pointer, a crash
in the next model draw), and windows 3 and 4 wrote past the array every frame.
csmemcpy is a stand-in here that refuses any write outside the array.
test_fog_screen_cameras.py takes the code (under_test.inc).
*/

#include "harness.h"

typedef unsigned short word;

#include "under_test_types.inc"

#define MAXIMUM_WINDOWS 4
#define MAXIMUM_ENVIRONMENT_FOG_SCREEN_LAYERS 4
#define MATRIX_REALS ((long)(sizeof(real_matrix4x3) / sizeof(real)))

struct rasterizer_environment_fog_screen_layer { real u, v; };
struct rasterizer_environment_fog_screen_wind { real a, b; };
struct rasterizer_environment_fog_screen_window
{
	struct rasterizer_environment_fog_screen_layer layers[MAXIMUM_ENVIRONMENT_FOG_SCREEN_LAYERS];
	struct rasterizer_environment_fog_screen_wind wind;
};
struct fog_screen { struct { long index; } map; short layer_count; };
/* (the camera's first matrix-sized bytes are what the first frame copies) */
struct render_camera { real values[16]; };

static struct
{
	short window_index;
	struct render_camera camera;
	struct { real_matrix4x3 world_to_view; } frustum;
} global_window_parameters;
static struct { real dt; } global_frame_parameters;
static const real_matrix4x3 identity = { 1.0f };
static const real_matrix4x3 *global_identity4x3 = &identity;
static boolean local_fog_screen_first_time = TRUE;
static struct rasterizer_environment_fog_screen_window windows[MAXIMUM_WINDOWS];
static real_matrix4x3 previous_camera_matrix[MAXIMUM_WINDOWS];
/* the matrix the frame's motion was taken from (matrix4x3_inverse's input) */
static const real_matrix4x3 *inverted;

static void copy_into_array(void *destination, const void *source, size_t size)
{
	unsigned char *start = (unsigned char *)previous_camera_matrix;
	unsigned char *end = (unsigned char *)(previous_camera_matrix + MAXIMUM_WINDOWS);
	unsigned char *at = destination;

	CHECK(at >= start && at + size <= end && (at - start) % (long)sizeof(real_matrix4x3) == 0,
		"a previous camera written outside the %d windows' slots (at slot %+ld of %ld bytes)", MAXIMUM_WINDOWS,
		(long)((at - start) / (long)sizeof(real_matrix4x3)), (long)size);
	memcpy(destination, source, size);
}

#define csmemcpy(destination, source, size) copy_into_array((destination), (source), (size))
#define csmemset(destination, value, size) memset((destination), (value), (size))
#define bitmap_group_get(index) ((void)(index), (void *)0)
#define real_local_random() 0.5f
#define rasterizer_environment_fog_screen_wind_update(screen, wind) ((void)(screen), (void)(wind))
#define rasterizer_environment_fog_screen_wind_get_vector(window_index, dt, vector) \
	((void)(window_index), (void)(dt), (vector)->i = 0.0f, (vector)->j = 0.0f, (vector)->k = 0.0f)
#define matrix4x3_inverse(matrix, result) (inverted = (matrix), *(result) = *(matrix))
#define matrix4x3_multiply(a, b, result) ((void)(a), *(result) = *(b))

#include "under_test.inc"

static void fill(real_matrix4x3 *matrix, real value)
{
	real *reals = (real *)matrix;
	long index;

	for (index = 0; index < MATRIX_REALS; index++)
		reals[index] = value + (real)index;
}

static boolean same(const real_matrix4x3 *matrix, real value)
{
	real_matrix4x3 expected;

	fill(&expected, value);
	return memcmp(matrix, &expected, sizeof(expected)) == 0;
}

static void frame(short window_index, real value)
{
	struct fog_screen screen = { { 7 }, 2 };

	global_window_parameters.window_index = window_index;
	fill((real_matrix4x3 *)global_window_parameters.camera.values, 1000.0f + value);
	fill(&global_window_parameters.frustum.world_to_view, value);
	fog_screen_cameras(&screen, &windows[window_index]);
}

int main(int argc, char **argv)
{
	const char *name = argc > 1 ? argv[1] : "";
	short window_index;

	if (!strcmp(name, "first-frame-in-window-2"))
	{
		/* the run's first fog screen frame is the second player's: every slot starts from that camera, none past
		the four */
		frame(1, 50.0f);
		CHECK(!local_fog_screen_first_time, "the first frame did not end the first time");
		CHECK(inverted == &previous_camera_matrix[1], "window 2 took its motion from slot %ld",
			(long)(inverted - previous_camera_matrix));
		CHECK(same(&previous_camera_matrix[0], 1050.0f), "slot 1 is not the first frame's camera");
		CHECK(same(&previous_camera_matrix[1], 50.0f), "window 2's slot is not its camera");
		for (window_index = 2; window_index < MAXIMUM_WINDOWS; window_index++)
			CHECK(same(&previous_camera_matrix[window_index], 1050.0f), "slot %d is not the first frame's camera",
				window_index + 1);
	}
	else if (!strcmp(name, "each-window-its-own"))
	{
		/* four players: each window reads and writes its own slot, every frame */
		local_fog_screen_first_time = FALSE;
		for (window_index = 0; window_index < MAXIMUM_WINDOWS; window_index++)
			fill(&previous_camera_matrix[window_index], -100.0f * (window_index + 1));
		for (window_index = 0; window_index < MAXIMUM_WINDOWS; window_index++)
		{
			short other;

			frame(window_index, 10.0f * (window_index + 1));
			CHECK(inverted == &previous_camera_matrix[window_index], "window %d took its motion from slot %ld",
				window_index + 1, (long)(inverted - previous_camera_matrix) + 1);
			CHECK(same(&previous_camera_matrix[window_index], 10.0f * (window_index + 1)),
				"window %d's camera is not in its slot", window_index + 1);
			for (other = (short)(window_index + 1); other < MAXIMUM_WINDOWS; other++)
				CHECK(same(&previous_camera_matrix[other], -100.0f * (other + 1)),
					"window %d's frame changed slot %d", window_index + 1, other + 1);
		}
	}
	else
	{
		fprintf(stderr, "no case %s\n", name);
		return 2;
	}
	return 0;
}
