#include <stdio.h>
#include "ae_fp_model_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	CHECK(ae_fp_model_needs_replacing(5, 5, 1, 9));   /* the shotgun */
	CHECK(!ae_fp_model_needs_replacing(5, 6, 1, 9));  /* own first-person model: untouched */
	CHECK(!ae_fp_model_needs_replacing(5, 5, 0, 9));  /* no named graph */
	CHECK(!ae_fp_model_needs_replacing(5, 5, 1, -1)); /* no model of the graph's name */
	CHECK(!ae_fp_model_needs_replacing(5, 5, 1, 5));  /* the same model */
	CHECK(!ae_fp_model_needs_replacing(-1, -1, 1, 9));/* nothing set */
	return failures ? 1 : 0;
}
