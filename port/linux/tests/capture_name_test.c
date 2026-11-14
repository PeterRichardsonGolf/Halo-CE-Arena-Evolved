#include <stdio.h>
#include "capture_name.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char long_name[600];

	CHECK(capture_name_valid("test-runs/shots/a-b_1-12.png", ".png"));
	CHECK(capture_name_valid("test-runs/clips/a.mp4", ".mp4"));
	CHECK(capture_name_valid("a.png", ".png"));
	CHECK(capture_name_valid("a.b/c.png", ".png"));
	/* the wrong or no extension */
	CHECK(!capture_name_valid("a.mp4", ".png"));
	CHECK(!capture_name_valid("a", ".png"));
	CHECK(!capture_name_valid(".png", ".png"));
	CHECK(!capture_name_valid("dir/.png", ".png"));
	/* never outside the folder */
	CHECK(!capture_name_valid("../a.png", ".png"));
	CHECK(!capture_name_valid("a/../b.png", ".png"));
	CHECK(!capture_name_valid("a/..", ".png"));
	CHECK(!capture_name_valid("a/./b.png", ".png"));
	CHECK(!capture_name_valid("/etc/a.png", ".png"));
	CHECK(!capture_name_valid("a//b.png", ".png"));
	CHECK(!capture_name_valid("a/b/", ".png"));
	CHECK(!capture_name_valid("C:/a.png", ".png"));
	CHECK(!capture_name_valid("C:\\a.png", ".png"));
	CHECK(!capture_name_valid("a\\b.png", ".png"));
	CHECK(!capture_name_valid("a b.png", ".png"));
	/* empty, null, too long */
	CHECK(!capture_name_valid("", ".png"));
	CHECK(!capture_name_valid(NULL, ".png"));
	memset(long_name, 'a', sizeof(long_name));
	strcpy(long_name + CAPTURE_NAME_RELATIVE_MAXIMUM - 4, ".png");
	CHECK(!capture_name_valid(long_name, ".png"));
	strcpy(long_name + CAPTURE_NAME_RELATIVE_MAXIMUM - 5, ".png");
	CHECK(capture_name_valid(long_name, ".png"));

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
