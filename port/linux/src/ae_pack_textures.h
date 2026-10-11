/*
AE_PACK_TEXTURES.H

port: weapon pack. What the texture cache (xbox_textures.c) asks the game's
weapon pack code (port/linux/game/ae_weapon_pack.c) as it uploads a texture:
whether its pixels are a pack bitmap flagged SMOOTH (a TAGS record's
flags, docs/weapon-packs.md). Such a bitmap (the rifle's sharp HUD rounds,
drawn four times their size at a quarter scale) is sampled linear with its
mip levels, as the high-res HUD's textures are (xgpu.h's hires), and a
METER COVERAGE one's green is its shapes' coverage (hires_coverage).

No game header is needed: the types are plain.
*/

#ifndef __AE_PACK_TEXTURES_H
#define __AE_PACK_TEXTURES_H

/* the pixels at address (an Xbox address on 64-bit builds, as hud_hires_override_find's): 1 a SMOOTH pack bitmap,
2 also METER COVERAGE, 0 not one */
long ae_weapon_pack_smooth_bitmap_at(unsigned long address, long width, long height);

#endif
