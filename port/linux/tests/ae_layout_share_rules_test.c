/* Layouts travel with maps (port/linux/game/ae_layout_share_rules.c): the frames, a guest's inbox, which layout a
guest plays and what it writes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_layout_share_rules.h"
#include "ae_map_share_rules.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_layout_share_inbox inbox;
static struct ae_layout scratch;
static unsigned char bytes[AE_LAYOUT_SHARE_FRAME_MAXIMUM_BYTES + 64];

/* a frame through the wire: made, encoded, decoded */
static long wire(struct ae_layout_share_frame const *frame, struct ae_layout_share_frame *out)
{
	long size = ae_layout_share_frame_encode(frame, bytes, sizeof(bytes));

	if (!size || !ae_layout_share_frame_decode(bytes, size, out))
		return 0;
	return size;
}

static void test_frames(void)
{
	struct ae_layout_share_frame frame, back;
	char text[2000];
	long size;
	int index;

	for (index = 0; index < (int)sizeof(text); index++)
		text[index] = (char)('a' + index % 26);
	AE_CHECK(ae_layout_share_frame_make("h2d_salicreek", 7, text, sizeof(text), 0, &frame));
	AE_CHECK(frame.length == AE_LAYOUT_SHARE_CHUNK && frame.total == (long)sizeof(text) && frame.serial == 7);
	size = wire(&frame, &back);
	AE_CHECK(size == AE_LAYOUT_SHARE_FRAME_HEADER_BYTES + AE_LAYOUT_SHARE_CHUNK);
	AE_CHECK(back.length == frame.length && back.total == frame.total && back.offset == 0 && back.serial == 7);
	AE_CHECK(!strcmp(back.level, "h2d_salicreek") && !memcmp(back.data, text, (size_t)back.length));
	/* (the last frame: what is left) */
	AE_CHECK(ae_layout_share_frame_make("h2d_salicreek", 7, text, sizeof(text), 2 * AE_LAYOUT_SHARE_CHUNK, &frame));
	AE_CHECK(frame.length == (int)sizeof(text) - 2 * AE_LAYOUT_SHARE_CHUNK && wire(&frame, &back));
	AE_CHECK(!ae_layout_share_frame_make("h2d_salicreek", 7, text, sizeof(text), sizeof(text), &frame));
	/* (a frame stays under a list frame, which every version takes the size of) */
	AE_CHECK(AE_LAYOUT_SHARE_FRAME_MAXIMUM_BYTES <=
		AE_MAP_SHARE_WIRE_FRAME_BYTES + AE_MAP_SHARE_FRAME_ENTRIES * AE_MAP_SHARE_WIRE_ENTRY_BYTES);
	/* (none) */
	AE_CHECK(ae_layout_share_frame_make("bloodgulch", 1, NULL, 0, 0, &frame));
	AE_CHECK(wire(&frame, &back) == AE_LAYOUT_SHARE_FRAME_HEADER_BYTES && (back.flags & AE_LAYOUT_SHARE_FLAG_NONE));
	AE_CHECK(back.total == 0 && back.length == 0);
	/* (refused to make: no level, a long one, too big a text) */
	AE_CHECK(!ae_layout_share_frame_make("", 1, text, 10, 0, &frame));
	{
		char level[AE_LAYOUT_SHARE_LEVEL_BYTES + 1];

		memset(level, 'x', sizeof(level) - 1);
		level[sizeof(level) - 1] = 0;
		AE_CHECK(!ae_layout_share_frame_make(level, 1, text, 10, 0, &frame));
	}
	AE_CHECK(!ae_layout_share_frame_make("map", 1, text, AE_LAYOUT_MAXIMUM_FILE_SIZE + 1, 0, &frame));
}

