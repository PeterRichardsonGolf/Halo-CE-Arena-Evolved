#include <stdio.h>
#include <string.h>
#include "ae_test_run_hide_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void short_press_tests(void)
{
	struct ae_test_run_hide hide;

	memset(&hide, 0, sizeof hide);
	CHECK(ae_test_run_hide_update(&hide, 1, 1000, 0));
	CHECK(ae_test_run_hide_update(&hide, 0, 1299, 0));
	CHECK(ae_test_run_hide_update(&hide, 1, 1400, 0));
	CHECK(!ae_test_run_hide_update(&hide, 0, 1401, 0));
}

static void hold_tests(void)
{
	struct ae_test_run_hide hide;

	memset(&hide, 0, sizeof hide);
	CHECK(ae_test_run_hide_update(&hide, 1, 1000, 0));
	CHECK(ae_test_run_hide_update(&hide, 1, 1300, 0));
	CHECK(!ae_test_run_hide_update(&hide, 0, 1300, 0));
}

static void reveal_tests(void)
{
	struct ae_test_run_hide hide;

	memset(&hide, 0, sizeof hide);
	ae_test_run_hide_update(&hide, 1, 1000, 0);
	ae_test_run_hide_update(&hide, 0, 1100, 0);
	CHECK(ae_test_run_hide_update(&hide, 0, 1200, 0));
	CHECK(!ae_test_run_hide_update(&hide, 0, 1201, 1));
	CHECK(!ae_test_run_hide_update(&hide, 1, 1300, 1));
	CHECK(!ae_test_run_hide_update(&hide, 0, 1350, 0));
}

int main(void)
{
	short_press_tests();
	hold_tests();
	reveal_tests();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_test_run_hide_rules: all passed\n");
	return failures != 0;
}
