#include <stdio.h>
#include <string.h>
#include "ae_cache_sets_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the prefix's first part left out */
	CHECK(!strcmp(ae_cache_set_relative_name("ae\\packs\\bomb\\bomb\\bomb"), "bomb\\bomb"));
	CHECK(!strcmp(ae_cache_set_relative_name("AE\\Packs\\vehicle_damage\\vehicles\\x"), "vehicles\\x"));
	CHECK(!strcmp(ae_cache_set_relative_name("ae/packs/bomb/sounds/pickup"), "sounds/pickup"));
	/* a stock tag, an empty name, NULL, the bare prefix: unchanged */
	CHECK(!strcmp(ae_cache_set_relative_name("weapons\\rocket launcher\\explosion"), "weapons\\rocket launcher\\explosion"));
	CHECK(!strcmp(ae_cache_set_relative_name(""), ""));
	CHECK(!strcmp(ae_cache_set_relative_name(NULL), ""));
	CHECK(!strcmp(ae_cache_set_relative_name("ae\\packs\\"), "ae\\packs\\"));
	CHECK(!strcmp(ae_cache_set_relative_name("ae\\packs\\bomb"), "ae\\packs\\bomb"));
	CHECK(!strcmp(ae_cache_set_relative_name("ae\\base\\x"), "ae\\base\\x"));

	/* the same tag under either set's prefix, or with none */
	CHECK(ae_cache_set_name_equal("ae\\packs\\bomb\\vehicles\\_shared\\warthog\\effects\\body",
		"ae\\packs\\vehicle_damage\\vehicles\\_shared\\warthog\\effects\\body"));
	CHECK(ae_cache_set_name_equal("bomb\\bomb", "ae\\packs\\bomb\\bomb\\bomb"));
	CHECK(ae_cache_set_name_equal("Bomb/Bomb", "ae\\packs\\bomb\\bomb\\bomb"));
	CHECK(!ae_cache_set_name_equal("bomb\\bomb", "ae\\packs\\bomb\\bomb\\bombs"));
	CHECK(!ae_cache_set_name_equal("bomb\\bomb\\x", "ae\\packs\\bomb\\bomb\\bomb"));
	CHECK(!ae_cache_set_name_equal(NULL, "a"));
	CHECK(ae_cache_set_name_equal(NULL, ""));
	if (failures)
		return 1;
	printf("ae_cache_sets_rules_test: ok\n");
	return 0;
}
