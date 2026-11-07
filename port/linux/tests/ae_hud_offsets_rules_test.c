#include <stdio.h>
#include <string.h>

#include "ae_hud_offsets_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_hud_table table;

static struct ae_hud_element make(int index, float dx, float dy, float scale, int hide)
{
	struct ae_hud_element element;

	element.index = index;
	element.dx = dx;
	element.dy = dy;
	element.scale = scale;
	element.hide = hide;
	return element;
}

/* a move is to the screen's right and down whichever corner the element hangs from; a scale multiplies the tag's; hidden goes
off the screen */
static void apply_tests(void)
{
	struct ae_hud_element move = make(0, 10.0f, -4.0f, 1.5f, 0);
	short x, y;
	float sx, sy;
	int corner;

	/* (top left: as given) */
	x = 20; y = 30; sx = 1.0f; sy = 2.0f;
	ae_hud_element_apply(&move, 0, &x, &y, &sx, &sy);
	CHECK(x == 30 && y == 26 && sx == 1.5f && sy == 3.0f);
	/* (a right anchor's x counts to the left: the offset goes down so the element goes right) */
	x = 20; y = 30; sx = sy = 1.0f;
	ae_hud_element_apply(&move, 1, &x, &y, &sx, &sy);
	CHECK(x == 10 && y == 26);
	/* (a bottom anchor's y counts upward) */
	x = 20; y = 30;
	ae_hud_element_apply(&move, 2, &x, &y, &sx, &sy);
	CHECK(x == 30 && y == 34);
	x = 20; y = 30;
	ae_hud_element_apply(&move, 3, &x, &y, &sx, &sy);
	CHECK(x == 10 && y == 34);
	/* (the centre and another anchor: as given) */
	for (corner = 4; corner < 10; corner++)
	{
		x = 20; y = 30;
		ae_hud_element_apply(&move, corner, &x, &y, &sx, &sy);
		CHECK(x == 30 && y == 26);
	}
	/* (rounded to the nearest, either side of zero) */
	move = make(0, 0.4f, -0.6f, 1.0f, 0);
	x = 0; y = 0; sx = sy = 1.0f;
	ae_hud_element_apply(&move, 0, &x, &y, &sx, &sy);
	CHECK(x == 0 && y == -1);
	/* (hidden: far off to the right, inside a short whichever corner) */
	move = make(0, 0.0f, 0.0f, 1.0f, 1);
	for (corner = 0; corner < 4; corner++)
	{
		x = -100; y = 5; sx = sy = 1.0f;
		ae_hud_element_apply(&move, corner, &x, &y, &sx, &sy);
		CHECK(x > 5000 || x < -5000);
		CHECK(y == 5);
	}
}

static void valid_tests(void)
{
	CHECK(ae_hud_element_is_identity(&(struct ae_hud_element){ 3, 0.0f, 0.0f, 1.0f, 0 }));
	CHECK(!ae_hud_element_is_identity(&(struct ae_hud_element){ 3, 0.0f, 0.0f, 1.0f, 1 }));
	CHECK(ae_hud_element_valid(&(struct ae_hud_element){ 31, 600.0f, -600.0f, 4.0f, 0 }));
	CHECK(!ae_hud_element_valid(&(struct ae_hud_element){ 32, 0.0f, 0.0f, 1.0f, 0 }));
	CHECK(!ae_hud_element_valid(&(struct ae_hud_element){ -1, 0.0f, 0.0f, 1.0f, 0 }));
	CHECK(!ae_hud_element_valid(&(struct ae_hud_element){ 0, 600.5f, 0.0f, 1.0f, 0 }));
	CHECK(!ae_hud_element_valid(&(struct ae_hud_element){ 0, 0.0f, 0.0f, 0.05f, 0 }));
	CHECK(!ae_hud_element_valid(&(struct ae_hud_element){ 0, 0.0f, 0.0f, 4.5f, 0 }));
	CHECK(!ae_hud_element_valid(&(struct ae_hud_element){ 0, 0.0f / 0.0f, 0.0f, 1.0f, 0 }));
}

