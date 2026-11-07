/*
AE_GAMEPADS_RULES.C

Arena Evolved: whether this kernel has a HID driver as a module or built in
(ae_gamepads.h), pure, so port/linux/tests/ae_gamepads_rules_test.c checks it.
*/

#include "ae_gamepads.h"

#include <string.h>

int ae_gamepads_kernel_has(
	const char *modules_dep,
	const char *modules_builtin,
	const char *module)
{
	const char *texts[2];
	char needle[80];
	size_t length;
	int index;

	if (!module || !module[0])
		return 0;
	length = strlen(module);
	if (length + 5 > sizeof(needle))
		return 0;
	/* ("/hid-playstation.ko": a path's last part, compressed (.ko.zst,
	.ko.xz) or not; never another module whose name only starts so) */
	needle[0] = '/';
	memcpy(needle + 1, module, length);
	memcpy(needle + 1 + length, ".ko", 4);
	texts[0] = modules_dep;
	texts[1] = modules_builtin;
	for (index = 0; index < 2; index++)
	{
		if (texts[index] && strstr(texts[index], needle))
			return 1;
	}
	return 0;
}
