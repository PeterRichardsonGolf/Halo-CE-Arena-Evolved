# Weapon cache: the engine seam ledger

Read-only map of every place the engine touches the weapon-pack system today, so a single weapon cache loader (one
always-attached file, weapons named by a stable global id) can replace the per-slot packs behind one ledger.

Base: `ae/release-076` at `7ab12a7f2`. The slot-widening branch `ae/pack-slots-076` (`fd4881640`) is NOT in that base
(`git merge-base --is-ancestor` says so); it is read here as a diff and noted per seam. All line numbers are the base's
unless marked "(w)" for the widening branch. Paths are under `port/linux/game/` unless they start with `source/`, `tools/`
or `port/`.

Terms. SLOT = a pack's place 1..14 (`enum ae_weapon_slot`, `ae_weapon_slots.h`); the gametype stores a MASK of slots.
DEFINITION = a weapon tag's index on the loaded map; it exists only after the pack's tags were placed on that map.
GLOBAL ID = what the cache names a weapon by; stable across releases, never reused.

## 0. How the pieces fit today (one paragraph)

At start `ae_weapon_pack_initialize` (`ae_weapon_pack.c:1998`) reads one file per slot (`slot_files[]`, `:170`) plus two
tag-only packs (vehicle damage, bomb). When a multiplayer map's tags finish loading, one of four hooks calls
`placed_add_slots` (`:2393`); it asks the pending game variant for its slot MASK, and for each named slot, in slot
order, grows the map's tag table with that pack's own tags (`placed_add`, `:1622`), validates them, and applies the map
checks. Tag indices therefore depend on (map, mask, order). Every consumer then asks "which slot does this definition belong
to" (`ae_weapon_pack_definition_slot`, `:2840`) or "which definition is slot n's weapon k" (`ae_weapon_pack_weapon_definition`,
`:2824`). Machines agree only because the same mask and the same pack bytes give the same placement; no fingerprint
crosses the wire.

## 1. The ledger

Columns: where / consumes today / under global ids / test that pins it / risk.
"Test" names the closest existing pins (full list in section 3).

### S1 Discovery and load at start
- Where: `ae_weapon_pack.c:1998` initialize, `:2026` dispose, `slot_files[]` `:156-170`, `slot_read` `:541`,
  `tags_only_pack_read` `:619`; called from `source/game/game.c:348` / `:851`. Config: `game.weapon_packs`
  (`port/linux/src/port_config.c:986`), `debug.weapon_pack_file` (`:2113`, slot 1 only).
- Today: file name per slot under `d:\packs`, pack id string must equal the slot's id; reader checks containers and SHA-256,
  then every resource once (`ae_pack_verify_resources`). Failure policy: no packs folder = silent none; missing slot 1 =
  refusal line; missing later slot = one "none here" line; wrong id / no weapon / bad hash = refusal line, slot stays empty,
  game plays on with stock weapons. A pack flagged as a direct port loads only when the MCC install is found (`:596-602`).
- Under ids: one file, one open handle, one identity (format version, schema stamp, SHA-256). A missing or stale cache
  cannot be "slot empty": it is a start-time state every join and every gametype check must read (see S6).
- Test: `tools/test_weapon_pack_load.py`, `tools/test_weapon_pack_format.py`, `port/linux/tests/ae_weapon_slots_test.c`
  (slot table), `tools/test_release_packs.py` (reads the slot table).
- Risk: medium. The per-slot "empty" outcome is a feature today (a machine with 5 of 14 packs still plays). The cache turns
  it into all-or-nothing, which is the intent, but every "slot_loaded" caller (S6, S16, S17) must be re-read.