static void table_tests(void)
{
	struct ae_hud_element out, a = make(4, -12.0f, 6.0f, 1.0f, 0), b = make(5, 0.0f, 0.0f, 2.0f, 0);

	ae_hud_table_clear_all(&table);
	CHECK(!ae_hud_table_get(&table, "sniper_bolt", 4, &out) && ae_hud_element_is_identity(&out) && out.index == 4);
	CHECK(ae_hud_table_set(&table, "sniper_bolt", &a) && ae_hud_table_set(&table, "Sniper_Bolt", &b));
	CHECK(table.count == 1 && table.weapon[0].count == 2);
	CHECK(ae_hud_table_get(&table, "SNIPER_BOLT", 4, &out) && out.dx == -12.0f && out.dy == 6.0f);
	CHECK(ae_hud_table_get(&table, "sniper_bolt", 5, &out) && out.scale == 2.0f);
	/* (set again: replaced, not added) */
	a.dx = 3.0f;
	CHECK(ae_hud_table_set(&table, "sniper_bolt", &a) && table.weapon[0].count == 2);
	CHECK(ae_hud_table_get(&table, "sniper_bolt", 4, &out) && out.dx == 3.0f);
	/* (refused: no key, a key with a space, a too long key, numbers out of range) */
	CHECK(!ae_hud_table_set(&table, "", &a) && !ae_hud_table_set(&table, "a b", &a) &&
		!ae_hud_table_set(&table, "a#", &a) &&
		!ae_hud_table_set(&table, "0123456789012345678901234567890123456789012345678", &a));
	a = make(4, 9999.0f, 0.0f, 1.0f, 0);
	CHECK(!ae_hud_table_set(&table, "sniper_bolt", &a));
	/* (the identity drops the element; the weapon goes with its last) */
	a = ae_hud_element_identity(4);
	CHECK(ae_hud_table_set(&table, "sniper_bolt", &a) && table.weapon[0].count == 1);
	CHECK(ae_hud_table_clear(&table, "sniper_bolt", 5) == 1 && table.count == 0);
	CHECK(ae_hud_table_clear(&table, "sniper_bolt", 5) == 0);
}

static void clear_tests(void)
{
	struct ae_hud_element a = make(1, 1.0f, 1.0f, 1.0f, 0), b = make(2, 1.0f, 1.0f, 1.0f, 0);

	ae_hud_table_clear_all(&table);
	ae_hud_table_set(&table, "ar", &a);
	ae_hud_table_set(&table, "ar", &b);
	ae_hud_table_set(&table, "smg", &a);
	CHECK(table.count == 2);
	CHECK(ae_hud_table_clear(&table, "ar", -1) == 2 && table.count == 1);
	CHECK(ae_hud_table_get(&table, "smg", 1, &a));
	CHECK(ae_hud_table_clear(&table, "ar", -1) == 0);
}

/* the file's text: a comment, a bad line skipped, hide as 1; formatted back as read */
static void file_tests(void)
{
	static char text[2048];
	char const *file = "# placed by hand\nsniper_bolt 4 -12 6 1 0\nar_modern 2 0 0 1 1\nbad line\nar_modern 40 1 1 1 0\n"
		"ar_modern 3 1 1\nsmg 1 1 1 1 0 extra\nsmg 1 2.5 -3 0.5 0";
	struct ae_hud_element out;
	struct ae_hud_table again;

	ae_hud_table_clear_all(&table);
	CHECK(ae_hud_table_parse(&table, file) == 3);
	CHECK(ae_hud_table_get(&table, "sniper_bolt", 4, &out) && out.dx == -12.0f && out.dy == 6.0f && !out.hide);
	CHECK(ae_hud_table_get(&table, "ar_modern", 2, &out) && out.hide);
	CHECK(ae_hud_table_get(&table, "smg", 1, &out) && out.dx == 2.5f && out.dy == -3.0f && out.scale == 0.5f);
	CHECK(ae_hud_table_format(&table, text, sizeof text));
	ae_hud_table_clear_all(&again);
	CHECK(ae_hud_table_parse(&again, text) == 3 && !memcmp(&again, &table, sizeof table));
	CHECK(!ae_hud_table_format(&table, text, 20));
	/* (an empty table: an empty file) */
	ae_hud_table_clear_all(&again);
	CHECK(ae_hud_table_format(&again, text, sizeof text) && !text[0]);
}

int main(void)
{
	apply_tests();
	valid_tests();
	table_tests();
	clear_tests();
	file_tests();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_hud_offsets_rules: ok\n");
	return failures != 0;
}
