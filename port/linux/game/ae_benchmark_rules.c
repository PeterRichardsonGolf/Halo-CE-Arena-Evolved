/*
AE_BENCHMARK_RULES.C

The benchmark's stage seconds (ae_benchmark_rules.h). No engine includes.
*/

#include <math.h>
#include <stdlib.h>

#include "ae_benchmark_rules.h"

int ae_benchmark_seconds(char const *text, double *seconds)
{
	char *end;
	double value;

	if (!text)
	{
		*seconds = AE_BENCHMARK_SECONDS_DEFAULT;
		return 1;
	}
	value = strtod(text, &end);
	if (end == text || *end || !isfinite(value) || value <= 0.0 || value > AE_BENCHMARK_SECONDS_MAXIMUM)
		return 0;
	*seconds = value;
	return 1;
}
