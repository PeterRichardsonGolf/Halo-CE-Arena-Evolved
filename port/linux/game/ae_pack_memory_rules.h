/*
AE_PACK_MEMORY_RULES.H

A pack's tags on a Custom Edition map (ae_weapon_pack.c), pure (port/linux/tests/ae_pack_memory_rules_test.c):

- THE PACK'S MEMORY. Such a map's tag reads outside its tag cache read as zeroes (ce_map_checks.c's
  ce_tags_pointer); a pack's tags lie in memory of their own (its arena: a tag header, the table, the names, the
  tags), which those reads let through when the whole read lies in it. A read that starts in it and runs past its
  end is not let through (it reads as zeroes, and is logged), nor one of no bytes or of a negative size.
- ITS BITMAPS' LAYOUT. Every texture of such a map is uploaded in Halo PC's layout (xbox_texture_cache.c), its HUD
  meters and model multipurpose maps in Halo PC's channels; a pack's own bitmap is laid out as the Xbox's on any
  map, so those three flags are taken off it. Any other flag (the texture's type, its count) is kept, and a bitmap
  that is not the pack's keeps what the map gave it.
*/

#ifndef __AE_PACK_MEMORY_RULES_H
#define __AE_PACK_MEMORY_RULES_H

/* xdk_d3d8.h's D3DCOMMON_PORT_PC_LAYOUT, D3DCOMMON_PORT_PC_METER and D3DCOMMON_PORT_PC_MULTIPURPOSE */
#define AE_PACK_TEXTURE_PC_LAYOUT 0x40000000UL
#define AE_PACK_TEXTURE_PC_METER 0x20000000UL
#define AE_PACK_TEXTURE_PC_MULTIPURPOSE 0x10000000UL
#define AE_PACK_TEXTURE_PC_FLAGS (AE_PACK_TEXTURE_PC_LAYOUT | AE_PACK_TEXTURE_PC_METER | AE_PACK_TEXTURE_PC_MULTIPURPOSE)

#ifdef D3DCOMMON_PORT_PC_LAYOUT
typedef char verify_ae_pack_texture_pc_flags[
	AE_PACK_TEXTURE_PC_LAYOUT == D3DCOMMON_PORT_PC_LAYOUT && AE_PACK_TEXTURE_PC_METER == D3DCOMMON_PORT_PC_METER &&
	AE_PACK_TEXTURE_PC_MULTIPURPOSE == D3DCOMMON_PORT_PC_MULTIPURPOSE ? 1 : -1];
#endif

/* whether size bytes at address lie in the length bytes from start (no sum overflows) */
static inline int ae_pack_memory_holds(
	unsigned long start,
	unsigned long length,
	unsigned long address,
	long size)
{
	return size > 0 && length > 0 && address >= start && address - start < length &&
		(unsigned long)size <= length - (address - start);
}

/* a texture's common flags for its bitmap: a pack's own (own) without Halo PC's layout and channels */
static inline unsigned long ae_pack_texture_common(
	unsigned long common,
	int own)
{
	return own ? common & ~AE_PACK_TEXTURE_PC_FLAGS : common;
}

#endif
