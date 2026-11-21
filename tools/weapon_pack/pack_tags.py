"""A weapon pack's tags read as the numbers a design is held to: the weapon's trigger, zoom and magazines, the
projectile and its damage effect, the weapon HUD's crosshairs, the models' nodes and bounds, the sounds' lengths.

The pack holds each own tag in the pack layout (little-endian, no file header; a tag reference is 16 bytes, a block 12:
count, address, definition), its pointers and tag references named by fixups (aepack.py). The field offsets below are the
engine's own structs (source/items/weapon_definitions.h and its neighbours) added up, each checked against the size or
the offset those headers assert; a field this does not name is not read. Nothing here changes a pack.

    pack_tags.py <pack.aepack>...        every weapon of each pack, as JSON lines
"""
import json
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import aepack

REFERENCE = 16
OBJECT_SIZE = 380              # _object_definition
ITEM_SIZE = 396                # _item_definition
WEAPON_AT = OBJECT_SIZE + ITEM_SIZE
# _weapon_definition (weapon tag offsets from WEAPON_AT)
W_FLAGS, W_ZOOM_LEVELS, W_ZOOM_MIN, W_ZOOM_MAX, W_INTERFACE, W_MAGAZINES, W_TRIGGERS = 0, 210, 212, 216, 340, 488, 500
W_MELEE_DAMAGE = 140
MAGAZINE_SIZE, MAGAZINE_AMMO_SIZE = 112, 28
TRIGGER_SIZE = 276
T_FIRING_EFFECTS, FIRING_EFFECT_SIZE, FIRING_EFFECT_AT = 264, 132, 36
# trigger offsets
T_FLAGS, T_RATE_INITIAL, T_RATE_FINAL, T_RATE_ACCEL, T_RATE_DECEL, T_MAGAZINE, T_ROUNDS, T_ERROR_INITIAL = 0, 4, 8, 12, 16, 32, 34, 48
T_CHARGING, T_CHARGED, T_OVERCHARGED, T_PER_SHOT, T_SPREAD, T_FP_OFFSET, T_PROJECTILE = 72, 76, 80, 110, 112, 132, 148
T_ERROR_LOWER, T_ERROR_UPPER = 124, 128
# trigger flags (weapon_definitions.h)
TRIGGER_FLAG_NAMES = ("tracks_projectile", "random_firing_effects", "can_fire_with_partial_ammunition", "latched", "toggles",
                      "uses_weapon_origin", "sticks_when_dropped", "ejection_port_during_chamber", "discharging_spews",
                      "analog_rate_of_fire", "use_error_when_unzoomed", "projectiles_cannot_be_aimed",
                      "projectiles_identical_error", "client_side_only")
# the projectile (offsets from the tag's start)
P_FLAGS, P_ARMING, P_IMPACT, P_EFFECT, P_MIN_V, P_RANGE, P_AIR_LO, P_AIR_HI, P_V0, P_V1 = 380, 420, 548, 428, 452, 456, 464, 468, 484, 488
P_TIMER_LO, P_TIMER_UP, P_GRAVITY, P_RESPONSES = 444, 448, 460, 576
P_ATTACHED, P_RADIUS = 532, 416
RESPONSE_SIZE = 160
RESPONSE_NAMES = ("disappear", "detonate", "reflect", "overpenetrate", "attach")
# the damage effect (offsets from the tag's start)
D_DAMAGE, D_FALLOFF, D_CUTOFF = 452, 0, 4
D_MINIMUM, D_LOWER, D_UPPER, D_MODIFIERS = 464, 468, 472, 512
MATERIALS = ("dirt", "sand", "stone", "snow", "wood", "hollow_metal", "thin_metal", "thick_metal", "rubber", "glass", "force_field",
             "grunt", "hunter_armor", "hunter_skin", "elite", "jackal", "jackal_energy_shield", "engineer_skin",
             "engineer_force_field", "flood_combat_form", "flood_carrier_form", "cyborg_armor", "cyborg_energy_shield",
             "human_armor", "human_skin", "sentinel", "monitor", "plastic", "water", "leaves", "elite_energy_shield", "ice",
             "hunter_shield")
