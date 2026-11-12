# Native weapon cache: join compatibility and migration

Status: design input for the cache's join gate, with an executable matrix (`tools/test_cache_compat.py`).
Owner decisions this follows: old AE builds are refused when a game uses cache weapons; the 24-slot widen is a test
bridge only; every cache weapon works on every map family.

Sources read: `ae_silent_rules.[ch]` and `ae_weapon_slots.[ch]` on `ae/release-076` (base, the 0.7.5 rules),
`ae/pack-slots-076` fd4881640 (the widen: "bridge"), `ae/grenades-engine` 6b094ec4a (the grenade feature mark: "wave"),
`ae_silent.c`, `ae_joiner_notice.c`, `ae_disc_join*`. "0.7.5 dev" builds carry the same join files as the 0.7.5 release,
so they are one row ("0.7.5") below.

## 1. The join name field, as it stands

The join request's machine name is `MAXIMUM_MACHINE_NAME_LENGTH` = 32 16-bit characters
(`source/networking/network_server_manager.c:523`). A stock machine sends its name then zeros. AE writes AFTER the name's
end (a zero must exist before every mark: a name that runs into a mark is no mark). Index 0 is the first character.

| index | value | writer | meaning | read by |
|---|---|---|---|---|
| 0 .. end-1 | name text | the system | machine name (cut, see below) | everyone |
| 31 | `0xAE5x` (`AE_SILENT_JOIN_MARK` 0xAE50 \| nibble) | `ae_silent_join_mark`, then the level | AE machine; nibble = capability level 1..15 (1 SILENT options, 2 battle rifle, 3..14 the slot packs) | every AE build since 0.2.3 |
| 30 | `0xADxx` (`AE_SILENT_JOIN_LEVEL_MARK` \| level) | bridge, `ae_silent_join_level` (only for level 16..255) | level past 15; the nibble then holds 15; 0 for a level of 15 or less | bridge and later; 0.7.5 reads only index 31 and sees 15 |
| 29 | `0xACxx` (`AE_SILENT_JOIN_FEATURE_MARK` \| bits) | wave, `ae_silent_join_features` | bit 0 grenade layer, bits 1-3 its version 1..7 | wave and later |
| 28 .. 25 | free today | | | |
| 24 .. end | zeros | | name text ends at 28 at the latest with the feature mark; 29 with the level only; 30 with the nibble only | |

Name cut: mark alone: name ends at 29 at the latest (index 30 zero); with the level character, 28; with the feature mark,
28 (the writer zeroes `[end .. 28]`).

Order the request writes them (`ae_silent_join_request_mark`): mark, level, features. Facts the harness asserts
(`test_name_field_byte_map`).

## 2. The cache identity: what space remains, and the proposed encoding

Room: indices 28..25 (and 24 and below by shortening the name). Top bytes in use: 0xAB free, 0xAC feature, 0xAD level,
0xAE mark.

Proposal: a CACHE BLOCK of four characters, written LAST, name ending at 24 at the latest (a 24-character machine name):

| index | value | meaning |
|---|---|---|
| 28 | `0xAB00 \| format` (`AE_SILENT_JOIN_CACHE_MARK`) | cache format version 1..255 (the aepack v2 container revision) |
| 27 | generation (16 bits, raw) | the cache's release ordinal, 1.. (appends one per release that changes the cache; lets the host say older/newer) |
| 26 | digest high 16 | 32-bit digest of the base layer + add-on set (manifest hashes in catalogue-id order) |
| 25 | digest low 16 | |

Rules: read only with the AE mark at 31 and a zero before index 25 (the same name-end rule as the other marks);
absent block = a machine without the cache (0.7.5, bridge, wave all fall here; they never write 0xABxx there).
API (pure, in `ae_silent_rules.[ch]`): `ae_silent_join_cache(name, length, format, generation, digest)` and
`ae_silent_join_cache_told(name, length, &format, &generation, &digest)` (returns nonzero when told). 32 bits is for
accidents (two honest builds), not security; a cheater can send any block (the digest is a gate for honest mismatch).

The cache build ALSO keeps telling the legacy level (its cache holds the 14 slot weapons; with the widen's bridge it can
tell up to 24): an old or bridge host then keeps accepting it into games that name only legacy slots. Nothing tells a
level past 32.

Writer hazards found (both asserted in the harness):
1. `ae_silent_join_features` zeroes `[name end .. length-4]` = through index 28. A cache block written before it is
   wiped (`test_feature_writer_wipes_a_block_written_before_it`). Write the block after the features, or change the
   feature writer to stop at index 24 when it lands.
