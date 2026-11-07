# AECW at 0x80: verdict (T3-1 first step)

**Free. Use 0x80; the AEXV fallback is not needed.**

Evidence (release-076 tree, `ae/elimination` 46087c4ef read, OpenCE main and Chupa main checked):
1. Layout: variant 0x68 + signature 20 = 0x7C; `GPVO` v2 at 0x100 (to 0x148); `AEDN` 0x150-0x1AC; `AEXV` 0x1B0-0x1FC
   (8 + 48 + 20). `AECW` was 0x80 + 8 + 84 + 20 = 0xF0; with N config pairs (owner scope change) the payload is 100 bytes, so the block is 0x80 + 8 + 100 + 20 = 0x100: the whole window, no margin, still before `GPVO`. `ae/elimination` only fills
   `AEXV`'s reserved bytes (round_flags, zone_wait_s, zone_shrink_s, revive_10ths: `AE_VARIANT_EXT_SIZE` stays 48,
   offsets unchanged), so it cannot reach 0x80.
2. Writers: every gametype block is built by `playlist_profile_block_build_version` (`csmemset` 0, then the variant,
   its signature, `GPVO`; `AEDN` and `AEXV` placed by their own carry code at their own offsets).
   `playlist_profile_new` writes the variant and signature only. Seeds compare whole-block hashes of those same
   builders (`arena_gametypes.c old_block_hash`), so a block written only when not empty keeps every seed byte for
   byte. `gametype_files.c` is the dedicated server's file format, not this block.
3. Readers: the stock/OpenCE reader checks the checksum over the first 0x68 bytes only
   (`PLAYLIST_PROFILE_CHECKSUM_DATA_SIZE`), and reads `GPVO` at 0x100. OpenCE main and Chupa main use the same
   offsets and no byte in 0x7C-0xFF.
4. Measured: 332 gametype files of a real play root (`GPVO` at 0x100), bytes 0x7C-0xFF all zero.
5. Checked in code: `tools/ae_gametype_layout_check.c` now asserts the AECW range against the variant, `GPVO`, `AEDN`
   and `AEXV`.

Open hazard that stays with the implementation, not the offset: a save by a build that does not know `AECW` rebuilds
the block with zeros there (design 7.4: safe, the gametype then names ids 1-16 only), and the carry code
(`ae_variant_ext_carry`-style) must copy `AECW` on every save, `playlist_profile_expected_block` / `_matches` must
include it, and the dedicated server's gametype file (`gametype_files.c`, `server_gametype_variant.c`) needs its own
text form for ids 17+ and START pairs (T3-1 "server rotation").

## Update 11-05 (owner: sets model): AECW is version 3, 40 bytes

The named[] array, the pairs and the map mask are gone: the block is 8 + 12 + 20 = 40 bytes (0x80-0xA8) and only
version 3 is read. The proven-free window is still 0x7C-0x100 (the layout check asserts it); a write clears the whole
window, so no byte of an older, longer block outlives a newer one. The save-carry and `playlist_profile_matches`
(`ae_weapon_selection_block_match`) pins stand and cover the smaller block.