# the weapon HUD
H_CROSSHAIRS = 132
CROSSHAIR_SIZE, CROSSHAIR_ITEM_SIZE = 104, 108
CROSSHAIR_FLAGS = ("flashes", "not_a_sprite", "not_on_default_zoom", "show_sniper_data", "hide_outside_area",
                   "one_zoom_level", "only_on_default_zoom")
CROSSHAIR_STATES = ("aim", "zoom", "charge", "flash_ammo", "flash_heat", "flash_total_ammo", "flash_total_battery", "reload")
# models
MODEL_ROOT = 232
MD_MARKERS, MD_NODES, MD_REGIONS, MD_GEOMETRIES, MD_SHADERS = 0xAC, 0xB8, 0xC4, 0xD0, 0xDC
NODE_SIZE, REGION_SIZE, PERMUTATION_SIZE, MARKER_SIZE, GEOMETRY_SIZE, PART_SIZE, SHADER_SIZE = 156, 76, 88, 80, 48, 132, 32
# the model's own markers (model_definitions.h's model_marker: a name, 20 bytes, its instances) and an instance (region,
# permutation, node, a pad byte, the translation, the rotation)
MODEL_MARKER_SIZE, MARKER_INSTANCE_SIZE = 64, 32


class Tag:
    """one tag of a pack: its bytes, and its pointers and references"""

    def __init__(self, parsed, index):
        record = parsed.tags[index]
        self.parsed, self.index, self.group = parsed, index, record[0]
        self.own = record[4] == aepack.OWN
        self.name = (parsed.strings[record[3]:parsed.strings.index(b"\0", record[3])].decode("utf-8")
                     if record[3] != aepack.NONE else "")
        tag_bytes = parsed.sections[aepack.TAG_BYTES]
        self.bytes = tag_bytes[record[9]:record[9] + record[10]] if self.own else b""
        self.references, self.pointers = {}, {}
        for offset, kind, _pad, value in parsed.fixups[record[11]:record[11] + record[12]]:
            if kind == aepack.FIXUP_REFERENCE:
                self.references[offset] = None if value == aepack.NONE else value
            elif kind == aepack.FIXUP_POINTER:
                self.pointers[offset] = value
        self.parts = parsed.parts[record[13]:record[13] + record[14]]
        self.resources = [resource for resource in parsed.resources if resource[0] == index]

    def f32(self, at):
        return struct.unpack_from("<f", self.bytes, at)[0]

    def i16(self, at):
        return struct.unpack_from("<h", self.bytes, at)[0]

    def u16(self, at):
        return struct.unpack_from("<H", self.bytes, at)[0]

    def i32(self, at):
        return struct.unpack_from("<i", self.bytes, at)[0]

    def u32(self, at):
        return struct.unpack_from("<I", self.bytes, at)[0]

    def reference(self, at):
        """the pack's tag index a reference at `at` names, or None"""
        return self.references.get(at)

    def block(self, at):
        """(count, offset of the first element) of the block at `at`"""
        count = self.i32(at)
        return count, self.pointers.get(at + 4, 0) if count > 0 else 0

    def string(self, at, size):
        return self.bytes[at:at + size].split(b"\0")[0].decode("latin-1")


def group_name(value):
    return struct.pack("<I", value).decode("latin-1")


class WeaponPack:
    """a parsed pack with its tags read lazily"""

    def __init__(self, image):
        self.parsed = aepack.read(image)
        self._tags = {}

    def tag(self, index):
        if index is None:
            return None
        if index not in self._tags:
            self._tags[index] = Tag(self.parsed, index)
        return self._tags[index]

    def key(self, weapon):
        offset = weapon[0]
        return self.parsed.strings[offset:self.parsed.strings.index(b"\0", offset)].decode("utf-8")

    def reference_group(self, tag, at, group):
        """the own tag of `group` a reference at `at` of `tag` names (None for none, an external or another group)"""
        target = self.tag(tag.reference(at))
        return target if target is not None and target.own and target.group == aepack.group(group) else None


