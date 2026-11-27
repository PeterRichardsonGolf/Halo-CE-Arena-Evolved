#include "harness.h"
/* ae_screen_music.c's order_put_back: BACK puts a held PLAY ORDER song back where it was taken from, or as near as
the list now goes; a song taken out of the list while it was held (a click in the songs) leaves the place past the
list's end, where a move moves nothing, and the put-back ends there rather than trying forever */

static struct { struct { struct { short focus; } list; } order; short moving_from; short count; } pick;
static int moves;
static void order_move(int direction)
{
	short to = (short)(pick.order.list.focus + direction);

	moves++;
	if (moves > 1000)
	{
		fprintf(stderr, "the put-back never ends\n");
		exit(1);
	}
	if (to >= 0 && to < pick.count)
		pick.order.list.focus = to;
}
#include "under_test.inc"

int main(int argc, char **argv)
{
	char const *case_name = argv[1];

	CASE("back")
	{
		pick.count = 6;
		pick.order.list.focus = 5;
		pick.moving_from = 1;
		order_put_back();
		CHECK(pick.order.list.focus == 1, "put back at %d, not 1", pick.order.list.focus);
		/* the held song's place is past the end now: as near as the list goes */
		pick.count = 5;
		pick.order.list.focus = 4;
		pick.moving_from = 5;
		moves = 0;
		order_put_back();
		CHECK(pick.order.list.focus == 4 && moves == 1, "focus %d after %d moves", pick.order.list.focus, moves);
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