static void test_decode_refusals(void)
{
	struct ae_layout_share_frame frame, back;
	char text[100];
	long size;

	memset(text, 'q', sizeof(text));
	AE_CHECK(ae_layout_share_frame_make("map", 3, text, sizeof(text), 0, &frame));
	size = ae_layout_share_frame_encode(&frame, bytes, sizeof(bytes));
	AE_CHECK(size > 0 && ae_layout_share_frame_decode(bytes, size, &back));
	/* (another size than its length says) */
	AE_CHECK(!ae_layout_share_frame_decode(bytes, size - 1, &back));
	AE_CHECK(!ae_layout_share_frame_decode(bytes, size + 1, &back));
	/* (another type: the list's frames) */
	bytes[0] = 2;
	AE_CHECK(!ae_layout_share_frame_decode(bytes, size, &back));
	bytes[0] = AE_LAYOUT_SHARE_FRAME_TYPE;
	/* (an unknown flag) */
	bytes[1] = 2;
	AE_CHECK(!ae_layout_share_frame_decode(bytes, size, &back));
	bytes[1] = 0;
	/* (a level name not ended) */
	memset(bytes + 16, 'x', AE_LAYOUT_SHARE_LEVEL_BYTES);
	AE_CHECK(!ae_layout_share_frame_decode(bytes, size, &back));
	/* (an empty level) */
	memset(bytes + 16, 0, AE_LAYOUT_SHARE_LEVEL_BYTES);
	AE_CHECK(!ae_layout_share_frame_decode(bytes, size, &back));
	memcpy(bytes + 16, "map", 4);
	AE_CHECK(ae_layout_share_frame_decode(bytes, size, &back));
	/* (bytes past the total; a total over the limit; "none" with bytes) */
	bytes[4] = 50;
	AE_CHECK(!ae_layout_share_frame_decode(bytes, size, &back));
	bytes[4] = 0;
	bytes[5] = 0;
	bytes[6] = 2;
	AE_CHECK(!ae_layout_share_frame_decode(bytes, size, &back));
	bytes[4] = 100;
	bytes[6] = 0;
	bytes[1] = AE_LAYOUT_SHARE_FLAG_NONE;
	AE_CHECK(!ae_layout_share_frame_decode(bytes, size, &back));
	bytes[1] = 0;
	/* (an offset past the total) */
	bytes[8] = 100;
	AE_CHECK(!ae_layout_share_frame_decode(bytes, size, &back));
	bytes[8] = 0;
	AE_CHECK(ae_layout_share_frame_decode(bytes, size, &back));
	/* (too short, NULL) */
	AE_CHECK(!ae_layout_share_frame_decode(bytes, AE_LAYOUT_SHARE_FRAME_HEADER_BYTES - 1, &back));
	AE_CHECK(!ae_layout_share_frame_decode(NULL, size, &back));
	/* (old guests: the list's decoder refuses a layout frame, so a 0.3.0-0.3.4 guest drops it) */
	{
		struct ae_map_share_frame list;

		AE_CHECK(!ae_map_share_frame_decode(bytes, size, &list));
		AE_CHECK(ae_layout_share_frame_make("map", 3, NULL, 0, 0, &frame));
		size = ae_layout_share_frame_encode(&frame, bytes, sizeof(bytes));
		AE_CHECK(size > 0 && !ae_map_share_frame_decode(bytes, size, &list));
	}
}

/* every frame of a text taken in order: the last take's result */
static int send_all(char const *level, unsigned int serial, char const *text, long length, int *slot)
{
	struct ae_layout_share_frame frame, back;
	long offset = 0;
	int result = -1;

	while (ae_layout_share_frame_make(level, serial, text, length, offset, &frame))
	{
		if (!wire(&frame, &back))
			return -2;
		result = ae_layout_share_take(&inbox, &back, slot);
		offset += frame.length;
		if (!text)
			break;
	}
	return result;
}