def ref_tag(pack, tag, at):
    """the tag (own or not) a reference names"""
    return pack.tag(tag.reference(at))


# ---------- the weapon

def read_triggers(pack, weapon_tag):
    base = WEAPON_AT + W_TRIGGERS
    count, first = weapon_tag.block(base)
    triggers = []
    for index in range(count):
        at = first + index * TRIGGER_SIZE
        flags = weapon_tag.u32(at + T_FLAGS)
        projectile = ref_tag(pack, weapon_tag, at + T_PROJECTILE)
        triggers.append({
            "flags": [name for bit, name in enumerate(TRIGGER_FLAG_NAMES) if flags >> bit & 1],
            "rate_initial": weapon_tag.f32(at + T_RATE_INITIAL), "rate_final": weapon_tag.f32(at + T_RATE_FINAL),
            "rate_accel_time": weapon_tag.f32(at + T_RATE_ACCEL),
            "rounds_per_shot": weapon_tag.i16(at + T_ROUNDS), "magazine": weapon_tag.i16(at + T_MAGAZINE),
            "charging_time": weapon_tag.f32(at + T_CHARGING), "charged_time": weapon_tag.f32(at + T_CHARGED),
            "overcharged_action": weapon_tag.i16(at + T_OVERCHARGED),
            "projectiles_per_shot": weapon_tag.i16(at + T_PER_SHOT), "spread_angle": weapon_tag.f32(at + T_SPREAD),
            "error_lower": weapon_tag.f32(at + T_ERROR_LOWER), "error_upper": weapon_tag.f32(at + T_ERROR_UPPER),
            "fp_offset": [weapon_tag.f32(at + T_FP_OFFSET + 4 * axis) for axis in range(3)],
            "projectile": projectile.index if projectile is not None and projectile.own else None,
            "projectile_name": projectile.name if projectile is not None else None,
        })
    return triggers


def read_weapon(pack, root_index):
    tag = pack.tag(root_index)
    base = WEAPON_AT
    magazines = []
    count, first = tag.block(base + W_MAGAZINES)
    for index in range(count):
        at = first + index * MAGAZINE_SIZE
        magazines.append({"initial": tag.i16(at + 6), "maximum": tag.i16(at + 8), "loaded": tag.i16(at + 10),
                          "reload_time": tag.f32(at + 20)})
    interface = base + W_INTERFACE
    fp_model = pack.reference_group(tag, interface, "mode") or pack.reference_group(tag, interface, "mod2")
    return {
        "bounding_radius": tag.f32(4), "item_flags": tag.u32(OBJECT_SIZE), "weapon_flags": tag.u32(base + W_FLAGS),
        "label": tag.string(base + 4, 32),
        "zoom_levels": tag.i16(base + W_ZOOM_LEVELS), "zoom_min": tag.f32(base + W_ZOOM_MIN),
        "zoom_max": tag.f32(base + W_ZOOM_MAX),
        "magazines": magazines, "triggers": read_triggers(pack, tag),
        "model_3p": (ref_tag(pack, tag, 40).index if ref_tag(pack, tag, 40) is not None else None),
        "model_fp": (ref_tag(pack, tag, interface).index if ref_tag(pack, tag, interface) is not None else None),
        "hud": (ref_tag(pack, tag, interface + 36).index if ref_tag(pack, tag, interface + 36) is not None else None),
        "animations": (ref_tag(pack, tag, interface + 16).index if ref_tag(pack, tag, interface + 16) is not None else None),
        "collision": describe_ref(pack, tag, 112), "physics": describe_ref(pack, tag, 128),
        "pickup_sound": ref_tag(pack, tag, interface + 52).name if ref_tag(pack, tag, interface + 52) is not None else None,
    }


