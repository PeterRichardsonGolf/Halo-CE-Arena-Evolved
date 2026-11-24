#!/usr/bin/env python3
"""Field-diff gate for converted multiplayer maps.

Compares the tag data of a source map with the tag data of its converted copy.
Both maps are extracted to tag trees first (invader-extract). For every tag of
a gameplay group in the source tree, the tag's field dump (invader-edit -L) is
compared with the converted tag's dump. A tag is reported when

  missing   the converted map has no tag at that path,
  ref       a reference (dependency) points at a different tag path,
  field     any other field differs,
  error     a dump failed or came back empty (the tag is not compared).

Every dump line is kept with the path of the arrays and structs around it, and
the two dumps are compared as multisets, so a value that changes in one element
of an array is not hidden by an equal value elsewhere. Enumerations are
compared by their selected option only. Fields that only the source format
carries are ignored, and a damage acceleration is compared by its first
component. Tags listed in the allow file (one `path.group` per line) are kept
stock on purpose by the converter and are counted apart. For tags listed in the
fixed file (one `path.group` per line: the tags whose invalid indices the build
step repaired) the differences in index fields are not counted. The exit status is 1
when any tag is not allowed and differs.

usage: mcc_fielddiff.py [--invader-edit PATH] [--allow FILE] [--fixed FILE] [--json FILE]
                        [--groups g1,g2,...] SOURCE_TAGS CONVERTED_TAGS
"""
import argparse
import collections
import json
import os
import re
import subprocess
import sys

GAMEPLAY_GROUPS = (
    'weapon', 'projectile', 'damage_effect', 'continuous_damage_effect',
    'item_collection', 'equipment', 'garbage', 'vehicle', 'biped', 'physics',
    'point_physics', 'effect', 'device_control', 'device_machine',
    'device_light_fixture', 'scenery', 'actor_variant', 'sound_scenery',
    'model_collision_geometry', 'scenario', 'globals',
)

# Field names only the source format carries.
IGNORED_NAMES = frozenset((
    'mcc_upgrades', 'prediction_type', 'projectile_is_client_side_only',
    'use_3d_instantaneous_acceleration', 'tag_index',
    # script node handles: they carry a salt that changes whenever the scripts are compiled again
    'root_expression_index', 'initialization_expression_index',
))
# Field names whose value is the first component of a vector: only the first
# component is compared.
FIRST_COMPONENT = frozenset(('damage_instantaneous_acceleration',))

TREE_ART = ' │├└─'
HEADER_TYPES = ('array', 'struct', 'enum', 'bitmask')
REFERENCE = re.compile(r'\([^()]*\.[a-z_]+\)$')


class DumpError(Exception):
    """A tag dump that failed or came back empty."""


def is_reference(text):
    """A normalized line ending in a (path.group) is a tag reference, whatever type column the dump printed."""
    return bool(REFERENCE.search(text))


def field_name(text):
    return text.split(' ', 1)[0].split('(', 1)[0]


def parse_dump(lines):
    """Entries (path, text) of a dump: the lines that are compared, each with the names of the arrays and structs around it."""
    entries = []
    stack = {}
    skip_below = None
    for raw in lines:
        if not raw.strip():
            continue
        col = 0
        while col < len(raw) and raw[col] in TREE_ART:
            col += 1
        depth = col // 4
        if skip_below is not None:
            if depth > skip_below:
                continue
            skip_below = None
        text = re.sub(r'\s+', ' ', raw[col:].replace('gbxmodel', 'model')).strip()
        text = re.sub(r' dependency$', '', text)
        for d in [d for d in stack if d >= depth]:
            del stack[d]
        path = '/'.join(stack[d] for d in sorted(stack))
        kind = text.rsplit(' ', 1)[-1]
        if text.endswith(' enum value'):
            # an enumeration option: only the selected one is a value
            if text.endswith('(*) enum value'):
                entries.append((path, text))
            continue
        if kind in HEADER_TYPES:
            if field_name(text) in IGNORED_NAMES:
                skip_below = depth
                continue
            stack[depth] = field_name(text)
            continue
        name = field_name(text)
        if name in IGNORED_NAMES:
            continue
        if name in FIRST_COMPONENT:
            text = re.sub(r'\(([^,)]*)[^)]*\)', r'(\1)', text, count=1)
        entries.append((path, text))
    return entries


