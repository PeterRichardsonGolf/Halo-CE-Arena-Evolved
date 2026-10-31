#include <stdio.h>
#include "ae_pack_memory_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* the pack's memory: a read wholly in the arena, at its start, ending at its end */
	{
		unsigned long const start = 0x42001000UL, length = 0x2000UL;

		CHECK(ae_pack_memory_holds(start, length, start, 1));
		CHECK(ae_pack_memory_holds(start, length, start, (long)length));
		CHECK(ae_pack_memory_holds(start, length, start + length - 4, 4));
		CHECK(ae_pack_memory_holds(start, length, start + 0x100, 0x40));
		/* (before it, past it, running past its end, straddling its start) */
		CHECK(!ae_pack_memory_holds(start, length, start - 1, 1));
		CHECK(!ae_pack_memory_holds(start, length, start + length, 1));
		CHECK(!ae_pack_memory_holds(start, length, start + length - 4, 5));
		CHECK(!ae_pack_memory_holds(start, length, start - 4, 8));
		/* (no bytes, a negative size, no arena) */
		CHECK(!ae_pack_memory_holds(start, length, start, 0));
		CHECK(!ae_pack_memory_holds(start, length, start, -1));
		CHECK(!ae_pack_memory_holds(start, 0, start, 1));
		/* (a size whose end would wrap the address space) */
		CHECK(!ae_pack_memory_holds(start, length, start + 8, 0x7fffffffL));
		CHECK(!ae_pack_memory_holds(0xfffff000UL, 0x1000UL, 0xfffffff0UL, 0x20));
		CHECK(ae_pack_memory_holds(0xfffff000UL, 0x1000UL, 0xfffffff0UL, 0x10));
	}

	/* its bitmaps' layout: a pack's own bitmap loses Halo PC's layout, meter and multipurpose flags and keeps the
	rest; a map's bitmap keeps all of them */
	{
		unsigned long const type_and_count = 0x00040001UL;
		unsigned long const pc_all = type_and_count | AE_PACK_TEXTURE_PC_LAYOUT | AE_PACK_TEXTURE_PC_METER |
			AE_PACK_TEXTURE_PC_MULTIPURPOSE;

		CHECK(ae_pack_texture_common(pc_all, 1) == type_and_count);
		CHECK(ae_pack_texture_common(type_and_count | AE_PACK_TEXTURE_PC_LAYOUT, 1) == type_and_count);
		CHECK(ae_pack_texture_common(type_and_count, 1) == type_and_count);
		CHECK(ae_pack_texture_common(pc_all, 0) == pc_all);
		CHECK(ae_pack_texture_common(type_and_count | AE_PACK_TEXTURE_PC_LAYOUT, 0) ==
			(type_and_count | AE_PACK_TEXTURE_PC_LAYOUT));
		/* (the flags are the renderer's: xdk_d3d8.h) */
		CHECK(AE_PACK_TEXTURE_PC_LAYOUT == 0x40000000UL && AE_PACK_TEXTURE_PC_METER == 0x20000000UL &&
			AE_PACK_TEXTURE_PC_MULTIPURPOSE == 0x10000000UL);
	}

	if (failures)
		return 1;
	printf("ae_pack_memory_rules: all passed\n");
	return 0;
}