def describe_ref(pack, tag, at):
    """{"name", "own", "index"} of the tag a reference names, None for none"""
    target = ref_tag(pack, tag, at)
    return None if target is None else {"name": target.name, "own": target.own, "index": target.index}


# ---------- damage

def read_damage_effect(tag):
    base = D_DAMAGE
    modifiers = {name: round(tag.f32(D_MODIFIERS + 4 * index), 4) for index, name in enumerate(MATERIALS)
                 if D_MODIFIERS + 4 * index + 4 <= len(tag.bytes)}
    return {"name": tag.name, "damage_minimum": tag.f32(D_MINIMUM), "lower": tag.f32(D_LOWER), "upper": tag.f32(D_UPPER),
            "falloff_radius": tag.f32(D_FALLOFF), "cutoff_radius": tag.f32(D_CUTOFF),
            "modifiers": {name: value for name, value in modifiers.items() if value != 1.0}, "all_modifiers": modifiers}


def read_projectile(pack, index):
    tag = pack.tag(index)
    if tag is None or not tag.own:
        return None
    responses = []
    count, first = tag.block(P_RESPONSES)
    for item in range(count):
        at = first + item * RESPONSE_SIZE
        responses.append({"default": RESPONSE_NAMES[tag.i16(at + 2)] if 0 <= tag.i16(at + 2) < 5 else tag.i16(at + 2),
                          "potential": RESPONSE_NAMES[tag.i16(at + 36)] if 0 <= tag.i16(at + 36) < 5 else tag.i16(at + 36),
                          "reflect_skip": round(tag.f32(at + 40), 4),
                          "angle_lower": round(math.degrees(tag.f32(at + 44)), 2), "angle_upper": round(math.degrees(tag.f32(at + 48)), 2),
                          "speed_lower": round(tag.f32(at + 52), 4), "speed_upper": round(tag.f32(at + 56), 4)})
    damage = pack.reference_group(tag, P_IMPACT, "jpt!")
    attached = pack.reference_group(tag, P_ATTACHED, "jpt!")
    effect = ref_tag(pack, tag, P_EFFECT)
    explosion = None
    if effect is not None and effect.own:
        found = [pack.tag(t) for _, t in sorted(effect.references.items())
                 if t is not None and pack.tag(t).own and pack.tag(t).group == aepack.group("jpt!")]
        explosion = read_damage_effect(found[0]) if found else None
    return {"name": tag.name, "flags": tag.u32(P_FLAGS), "arming_time": tag.f32(P_ARMING), "radius": tag.f32(P_RADIUS),
            "air_range_lower": tag.f32(P_AIR_LO), "air_range_upper": tag.f32(P_AIR_HI), "max_range": tag.f32(P_RANGE),
            "velocity_initial": tag.f32(P_V0), "velocity_final": tag.f32(P_V1), "gravity": tag.f32(P_GRAVITY),
            "timer_lower": tag.f32(P_TIMER_LO), "timer_upper": tag.f32(P_TIMER_UP),
            "impact_damage": read_damage_effect(damage) if damage is not None else None,
            "attached_damage": read_damage_effect(attached) if attached is not None else None,
            "explosion_damage": explosion, "material_responses": responses}


# ---------- the HUD

def read_hud(pack, index):
    tag = pack.tag(index)
    if tag is None or not tag.own:
        return None
    count, first = tag.block(H_CROSSHAIRS)
    elements = []
    for element in range(count):
        at = first + element * CROSSHAIR_SIZE
        bitmap = ref_tag(pack, tag, at + 36)
        items_count, items_first = tag.block(at + 52)
        items = []
        for item in range(items_count):
            item_at = items_first + item * CROSSHAIR_ITEM_SIZE
            flags = tag.u32(item_at + 72)
            items.append({"flags": [name for bit, name in enumerate(CROSSHAIR_FLAGS) if flags >> bit & 1],
                          "sequence": tag.i16(item_at + 70), "scale": [round(tag.f32(item_at + 4), 4), round(tag.f32(item_at + 8), 4)]})
        state = tag.i16(at)
        elements.append({"state": CROSSHAIR_STATES[state] if 0 <= state < len(CROSSHAIR_STATES) else state,
                         "bitmap": bitmap.name if bitmap is not None else None, "items": items})
    return {"name": tag.name, "crosshairs": elements}


