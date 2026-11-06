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
	struct ae_view_model model = { { 100.0f / 100.0f, 0.0f, -2.0f }, 0.5f };
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
	CHECK(ae_view_model_table_get(&table, "AR_MODERN", &model));
	CHECK(!ae_view_model_table_get(&table, "assault rifle", &model) && ae_view_model_is_identity(&model));
	/* (a file's lines over the table: a comment, a bad line and a refused one are skipped) */
	CHECK(ae_view_model_table_parse(&table, "# view models\nar_modern 1 2 3 0.9\nbad line\nsmg 0 0 0 9\nsmg 0.5 -1 0 1.1 x\n"
		"smg 0.5 -1 0 1.1\n") == 2);
	CHECK(ae_view_model_table_get(&table, "ar_modern", &model) && model.offset[0] == 1.0f && model.offset[2] == 3.0f &&
		close_enough(model.scale, 0.9f));
	CHECK(ae_view_model_table_get(&table, "smg", &model) && model.offset[1] == -1.0f);
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

int main(void)
{
	test_node();
	test_valid();
	test_table();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
