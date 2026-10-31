/* ae_ce_ui_check.c: a Custom Edition map's pause widgets, whole and broken. A small tag cache is built in memory
(a multiplayer collection, a screen, a box, a list of two buttons), checked whole, then broken one way at a time:
each break must be refused with a reason, read nothing outside the image (run under the sanitizers) and never
loop. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_ce_ui_check.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define BASE 0x40440000u
#define SIZE 0x8000u
#define DELA 0x44654C61u
#define SOUL 0x536F756Cu

enum { COLLECTION, SCREEN, BOX, LIST, ROW_A, ROW_B, TAG_COUNT };

struct rig
{
	unsigned char *bytes;
	unsigned char instances[TAG_COUNT * 0x20];
	struct ae_ce_ui_image image;
	uint32_t data[TAG_COUNT];
	uint32_t next;
};

static void put32(unsigned char *at, uint32_t value)
{
	at[0] = (unsigned char)value;
	at[1] = (unsigned char)(value >> 8);
	at[2] = (unsigned char)(value >> 16);
	at[3] = (unsigned char)(value >> 24);
}

static unsigned char *at(struct rig *r, uint32_t address)
{
	return r->bytes + (address - BASE);
}

static uint32_t take(struct rig *r, uint32_t size)
{
	uint32_t address = BASE + r->next;

	r->next += (size + 3) & ~3u;
	return address;
}

static int32_t tag(int index)
{
	return (int32_t)(((uint32_t)(0xE000 + index) << 16) | (uint32_t)index);
}

static void instance(struct rig *r, int index, uint32_t group, uint32_t address)
{
	unsigned char *entry = r->instances + index * 0x20;

	memset(entry, 0, 0x20);
	put32(entry, group);
	put32(entry + 0x0C, (uint32_t)tag(index));
	put32(entry + 0x14, address);
	r->data[index] = address;
}

/* a widget definition with handler_count handlers and the children named */
static void widget(struct rig *r, int index, int handler_count, const int *children, int child_count)
{
	uint32_t definition = take(r, 0x3EC), handlers = take(r, (uint32_t)handler_count * 0x48),
		references = take(r, (uint32_t)child_count * 0x50);
	int child;

	instance(r, index, DELA, definition);
	put32(at(r, definition + 0x54), (uint32_t)handler_count);
	put32(at(r, definition + 0x58), handlers);
	if (handler_count)
		memset(at(r, handlers), 0, (size_t)handler_count * 0x48);
	put32(at(r, definition + 0x3E0), (uint32_t)child_count);
	put32(at(r, definition + 0x3E4), references);
	for (child = 0; child < child_count; child++)
		put32(at(r, references + (uint32_t)child * 0x50 + 0x0C), (uint32_t)tag(children[child]));
}

static void rig_build(struct rig *r)
{
	static const int screen_children[] = { BOX, LIST }, list_children[] = { ROW_A, ROW_B };
	uint32_t collection, references;

	memset(r->bytes, 0xCD, SIZE);
	r->next = 0;
	collection = take(r, 0x0C);
	references = take(r, 0x10);
	instance(r, COLLECTION, SOUL, collection);
	put32(at(r, collection), 1);
	put32(at(r, collection + 4), references);
	put32(at(r, references + 0x0C), (uint32_t)tag(SCREEN));
	widget(r, SCREEN, 2, screen_children, 2);
	widget(r, BOX, 0, NULL, 0);
	widget(r, LIST, 1, list_children, 2);
	widget(r, ROW_A, 3, NULL, 0);
	widget(r, ROW_B, 3, NULL, 0);
	r->image.bytes = r->bytes;
	r->image.base = BASE;
	r->image.size = SIZE;
	r->image.instances = r->instances;
	r->image.instance_count = TAG_COUNT;
}

static int checked(struct rig *r, char *why)
{
	return ae_ce_ui_check(&r->image, tag(COLLECTION), NULL, 0, why, 160);
}

/* broken: refused, with a reason */
static void refused(struct rig *r, const char *expected)
{
	char why[160];

	CHECK(!checked(r, why));
	CHECK(strstr(why, expected) != NULL);
	if (!strstr(why, expected))
		printf("  wanted \"%s\", got \"%s\"\n", expected, why);
	rig_build(r);
}

