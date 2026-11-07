# Weapon cache inventory (donor packs)

Tool: tools/weapon_cache/merge_donors.py.
Manifest (every tag, digest, carrying packs): manifest.json in the inventory work folder (not in the repository).
Input set: the 25 packs of the pack-preflight folder (15 shipped 0.7.5 + bomb + 9 wave packs, pins in its pins.txt)
plus ae_gravity_hammer.aepack from the preview folder (ae/hammer-pack) = 26 packs. Run on the build box, 2026-11-04.

## Headline

| number | bytes |
|---|---|
| sum of the 26 pack files | 528,928,204 |
| sum of tag payloads (tag bytes + geometry + resources, stored) | 527,827,542 |
| UNIQUE payload, one copy per content digest = the size of one base cache | 191,156,038 (about 191 MB) |
| saved by dedup | 336,671,504 (64 percent) |
| content-identical groups | 291 (286 are the same path in several packs; 5 are the same bytes under different paths, e.g. a model's fp and tp, a shader pair) |
| real path collisions (same path after stripping the pack prefix, DIFFERENT content) | 9 names, 786,678 bytes in all variants (under 1 MB) |
| distinct tag paths (prefix stripped) | 1249 |
| distinct external tag names (named, not carried) | 245 |

Raw payload uncompressed in a stored container. Not counted: zlib on resources (the v1 writer stores only), and dedup of
equal pixel data under different tag headers. Per group, unique bytes: bitm 162 MB, mode 10 MB, snd! 6.3 MB, antr 1.9 MB,
everything else under 1 MB each. Bitmaps are 85 percent of the cache.

Sharing profile (unique bytes by how many packs carry a tag): 1 pack 115 MB (949 tags), 2 packs 10 MB, 7 packs 27 MB
(HUD meters/alphas: zteam\hud\h3\bitmaps\meters\weapon\meters 13.1 MB, alphas 11.0 MB, shared by 7 packs), 11-15 packs 8 MB
(particle and decal bitmaps every stock-derived pack copied). Largest single tags: the two HUD sheets above, a 8.4 MB
circular scope, 5.6 MB weapon body bitmaps (grenade launcher, focus beam).

## The packs

Pack ids are fixed to today's slot order; ids 15 and up are the wave in the order the branches were made. "Excl MB" = bytes
no other pack carries (what that weapon really adds to the cache). Behaviour * = value 4 or 5, which this tree's reader
(aepack.py, engine behaviours 0..3) refuses; merge_donors reads such a pack with the field zeroed in a copy.

| id | weapon | slot now | branch | rev | own tags | ext tags | file MB | excl MB | behaviour | replaces |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | battle_rifle | 1 | shipped 0.7.5 | 30 | 183 | 55 | 16.0 | 6.7 | 1 | 2 |
| 2 | smg | 2 | shipped 0.7.5 | 30 | 155 | 53 | 11.8 | 0.5 | 0 | 2 |
| 3 | smg_silenced | 3 | shipped 0.7.5 | 30 | 152 | 52 | 11.8 | 0.5 | 0 | 0 |
| 4 | carbine | 4 | shipped 0.7.5 | 30 | 205 | 48 | 15.9 | 6.6 | 0 | 0 |
| 5 | ar_silenced | 5 | shipped 0.7.5 | 30 | 15 | 32 | 4.2 | 3.7 | 0 | 0 |
| 6 | dmr | 6 | shipped 0.7.5 | 30 | 179 | 53 | 24.0 | 0.3 | 3 | 0 |
| 7 | evo_rifle | 7 | shipped 0.7.5 | 30 | 179 | 53 | 24.2 | 0.5 | 2 | 0 |
| 8 | needler | 8 | shipped 0.7.5 | 30 | 191 | 56 | 13.3 | 5.1 | 0 | 9 |
| 9 | sniper_bolt | 9 | shipped 0.7.5 | 31 | 24 | 23 | 3.7 | 3.7 | 0 | 0 |
| 10 | magnum_scoped | 10 | shipped 0.7.5 | 31 | 33 | 48 | 40.4 | 12.2 | 0 | 0 |
| 11 | brute_shot | 11 | shipped 0.7.5 | 31 | 117 | 29 | 21.3 | 2.3 | 0 | 0 |
| 12 | ar_modern | 12 | shipped 0.7.5 | 30 | 183 | 52 | 12.8 | 4.1 | 0 | 0 |
| 13 | sidearm | 13 | shipped 0.7.5 | 2 | 34 | 48 | 31.5 | 3.2 | 0 | 0 |
| 14 | energy_sword | 14 | shipped 0.7.5 | 2 | 46 | 6 | 1.9 | 1.2 | 0 | 0 |
| 15 | smg_tactical | - | ae/packs-076a | 1 | 155 | 54 | 13.2 | 1.9 | 0 | 0 |
| 16 | pdw | - | ae/pack-pdw | 2 | 28 | 48 | 30.7 | 2.4 | 0* | 0 |
| 17 | revolver | - | ae/pack-revolver | 1 | 27 | 46 | 32.7 | 3.5 | 0 | 0 |
| 18 | focus_beam | - | ae/pack-focus-beam | 1 | 86 | 13 | 30.9 | 15.2 | 0 | 0 |
| 19 | ravager | - | ae/pack-ravager-burst (ae/pack-spiker-ravager older) | 1 | 188 | 48 | 25.8 | 3.4 | 0 | 0 |
| 20 | spiker | - | ae/pack-spiker-ravager | 1 | 172 | 43 | 24.0 | 2.3 | 0 | 0 |
| 21 | auto_shotgun | - | ae/pack-auto-shotgun | 1 | 33 | 44 | 33.6 | 4.2 | 0 | 0 |
| 22 | grenade_launcher | - | ae/pack-grenade-launcher | 1 | 115 | 5 | 42.8 | 11.2 | 0 | 0 |
| 23 | concussion_rifle | - | ae/pack-concussion-rifle | 1 | 98 | 6 | 36.3 | 7.1 | 0 | 0 |
| 24 | gravity_hammer | - | ae/hammer-pack | 1 | 26 | 9 | 10.0 | 10.0 | 0 | 0 |

Tags-only packs (no weapon entries, flag NO_WEAPONS; found by name, not by slot):

| pack | own tags | ext tags | file MB | excl MB |
|---|---|---|---|---|
| vehicle_damage | 114 | 19 | 15.7 | 12.5 |
| bomb | 33 | 0 | 0.6 | 0.6 |

Grenade pack (ae/grenades-content, tip 395cab3be): a recipe only (tools/weapon_pack/grenades/recipe.json, id ae_grenades rev 1,
tags-only, today the EMP: proj/eqip/itmc/grhi/effe roots, CMT TSC:E 1.4 source, prefix ae\grenades\). NO built pack existed
on box or laptop, so it is NOT in the numbers. Estimate from the allowlist (decal, lens flare, light, 2 particle systems,
~6 bitmaps shared with the other packs): small, well under 2 MB exclusive. It is still committing; rerun when a build exists.
Bomb is the same shape (0.6 MB). Gravity hammer is outside the preflight set; its slot in ae/hammer-pack is 6, which
collides with the DMR on release-076: another reason ids must come from a registry, not slot numbers.

## Cross-pack analysis

Shared tags (single storage): 291 content groups. Candidates, by size: HUD meter and alpha sheets (24 MB x7), ui\hudv4.1 weapon
base/alpha/reticules sheets (5.2 / 1.8 / 2.1 MB, x6-7), zteam reticles (3.2 MB x4), decal and particle bitmaps (blood splats,
smoke, sparks, plasma arcs, muzzle flash: every stock-derived pack, 11-15 copies each, ~8 MB unique), contrails, ammo-pack
art, damage-effect and projectile helpers of the SMG family. Zero-byte difference after the donor-offset normalisation
below, so a content-addressed store collapses them with no per-pack knowledge.

Normalisation that matters: a tag with resources carries its donor map's FILE OFFSET (bitmap pixel offsets in the tag
bytes, and each resource record's map_offset). These differ per pack although the pixels are identical; before zeroing them
the same dedup found only 1.8 percent (9 MB). A cache packer must hash content with those offsets masked and assign its
own. (merge_donors masks aligned 32-bit words equal to the tag's resource map offsets.)

Namespacing: every pack already writes its tags under ae\packs\<dir>\ (or ae\grenades\), so path collisions inside the
packs are impossible as shipped. The collision analysis therefore strips that prefix and asks: if two packs were merged into
one flat namespace, which paths would clash with different content? Nine, all small:

- mode weapons\_shared\dmr\dmr: DMR 303 KB vs EVO RIFLE 453 KB (different mesh): must stay namespaced per weapon
- wphi ui\hudv4.1\weapons\evolved\human\dmr: DMR vs EVO HUD
- proj and jpt! of the SMG family (assault_rifle_bullet, smg_bullet_trigger): smg vs smg_silenced vs smg_tactical differ
- wphi hud\smg_ammo: smg vs silenced/tactical
- schi cmt_unsc_display_compass and numbers_100ct: ar_modern vs battle_rifle vs dmr/evo (shader variants)
- wphi zteam\hud\temp\weapon_interfaces\test\h3_brute_shot: concussion rifle vs grenade launcher (a shared TEMP path!)
Rule for the packer: namespace per weapon on write (keep ae\packs\<weapon>\ in the logical name), dedup by content digest,
so a collision can never reach a map. The temp path above is a donor leftover and should be renamed by its recipes.

Same-bytes-different-path (5 groups, ~0.4 MB): shotgun_fp = shotgun_tp, smg_tactical_fp = _tp, two energy sword blade shaders, two
rifle body shaders, ghost left/right wing effects. Harmless; a content store dedups them for free.

External dependencies (tags the pack names and the map must supply): 245 distinct names, per pack 5 to 56 (concussion rifle 6,
grenade launcher 5; magnum, battle rifle, smg family 48-55). By group: effe 91, snd! 38, bitm 36, part 27, pphy 10, jpt! 8,
deca 7, ligh 4, schi 4, soso 3. Most-shared externals: sound\sfx\impulse\material_effects\weapon (22 of 26 packs), master rounds
HUD (16), assault rifle melee and melee_response damage (16), bubbles, point physics smoke/stones, metal chips.
These are STOCK paths that exist in every Xbox disc map (the donor stock map) and by the same paths in CE-family maps, but
that is the unverified part of "every weapon on every map family": the per-map-family resolution check (Xbox disc map,
@<pack> map, @ce, @md, @pc, @mcc) needs the map tag lists and is the compat harness helper's job. The full external list per pack is in
manifest.json "externals" (name -> packs). The cache's stock-derived base layer (owner decision) exists to remove exactly this
list: carry copies of the ones the weapon needs, drop only a weapon whose remaining dependency is missing.

Cross-ref (the preflight integration table in the pack-preflight folder had no table file yet, only pins.txt and judge.sh; pins used
as the exact file hashes of the set above).

## Proposed stable global ids

Assigned, append-only, never reused, never renumbered, independent of the engine's mask slots (slots are a gametype-mask
position and already collide: the hammer branch uses 6). Registry file shape proposal: one line per entry, `id key status`
with status shipped or retired; retired ids stay reserved. Today's slot order is kept for 1-14 so migration of saved
masks is the identity map.

| id | key | note |
|---|---|---|
| 1 | battle_rifle | slot 1 |
| 2 | smg | slot 2 |
| 3 | smg_silenced | slot 3 |
| 4 | carbine | slot 4 |
| 5 | ar_silenced | slot 5 |
| 6 | dmr | slot 6 |
| 7 | evo_rifle | slot 7 |
| 8 | needler | slot 8 |
| 9 | sniper_bolt | slot 9 |
| 10 | magnum_scoped | slot 10 |
| 11 | brute_shot | slot 11 |
| 12 | ar_modern | slot 12 |
| 13 | sidearm | slot 13 |
| 14 | energy_sword | slot 14 |
| 15 | smg_tactical | ae/packs-076a |
| 16 | pdw | ae/pack-pdw |
| 17 | revolver | ae/pack-revolver |
| 18 | focus_beam | ae/pack-focus-beam |
| 19 | ravager | ae/pack-ravager-burst (ae/pack-spiker-ravager older) |
| 20 | spiker | ae/pack-spiker-ravager |
| 21 | auto_shotgun | ae/pack-auto-shotgun |
| 22 | grenade_launcher | ae/pack-grenade-launcher |
| 23 | concussion_rifle | ae/pack-concussion-rifle |
| 24 | gravity_hammer | ae/hammer-pack |

Non-weapon modules (tags-only, never in a loadout, not weapon ids): suggested separate series M1 vehicle_damage, M2 bomb,
M3 grenades (EMP first, one entry per grenade type later). Variants (owner decision: small weapon-config records) would be
(weapon id, config id) pairs, config ids per weapon starting at 0 = the shipped tuning; e.g. battle_rifle behaviour 1 is a config
of one weapon, evo_rifle (2) and dmr (3) share a mesh family but are separate weapons today because their meshes differ
(collision above), so they stay two ids unless the cache stores one mesh plus a config.
Weapon count today 24 plus 3 modules, 26 donor packs read (grenades unbuilt).

## Limits the cache must outgrow

Format v1 caps: 1024 tags, 512 own tags, 64 MiB file, 16 MiB tag bytes, 8 MiB per resource, 48 MiB resources, 4096 resources,
8 weapons a pack (aepack.py MAXIMUM_*). The merged set has 1249 distinct tag paths and 191 MB: a container above v1's file
and tag caps, with a content-addressed blob section, is required.
