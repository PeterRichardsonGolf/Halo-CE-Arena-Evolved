#include <stddef.h>
#include <stdio.h>
#include "ae_ui_deleted.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the shape of ui_widget.c's struct ui_mouse_target: the widget first, then the rest */
struct target { void *widget; short bounds[4]; short kind; };

static int a, b, c, d;

int main(void)
{
	struct target targets[4];
	long count, i;

	/* nothing deleted since the targets were noted: the table stays as it is */
	ae_ui_deleted_clear();
	targets[0].widget = &a; targets[0].kind = 0;
	targets[1].widget = &b; targets[1].kind = 1;
	count = 2;
	CHECK(!ae_ui_deleted_forget(targets, &count, sizeof(targets[0]), offsetof(struct target, widget)));
	CHECK(count == 2);

	/* a deleted widget's targets go, the others keep their order */
	targets[0].widget = &a; targets[0].kind = 0;
	targets[1].widget = &b; targets[1].kind = 1;
	targets[2].widget = &c; targets[2].kind = 2;
	targets[3].widget = &b; targets[3].kind = 3;
	count = 4;
	ae_ui_deleted_note(&b);
	CHECK(ae_ui_deleted_forget(targets, &count, sizeof(targets[0]), offsetof(struct target, widget)));
	CHECK(count == 2);
	CHECK(targets[0].widget == &a && targets[0].kind == 0);
	CHECK(targets[1].widget == &c && targets[1].kind == 2);

	/* the whole screen deleted (the crash: a join closing the menus between their render and the
	next frame's pointer): no target is left */
	ae_ui_deleted_clear();
	targets[0].widget = &a; targets[1].widget = &b; targets[2].widget = &c;
	count = 3;
	ae_ui_deleted_note(&c);
	ae_ui_deleted_note(&a);
	ae_ui_deleted_note(&b);
	CHECK(ae_ui_deleted_forget(targets, &count, sizeof(targets[0]), offsetof(struct target, widget)));
	CHECK(count == 0);

	/* cleared when the targets are noted again: a new widget at a freed one's address is kept */
	ae_ui_deleted_clear();
	targets[0].widget = &a;
	count = 1;
	CHECK(!ae_ui_deleted_forget(targets, &count, sizeof(targets[0]), offsetof(struct target, widget)));
	CHECK(count == 1);

	/* more deletions than the list holds: every target goes (none can be trusted) */
	ae_ui_deleted_clear();
	for (i = 0; i < AE_UI_DELETED_MAXIMUM + 1; i++)
		ae_ui_deleted_note(&d);
	targets[0].widget = &a; targets[1].widget = &b;
	count = 2;
	CHECK(ae_ui_deleted_forget(targets, &count, sizeof(targets[0]), offsetof(struct target, widget)));
	CHECK(count == 0);

	/* an empty table, and no table, are left alone */
	count = 0;
	CHECK(!ae_ui_deleted_forget(targets, &count, sizeof(targets[0]), offsetof(struct target, widget)));
	CHECK(!ae_ui_deleted_forget(NULL, NULL, sizeof(targets[0]), 0));
	ae_ui_deleted_clear();

	if (failures)
		printf("%d failed\n", failures);
	else
		printf("ae_ui_deleted: all passed\n");
	return failures != 0;
}
