#include <stdio.h>
#include "ae_sound.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	ae_sound_reset();
	/* nothing asked: nothing */
	CHECK(ae_sound_take(0) == AE_SOUND_NONE);
	/* cursor + back in one frame: back */
	ae_sound_request(AE_SOUND_CURSOR, 0);
	ae_sound_request(AE_SOUND_BACK, 0);
	CHECK(ae_sound_take(10) == AE_SOUND_BACK);
	/* take clears */
	CHECK(ae_sound_take(20) == AE_SOUND_NONE);
	/* failure beats all, whatever the order */
	ae_sound_request(AE_SOUND_FAILURE, 0);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_sound_request(AE_SOUND_BACK, 0);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	CHECK(ae_sound_take(30) == AE_SOUND_FAILURE);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	CHECK(ae_sound_take(40) == AE_SOUND_FORWARD);
	ae_sound_request(AE_SOUND_NONE, 0);
	ae_sound_request(99, 0);
	ae_sound_request(-1, 0);
	CHECK(ae_sound_take(45) == AE_SOUND_NONE);
	/* repeat cursors 30 ms apart: one plays at 0, none at 30 and 60, one at 90 (at most every 80 ms) */
	ae_sound_reset();
	ae_sound_request(AE_SOUND_CURSOR, 1); CHECK(ae_sound_take(1000) == AE_SOUND_CURSOR);
	ae_sound_request(AE_SOUND_CURSOR, 1); CHECK(ae_sound_take(1030) == AE_SOUND_NONE);
	ae_sound_request(AE_SOUND_CURSOR, 1); CHECK(ae_sound_take(1060) == AE_SOUND_NONE);
	ae_sound_request(AE_SOUND_CURSOR, 1); CHECK(ae_sound_take(1090) == AE_SOUND_CURSOR);
	/* a non-repeat cursor plays every frame it is asked */
	ae_sound_request(AE_SOUND_CURSOR, 0); CHECK(ae_sound_take(1100) == AE_SOUND_CURSOR);
	ae_sound_request(AE_SOUND_CURSOR, 0); CHECK(ae_sound_take(1110) == AE_SOUND_CURSOR);
	/* a repeat and a press in one frame: the press's (it plays) */
	ae_sound_request(AE_SOUND_CURSOR, 1); ae_sound_request(AE_SOUND_CURSOR, 0);
	CHECK(ae_sound_take(1120) == AE_SOUND_CURSOR);
	/* the throttle is the cursor's only: a repeat's stronger sound plays */
	ae_sound_request(AE_SOUND_CURSOR, 1); ae_sound_request(AE_SOUND_FAILURE, 1);
	CHECK(ae_sound_take(1125) == AE_SOUND_FAILURE);
	/* reset forgets the last cursor */
	ae_sound_reset();
	ae_sound_request(AE_SOUND_CURSOR, 1); CHECK(ae_sound_take(1126) == AE_SOUND_CURSOR);
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
