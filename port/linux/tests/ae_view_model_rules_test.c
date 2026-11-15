#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_view_model_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int close_enough(float a, float b)
{
	return fabsf(a - b) < 0.00001f;
}

/* The node math: scale about the gun's root, then the offset along the gun's own axes (model units), the scale and
its own orientation as they are. */
static void test_node(void)
{
	/* a gun turned a quarter about up: forward is world +y, left is world -x, up stays up; at (10, 20, 30), scale 1 */
	float root[3] = { 10.0f, 20.0f, 30.0f };
	float axes[3][3] = { { 0.0f, 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
	struct ae_view_model model = { { 100.0f / 100.0f, 0.0f, -2.0f }, 0.5f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.0f };
	float position[3] = { 12.0f, 20.0f, 34.0f };
	float scale = 1.0f;

	/* (the root stays at itself moved by the offset: 1 model unit forward = 0.01 world units along +y; 2 down) */
	{
		float at[3] = { root[0], root[1], root[2] };
		float at_scale = 1.0f;

		ae_view_model_node(&model, root, axes, 1.0f, at, &at_scale);
		CHECK(close_enough(at[0], 10.0f) && close_enough(at[1], 20.01f) && close_enough(at[2], 29.98f));
		CHECK(close_enough(at_scale, 0.5f));
	}
	/* (a node 2 to the world's +x and 4 up of the root: halved about it, then the same offset) */
	ae_view_model_node(&model, root, axes, 1.0f, position, &scale);
	CHECK(close_enough(position[0], 11.0f) && close_enough(position[1], 20.01f) && close_enough(position[2], 31.98f));
	CHECK(close_enough(scale, 0.5f));
	/* (the identity moves nothing; the root's own scale scales the offset) */
	{
		struct ae_view_model identity = ae_view_model_identity();
		float at[3] = { 1.0f, 2.0f, 3.0f };
		float at_scale = 2.0f;

		CHECK(ae_view_model_is_identity(&identity));
		ae_view_model_node(&identity, root, axes, 1.0f, at, &at_scale);
		CHECK(close_enough(at[0], 1.0f) && close_enough(at[1], 2.0f) && close_enough(at[2], 3.0f) && at_scale == 2.0f);
		model.scale = 1.0f;
		model.offset[0] = 100.0f;
		model.offset[1] = model.offset[2] = 0.0f;
		ae_view_model_node(&model, root, axes, 3.0f, at, &at_scale);
		CHECK(close_enough(at[1], 5.0f));
	}
}

static void test_valid(void)
{
	struct ae_view_model model = ae_view_model_identity();

	CHECK(ae_view_model_valid(&model));
	model.scale = 0.1f;
	CHECK(!ae_view_model_valid(&model));
	model.scale = 3.5f;
	CHECK(!ae_view_model_valid(&model));
	model.scale = 1.0f;
	model.offset[1] = 41.0f;
	CHECK(!ae_view_model_valid(&model));
	model.offset[1] = NAN;
	CHECK(!ae_view_model_valid(&model));
}

/* The shipped numbers, a weapon not named (the identity), the file's text over them and the round trip. */
static void test_table(void)
{
	struct ae_view_model_table table, again;
	struct ae_view_model model;
	static char text[1024];

	ae_view_model_table_defaults(&table);
	CHECK(ae_view_model_table_get(&table, "ar_modern", &model) && close_enough(model.scale, 0.78f));
	CHECK(ae_view_model_is_placed(&model) && close_enough(model.place[2], -5.016f) && close_enough(model.turn[2], -7.047f));
	CHECK(ae_view_model_table_get(&table, "AR_MODERN", &model));
	CHECK(!ae_view_model_table_get(&table, "assault rifle", &model) && ae_view_model_is_identity(&model));
	/* (a file's lines over the table: a comment, a bad line and a refused one are skipped) */
	CHECK(ae_view_model_table_parse(&table, "# view models\nar_modern 1 2 3 0.9\nbad line\nsmg 0 0 0 9\nsmg 0.5 -1 0 1.1 x\n"
		"smg 0.5 -1 0 1.1\n") == 2);
	CHECK(ae_view_model_table_get(&table, "ar_modern", &model) && model.offset[0] == 1.0f && model.offset[2] == 3.0f &&
		close_enough(model.scale, 0.9f));
	CHECK(ae_view_model_table_get(&table, "smg", &model) && model.offset[1] == -1.0f);
	/* (a four-number line over the shipped combat rifle keeps its one-piece placement) */
	CHECK(ae_view_model_table_get(&table, "ar_modern", &model) && ae_view_model_is_placed(&model) &&
		close_enough(model.turn[2], -7.047f));
	/* (the round trip: the text read back is the table) */
	CHECK(ae_view_model_table_format(&table, text, sizeof text));
	memset(&again, 0, sizeof again);
	CHECK(ae_view_model_table_parse(&again, text) == table.count && again.count == table.count);
	CHECK(ae_view_model_table_get(&again, "ar_modern", &model) && model.offset[1] == 2.0f && close_enough(model.scale, 0.9f));
	CHECK(ae_view_model_table_get(&again, "smg", &model) && close_enough(model.scale, 1.1f));
	CHECK(!ae_view_model_table_format(&table, text, 8));
	/* (keys: none empty, long or with a space; a full table takes no new weapon; clear drops one) */
	model = ae_view_model_identity();
	CHECK(!ae_view_model_table_set(&table, "", &model) && !ae_view_model_table_set(&table, "two words", &model));
	CHECK(ae_view_model_table_clear(&table, "smg") && !ae_view_model_table_clear(&table, "smg"));
	CHECK(!ae_view_model_table_get(&table, "smg", &model));
}

/* The arms and gun as one piece, in the camera's frame (x forward, y left, z up), about the pivot. */
static int near3(float const a[3], float x, float y, float z)
{
	return fabsf(a[0] - x) < 0.0001f && fabsf(a[1] - y) < 0.0001f && fabsf(a[2] - z) < 0.0001f;
}

static void place(struct ae_view_model const *model, float const forward[3], float const up[3], float const pivot[3],
	float position[3], float axes[3][3], float *scale)
{
	struct ae_view_model_frame frame;

	ae_view_model_place_frame(model, forward, up, pivot, &frame);
	ae_view_model_place_node(&frame, position, axes, scale);
}

static void test_place(void)
{
	float const forward[3] = { 1.0f, 0.0f, 0.0f }, up[3] = { 0.0f, 0.0f, 1.0f }, pivot[3] = { 1.0f, 0.0f, 0.0f };
	struct ae_view_model model = ae_view_model_identity();
	struct ae_view_model_frame frame;
	float position[3], axes[3][3], scale;

#define RESET(x, y, z) do { position[0] = (x); position[1] = (y); position[2] = (z); \
	memset(axes, 0, sizeof axes); axes[0][0] = axes[1][1] = axes[2][2] = 1.0f; scale = 1.0f; } while (0)

	/* (nothing placed: no frame, and a legacy entry with no place_scale is not placed) */
	CHECK(!ae_view_model_is_placed(&model) && !ae_view_model_place_frame(&model, forward, up, pivot, &frame));
	{
		struct ae_view_model legacy = { { 1.0f, 0.0f, 0.0f }, 0.8f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.0f };

		CHECK(!ae_view_model_is_placed(&legacy) && ae_view_model_valid(&legacy));
	}
	/* (a move: 100 model units right is one world unit to the camera's right, -y here; 50 up, 200 forward) */
	model.place[0] = 100.0f;
	model.place[1] = 50.0f;
	model.place[2] = 20.0f;
	RESET(5.0f, 5.0f, 5.0f);
	place(&model, forward, up, pivot, position, axes, &scale);
	CHECK(near3(position, 5.2f, 4.0f, 5.5f) && near3(axes[0], 1.0f, 0.0f, 0.0f) && scale == 1.0f);
	/* (yaw 45... a quarter turn is past the limit; 45 degrees to the left about the pivot) */
	model = ae_view_model_identity();
	model.turn[0] = 45.0f;
	RESET(2.0f, 0.0f, 0.0f);
	place(&model, forward, up, pivot, position, axes, &scale);
	CHECK(near3(position, 1.0f + 0.70710678f, 0.70710678f, 0.0f) && near3(axes[0], 0.70710678f, 0.70710678f, 0.0f));
	/* (the pivot itself stays put under a turn) */
	RESET(pivot[0], pivot[1], pivot[2]);
	place(&model, forward, up, pivot, position, axes, &scale);
	CHECK(near3(position, 1.0f, 0.0f, 0.0f));
	/* (pitch up 45) */
	model = ae_view_model_identity();
	model.turn[1] = 45.0f;
	RESET(2.0f, 0.0f, 0.0f);
	place(&model, forward, up, pivot, position, axes, &scale);
	CHECK(near3(position, 1.0f + 0.70710678f, 0.0f, 0.70710678f) && near3(axes[2], -0.70710678f, 0.0f, 0.70710678f));
	/* (roll 45: the top goes to the right, -y) */
	model = ae_view_model_identity();
	model.turn[2] = 45.0f;
	RESET(1.0f, 0.0f, 1.0f);
	place(&model, forward, up, pivot, position, axes, &scale);
	CHECK(near3(position, 1.0f, -0.70710678f, 0.70710678f) && near3(axes[2], 0.0f, -0.70710678f, 0.70710678f));
	/* (a scale about the pivot: the node halfway in, the node's scale halved) */
	model = ae_view_model_identity();
	model.place_scale = 0.5f;
	RESET(3.0f, 0.0f, 0.0f);
	scale = 2.0f;
	place(&model, forward, up, pivot, position, axes, &scale);
	CHECK(near3(position, 2.0f, 0.0f, 0.0f) && fabsf(scale - 1.0f) < 0.0001f);
	/* (a camera looking along +y: its right is +x) */
	{
		float const along_y[3] = { 0.0f, 1.0f, 0.0f };

		model = ae_view_model_identity();
		model.place[0] = 100.0f;
		RESET(0.0f, 0.0f, 0.0f);
		place(&model, along_y, up, pivot, position, axes, &scale);
		CHECK(near3(position, 1.0f, 0.0f, 0.0f));
	}
	/* (limits: a turn past 45 degrees, a move past 60 model units, a place_scale outside the range) */
	model = ae_view_model_identity();
	model.turn[1] = 46.0f;
	CHECK(!ae_view_model_valid(&model));
	model = ae_view_model_identity();
	model.place[2] = 61.0f;
	CHECK(!ae_view_model_valid(&model));
	model = ae_view_model_identity();
	model.place_scale = 3.5f;
	CHECK(!ae_view_model_valid(&model));
	model.place_scale = NAN;
	CHECK(!ae_view_model_valid(&model));
#undef RESET
}

/* The file's eleven-number lines: read, written back as eleven, a four-number line still the gun alone. */
static void test_place_file(void)
{
	struct ae_view_model_table table, again;
	struct ae_view_model model;
	static char text[1024];

	memset(&table, 0, sizeof table);
	CHECK(ae_view_model_table_parse(&table, "ar_modern 1 0 0.3 0.78 0 -0.5 1 2 3 -4 0.9\nsmg 0 0 0 1\n"
		"bad 1 2 3 4 5\nbig 0 0 0 1 0 0 0 0 90 0 1\n") == 2);
	CHECK(ae_view_model_table_get(&table, "ar_modern", &model) && model.place[1] == -0.5f && model.turn[2] == -4.0f &&
		close_enough(model.place_scale, 0.9f) && ae_view_model_is_placed(&model));
	CHECK(ae_view_model_table_get(&table, "smg", &model) && !ae_view_model_is_placed(&model));
	CHECK(!ae_view_model_table_get(&table, "bad", &model) && !ae_view_model_table_get(&table, "big", &model));
	CHECK(ae_view_model_table_format(&table, text, sizeof text));
	CHECK(strstr(text, "ar_modern 1 0 0.3 0.78 0 -0.5 1 2 3 -4 0.9\n") && strstr(text, "smg 0 0 0 1\n"));
	memset(&again, 0, sizeof again);
	CHECK(ae_view_model_table_parse(&again, text) == 2);
	CHECK(ae_view_model_table_get(&again, "ar_modern", &model) && model.turn[0] == 2.0f);
}

int main(void)
{
	test_node();
	test_valid();
	test_table();
	test_place();
	test_place_file();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