# ---------- models

def read_model(pack, index):
    """the model's bounds, nodes (each with its rest rotation, a quaternion i j k w), part bounds, every vertex's position
    ("points") and its markers ({"name", "node", "translation", "rotation"} for each instance), in world units (a tag's
    own), rest pose; None for a tag that is not a model or has no geometry"""
    tag = pack.tag(index)
    if tag is None or not tag.own or not tag.bytes:
        return None
    nodes = []
    count, first = tag.block(MD_NODES)
    for node in range(count):
        at = first + node * NODE_SIZE
        nodes.append({"name": tag.string(at, 32), "parent": tag.i16(at + 36),
                      "position": [tag.f32(at + 40 + 4 * axis) for axis in range(3)],
                      "rotation": [tag.f32(at + 52 + 4 * axis) for axis in range(4)]})
    markers = []
    count, first = tag.block(MD_MARKERS)
    for marker in range(count):
        at = first + marker * MODEL_MARKER_SIZE
        name = tag.string(at, 32)
        instances, instance_first = tag.block(at + 52)
        for instance in range(instances):
            instance_at = instance_first + instance * MARKER_INSTANCE_SIZE
            markers.append({"name": name, "node": tag.bytes[instance_at + 2],
                            "translation": [tag.f32(instance_at + 4 + 4 * axis) for axis in range(3)],
                            "rotation": [tag.f32(instance_at + 16 + 4 * axis) for axis in range(4)]})
    shaders = []
    count, first = tag.block(MD_SHADERS)
    for shader in range(count):
        target = ref_tag(pack, tag, first + shader * SHADER_SIZE)
        shaders.append(target.name if target is not None else "")
    # the part table lives in the geometry's parts; each pack Part record carries its vertices
    parts = []
    points = []
    for part in tag.parts:
        offset, vertex_type, _triangle_type, vertex_count = part[0], part[1], part[2], part[3]
        vertex_size = aepack.VERTEX_SIZES[vertex_type]
        vertices = pack_geometry(pack, part)
        low, high = [1e9] * 3, [-1e9] * 3
        for vertex in range(vertex_count):
            position = struct.unpack_from("<fff", vertices, vertex * vertex_size)
            points.append(position)
            for axis in range(3):
                low[axis], high[axis] = min(low[axis], position[axis]), max(high[axis], position[axis])
        shader_index = tag.i16(offset + 4) if offset + 6 <= len(tag.bytes) else -1
        parts.append({"offset": offset, "vertices": vertex_count, "shader": shaders[shader_index] if 0 <= shader_index < len(shaders) else None,
                      "low": low, "high": high})
    return {"name": tag.name, "nodes": nodes, "shaders": shaders, "parts": parts, "points": points, "markers": markers}


def pack_geometry(pack, part):
    return pack.parsed.sections[aepack.GEOMETRY][part[5]:part[5] + part[6]]


def model_bounds(model, only=None):
    """(low, high) over the model's parts (those `only` accepts, by shader name), or None"""
    parts = [part for part in (model or {}).get("parts", ()) if only is None or only(part)]
    if not parts:
        return None
    return ([min(part["low"][axis] for part in parts) for axis in range(3)],
            [max(part["high"][axis] for part in parts) for axis in range(3)])


