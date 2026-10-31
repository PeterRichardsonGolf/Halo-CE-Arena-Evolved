/* ae_resolve_state.c: the lookup thread's bookkeeping (JOIN BY ADDRESS) */
#include <stdio.h>

#include "ae_resolve_state.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_resolve_state s;
	unsigned long address = 99, a, b;

	ae_resolve_state_init(&s);
	CHECK(ae_resolve_state_poll(&s, &address) == -1 && address == 99);
	/* begun, going, answered once */
	a = ae_resolve_state_begin(&s);
	CHECK(ae_resolve_state_poll(&s, &address) == 0);
	ae_resolve_state_finish(&s, a, 0x0100007F);
	CHECK(ae_resolve_state_poll(&s, &address) == 1 && address == 0x0100007F);
	CHECK(ae_resolve_state_poll(&s, &address) == -1);
	/* a name not found answers 0 */
	a = ae_resolve_state_begin(&s);
	ae_resolve_state_finish(&s, a, 0);
	CHECK(ae_resolve_state_poll(&s, &address) == 1 && address == 0);
	/* cancelled: the late answer is dropped, nothing to poll */
	a = ae_resolve_state_begin(&s);
	ae_resolve_state_cancel(&s);
	CHECK(ae_resolve_state_poll(&s, &address) == -1);
	ae_resolve_state_finish(&s, a, 5);
	CHECK(ae_resolve_state_poll(&s, &address) == -1);
	/* cancelled, then another begun: the old lookup's answer never becomes the new one's */
	a = ae_resolve_state_begin(&s);
	ae_resolve_state_cancel(&s);
	b = ae_resolve_state_begin(&s);
	CHECK(a != b);
	ae_resolve_state_finish(&s, a, 7);
	CHECK(ae_resolve_state_poll(&s, &address) == 0);
	ae_resolve_state_finish(&s, b, 8);
	ae_resolve_state_finish(&s, b, 9);
	CHECK(ae_resolve_state_poll(&s, &address) == 1 && address == 8);
	/* replaced without a cancel: the same */
	a = ae_resolve_state_begin(&s);
	b = ae_resolve_state_begin(&s);
	ae_resolve_state_finish(&s, a, 1);
	CHECK(ae_resolve_state_poll(&s, NULL) == 0);
	ae_resolve_state_finish(&s, b, 2);
	CHECK(ae_resolve_state_poll(&s, NULL) == 1);
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
