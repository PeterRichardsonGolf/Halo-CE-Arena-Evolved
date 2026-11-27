/*
AE_BENCHMARK_RULES.H

The benchmark's stage seconds (benchmark.c) as a pure rule: no engine includes (port/linux/tests/ae_benchmark_rules_test.c
compiles it alone).
*/

#ifndef __AE_BENCHMARK_RULES_H
#define __AE_BENCHMARK_RULES_H

/* the seconds a stage may be measured for, at most, and the seconds with none given */
#define AE_BENCHMARK_SECONDS_MAXIMUM 600.0
#define AE_BENCHMARK_SECONDS_DEFAULT 30.0

/* the text after a stage's colon (NULL: none given) as its seconds: 1 and *seconds, or 0 when it is not a number that is
finite, above 0 and at most AE_BENCHMARK_SECONDS_MAXIMUM, with nothing after it ("15", "7.5"; not "nan", "inf", "1e999",
"15s", "", "-3") */
int ae_benchmark_seconds(char const *text, double *seconds);

#endif