static void test_inbox(void)
{
	static char text[AE_LAYOUT_MAXIMUM_FILE_SIZE];
	struct ae_layout_share_frame frame;
	int slot, index;

	ae_layout_share_clear(&inbox);
	for (index = 0; index < (int)sizeof(text); index++)
		text[index] = (char)(' ' + index % 90);
	/* (a whole file of the limit, in frames) */
	AE_CHECK(send_all("h2d_salicreek", 1, text, sizeof(text), &slot) == 1 && slot >= 0);
	AE_CHECK(inbox.slots[slot].state == AE_LAYOUT_SHARE_TEXT && inbox.slots[slot].received == (long)sizeof(text));
	AE_CHECK(!memcmp(inbox.slots[slot].text, text, sizeof(text)));
	AE_CHECK(ae_layout_share_find(&inbox, "H2D_SALICREEK") == slot && ae_layout_share_find(&inbox, "other") == -1);
	/* (a frame that is not the next is dropped; the layout stays partial) */
	AE_CHECK(ae_layout_share_frame_make("coag", 2, text, 3000, 0, &frame));
	AE_CHECK(ae_layout_share_take(&inbox, &frame, &slot) == 0);
	AE_CHECK(ae_layout_share_frame_make("coag", 2, text, 3000, 2 * AE_LAYOUT_SHARE_CHUNK, &frame));
	AE_CHECK(ae_layout_share_take(&inbox, &frame, &slot) == -1);
	/* (... and one of another serial) */
	AE_CHECK(ae_layout_share_frame_make("coag", 3, text, 3000, AE_LAYOUT_SHARE_CHUNK, &frame));
	AE_CHECK(ae_layout_share_take(&inbox, &frame, &slot) == -1);
	AE_CHECK(inbox.slots[ae_layout_share_find(&inbox, "coag")].state == AE_LAYOUT_SHARE_PARTIAL);
	/* (a frame past the start of a level not begun) */
	AE_CHECK(ae_layout_share_frame_make("never", 3, text, 3000, AE_LAYOUT_SHARE_CHUNK, &frame));
	AE_CHECK(ae_layout_share_take(&inbox, &frame, &slot) == -1 && ae_layout_share_find(&inbox, "never") == -1);
	/* (a new text of the level begins it anew) */
	AE_CHECK(send_all("coag", 4, text + 5, 2000, &slot) == 1);
	AE_CHECK(inbox.slots[slot].state == AE_LAYOUT_SHARE_TEXT && inbox.slots[slot].total == 2000);
	AE_CHECK(!memcmp(inbox.slots[slot].text, text + 5, 2000));
	/* (none: the host has no layout for the level, in place of the text it had) */
	AE_CHECK(send_all("coag", 5, NULL, 0, &slot) == 1 && inbox.slots[slot].state == AE_LAYOUT_SHARE_NONE);
	/* (more levels than slots: the least lately used given up, the others kept) */
	ae_layout_share_clear(&inbox);
	for (index = 0; index < AE_LAYOUT_SHARE_SLOTS + 2; index++)
	{
		char level[16];

		snprintf(level, sizeof(level), "map%d", index);
		AE_CHECK(send_all(level, 1, text, 100, &slot) == 1);
	}
	AE_CHECK(ae_layout_share_find(&inbox, "map0") == -1 && ae_layout_share_find(&inbox, "map1") == -1);
	AE_CHECK(ae_layout_share_find(&inbox, "map2") >= 0 && ae_layout_share_find(&inbox, "map9") >= 0);
	ae_layout_share_clear(&inbox);
	AE_CHECK(ae_layout_share_find(&inbox, "map9") == -1);
}