def read_physics(pack, index):
    """the own physics tag's mass points: [{"name", "position", "radius"}]"""
    tag = pack.tag(index)
    if tag is None or not tag.own:
        return None
    count, first = tag.block(116)
    points = []
    for item in range(count):
        at = first + item * 128
        points.append({"name": tag.string(at, 32), "position": [tag.f32(at + 0x38 + 4 * axis) for axis in range(3)],
                       "radius": tag.f32(at + 0x68)})
    return {"name": tag.name, "radius": tag.f32(0), "mass": tag.f32(8), "mass_points": points}


# ---------- sounds

SAMPLE_RATES = (22050, 44100)
BLOCK_BYTES, BLOCK_SAMPLES = 36, 64
INDEX_STEPS = (-1, -1, -1, -1, 2, 4, 6, 8)
STEP_TABLE = (7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
              118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
              963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
              5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385,
              24623, 27086, 29794, 32767)


def decode_adpcm_block(block):
    """one channel's 36-byte IMA block: 64 samples (the header's sample, then 63 decoded)"""
    predictor, index = struct.unpack_from("<hB", block, 0)
    index = min(index, 88)
    samples = [predictor]
    for byte in block[4:]:
        for nibble in (byte & 15, byte >> 4):
            step = STEP_TABLE[index]
            delta = step >> 3
            if nibble & 1:
                delta += step >> 2
            if nibble & 2:
                delta += step >> 1
            if nibble & 4:
                delta += step
            predictor += -delta if nibble & 8 else delta
            predictor = max(-32768, min(32767, predictor))
            index = max(0, min(88, index + INDEX_STEPS[nibble & 7]))
            samples.append(predictor)
    return samples[:BLOCK_SAMPLES]


