#include <stdio.h>
#include <string.h>
#define AE_UPDATER_PICK_TEST
#include "../src/updater.c"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void none(const char *json)
{
	char version[32] = "unchanged";

	CHECK(!updater_pick_release(json, version, sizeof(version)));
	CHECK(!version[0]);
}

int main(void)
{
	char version[32];
	const char *releases = "[{\"tag_name\":\"map-pack-1\",\"draft\":false},"
		"{\"tag_name\":\"v0.2.2-beta\",\"draft\":false,\"prerelease\":true},"
		"{\"tag_name\":\"v0.2.1-beta\",\"draft\":false}]";
	const char *draft = "[{\"tag_name\":\"v9.0.0\",\"draft\":true},"
		"{\"tag_name\":\"v0.2.2-beta\",\"draft\":false}]";
	const char *long_tag = "[{\"tag_name\":\"v1234567890123456789012345678901234567890\",\"draft\":false}]";

	CHECK(updater_pick_release(releases, version, sizeof(version)));
	CHECK(!strcmp(version, "0.2.2-beta"));
	CHECK(updater_pick_release(draft, version, sizeof(version)));
	CHECK(!strcmp(version, "0.2.2-beta"));
	none("[{\"tag_name\":\"map-pack-1\",\"draft\":false}]");
	none("");
	none("[{\"tag_name\":\"v0.2");
	none("not json at all");
	none(long_tag);
	if (!failures)
		printf("ae_updater_pick_test: ok\n");
	return failures ? 1 : 0;
}