int main(void)
{
	struct rig r;
	char why[160];
	int32_t screens[2];

	memset(&r, 0, sizeof(r));
	r.bytes = malloc(SIZE);
	if (!r.bytes)
		return 1;
	rig_build(&r);

	/* whole: the collection's screens, and screens named on their own (the campaign's), NONE among them */
	CHECK(checked(&r, why));
	CHECK(why[0] == 0);
	screens[0] = tag(SCREEN);
	screens[1] = -1;
	CHECK(ae_ce_ui_check(&r.image, -1, screens, 2, why, sizeof(why)));
	CHECK(ae_ce_ui_check(&r.image, -1, NULL, 0, why, sizeof(why)));

	/* a child count past the limit, a negative one */
	put32(at(&r, r.data[LIST] + 0x3E0), 0x7FFFFFFF);
	refused(&r, "count is out of range");
	put32(at(&r, r.data[LIST] + 0x3E0), 0xFFFFFFFF);
	refused(&r, "count is out of range");
	/* a count in range whose elements run past the image's end */
	put32(at(&r, r.data[LIST] + 0x3E4), BASE + SIZE - 2 * 0x50);
	put32(at(&r, r.data[LIST] + 0x3E0), 3);
	refused(&r, "block lies outside the tags");
	/* a count in range over bytes that are no widgets */
	put32(at(&r, r.data[LIST] + 0x3E0), 200);
	refused(&r, "past the tag table");
	/* children or handlers anywhere in memory */
	put32(at(&r, r.data[LIST] + 0x3E4), 0x10000000);
	refused(&r, "block lies outside the tags");
	put32(at(&r, r.data[ROW_B] + 0x58), BASE + SIZE - 0x10);
	refused(&r, "block lies outside the tags");
	put32(at(&r, r.data[ROW_B] + 0x58), BASE - 4);
	refused(&r, "block lies outside the tags");
	/* the collection's screens */
	put32(at(&r, r.data[COLLECTION]), 1000);
	refused(&r, "count is out of range");
	put32(at(&r, r.data[COLLECTION] + 4), 0xFFFFFFF0);
	refused(&r, "block lies outside the tags");
	/* a child naming no tag, another's, one of another group, a tag whose data is outside, a definition cut by the
	image's end */
	put32(at(&r, r.data[LIST] + 0x3E4), 0);
	refused(&r, "outside the tags");
	{
		uint32_t references = (uint32_t)(at(&r, r.data[LIST] + 0x3E4)[0] | at(&r, r.data[LIST] + 0x3E4)[1] << 8 |
			at(&r, r.data[LIST] + 0x3E4)[2] << 16 | (uint32_t)at(&r, r.data[LIST] + 0x3E4)[3] << 24);

		put32(at(&r, references + 0x0C), 0x0000FFFE);
		refused(&r, "past the tag table");
		put32(at(&r, references + 0x0C), (uint32_t)tag(ROW_A) ^ 0x00010000);
		refused(&r, "is not its tag's");
		put32(at(&r, references + 0x0C), (uint32_t)tag(COLLECTION));
		refused(&r, "of another group");
		/* a loop: a row whose child is the list */
		{
			uint32_t row = r.data[ROW_A];
			uint32_t loop = BASE + r.next;

			r.next += 0x50;
			put32(at(&r, row + 0x3E0), 1);
			put32(at(&r, row + 0x3E4), loop);
			memset(at(&r, loop), 0, 0x50);
			put32(at(&r, loop + 0x0C), (uint32_t)tag(LIST));
			/* screens link to each other: a loop is whole, each widget checked once */
			CHECK(checked(&r, why));
			rig_build(&r);
		}
	}
	/* open-widget handlers: one opening its own screen or the list's parent is a cycle and whole; one with garbage
	in its target but not the open bit is not followed; one that opens a bad index is refused */
	{
		uint32_t handler = (uint32_t)(at(&r, r.data[ROW_A] + 0x58)[0] | at(&r, r.data[ROW_A] + 0x58)[1] << 8 |
			at(&r, r.data[ROW_A] + 0x58)[2] << 16 | (uint32_t)at(&r, r.data[ROW_A] + 0x58)[3] << 24);

		put32(at(&r, handler), 1u << 3);
		put32(at(&r, handler + 0x14), (uint32_t)tag(SCREEN));
		CHECK(checked(&r, why));
		put32(at(&r, handler + 0x14), (uint32_t)tag(ROW_A));
		CHECK(checked(&r, why));
		put32(at(&r, handler + 0x14), 0xE8CE0000u | 0x7FFF);
		refused(&r, "past the tag table");
		put32(at(&r, handler), 0);
		put32(at(&r, handler + 0x14), 0xE8CE0000u | 0x7FFF);
		CHECK(checked(&r, why));
		rig_build(&r);
	}
	put32(r.instances + ROW_A * 0x20 + 0x14, 0x20000000);
	refused(&r, "tag lies outside the tags");
	put32(r.instances + ROW_A * 0x20 + 0x14, BASE + SIZE - 0x100);
	refused(&r, "tag lies outside the tags");
	/* the collection itself */
	put32(r.instances + COLLECTION * 0x20, DELA);
	refused(&r, "of another group");
	r.image.instance_count = 2;
	refused(&r, "past the tag table");

	/* whole again after all of that */
	CHECK(checked(&r, why));
	free(r.bytes);
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_ce_ui_check: all passed\n");
	return failures != 0;
}
