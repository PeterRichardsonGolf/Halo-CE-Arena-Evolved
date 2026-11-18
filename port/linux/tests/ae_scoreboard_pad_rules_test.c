/* A controller's D-pad on the open scoreboard (port/linux/game/ae_scoreboard_pad_rules.c) */
#include <stdio.h>
#include <string.h>
#include "ae_scoreboard_pad_rules.h"

void platform_log(char const *format, ...);
void platform_log(char const *format, ...) { (void)format; }

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_sb_pad s;
	unsigned short b;
	long pages, total;
	unsigned long ms;

	/* closed: the buttons go through untouched, no pages */
	memset(&s, 0, sizeof(s));
	b = AE_SB_PAD_UP | 0x0004;
	CHECK(ae_sb_pad_take(&s, 0, &b, 0) == 0 && b == (AE_SB_PAD_UP | 0x0004));

	/* open: a press of down is one page down and the game gets no down; other buttons are left */
	memset(&s, 0, sizeof(s));
	b = AE_SB_PAD_DOWN | 0x0004;
	pages = ae_sb_pad_take(&s, 1, &b, 100);
	CHECK(pages == 1 && b == 0x0004);
	/* held: no more till 400 ms, then a page again */
	b = AE_SB_PAD_DOWN;
	CHECK(ae_sb_pad_take(&s, 1, &b, 300) == 0 && b == 0);
	b = AE_SB_PAD_DOWN;
	CHECK(ae_sb_pad_take(&s, 1, &b, 520) == 1);
	/* let go and pressed again: at once */
	b = 0;
	CHECK(ae_sb_pad_take(&s, 1, &b, 530) == 0);
	b = AE_SB_PAD_DOWN;
	CHECK(ae_sb_pad_take(&s, 1, &b, 540) == 1);

	/* up is a page up */
	memset(&s, 0, sizeof(s));
	b = AE_SB_PAD_UP;
	CHECK(ae_sb_pad_take(&s, 1, &b, 0) == -1 && b == 0);

	/* both at once: none, and neither reaches the game */
	memset(&s, 0, sizeof(s));
	b = AE_SB_PAD_UP | AE_SB_PAD_DOWN;
	CHECK(ae_sb_pad_take(&s, 1, &b, 0) == 0 && b == 0);

	/* held for 2 s: the repeat keeps its pace (a few pages, not one per read) */
	memset(&s, 0, sizeof(s));
	total = 0;
	for (ms = 0; ms <= 2000; ms += 16)
	{
		b = AE_SB_PAD_DOWN;
		total += ae_sb_pad_take(&s, 1, &b, ms);
	}
	CHECK(total >= 10 && total <= 40);

	/* closing while down is held: the game gets none of it till let go, then it does again */
	memset(&s, 0, sizeof(s));
	b = AE_SB_PAD_DOWN;
	ae_sb_pad_take(&s, 1, &b, 0);
	b = AE_SB_PAD_DOWN | 0x0004;
	CHECK(ae_sb_pad_take(&s, 0, &b, 50) == 0 && b == 0x0004);
	b = 0;
	ae_sb_pad_take(&s, 0, &b, 60);
	b = AE_SB_PAD_DOWN;
	CHECK(ae_sb_pad_take(&s, 0, &b, 70) == 0 && b == AE_SB_PAD_DOWN);

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures != 0;
}