2. `ae_silent_join_level` (level > 15) zeroes index 29, the feature slot: the level is written before the features
   (as `ae_silent_join_request_mark` does; `test_level_writer_zeroes_the_feature_slot`).
No collision between the level character (30), the feature mark (29) and the block (28..25): distinct indices and top
bytes; the existing readers decode a name with the block exactly as without it, for every level 2..32 and grenade
version 0..7 (`test_cache_block_coexists_with_every_shipped_reader`).

Join TOKEN fold (the research note's suggestion, `ae_disc_join_fold`): NOT recommended as the gate. The client folds
before it knows the game; a cache client would then fail the token of every 0.7.5, bridge and wave host (legacy games
included) and every refused machine sees "bad join token" ("modded disc" wording). Keep the token stock; gate by the
block and AE's own rejection codes, which carry readable text. (This reads the fold as hosted at
`network_server_manager.c:3961`; verify when the loader lands.)

## 3. The decision (host with the cache)

A game "uses the cache" when its gametype names a weapon with no legacy slot, or any weapon while the owner picks the
strict mode (open question 1). Per join, a cache host:

1. game does not use the cache: today's rules only (level vs required, grenades, territories, SILENT).
2. uses the cache, no block told: refuse with code 66 (`AE_PACKS_REJECTION_CODE`). Every AE build since 0.7.5 knows
   66; 0.7.5 shows the "newest Arena Evolved" text, the bridge and wave "0.7.6 or later". Stock machines are refused
   earlier (level 0).
3. block told, format differs: code 69 `format`.
4. block told, generation differs: code 69, `client older` / `host older`.
5. same generation, digest differs: code 69 `different build`.
6. otherwise accepted.