static void test_decide(void)
{
	int disk;

	/* (no word from the host: the guest's own, nothing written) */
	AE_CHECK(ae_layout_share_decide(AE_LAYOUT_SHARE_HOST_UNKNOWN, 0, 1, 0, 1, &disk) == AE_LAYOUT_SHARE_APPLY_OWN);
	AE_CHECK(disk == AE_LAYOUT_SHARE_DISK_NOTHING);
	/* (the host has none: none, the guest's file untouched) */
	AE_CHECK(ae_layout_share_decide(AE_LAYOUT_SHARE_HOST_NONE, 0, 1, 0, 1, &disk) == AE_LAYOUT_SHARE_APPLY_NOTHING);
	AE_CHECK(disk == AE_LAYOUT_SHARE_DISK_NOTHING);
	/* (the host's, for another version of the map: the guest's own, the host's not kept) */
	AE_CHECK(ae_layout_share_decide(AE_LAYOUT_SHARE_HOST_TEXT, 0, 0, 0, 1, &disk) == AE_LAYOUT_SHARE_APPLY_OWN);
	AE_CHECK(disk == AE_LAYOUT_SHARE_DISK_NOTHING);
	/* (the host's for this map: used; written where the guest has none, beside a different one, not over the same) */
	AE_CHECK(ae_layout_share_decide(AE_LAYOUT_SHARE_HOST_TEXT, 1, 0, 0, 1, &disk) == AE_LAYOUT_SHARE_APPLY_HOST);
	AE_CHECK(disk == AE_LAYOUT_SHARE_DISK_WRITE);
	AE_CHECK(ae_layout_share_decide(AE_LAYOUT_SHARE_HOST_TEXT, 1, 1, 0, 1, &disk) == AE_LAYOUT_SHARE_APPLY_HOST);
	AE_CHECK(disk == AE_LAYOUT_SHARE_DISK_BESIDE);
	AE_CHECK(ae_layout_share_decide(AE_LAYOUT_SHARE_HOST_TEXT, 1, 1, 1, 1, &disk) == AE_LAYOUT_SHARE_APPLY_HOST);
	AE_CHECK(disk == AE_LAYOUT_SHARE_DISK_NOTHING);
	/* (without the player's yes: used for the match, nothing written) */
	AE_CHECK(ae_layout_share_decide(AE_LAYOUT_SHARE_HOST_TEXT, 1, 0, 0, 0, &disk) == AE_LAYOUT_SHARE_APPLY_HOST);
	AE_CHECK(disk == AE_LAYOUT_SHARE_DISK_NOTHING);
	AE_CHECK(ae_layout_share_decide(AE_LAYOUT_SHARE_HOST_TEXT, 1, 1, 0, 0, NULL) == AE_LAYOUT_SHARE_APPLY_HOST);
}

static void test_beside_and_fits(void)
{
	char out[64];
	char const *good = "layout 1 h2d_salicreek f1955e18\nremove scenery \"protected 54\"\n";

	AE_CHECK(ae_layout_share_beside_path("d:\\maps\\H2 Demakes\\layouts\\h2d_salicreek.txt", out, sizeof(out)));
	AE_CHECK(!strcmp(out, "d:\\maps\\H2 Demakes\\layouts\\h2d_salicreek.host.txt"));
	AE_CHECK(ae_layout_share_beside_path("a.TXT", out, sizeof(out)) && !strcmp(out, "a.host.txt"));
	AE_CHECK(!ae_layout_share_beside_path("layouts\\a.map", out, sizeof(out)));
	AE_CHECK(!ae_layout_share_beside_path("layouts\\.txt", out, sizeof(out)));
	AE_CHECK(!ae_layout_share_beside_path("abcdef.txt", out, 10));
	AE_CHECK(ae_layout_share_fits(good, (long)strlen(good), "h2d_salicreek", 0xf1955e18UL, &scratch));
	AE_CHECK(ae_layout_share_fits(good, (long)strlen(good), "H2D_SALICREEK", 0xf1955e18UL, &scratch));
	/* (another version of the map; another map; a line not understood; too big) */
	AE_CHECK(!ae_layout_share_fits(good, (long)strlen(good), "h2d_salicreek", 0xf1955e19UL, &scratch));
	AE_CHECK(!ae_layout_share_fits(good, (long)strlen(good), "h2d_coagulation", 0xf1955e18UL, &scratch));
	AE_CHECK(!ae_layout_share_fits("layout 1 h2d_salicreek f1955e18\nfly away\n", 41, "h2d_salicreek", 0xf1955e18UL,
		&scratch));
	AE_CHECK(!ae_layout_share_fits(good, AE_LAYOUT_MAXIMUM_FILE_SIZE + 1, "h2d_salicreek", 0xf1955e18UL, &scratch));
}

int main(void)
{
	test_frames();
	test_decode_refusals();
	test_inbox();
	test_decide();
	test_beside_and_fits();
	printf("%s\n", failures ? "FAILED" : "ok");
	return failures != 0;
}