def sound_permutations(pack, index, image):
    """[{"name", "seconds", "tail_seconds"}] of an own sound tag whose samples are 16-bit PCM or IMA ADPCM (mono or
    stereo: the first channel is measured), or {"unreadable": why}. tail_seconds: from the start to the last 10 ms
    window whose level is within 30 dB of the loudest window's"""
    tag = pack.tag(index)
    if tag is None:
        return {"unreadable": "no tag"}
    if not tag.own:
        return {"unreadable": "a stock sound (not in the pack)", "name": tag.name}
    rate = SAMPLE_RATES[tag.u16(6)] if tag.u16(6) < 2 else 0
    stereo, compression = tag.u16(108), tag.u16(110)
    channels = 2 if stereo else 1
    count, first = tag.block(224)
    result = []
    for number in range(count):
        at = first + number * 124
        size = tag.u32(at + 64)
        resource = next((r for r in tag.resources if r[2] == size and r[1] == tag.u32(at + 72)), None)
        if resource is None:
            resource = next((r for r in tag.resources if r[2] == size), None)
        if resource is None:
            result.append({"name": tag.string(at, 32), "unreadable": "no resource of %d bytes" % size})
            continue
        data = aepack.read_resource_data(image, pack.parsed, resource)
        if compression in (1, 2):
            samples = []
            step = BLOCK_BYTES * channels
            for block in range(0, len(data) - step + 1, step):
                samples += decode_adpcm_block(data[block:block + BLOCK_BYTES])
        elif compression == 0:
            samples = list(struct.unpack_from(">%dh" % (len(data) // 2 // channels * channels), data))[::channels]
        else:
            result.append({"name": tag.string(at, 32), "unreadable": "compression %d (not read)" % compression})
            continue
        window = max(1, rate // 100)
        levels = [math.sqrt(sum(sample * sample for sample in samples[i:i + window]) / max(1, len(samples[i:i + window])))
                  for i in range(0, len(samples), window)]
        loudest = max(levels) if levels else 0.0
        last = max((i for i, level in enumerate(levels) if loudest and level >= loudest * 0.0316), default=0)
        result.append({"name": tag.string(at, 32), "rate": rate, "seconds": round(len(samples) / rate, 3),
                       "tail_seconds": round((last + 1) * window / rate, 3), "peak": loudest})
    return result


def firing_sounds(pack, root_index, image):
    """the sounds the first trigger's firing effects play: {"effects": [...], "sounds": {name: permutations}}"""
    weapon = pack.tag(root_index)
    count, first = weapon.block(WEAPON_AT + W_TRIGGERS)
    if not count:
        return {"effects": [], "sounds": {}}
    at = first + T_FIRING_EFFECTS
    effects_count, effects_first = weapon.block(at)
    effects, sounds = [], {}
    for element in range(effects_count):
        reference = effects_first + element * FIRING_EFFECT_SIZE + FIRING_EFFECT_AT
        effect = ref_tag(pack, weapon, reference)
        if effect is None or effect.name in effects:
            continue
        effects.append(effect.name)
        for target in sorted({t for t in effect.references.values() if t is not None}):
            sound = pack.tag(target)
            if sound.group == aepack.group("snd!"):
                sounds[sound.name] = sound_permutations(pack, target, image)
    return {"effects": effects, "sounds": sounds}


# ---------- the idle pose of the first-person animation

ANIMATIONS_AT, ANIMATION_SIZE = 116, 180
A_FRAME_COUNT, A_FRAME_SIZE, A_NODE_COUNT = 34, 36, 44
A_TRANSLATION_FLAGS, A_SCALE_FLAGS, A_ROTATION_FLAGS = 92, 100, 108
A_DEFAULT_SIZE, A_DEFAULT_POINTER, A_FRAME_POINTER = 140, 152, 172


def idle_root(pack, animation_index, node_count=None):
    """the first-person idle animation's first frame for the model's root node (node 0): {"translation": [x, y, z],
    "rotation": [i, j, k, w], "animation": name, "frames": n}, or {"unreadable": why}. Frames and defaults are walked
    node by node (rotation 8 bytes, translation 12, scale 4: a flag bit set means the frame holds it, else the default
    data does)"""
    tag = pack.tag(animation_index)
    if tag is None or not tag.own or not tag.bytes:
        return {"unreadable": "no animation graph of its own"}
    count, first = tag.block(ANIMATIONS_AT)
    chosen = None
    for index in range(count):
        name = tag.string(first + index * ANIMATION_SIZE, 32)
        if name.lower().endswith("idle"):
            chosen = (index, name)
            if "first-person" in name.lower():
                break
    if chosen is None:
        return {"unreadable": "no idle animation"}
    at = first + chosen[0] * ANIMATION_SIZE
    nodes = tag.u16(at + A_NODE_COUNT)
    flags = {}
    for label, offset in (("translation", A_TRANSLATION_FLAGS), ("scale", A_SCALE_FLAGS), ("rotation", A_ROTATION_FLAGS)):
        flags[label] = tag.u32(at + offset) | tag.u32(at + offset + 4) << 32
    default_pointer = tag.pointers.get(at + A_DEFAULT_POINTER)
    frame_pointer = tag.pointers.get(at + A_FRAME_POINTER)
    if default_pointer is None:
        return {"unreadable": "no default data"}
    reading = {"default": default_pointer, "frame": frame_pointer}
    result = {}
    cursor = {"default": 0, "frame": 0}
    for label, size in (("rotation", 8), ("translation", 12), ("scale", 4)):
        pass
    # node 0 comes first in both: its rotation, translation and scale in that order, from the frame when flagged
    node_values = {}
    for label, size in (("rotation", 8), ("translation", 12), ("scale", 4)):
        animated = bool(flags[label] & 1)
        source = "frame" if animated else "default"
        if animated and frame_pointer is None:
            return {"unreadable": "no frame data"}
        base = reading[source] + cursor[source]
        cursor[source] += size
        node_values[label] = tag.bytes[base:base + size]
    rotation = [value / 32767.0 for value in struct.unpack("<4h", node_values["rotation"])]
    translation = list(struct.unpack("<3f", node_values["translation"]))
    return {"translation": translation, "rotation": rotation, "animation": chosen[1], "frames": tag.u16(at + A_FRAME_COUNT),
            "nodes": nodes, "root_animated": [label for label in flags if flags[label] & 1]}
