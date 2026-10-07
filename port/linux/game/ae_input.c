/*
AE_INPUT.C

The AE menus' input bridge (ae_input.h). M1 Task 1 leaves both entry points
empty, so the hooks link; Task 5 fills them in.
*/

#include <stddef.h>
#include "ae_input.h"

void ae_input_poll(
	void)
{
}

void ae_input_pointer(
	struct halo_ui_pointer const *pointer)
{
	(void)pointer;
}
