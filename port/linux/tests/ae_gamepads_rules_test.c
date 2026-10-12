#include <stdio.h>
#include "ae_gamepads.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	static const char dep_zst[] =
		"kernel/drivers/hid/hid-sony.ko.zst: kernel/drivers/hid/hid-ff.ko.zst\n"
		"kernel/drivers/hid/hid-playstation.ko.zst: kernel/drivers/leds/led-class-multicolor.ko.zst\n";
	static const char dep_plain[] = "kernel/drivers/hid/hid-playstation.ko:\n";
	static const char dep_xz[] = "kernel/drivers/hid/hid-sony.ko.xz:\n";
	static const char dep_other[] = "kernel/drivers/hid/hid-playstation2.ko.zst:\nkernel/drivers/hid/xhid-sony.ko:\n";
	static const char builtin[] = "kernel/drivers/hid/hid-generic.ko\nkernel/drivers/hid/hid-playstation.ko\n";

	/* a module, compressed or not */
	CHECK(ae_gamepads_kernel_has(dep_zst, NULL, "hid-playstation"));
	CHECK(ae_gamepads_kernel_has(dep_zst, NULL, "hid-sony"));
	CHECK(ae_gamepads_kernel_has(dep_plain, NULL, "hid-playstation"));
	CHECK(ae_gamepads_kernel_has(dep_xz, NULL, "hid-sony"));
	CHECK(!ae_gamepads_kernel_has(dep_xz, NULL, "hid-playstation"));
	/* built in */
	CHECK(ae_gamepads_kernel_has(NULL, builtin, "hid-playstation"));
	CHECK(ae_gamepads_kernel_has("", builtin, "hid-playstation"));
	CHECK(!ae_gamepads_kernel_has(NULL, builtin, "hid-sony"));
	/* another module whose name only starts or ends so */
	CHECK(!ae_gamepads_kernel_has(dep_other, NULL, "hid-playstation"));
	CHECK(!ae_gamepads_kernel_has(dep_other, NULL, "hid-sony"));
	/* absent, unread, nonsense */
	CHECK(!ae_gamepads_kernel_has(NULL, NULL, "hid-playstation"));
	CHECK(!ae_gamepads_kernel_has("", "", "hid-playstation"));
	CHECK(!ae_gamepads_kernel_has(dep_zst, builtin, ""));
	CHECK(!ae_gamepads_kernel_has(dep_zst, builtin, NULL));
	CHECK(!ae_gamepads_kernel_has(dep_zst, builtin,
		"a-module-name-far-too-long-for-any-kernel-to-have-it-in-its-list-of-modules-at-all"));

	if (failures)
		return 1;
	printf("ae_gamepads_rules_test: all passed\n");
	return 0;
}