New code `AE_CACHE_REJECTION_CODE` 69 (68 is the grenades'). A build that does not know 69 shows only "could not
join"; only cache clients tell a block, so only cache clients ever receive 69.

## 4. Refusal matrix (host x client)

Game kinds: L = names legacy slots only (levels 2..14); W = names slot packs 16..24 (bridge/wave/cache hosts only; a
0.7.5 host cannot name them); C = uses the cache. Client levels: C075 tells at most 14; Cbridge up to 24; Ccache tells
its legacy level and a block. Executed today for the first three columns by `test_current_matrix` (all host x client
builds, client levels 0..32, required 2/3/14/16/24) and `test_refusal_texts_by_cell` (texts read from each build's source).

Texts (exact, from source):
- T66-075 (0.7.5 client shown code 66): "This game uses Arena Evolved's weapon packs, which this copy has not. Use the newest Arena Evolved with its packs folder (on a Mac, the extras in Application Support), and SETTINGS > SYSTEM > FILES > WEAPON PACKS on."
- T66-076 (bridge, wave, cache clients shown code 66): the same with "Use the newest Arena Evolved (0.7.6 or later for the newest packs) with its packs folder ...".
- T65 (code 65, battle rifle alone): "This game uses Arena Evolved's battle rifle pack, which this copy has not. Use the newest Arena Evolved with its packs folder (on a Mac, the extras in Application Support), and SETTINGS > SYSTEM > FILES > WEAPON PACKS on."
- T68 (wave and cache clients, code 68): "This game uses Arena Evolved's grenade types, which this copy has not. Update Arena Evolved to play this game."
- Host chat, required level <= 14 (all AE hosts): "A player tried to join without the weapon packs this gametype uses. They need Arena Evolved with its packs, or pick a gametype without packs to let them in."
- Host chat, required > 14 (bridge, wave, cache hosts): "A player tried to join without the weapon packs this gametype uses. Its newest packs need Arena Evolved 0.7.6 or later with its packs, or pick a gametype without them to let more players in."
- T69 (cache clients, PROPOSED, in `ae_silent_join_rejected` beside the others, plus matching host chat lines):
  - format: "This game's weapon cache is a different format from this copy's. Update Arena Evolved to the host's version."
  - client older: "This copy's weapon cache is older than the host's. Update Arena Evolved (SETTINGS > SYSTEM > UPDATES), then join again."
  - host older: "The host's weapon cache is older than this copy's. Ask the host to update Arena Evolved."
  - different build: "This copy's weapon cache is not the host's, though the version is the same. Reinstall the packs folder, or ask the host which build it plays."
- Cache game, client without a block, host chat: "A player tried to join without the weapon cache this gametype uses. Arena Evolved 0.7.6 or later (the newest) is needed, or pick a gametype that does not use it." (PROPOSED)

| host \ client | stock | C075 | Cbridge | Ccache |
|---|---|---|---|---|
| 0.7.5, game L | refused 65/66 (stock: could not join) | joins | joins (told 15 or less in nibble) | joins (legacy level) |
| 0.7.5, game C / W | n/a (cannot name) | | | |
| 0.7.6 bridge, game L | refused 65/66 | joins | joins | joins |
| 0.7.6 bridge, game W | refused 66 | refused 66, T66-075; host chat >14 | joins if level >= required, else 66 T66-076 | joins (levels told up to 24) |
| 0.7.6 bridge, game C | n/a | | | |
| 0.7.6 wave (as bridge, plus grenades) | as bridge; grenade games add 68/T68 for C075 and bridge | | | |
| 0.7.6 cache, game L | refused 65/66 | joins | joins | joins |
| 0.7.6 cache, game W | n/a unless widen kept (open question 2) | | | |
| 0.7.6 cache, game C | refused 66 | refused 66, T66-075 | refused 66, T66-076 | digest equal: joins; else 69 with the cell's T69 |

Cross-check of client direction: a cache client joining a 0.7.5 / bridge / wave host plays that host's legacy game and
is accepted (it tells a legacy level and a block those hosts ignore). The harness proves the readers ignore the block
(coexistence test); the CONTENT of the legacy weapons (cache build vs the host's pack revision) is not compared by
anything: the pack fingerprint is only logged today (research note, section 1). See open question 1.

## 5. Migration checklist: 24-slot bridge -> cache identity

What flips:
- Cache games gate on (format, generation, digest) instead of the slot level. The level stays told and compared for
  legacy games.
- Gametypes name weapons by catalogue id; the old slot mask is translated on load (24 bits stay readable: variant
  byte, `weapon_pack_high`, `weapon_pack_top` at 0x2B).
- `ae_silent_join_request_mark` gains the cache write LAST; new `ae_joiner_notice_cache_refusal`; `ae_silent_join_rejected`
  gains code 69; the host's drop path (`ae_silent_host_machine`, lobby gametype changes) calls the same decision.
- The wide-level path (16..32) is used by NO weapon after the cache (the widen is a test bridge; slots 25..32 never
  stored). Keep the writer/reader (old hosts read it) but stop adding levels.

What dies:
- Per-slot capability levels for new weapons (a weapon past slot 14 gets no level); `AE_WEAPON_SLOT_STORED_MAXIMUM`
  as a limit; the "0.7.6 or later for the newest packs" chat branch for cache games (replaced by the cache lines).
- `debug.network_test_weapon_packs` slots 17..24 form (re-pointed, below).

Tests that re-point:
1. `tools/test_cache_compat.py`: the two skipped TODO cases activate once the driver defines
   `AE_SILENT_JOIN_CACHE_MARK` (add the two pure functions above); add the cache build as a fourth machine.
2. `port/linux/tests/ae_silent_rules_test.c`: cache block write/read, name-end rule, ordering, coexistence with level
   and feature mark (mirror the pure cases here).
3. `tools/test_ae_silent.py::test_game_join_refusal`: bots get `--ae-cache` (a told block) and `--ae-cache-old`; add the
   69 cells next to the existing 64/65/66 ones.
4. `ae_weapon_slots_test.c`: capability stays for legacy; add catalogue-id round trip from masks.
5. `network_test.c` / `HALO_NETWORK_TEST_WEAPON_PACKS` -> a cache variant (`HALO_NETWORK_TEST_CACHE=<generation>:<digest>`).
6. `tools/net_compare.py` runs: cache build vs 0.7.5 release in both directions on legacy games (accepted, in step) and
   a cache game (0.7.5 refused).
7. `tools/ae_gametype_layout_check.c`: new field for ids; the 0x2B byte stays.

## 6. Open questions for the owner

1. Strict mode: should a cache host also demand a matching block for LEGACY-slot games (cache weapons might differ in
   revision from an old build's packs; nothing compares them today), refusing 0.7.5/bridge from every pack game, or keep
   old builds joining legacy games (rec: keep, and freeze the 14 legacy weapons' data across the cache move)?
2. Does the cache build keep reading/writing slot masks past 14 (W games) or only ids?
3. Generation numbering: a monotonic integer per release that changes the cache (rec) or the release's version?
