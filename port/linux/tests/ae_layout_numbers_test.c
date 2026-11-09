#include <stdio.h>
#include "ae_layout_numbers.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int layout;

	/* the Xbox's five and BUMPER JUMPER stay */
	for (layout = 0; layout <= 5; layout++)
		CHECK(ae_layout_number_from_04x(layout) == layout);
	/* 0.4.x's RECON (the Master Chief Collection's) to 8; its HERO is OpenCE's RECON, 7 */
	CHECK(ae_layout_number_from_04x(6) == 8);
	CHECK(ae_layout_number_from_04x(7) == 7);
	/* THE DUKE 8 .. GREEN FINGERS 13 one on; CUSTOM 14 to 15 */
	CHECK(ae_layout_number_from_04x(8) == 9);
	CHECK(ae_layout_number_from_04x(13) == 14);
	CHECK(ae_layout_number_from_04x(14) == 15);
	/* nothing lands on FISHSTICKS (6), which 0.4.x had not */
	for (layout = 0; layout <= 14; layout++)
		CHECK(ae_layout_number_from_04x(layout) != 6);
	if (failures)
	{
		printf("ae_layout_numbers_test: %d failed\n", failures);
		return 1;
	}
	printf("ae_layout_numbers_test: ok\n");
	return 0;
}
