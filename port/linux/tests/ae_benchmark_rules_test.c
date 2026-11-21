#include <stdio.h>

#include "ae_benchmark_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	double seconds = 0.0;

	CHECK(ae_benchmark_seconds(NULL, &seconds) && seconds == 30.0);
	CHECK(ae_benchmark_seconds("15", &seconds) && seconds == 15.0);
	CHECK(ae_benchmark_seconds("7.5", &seconds) && seconds == 7.5);
	CHECK(ae_benchmark_seconds("600", &seconds) && seconds == 600.0);
	CHECK(!ae_benchmark_seconds("600.5", &seconds) && !ae_benchmark_seconds("1e9", &seconds));
	CHECK(!ae_benchmark_seconds("nan", &seconds) && !ae_benchmark_seconds("NaN", &seconds));
	CHECK(!ae_benchmark_seconds("inf", &seconds) && !ae_benchmark_seconds("infinity", &seconds) &&
		!ae_benchmark_seconds("1e999", &seconds));
	CHECK(!ae_benchmark_seconds("0", &seconds) && !ae_benchmark_seconds("-3", &seconds) &&
		!ae_benchmark_seconds("", &seconds) && !ae_benchmark_seconds("abc", &seconds) &&
		!ae_benchmark_seconds("15s", &seconds) && !ae_benchmark_seconds("15 5", &seconds));
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_benchmark_rules: ok\n");
	return failures != 0;
}
