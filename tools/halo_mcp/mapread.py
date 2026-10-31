"""Our own reader for Xbox-format Halo CE cache files (weapon / damage numbers).

Struct layouts are parsed from the engine's own headers (source/ of this repo),
so the reader follows the engine's tag definitions. Pure Python, no Invader code.
"""
import math
import re
import struct
import zlib

SZ = {'real': 4, 'short': 2, 'word': 2, 'long': 4, 'byte': 1, 'char': 1, 'unsigned long': 4,
      'unsigned short': 2, 'boolean': 4, 'real_point3d': 12, 'real_vector3d': 12,
      'real_rgb_color': 12, 'real_argb_color': 16, 'real_rectangle3d': 24, 'rectangle2d': 8,
      'struct tag_reference': 16, 'struct tag_block': 12}
CONST = {'TAG_STRING_LENGTH': 31, 'MAXIMUM_NUMBER_OF_MATERIAL_TYPES': 40, 'NUMBER_OF_vibrate_FREQUENCIES': 2}
HEADERS = ['objects/object_definitions.h', 'items/item_definitions.h', 'items/weapon_definitions.h',
           'items/projectile_definitions.h', 'objects/damage_effect_definitions.h',
           'units/unit_definitions.h', 'units/biped_definitions.h', 'game/aim_assist.h',
           'objects/damage_resistances.h', 'physics/collision_model_definitions.h']


class Layouts:
    def __init__(self, src):
        self.src = src.rstrip('/') + '/'
        self.structs = {}
        self._cache = {}
        for p in HEADERS:
            self._load(p)
        with open(self.src + 'game/game_globals.h') as fh:
            t = fh.read()
        a = t.index('_material_dirt = 0')
        b = t.index('NUMBER_OF_MATERIAL_TYPES')
        self.materials = [x.replace('_material_', '') for x in re.findall(r'_material_\w+', t[a:b])]

    def _load(self, path):
        with open(self.src + path) as fh:
            t = fh.read()
        t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
        t = re.sub(r'//.*', '', t)
        for m in re.finditer(r'struct (\w+)\s*\{(.*?)\n\};', t, re.S):
            fields = []
            for line in m.group(2).split(';'):
                line = line.strip()
                if not line:
                    continue
                mm = re.match(r'(.+?)\s+(\w+)(?:\[(\w+)(?:\+(\d+))?\])?$', line)
                if not mm:  # bitfields, unions and the like: say so rather than shift every later offset
                    raise ValueError('cannot parse the field %r in struct %s of %s' % (line, m.group(1), path))
                fields.append((mm.group(1).strip(), mm.group(2), mm.group(3), mm.group(4)))
            self.structs[m.group(1)] = fields

    def _size(self, ty):
        if ty in SZ:
            return SZ[ty]
        if ty.startswith('struct '):
            ty = ty[7:]
        if ty in self.structs:
            return self.layout(ty)[1]
        raise KeyError('unknown type ' + ty)

    def layout(self, name):
        if name in self._cache:
            return self._cache[name]
        off, d = 0, {}
        for ty, fn, n, pl in self.structs[name]:
            s = self._size(ty)
            cnt = 1
            if n:
                cnt = (CONST[n] if n in CONST else int(n)) + (int(pl) if pl else 0)
            sub = ty[7:] if ty.startswith('struct ') else ty
            if sub in self.structs:
                for k, v in self.layout(sub)[0].items():
                    d[fn + '.' + k] = off + v
            d[fn] = off
            off += s * cnt
        self._cache[name] = (d, off)
        return self._cache[name]


class CacheMap:
    BASE = 0x803A6000

    def __init__(self, path, max_bytes=512 * 1024 * 1024):
        with open(path, 'rb') as fh:
            raw = fh.read()
        hdr = raw[:2048]
        d = zlib.decompressobj()
        body = d.decompress(raw[2048:], max_bytes)  # (a size cap: a hostile map must not inflate without bound)
        if d.unconsumed_tail:
            raise ValueError('cache file inflates beyond %d bytes' % max_bytes)
        self.full = hdr + body
        _ver, flen, _, self.toff, _tsz = struct.unpack_from('<iiiii', hdr, 4)
        if len(self.full) != flen:
            raise ValueError('not an Xbox-format cache file (length mismatch)')
        th = struct.unpack_from('<IiIi', self.full, self.toff)
        inst, count = th[0], th[3]
        self.tags = {}
        for i in range(count):
            g, _p1, _p2, ti, nm, ba, _u1, _u2 = struct.unpack_from('<iiiiIIII', self.full, self.off(inst) + i * 32)
            self.tags[ti] = (g.to_bytes(4, 'big').decode('latin1'), self.cstr(nm), ba)
        self.by = {(v[0], v[1]): (k, v[2]) for k, v in self.tags.items()}

    def off(self, a):
        return self.toff + (a - self.BASE)

    def cstr(self, a):
        o = self.off(a)
        return self.full[o:self.full.index(b'\0', o)].decode('latin1')

    def f(self, ba, o, fmt):
        return struct.unpack_from('<' + fmt, self.full, self.off(ba) + o)[0]

    def ref(self, ba, o):
        _g, _np, _l, idx = struct.unpack_from('<iIii', self.full, self.off(ba) + o)
        return self.tags[idx][1] if idx in self.tags else None

    def block(self, ba, o):
        return struct.unpack_from('<iI', self.full, self.off(ba) + o)


