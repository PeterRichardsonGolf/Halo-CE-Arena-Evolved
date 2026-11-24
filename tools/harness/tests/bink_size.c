#include "harness.h"
/* port/linux/game/ae_bink_size.h: which movie sizes bink_playback.c's frame buffer (at the end of a 4 MB pool, 16 MB
for the movies that ask for more) and texture Size word (12-bit width and height, 8-bit pitch in 64-byte units)
can hold */

#include "under_test.inc"

#define MB (1024UL * 1024UL)

int main(int argc, char **argv)
{
	char const *case_name = argv[1];

	CASE("sizes")
	{
		CHECK(ae_bink_size_fits(640, 480, 4 * MB), "the game's own 640x480 refused");
		CHECK(ae_bink_size_fits(1280, 720, 4 * MB), "1280x720 refused in 4 MB");
		CHECK(!ae_bink_size_fits(1920, 1080, 4 * MB), "1920x1080 taken in 4 MB (8.3 MB of frame)");
		CHECK(ae_bink_size_fits(1920, 1080, 16 * MB), "1920x1080 refused in 16 MB");
		CHECK(!ae_bink_size_fits(8, 480, 4 * MB), "8 wide taken (its pitch field wraps to 255)");
		CHECK(!ae_bink_size_fits(64, 16385, 16 * MB), "16385 high taken (its height spills into the pitch field)");
		CHECK(!ae_bink_size_fits(650, 480, 4 * MB), "a width not a multiple of 16 taken (its pitch rounds down)");
		CHECK(!ae_bink_size_fits(4112, 16, 16 * MB), "4112 wide taken (12-bit width)");
		CHECK(!ae_bink_size_fits(640, 0, 4 * MB), "no height taken");
		CHECK(!ae_bink_size_fits(0, 480, 4 * MB), "no width taken (its pitch field wraps to 255)");
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
