#include <stdio.h>

#include "ae_profiles_cache_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* (0.8.2: the main menu listed every profile from disk every frame) a list built at 1000 is reused for the cache's
	time, not after, and never once a change made it invalid */
	CHECK(ae_profiles_cache_fresh(1, 1000, 1000, AE_PROFILES_CACHE_MS));
	CHECK(ae_profiles_cache_fresh(1, 1000 + AE_PROFILES_CACHE_MS - 1, 1000, AE_PROFILES_CACHE_MS));
	CHECK(!ae_profiles_cache_fresh(1, 1000 + AE_PROFILES_CACHE_MS, 1000, AE_PROFILES_CACHE_MS));
	CHECK(!ae_profiles_cache_fresh(0, 1000, 1000, AE_PROFILES_CACHE_MS));
	/* the millisecond clock wrapping round */
	CHECK(ae_profiles_cache_fresh(1, 0x10ul, 0xFFFFFFF0ul, AE_PROFILES_CACHE_MS));
	CHECK(AE_PROFILES_CACHE_MS > 0 && AE_PROFILES_CACHE_MS <= 2000);
	if (failures)
	{
		printf("ae_profiles_cache_rules_test: %d failure(s)\n", failures);
		return 1;
	}
	printf("ae_profiles_cache_rules_test: ok\n");
	return 0;
}