def _r(x):
    return round(x, 6) if isinstance(x, float) else x


def weapon_numbers(cm, lay, weapon):
    """Weapon -> triggers/magazines, its projectile and impact damage effect."""
    key = ('weap', weapon)
    if key not in cm.by:
        raise KeyError('weapon tag not in map: ' + weapon)
    W = lay.layout('weapon_definition')[0]
    M, MS = lay.layout('weapon_magazine_definition')
    T, TS = lay.layout('weapon_trigger_definition')
    P = lay.layout('projectile_definition')[0]
    D = lay.layout('damage_effect_definition')[0]
    ba = cm.by[key][1]
    out = {'weapon': weapon,
           'flags': cm.f(ba, W['weapon.flags'], 'I'),
           'zoom_level_count': cm.f(ba, W['weapon.zoom_level_count'], 'h'),
           'zoom_magnification_minimum': _r(cm.f(ba, W['weapon.zoom_magnification_minimum'], 'f')),
           'zoom_magnification_maximum': _r(cm.f(ba, W['weapon.zoom_magnification_maximum'], 'f')),
           'magazines': [], 'triggers': []}
    mc, ma = cm.block(ba, W['weapon.magazines'])
    for i in range(mc):
        b = ma + i * MS
        d = {k: cm.f(b, M[k], 'h') for k in ['rounds_total_initial', 'rounds_total_maximum',
                                              'rounds_loaded_maximum', 'rounds_recharged_per_second']}
        d['reload_time'] = _r(cm.f(b, M['reload_time'], 'f'))
        out['magazines'].append(d)
    proj = None
    tc, ta = cm.block(ba, W['weapon.triggers'])
    for i in range(tc):
        b = ta + i * TS

        def g(k, f='f'):
            return cm.f(b, T[k], f)
        t = {'flags': g('flags', 'I'), 'initial_rate_of_fire': _r(g('initial_rate_of_fire')),
             'final_rate_of_fire': _r(g('final_rate_of_fire')),
             'rounds_per_shot': g('rounds_per_shot', 'h'), 'magazine_index': g('magazine_index', 'h'),
             'initial_error': _r(g('initial_error')), 'final_error': _r(g('final_error')),
             'projectiles_per_shot': g('projectiles_per_shot', 'h'),
             'projectile_distribution_angle': _r(g('projectile_distribution_angle')),
             'error_angle_lower_deg': _r(math.degrees(g('projectile_error_angle_lower_bound'))),
             'error_angle_upper_deg': _r(math.degrees(g('projectile_error_angle_upper_bound'))),
             'projectile': cm.ref(b, T['projectile'])}
        out['triggers'].append(t)
        proj = proj or t['projectile']
    if proj and ('proj', proj) in cm.by:
        pb = cm.by[('proj', proj)][1]
        pd = {k.split('.', 1)[1]: _r(cm.f(pb, P[k], 'I' if 'flags' in k else 'f'))
              for k in ['projectile.flags', 'projectile.collision_radius', 'projectile.minimum_velocity',
                        'projectile.maximum_range', 'projectile.air_damage_range_lower_bound',
                        'projectile.air_damage_range_upper_bound', 'projectile.initial_velocity',
                        'projectile.final_velocity']}
        pd['tag'] = proj
        out['projectile'] = pd
        jname = cm.ref(pb, P['projectile.impact_damage'])
        if jname and ('jpt!', jname) in cm.by:
            jb = cm.by[('jpt!', jname)][1]
            dm = {'tag': jname}
            for k, f in [('damage.damage_lower_bound', 'f'), ('damage.damage_upper_bound', 'f'),
                         ('damage.damage_minimum', 'f'), ('damage.stun', 'f'), ('damage.flags', 'I')]:
                dm[k.split('.', 1)[1]] = _r(cm.f(jb, D[k], f))
            base = D['damage.material_modifiers']
            dm['material_modifiers_not_1'] = {lay.materials[i]: round(cm.f(jb, base + 4 * i, 'f'), 3)
                                              for i in range(len(lay.materials))
                                              if cm.f(jb, base + 4 * i, 'f') != 1.0}
            out['damage_effect'] = dm
    return out