def index_value(text):
    m = re.search(r'\((-?\d+)\) index$', text)
    return m.group(1) if m else None


def diff_dumps(a_lines, b_lines, repaired=False):
    """(field_diffs, ref_diffs, exempted): entries only in a or only in b, split by kind; all hold 'path | text'.

    With repaired set, an index field that the build step repaired is exempted: the source has exactly one
    entry for that path and field, the converted side has exactly one, and its value is 65535 (none).
    """
    a = collections.Counter(parse_dump(a_lines))
    b = collections.Counter(parse_dump(b_lines))
    only_a = list((a - b).elements())
    only_b = list((b - a).elements())
    exempted = []
    if repaired:
        by_a = collections.defaultdict(list)
        by_b = collections.defaultdict(list)
        for e in only_a:
            if e[1].endswith(' index'):
                by_a[(e[0], field_name(e[1]))].append(e)
        for e in only_b:
            if e[1].endswith(' index'):
                by_b[(e[0], field_name(e[1]))].append(e)
        for key, left in by_a.items():
            right = by_b.get(key, [])
            if len(left) == 1 and len(right) == 1 and index_value(right[0][1]) == '65535':
                exempted.append('%s | %s -> %s' % (left[0][0], left[0][1], index_value(right[0][1])))
                only_a.remove(left[0])
                only_b.remove(right[0])
    ref_a = [e for e in only_a if is_reference(e[1])]
    ref_b = [e for e in only_b if is_reference(e[1])]
    pairs = _pair_references(ref_a, ref_b)
    paired_a = {x for x, _ in pairs if x}
    paired_b = {y for _, y in pairs if y}
    fmt = lambda e: '%s | %s' % e if e else ''
    ref = [(fmt(x), fmt(y)) for x, y in pairs]
    field = [fmt(e) for e in only_a if e not in paired_a] + \
            [fmt(e) for e in only_b if e not in paired_b]
    return field, ref, exempted


def _pair_references(only_a, only_b):
    """Pair differing references by path and field name; an unmatched one pairs with ''."""
    pairs = []
    pool = collections.defaultdict(list)
    for y in only_b:
        pool[(y[0], field_name(y[1]))].append(y)
    for x in only_a:
        bucket = pool.get((x[0], field_name(x[1])))
        pairs.append((x, bucket.pop(0) if bucket else None))
    for bucket in pool.values():
        pairs.extend((None, y) for y in bucket)
    return pairs


def tag_files(root):
    found = {}
    for dirpath, _, files in os.walk(root):
        for name in files:
            full = os.path.join(dirpath, name)
            found[os.path.relpath(full, root)] = full
    return found


def group_of(path):
    return path.rsplit('.', 1)[-1]