### S2 Map attach: where and when
- Where: hooks `source/cache/cache_files.c:2025` (Xbox maps), `:1912` (Custom Edition / PC / HaloMD / map-pack maps, after
  the map's tags are made the Xbox layout), `:1764` (MCC stock and Workshop maps), `:797` (unload). Entry functions
  `ae_weapon_pack_tags_loaded` `:2429`, `_ce` `:2440`, `_mcc` `:2454`; shared `placed_add_slots` `:2393`.
- Today: only a multiplayer scenario with a pending game variant that names a pack gets any (`refusal_asked` `:1957`;
  menus and campaign get none). Order: named slots by slot number, then vehicle pack (if VEHICLES DESTRUCTIBLE and the map has
  vehicles), then bomb pack (AE SND), then DEV MODE extras (`dev_packs_add` `:2355`). CE-family maps are named by file name
  (`map_family_parse`), MCC by identity (`ae_mcc_install_level_identity`), Xbox by header name; the name keys the refused-map
  memory used by the lobby line (S10). `custom_edition` skips only the build-string check.
- Under ids: attach the whole cache on every multiplayer map of every family, in one fixed order, regardless of the
  gametype; only the family differs (build check, name key). The four hook sites and the unload hook stay as they are.
- Test: `tools/test_weapon_pack_add.py`, `tools/test_weapon_pack_checks.py` (map families, rotation), `tools/test_map_packs.py`.
- Risk: HIGH (see section 2). Attaching everything is a different cost and failure profile from attaching the named packs.

### S3 Externals, name collisions, per-map checks
- Where: `placed_add` `:1622-1880` (externals resolved by tag group + name on the map; optional ones become no tag; required
  missing = `missing <group> <name>` refusal), own-tag placement past the table end, validator pass, model buffers;
  map's-own-rifle replacement `placed_own_rifle_replace` `:1505`, restore `:1603`. Limits: `PLACED_TABLE_TAGS_MAXIMUM`
  0xC000, `PLACED_SALT_MAXIMUM` 0xFFFE, `PLACED_MEMORY_MAXIMUM` 8 MiB (`:212-224`); format caps `AE_PACK_MAXIMUM_TAGS` 1024,
  `_OWN_TAGS` 512, `_WEAPONS` 8, `_TAG_BYTES` 16 MiB, `_FILE_SIZE` 64 MiB (`ae_pack_format.h:133-145`).
- Today: refusal is per PACK per map. A map lacking one stock tag (most Custom Edition maps) loses the whole pack for that map
  on every machine alike.
- Under ids: refusal must be per WEAPON (project decision: drop only that weapon, stock fallback), deterministic from
  (cache, map) alone so all machines agree. A self-contained base layer (the cache carries its own copies of the stock tags it
  borrows) removes most refusals, and is what makes "every weapon on every map family" possible. The map-own-rifle and
  needler replacement stays map-side but becomes keyed by weapon id, not slot.
- Test: `tools/test_weapon_pack_checks.py`, `tools/test_weapon_pack_add.py`, `port/linux/tests/ae_pack_rifle_rules_test.c`,
  `port/linux/tests/ae_pack_memory_rules_test.c`.
- Risk: HIGH. The 8 MiB memory cap, 0xC000 table cap, and every format cap are sized for 1-4 packs, not the whole set.

### S4 Bulk reads, texture and memory seams
- Where: `source/cache/cache_files_windows.c:1122` (MCC branch) and `:1152`; `ae_weapon_pack_read` `:2677` (loops all
  placements); texture flags `source/cache/xbox_texture_cache.c:734` -> `ae_weapon_pack_texture_common` `:2722`;
  `port/linux/src/xbox_textures.c:1817` -> `ae_weapon_pack_smooth_bitmap_at` `:3048`; CE-map memory allow
  `ae_glue_ce_maps.c:1007` -> `ae_weapon_pack_memory` `:2697`; `ae_weapon_pack_bitmap_is_own` `:2716`.
- Today: a read for tag index T is served if T lies in any placement's own range; pixels and samples come from the pack file
  on demand, never hashed again.
- Under ids: identical, with one placement. These are the safest seams: they key on tag index and memory range, not slot.
- Test: `tools/test_weapon_pack_look.py` (smooth bitmaps), `tools/test_weapon_pack_add.py`, `port/linux/tests/ae_pack_memory_rules_test.c`.
- Risk: low.

### S5 Definition / slot / pack identity lookups (the pivot)
- Where: `ae_weapon_pack_weapon_definition(slot, weapon)` `:2824`, `ae_weapon_pack_definition_slot(definition)` `:2840` (linear
  scan over slots x weapons), `ae_weapon_pack_tag_is_own` `:2808`, `ae_weapon_pack_slot_added` `:2752`, `_added_mask`
  `:2765`, `_played_mask` `:2758`, `_weapon_count` `:2075`, `_weapon_entry` `:2083`, `_string` `:2094`, `_identity` `:2059`,
  `_fingerprint` `:2777`.
- Today: the weapon's identity is (slot, k-th weapon of that pack). Callers that switch on the slot number are listed in S12-S15.
- Under ids: the same two functions become `definition_of(id)` and `id_of(definition)`; `definition_slot` can stay as a shim
  returning a legacy slot number for ids 1..14 while consumers migrate, then be removed.
- Test: none direct. Pinned only through consumers' tests and the game tests in section 3.
- Risk: HIGH as a group: every consumer keyed by slot is one missed re-point from a silent wrong picture, role or bot value.

### S6 Capability, join mark, refusal
- Where: `ae_silent.c:233` `ae_silent_join_request_mark` (client writes its level into the machine name's mark character),
  `:252` `packs_required`, `:263` `rifle_join_refused`, `:270` rejection code, `:325` `ae_silent_join_refused_now`, `:377`
  drop path; callers `source/networking/network_server_manager.c:2430`, `network_client_manager.c:3200` (mark) and `:2630`
  (rejection read), `network_server_message_handler.c:2102` (heard). Rules: `ae_silent_rules.c:71` `ae_silent_join_mark`,
  `:80` capability read, `:100` refused; `AE_PACKS_REJECTION_CODE` 66 (`ae_silent_rules.h:82`); level maths
  `ae_weapon_slots.c:213` `ae_weapon_slots_capability`, `:233` `ae_weapon_slots_required`; level defines
  `ae_weapon_slots.h:89-107`; chat notice `ae_joiner_notice.c` `ae_joiner_notice_pack_refusal`.
- Today: a machine tells one integer (highest level with all lower slots loaded, 4 bits, 15 max); a host refuses a machine below
  the highest slot the game names. "Level" means "has a pack in that slot", not "has the same pack": the fingerprint is
  logged only. Ship order matters because levels are cumulative.
- Widening branch (w): `ae_silent_join_level` adds an extension character so levels 16..255 fit; `ae_joiner_notice_pack_refusal(int required)`
  names 0.7.6 when the level is past the 0.7.5 ceiling; capability table grows with `AE_WEAPON_SLOT_COUNT` to 32.
- Under ids: refusal compares the cache IDENTITY (format + schema stamp + hash, or a short fold of it), not a level. A machine
  without the same cache must be refused when the game names any cache weapon; older builds cannot read the new gametype
  bytes at all and must be refused by the join token or the stamp. The rejection code and notice text need a cache wording.
- Test: `port/linux/tests/ae_silent_rules_test.c`, `port/linux/tests/ae_weapon_slots_test.c`, `tools/test_ae_silent.py`.
- Risk: HIGH. Wire and join behaviour; a mistake locks every joiner out or lets a mismatched machine into a drifting game.

### S7 Gametype storage (the mask bytes and everything packed beside them)
- Where, base: `universal_variant.weapon_pack` (slots 1-8), `game_variant_options.weapon_pack_high` (slots 9-16) and
  `pack_start[2]` (`source/game/game_engine.h:548-549`), `universal_variant.pack_loadout` (two bits per loadout slot, bit 4
  MAP SPAWNS, bits 5-7 other AE fields), `flags` bits 12-15 (the "later" rank fields, `AE_WEAPON_SLOTS_LATER_MASK`), `ball_melee`
  bit 6 (`AE_WEAPON_SLOTS_EXTRA_BIT`, rank 6/7). Decode: `source/game/game_engine.c:6449` `game_variant_weapon_pack_mask`
  (`ae_weapon_slots_mask_wide`), `:6460` `game_variant_pack_start_slot`, `:6477-6503` loadout slot decode, `:6515` mask name.
  Rank maths `ae_weapon_slots.c` (`_rank`, `_rank_set`, `_loadout`, `_start`, `_loadout_remap`). Resets `game_modes.c:232-233`.
  Seeds: `source/saved games/arena_gametypes.c:305-337` (base rows), `:14499-14524`, `:16141-16146`, `:16357-16360`,
  `:16503-16592` (revision migrations that add the EVO/DMR/needler and call `ae_weapon_slots_rank_set` / `_loadout_remap`).
  Audit: `ae_gametype_audit.c:126,145`; layout check `tools/ae_gametype_layout_check.c:72-75`.
- Widening branch (w): third byte `weapon_pack_top` in the LAST reserved byte (0x2B), mask API 32 bits, but only 24 slots can be
  stored; slots 25-32 name nothing a file or the wire can carry. After this the options struct has no spare byte.
- Today: the storage is a bitmask by slot, plus a "which slot starts" encoding that squeezes ranks 1-7 into spare bits.
- Under ids: the cache needs a catalogue form: a list of ids (or "all"), start weapon ids per loadout slot. There is no room in
  the 0x2C-byte options for that; the project decision was that the cache replaces the widening. Likely shape: keep the legacy mask
  for ids 1..N as a compatibility view (old seeds, saved gametypes, the lobby rows), add one new stored field (an AEXV-style
  extension block already exists in the gametype file, see `ae_variant_ext.h`) for ids beyond it. Any gametype that names only
  legacy ids must keep byte-identical output.
- Test: `tools/test_weapon_pack_storage.py`, `port/linux/tests/ae_weapon_slots_test.c`, `tools/test_ae_gametype_audit.py`,
  `tools/test_ae_gametype_change.py` (+ `tools/ae_test/gametype_changes.json`, `tools/ae_test/gametype_file.py`),
  `tools/test_ae_test.py` (seed rules), `tools/test_ae_game_options.py`.
- Risk: HIGH. Seeds are built-in gametypes with recorded hashes; any change to a seed's bytes is a seed revision and a migration
  (`seeds last before a release`). Do not change the bytes of existing rows; add a translation layer instead.

### S8 Loadout and start weapon
- Where: `source/game/game_engine.c:11900-11945` (give each loadout slot its pack weapon 0 where the pack was added, else the
  stock value; `ae_weapon_pack_loadout_given` `:2879` logs the first spawn per map), `:6555-6573` engine accessors;
  `ae_setup_options.c:1159` `ae_setup_pack_source`, `:1156` `ae_setup_pack_on`, `:1226` (names/facts), `:1257-1263` (rank 6/7
  exclusivity); `ae_glue_setup_options.c:440-528` (`_encoding_pack_slot` read/write, `loadout_pack_place`, weapon byte = stock
  value `AE_SETUP_WEAPON_PACK_FIRST + slot - 1`), `:979-981`; `ae_rifle_menu.c:27-115`; `ae_glue_gametype_picker.c:105-127`
  (start words: BR/EVO special-cased by slot).
- Today: a loadout slot names a pack through (rank among named slots) or (START byte = slot number). Only "weapon 0" of a pack
  is ever given.
- Under ids: loadout names a weapon id; the rank machinery disappears for new gametypes but must keep decoding old ones.
  Packs with several weapons (`AE_PACK_MAXIMUM_WEAPONS` 8) are today reachable only as weapon 0 for starts; a catalogue makes
  each weapon addressable.
- Test: `port/linux/tests/ae_setup_options_test.c`, `port/linux/tests/ae_weapon_slots_test.c`, `tools/test_weapon_pack_add.py`,
  `tools/test_ae_game_options.py`, `tools/test_weapon_pack_dmr.py` / `_carbine.py` and siblings (start with the pack's weapon).
- Risk: medium-high (many small encodings; the stock-weapon byte doubles as a pack selector).

### S9 Map spawns and layout aliases
- Where: `ae_pack_spawns.c:44-78` (a map item that would spawn a stock pistol / assault rifle spawns the BR / SMG when that slot
  is added; uses `ae_weapon_pack_played_mask`), `ae_weapon_slots.c:244` `ae_weapon_slots_spawn`, `:252-306` alias, fallback,
  name, display, `of_pack`; layouts `ae_layout.c:421,655,704`, `ae_layout_rules.c:115-119` (layout word -> slot -> `pack_slot`
  on the item), `ae_layout_rules.h`.
- Today: layout files name packs by alias words (`battle_rifle`, `dmr`, `evo`...); each alias maps to a slot, each slot to the
  stock weapon it falls back to when the pack is absent on the machine or map.
- Under ids: alias -> id; fallback -> a catalogue field on the id. Old layout files must keep parsing (the word list is a public
  format authors use).
- Test: `port/linux/tests/ae_layout_rules_test.c`, `port/linux/tests/ae_weapon_slots_test.c`, `tools/test_weapon_pack_add.py`.
- Risk: medium.

### S10 Prediction, fallback lines, lobby line
- Where: `source/game/game_engine.c:9405-9415` (predict each named slot's weapon definition), `:9723-9770` (rules line with the
  mask name; "unavailable" console + log line per slot), `ae_weapon_pack_unavailable_line` `:2476`, `ae_weapon_pack_lobby_line`
  `:2491` (host's lobby says before the game that a named slot is unavailable on the map, using the refused-map memory
  `refused_maps_of[][16]` `:430`), `port/linux/game/menu_functions.c:7314`.
- Today: messages are per slot ("BATTLE RIFLE unavailable on <map>: <reason>"); the refused-map memory is per slot, 16 maps.
- Under ids: per weapon id; the lobby line needs the per-(weapon, map) refusal known BEFORE the map loads, which today works
  only for maps tried earlier. With deterministic per-weapon refusal (S3) the lobby can compute it without loading.
- Test: `tools/test_weapon_pack_checks.py`, `tools/test_weapon_pack_add.py`.
- Risk: low-medium.

### S11 HUD strings, message icons, high-resolution HUD
- Where: `source/interface/hud.c:761` -> `ae_weapon_pack_item_string` `:2926`; `source/interface/hud_messaging.c:632` ->
  `ae_weapon_pack_message_icon` `:2988`; `placed_looks_set` `:2143`; MCC HUD scale `mcc_hud_pack_weapon_scale` (`:2305`).
  Item message index = `ITEM_STRING_INDEX_FIRST` 0x7F00 + `ITEM_STRINGS_PER_SLOT` x (slot - 1) + 2 x weapon, written into the
  weapon tag's `hud_message_index` as a `short` (`:2175`).
- Today: slot x weapon decides the string index. The index is a 16-bit signed field.
- Risk note: with 8 weapons a pack the base reaches 0x7F00 + 16 x 13 + 14 = 32,742; slot 17 would exceed 32,767 and wrap
  negative. The widening branch moves slots to 32, so this breaks there, silently. Under ids the index must come from a dense
  weapon ordinal (0..N), not slot x 16.
- Fallback text: for a weapon without an icon the kill feed and swap prompt already write the name (kill feed in S12); the prompt
  draws no icon when refused.
- Test: `tools/test_weapon_pack_look.py`.
- Risk: medium (silent overflow on widening).

### S12 Kill feed
- Where: `ae_killfeed.c:143` (sword by hand definition), `:154-159`, `:166-171` (needler special case by `AE_WEAPON_SLOT_NEEDLER`),
  `:197-202`, `:477` (message icon); `ae_killfeed_rules.c:108-128` `ae_kf_weapon_of_slot` (a literal switch for slots 1..14, any
  other slot = UNKNOWN), `ae_killfeed_rules.h:126`.
- Today: slot -> kill-feed weapon enum -> picture/word. Slots 15+ already fall to UNKNOWN (weapons added on other branches
  after slot 14 have no feed identity).
- Under ids: a catalogue column "feed identity" (word, picture key). Replaces the literal switch.
- Test: `port/linux/tests/ae_killfeed_rules_test.c`.
- Risk: medium (one table, but the special cases for sword and needler are by slot constant).

### S13 Bots
- Where: `ae_bots.c:99-125` `ae_bots_weapon_kind` (value, tap ticks, range per slot via a switch), `ae_bots_combat.c:92`
  (carbine = precision rifle), `:104` (scoped rifles are BR and carbine slots).
- Today: bot weapon valuation is a switch on the slot with per-weapon constants in code.
- Under ids: catalogue fields (bot value, range, tap ticks, precision, scoped) or a switch on the id. Behaviour must be unchanged.
- Test: `port/linux/tests/ae_bots_combat_rules_test.c`; no bot-weapon-value pin by slot (gap).
- Risk: medium (silent balance change).

### S14 Medals and controller (trigger) roles
- Where: `ae_medals.c:515-531` (slot -> medal weapon role), `ae_ds5_game.c:85-105` `definition_pack` (slot -> trigger profile
  family), `:179` (log); `port/linux/src/ae_ds5_rules.h:64`.
- Today: switches on the slot constants for 1..7 only; others fall to a name search or "none".
- Under ids: catalogue columns "medal role", "trigger family". Same pattern as S12/S13.
- Test: `port/linux/tests/ae_medal_rules_test.c` (roles), `tools/test_ae_units.py`; no DS5 pin by slot (gap).
- Risk: low-medium.

### S15 Behaviour dispatch inside the pack (rifle profiles, sword lunge, needler)
- Where: `ae_burst_rifle.c:528-541` (loops every added slot, maps each weapon's `behaviour` field to a rifle kind - already
  independent of slot), `:372-417,552` (own-tag tests), `ae_sword_lunge.c:491-497` (looks up the sword by the constant
  `AE_WEAPON_SLOT_ENERGY_SWORD`), `ae_killfeed.c:168` (needler by constant), `ae_pack_format.h` behaviour field.
- Today: the rifle code is already keyed by the pack entry's behaviour, not slot: this is the model to follow. Sword lunge and the
  needler feed case are the slot-keyed exceptions.
- Under ids: sword and needler look up by id; the behaviour field stays the dispatch key.
- Test: `tools/test_weapon_pack_rifle.py`, `tools/test_weapon_pack_energy_sword.py`, `tools/test_weapon_pack_needler.py`,
  `port/linux/tests/ae_pack_rifle_rules_test.c`.
- Risk: low.

### S16 DEV MODE and the dev wheel
- Where: `ae_dev_packs_rules.[ch]` (extras = installed minus gametype, only where nobody can join), `ae_weapon_pack.c:2355`
  `dev_packs_add`, release order `ae_dev_packs_release_order` (reverse attach order); `ae_dev_wheel_rules.c:36,132-160` (pack tags
  recognised by the `ae\packs\<id>` path and shown with the slot's display name).
- Today: exists because attaching all packs was too costly; extras are attached after the game's own so indices of the real
  packs do not move.
- Under ids: the whole cache is attached, so the extras mechanism and its reverse-release order collapse; the wheel lists the
  catalogue. Keep the "joinable games get no extras" rule only if some weapons stay development-only.
- Test: `port/linux/tests/ae_dev_packs_rules_test.c`, `port/linux/tests/ae_dev_wheel_rules_test.c`.
- Risk: low (deletion), but check `give` by pack folder name paths.

### S17 Settings and create-game rows
- Where: `ae_setup_options.h:106` `AE_SETUP_PACKS = 14`, `:109` `AE_SETUP_WEAPON_PACK_FIRST = 12`; `ae_setup_options.c:909,1156-1263`
  (names, facts, per-slot rows); `ae_glue_setup_options.c:157-172` (one `AE_SO_PACK_*` row per slot, `_encoding_pack_slot` with a
  mask bit); `ae_screen_create_game.c:3949,3988-3993`; `ae_screen_game_options.c:1129,1693`; `tools/port_settings.py:205`
  (WEAPON PACKS on/off).
- Today: one on/off row per slot; the option table is sized by the slot count.
- Under ids: rows come from the catalogue. The player-facing choice may stay a per-weapon allow list inside the gametype or become
  a single "all cache weapons" switch with a start-weapon picker; that is a design choice, not an engine one.
- Test: `port/linux/tests/ae_setup_options_test.c`, `tools/test_ae_game_options.py`, `tools/test_ae_hud_settings.py`.
- Risk: medium (UI row counts, help text length rules).

### S18 Audit and log lines
- Where: `ae_gametype_audit.c:126,145` (weapon_pack byte and later bits recorded per seed), `tools/ae_gametype_audit.py:39,55`
  (which fields a BR-start row sets); rules line `game_engine.c:9723-9744` with `game_variant_weapon_pack_mask_name`
  (`:6515`, e.g. `battle_rifle,smg` or a number); per-pack lines in `ae_weapon_pack.c` ("weapon pack: ...", "<word> pack: let go
  from <map>", "first spawn: the primary weapon is the pack's <key>", "refused for <map>: <reason>"); the audit's
  `weapon_pack=255` style value is the byte as stored.
- Under ids: lines keep their wording where tests grep them; the audit records the new field(s) next to the byte.
- Test: `tools/test_ae_gametype_audit.py`, `tools/test_weapon_pack_add.py`, `tools/test_weapon_pack_load.py`, `tools/test_weapon_pack_checks.py`.
- Risk: medium: many tests grep these exact log lines.

### S19 TEST RUN and debug plumbing
- Where: `network_test.c` (field `weapon_packs`, `:701-745`: mask bytes into the variant; `debug.network_test_pack_loadout`,
  `_pack_start`, `_pack_map_spawns`), config rows `port/linux/src/port_config.c:2090,2102,2108,2113` and `debug.weapon_pack_ground`,
  `debug.weapon_pack_table_limit`; hook list `ae_hooks.txt` (32+ lines for packs; a test enforces that every upstream edit is
  listed); harness stand-ins `tools/harness/tests/*.c` (stubs for pack calls), TEST RUN lists in `tools/ae_test/test_runs/`.
- Widening branch (w): `debug.network_test_weapon_packs` takes bits 16-23 (third byte).
- Today: tests choose packs by mask integer.
- Under ids: needs `debug.network_test_weapon_ids` (or the same integer read as a catalogue bitmap for ids 1..24), and the
  `table_limit` knob must keep working for the refusal tests.
- Test: `tools/test_ae_test.py`, `tools/test_ae_hooks.py` (hook list), `tools/test_linux_port.py`, `tools/test_ae_units.py`.
- Risk: low-medium (test harness stubs break first).

### S20 Tag-only packs: vehicle damage and bomb
- Where: `VEHICLE_PACK_SLOT` / `BOMB_PACK_SLOT` = slots + 1 / + 2 (`ae_weapon_pack.c:200-202`), `vehicle_pack_asked` `:2315`,
  `bomb_pack_asked` `:2335`, queries `ae_weapon_pack_vehicle_pack_added` `:2740`, `_bomb_pack_added` `:2746`; consumers
  `ae_vehicle_pack.c:91`, `ae_bomb_pack.c:214,323-334`.
- Today: attached conditionally on gametype state, after the weapon slots; their slot numbers sit past the weapon slots, so
  widening the weapon slots renumbers them.
- Under ids: project decision: into the cache. They become cache sections with an attach condition; the "added" queries keep the
  names.
- Test: `tools/test_weapon_pack_vehicle_damage.py`, `tools/test_weapon_pack_bomb.py`, `port/linux/tests/ae_vehicle_pack_rules_test.c`.
- Risk: medium (conditional attach inside an attach-all world).

### S21 Per-slot global state
- Where: `placements[PLACED_SLOTS]`, `refusals[]`, `refused_maps_of[PLACED_SLOTS][16]`, `placed_select(slot)` `:438` (a "current slot"
  pointer set that every private function uses via the `placed` / `refusal` names). `PLACED_SLOTS = AE_WEAPON_PACK_SLOTS + 2`.
- Today: attach, validate, read and release all run through a selected-slot cursor; releasing re-selects in reverse order because
  each pack's tag table is the previous pack's grown table (`cache_files_set_tag_instances`, `:2558`).
- Under ids: one placement, so the cursor and reverse-order release disappear. The arrays' size no longer scales with weapons.
- Test: covered by `tools/test_weapon_pack_add.py` (let-go lines).
- Risk: medium (refactor of the core file's state, most of its 3,064 lines read `placed`).

### S22 Cross-machine agreement
- Where: `ae_weapon_pack_fingerprint` `:2777` (pack hash + map name + tag count + placed indices), logged, not exchanged.
- Today: two machines agree only if the same mask and the same pack bytes give the same placement; two revisions of one pack at
  the same level would drift without a message.
- Under ids: the join identity (S6) carries the cache hash, which makes agreement checkable. The fingerprint becomes a
  per-map diagnostic that can be compared in a test.
- Test: none.
- Risk: medium; this is where the cache pays for itself.

### S23 Build, release and docs tooling
- Where: `tools/weapon_pack/` (recipes per pack, `make_pack.py`, `pack_done.py`, `SHIPPED.txt`), `tools/ae_pack_build.c`,
  `tools/ae_pack_check.c`, `ae_pack_format.c/.h` (version 1, limits above), release kit `build-assets` copies packs per platform,
  `docs/weapon-packs.md`.
- Today: one `.aepack` per pack; `test_release_packs.py` ties `SHIPPED.txt` to the slot table.
- Under ids: recipes may stay as fragment builders; a linker produces the cache. Format v2 raises the caps (S3).
- Test: `tools/test_release_packs.py`, `tools/test_weapon_pack_build.py`, `tools/test_weapon_pack_tools.py`,
  `tools/test_weapon_pack_done.py`, `tools/test_weapon_pack_format.py`.
- Risk: medium (build time and byte-for-byte reproducibility checks).

Seam count: 23.

## 2. The riskiest three

1. Slot-keyed consumers (S5 with S12-S15, S9). The weapon's identity lives in the slot number in about a dozen switches and
   tables; each is a hand-written mapping and several are partial (kill feed knows 14, medals 7, trigger roles 7). A missed
   re-point is silent: a wrong picture, a bot that values a weapon at zero, a missing medal. Mitigation: one adapter
   (`definition -> id`), a catalogue with columns for every consumer, and a test that every catalogue id has a row in each
   consumer table.
2. Gametype storage and join identity (S6, S7). The pack mask rides in packed bytes of a fixed-size struct with no spare byte
   left once the third byte lands, seeds carry recorded hashes, and the join check is a 4-bit level that cannot tell two cache
   revisions apart. Changing it risks a lockout, a drift, or a seed revision. Mitigation: keep legacy bytes byte-identical for
   legacy ids, carry new ids in the extension block, refuse by cache identity with an explicit old-build refusal.
3. Attach-everything on every map (S2, S3, S11, S21). Today a game attaches 0-4 packs and refusal is per pack. Attaching the
   whole set raises tag count, memory (8 MiB cap), validation time per map load, and the chance that one missing external
   refuses a weapon on a family of maps. It also needs per-weapon deterministic refusal so every machine drops the same
   weapons, and fixes silent limits such as the 16-bit `hud_message_index` overflow past slot 16 (S11).

## 3. Tests that pin pack behavior today (the re-point list)

C unit tests (`port/linux/tests/`, run by `tools/test_ae_units.py`):
`ae_weapon_slots_test.c`, `ae_silent_rules_test.c`, `ae_setup_options_test.c`, `ae_layout_rules_test.c`,
`ae_pack_rifle_rules_test.c`, `ae_pack_memory_rules_test.c`, `ae_dev_packs_rules_test.c`, `ae_dev_wheel_rules_test.c`,
`ae_killfeed_rules_test.c`, `ae_vehicle_pack_rules_test.c`, `ae_hit_feedback_rules_test.c`, `ae_juggernaut_rules_test.c`,
`ae_bots_combat_rules_test.c`, `ae_medal_rules_test.c`.

Python / game tests (`tools/`):
`test_weapon_pack_add.py`, `test_weapon_pack_load.py`, `test_weapon_pack_storage.py`, `test_weapon_pack_checks.py`,
`test_weapon_pack_look.py`, `test_weapon_pack_rifle.py`, `test_weapon_pack_format.py`, `test_weapon_pack_build.py`,
`test_weapon_pack_tools.py`, `test_weapon_pack_done.py`, `test_weapon_pack_smg.py`, `test_weapon_pack_carbine.py`,
`test_weapon_pack_ar_silenced.py`, `test_weapon_pack_ar_modern.py`, `test_weapon_pack_dmr.py`,
`test_weapon_pack_magnum_scoped.py`, `test_weapon_pack_sniper_bolt.py`, `test_weapon_pack_brute_shot.py`,
`test_weapon_pack_sidearm.py`, `test_weapon_pack_energy_sword.py`, `test_weapon_pack_needler.py`,
`test_weapon_pack_vehicle_damage.py`, `test_weapon_pack_bomb.py`, `test_release_packs.py`, `test_map_packs.py`,
`test_ae_silent.py`, `test_ae_game_options.py`, `test_ae_gametype_audit.py`, `test_ae_gametype_change.py`,
`test_ae_hud_settings.py`, `test_ae_test.py`, `test_ae_units.py`, `test_linux_port.py`;
data and harness: `tools/ae_test/gametype_changes.json`, `tools/ae_test/gametype_file.py`, `tools/ae_test/harness.py`,
`tools/harness/tests/coop_scaling_menu.c`, `tools/harness/tests/sound_lifecycle.c` (pack-call stand-ins), `port/linux/game/ae_hooks.txt`
(hook list guard).

Gaps (no direct pin today): bot weapon values by slot, DS5 trigger family by slot, cross-machine placement agreement, the lobby
"unavailable" line for a map never loaded, and the 16-bit message index range.

## 4. Minimal seam list the cache loader must implement for parity (dependency order)

1. Catalogue and format v2 (S23, S3 caps): stable ids, key, display name, stock fallback, behaviour, and one column per consumer
   (feed identity, bot values, medal role, trigger family). Reader plus the stamp (format version, schema hash, SHA-256).
2. Single-cache load and failure policy (S1): one open handle, start-time state "cache ok / missing / stale".
3. Attach-all with per-weapon deterministic refusal (S2, S3, S21): one placement, same four map-family hooks, base layer so
   externals rarely refuse, per-weapon memory accounting against raised limits.
4. Id lookups (S5): `definition_of(id)`, `id_of(definition)`, own-tag test; keep `definition_slot` as a shim for ids 1..14 during
   migration.
5. Bulk-read and texture hooks (S4): unchanged behaviour on the single placement.
6. Gametype translation (S7): legacy mask/rank/start bytes decode to ids with byte-identical output; new ids in the extension
   block; seeds untouched until the seeds step.
7. Join identity and refusal (S6, S22): cache identity in the join token or mark, old-build refusal, rejection code and notice
   wording; fingerprint made comparable.
8. Loadout, start and prediction (S8, S10): start weapon by id, prediction over the catalogue, per-weapon unavailable lines,
   lobby line from deterministic refusal.
9. Consumers re-pointed to catalogue columns (S12, S13, S14, S15, S9): kill feed, bots, medals, trigger roles, sword and needler
   special cases, layout aliases and map spawns.
10. HUD strings and icons (S11): dense weapon ordinal for the message index; name-in-text fallback when no icon.
11. Tag-only sections (S20): vehicle damage and bomb as cache sections with their attach conditions.
12. DEV MODE and wheel (S16): list the catalogue; delete the extras mechanism.
13. Settings rows, audit and TEST RUN plumbing (S17, S18, S19): rows from the catalogue; new debug key for ids; keep the log
    wording the tests grep.

Steps 1-5 give a cache that attaches and reads; 6-8 give gametypes and joins; 9-13 give feature parity. Steps 6 and 7 must ship in
the same release (a gametype that names cache weapons must never reach a machine that cannot read them).