def dump_with_invader(invader_edit, root, tag):
    env = dict(os.environ)
    lib = os.path.dirname(os.path.abspath(invader_edit))
    env['LD_LIBRARY_PATH'] = lib + (':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
    r = subprocess.run([invader_edit, '-t', root, '-L', tag],
                       capture_output=True, env=env, check=False)
    if r.returncode != 0:
        raise DumpError('invader-edit exit %d: %s' % (r.returncode, r.stderr.decode('utf-8', errors='replace').strip()[:200]))
    lines = r.stdout.decode('utf-8', errors='replace').splitlines()
    if not any(x.strip() for x in lines):
        raise DumpError('empty dump')
    return lines


def checked_dump(dump, root, tag):
    lines = dump(root, tag)
    if not any(x.strip() for x in lines):
        raise DumpError('empty dump')
    return lines


def renamed_scenario(tag, src, conv):
    """The converted scenario that stands for a source scenario the build renamed after the map, or None.

    Only when the folder holds exactly one scenario on each side."""
    if group_of(tag) != 'scenario':
        return None
    folder = os.path.dirname(tag)
    in_src = [c for c in src if group_of(c) == 'scenario' and os.path.dirname(c) == folder]
    same = [c for c in conv if group_of(c) == 'scenario' and os.path.dirname(c) == folder]
    return same[0] if len(same) == 1 and len(in_src) == 1 else None


def compare_trees(src_root, conv_root, dump, groups=GAMEPLAY_GROUPS,
                  allowed=(), fixed=()):
    """Result dict: counts per group and per-tag details."""
    src = tag_files(src_root)
    conv = tag_files(conv_root)
    allowed = set(allowed)
    fixed = set(fixed)
    result = {'checked': 0, 'same': 0, 'allowed': 0,
              'missing': {}, 'ref': {}, 'field': {}, 'error': {}, 'exempted': {}}
    per_group = collections.Counter()

    def record(tag, kind, detail):
        if tag in allowed:
            result['allowed'] += 1
            return
        result[kind][tag] = detail
        per_group[(group_of(tag), kind)] += 1

    for tag in sorted(src):
        if group_of(tag) not in groups:
            continue
        result['checked'] += 1
        other = tag
        if tag not in conv:
            other = renamed_scenario(tag, src, conv)
            if other is None:
                record(tag, 'missing', [])
                continue
        try:
            field, ref, exempted = diff_dumps(checked_dump(dump, src_root, tag),
                                              checked_dump(dump, conv_root, other),
                                              repaired=tag in fixed)
        except DumpError as e:
            result['error'][tag] = [str(e)]
            per_group[(group_of(tag), 'error')] += 1
            continue
        if exempted:
            result['exempted'][tag] = exempted
        if not ref and not field:
            result['same'] += 1
        elif tag in allowed:
            result['allowed'] += 1
        else:
            if ref:
                result['ref'][tag] = ref[:6]
                per_group[(group_of(tag), 'ref')] += 1
            if field:
                result['field'][tag] = field[:6]
                per_group[(group_of(tag), 'field')] += 1
    result['per_group'] = {'%s/%s' % k: v for k, v in sorted(per_group.items())}
    result['failed'] = len(set().union(*(set(result[k]) for k in ('missing', 'ref', 'field', 'error'))))
    return result


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('source_tags')
    ap.add_argument('converted_tags')
    ap.add_argument('--invader-edit', default='invader-edit')
    ap.add_argument('--allow', help='file of path.group lines kept stock on purpose')
    ap.add_argument('--fixed', help='file of path.group lines whose invalid indices were repaired')
    ap.add_argument('--groups', help='comma list replacing the default groups')
    ap.add_argument('--json', help='write the full result here')
    args = ap.parse_args(argv)
    allowed = []
    if args.allow and os.path.exists(args.allow):
        allowed = [x.strip() for x in open(args.allow) if x.strip()]
    fixed = []
    if args.fixed and os.path.exists(args.fixed):
        fixed = [x.strip() for x in open(args.fixed) if x.strip()]
    groups = tuple(args.groups.split(',')) if args.groups else GAMEPLAY_GROUPS
    res = compare_trees(
        args.source_tags, args.converted_tags,
        lambda root, tag: dump_with_invader(args.invader_edit, root, tag),
        groups, allowed, fixed)
    if args.json:
        with open(args.json, 'w') as f:
            json.dump(res, f, indent=1)
    print('checked %d  same %d  allowed %d  missing %d  ref %d  field %d  error %d' % (
        res['checked'], res['same'], res['allowed'], len(res['missing']),
        len(res['ref']), len(res['field']), len(res['error'])))
    for k, v in res['per_group'].items():
        print('  %-36s %d' % (k, v))
    for tag, lines in sorted(res['exempted'].items()):
        for line in lines:
            print('  exempted (repaired index) %s: %s' % (tag, line))
    return 1 if res['failed'] else 0


if __name__ == '__main__':
    sys.exit(main())
