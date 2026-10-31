"""Tool logic for the Halo MCP server: path policy and thin wrappers over Invader.

Invader (GPL-3) is only ever run as a separate program (subprocess) and its text
output is parsed here. Nothing of it is imported, linked or copied.
"""
import os
import re
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path

HOME = Path(os.path.expanduser('~'))
INVADER_DIR = Path(os.environ.get('HALO_MCP_INVADER', HOME / 'tools/invader-0.44.0/build'))
SRC_DIR = Path(os.environ.get('HALO_MCP_SRC', HOME / 'ae/source'))
# Environment (all optional; the defaults are folders under the home directory):
#   HALO_MCP_INVADER      folder holding the invader-* programs   (default ~/tools/invader-0.44.0/build)
#   HALO_MCP_SRC          the engine's source/ folder (struct layouts for weapon_numbers)   (default ~/ae/source)
#   HALO_MCP_ROOTS        ':'-separated folders the tools may read   (default ~/halo-test:~/halo-data:~/tools)
#   HALO_MCP_WRITE_ROOTS  ':'-separated folders the tools may write (default ~/halo-test/mcp-out)
#   HALO_MCP_TIMEOUT      seconds a single Invader call may run   (default 900)
# Readable roots; writes are allowed only below WRITE_ROOTS.
READ_ROOTS = [Path(p).resolve() for p in
              os.environ.get('HALO_MCP_ROOTS', ':'.join(str(HOME / d) for d in ('halo-test', 'halo-data', 'tools'))).split(':') if p]
WRITE_ROOTS = [Path(p).resolve() for p in
               os.environ.get('HALO_MCP_WRITE_ROOTS', str(HOME / 'halo-test' / 'mcp-out')).split(':') if p]
TIMEOUT = int(os.environ.get('HALO_MCP_TIMEOUT', '900'))
MAX_CHARS = 60000
MAX_MAP_BYTES = 512 * 1024 * 1024  # a decompressed Xbox cache file is far smaller
ENGINES = ('xbox-2276', 'pc-custom', 'pc-retail', 'pc-demo', 'native')


class ToolError(Exception):
    pass


def _inside(p, roots):
    return any(p == r or r in p.parents for r in roots)


def safe_path(p, write=False, must_exist=True):
    """Resolve p (symlinks too) and require it to be under an allowed root."""
    if not isinstance(p, str) or not p or '\0' in p:
        raise ToolError('path must be a non-empty string')
    rp = Path(p).expanduser().resolve()
    roots = WRITE_ROOTS if write else READ_ROOTS
    if not roots:
        raise ToolError('no %s roots are configured (HALO_MCP_%sROOTS)' % ('write' if write else 'read', 'WRITE_' if write else ''))
    if not _inside(rp, roots):
        raise ToolError('path %s is outside the allowed %s roots: %s' %
                        (rp, 'write' if write else 'read', ', '.join(map(str, roots))))
    if must_exist and not rp.exists():
        raise ToolError('path does not exist: %s' % rp)
    return rp


def safe_tag(t):
    """A tag path relative to a tags dir, e.g. weapons/pistol/pistol.weapon."""
    if not isinstance(t, str) or not t or t.startswith('-') or t.startswith('/') or '..' in t.replace('\\', '/').split('/') or '\0' in t:
        raise ToolError('bad tag path %r (relative, no .., must not start with -)' % (t,))
    return t.replace('\\', '/')


def safe_word(s, what):
    if not isinstance(s, str) or not s or s.startswith('-') or '\0' in s:
        raise ToolError('bad %s %r' % (what, s))
    return s


def run(prog, args, cwd=None):
    exe = INVADER_DIR / prog
    if not exe.is_file():
        raise ToolError('%s not found in %s (set HALO_MCP_INVADER)' % (prog, INVADER_DIR))
    cmd = ['nice', '-n', '19', str(exe)] + [str(a) for a in args]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, errors='replace', timeout=TIMEOUT,
                           cwd=str(cwd) if cwd else None, stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        raise ToolError('%s timed out after %d s' % (prog, TIMEOUT))
    return r.returncode, r.stdout, r.stderr


def _clip(s):
    return s if len(s) <= MAX_CHARS else s[:MAX_CHARS] + '\n... [truncated, %d more characters]' % (len(s) - MAX_CHARS)


def _need_ok(prog, rc, out, err):
    if rc != 0:
        raise ToolError('%s failed (exit %d): %s' % (prog, rc, _clip((err or out).strip())[:4000]))


# ---- tools ----

def map_info(map):
    m = safe_path(map)
    rc, out, err = run('invader-info', [m])
    _need_ok('invader-info', rc, out, err)
    return out.strip()


def list_tags(map=None, tags_dir=None, group=None, contains=None, limit=500):
    """Tags of a cache file (invader-index) or of a tags directory (file walk)."""
    if bool(map) == bool(tags_dir):
        raise ToolError('give exactly one of map or tags_dir')
    rows = []
    if map:
        m = safe_path(map)
        if not WRITE_ROOTS:
            raise ToolError('no write root is configured (HALO_MCP_WRITE_ROOTS): the index needs a scratch file')
        WRITE_ROOTS[0].mkdir(parents=True, exist_ok=True)
        fd, name = tempfile.mkstemp(prefix='.halo_mcp_index_', suffix='.txt', dir=str(WRITE_ROOTS[0]))
        os.close(fd)
        tmp = Path(name)
        rc, out, err = run('invader-index', [m, tmp])
        _need_ok('invader-index', rc, out, err)
        try:
            rows = [l.rstrip('\n') for l in tmp.read_text(errors='replace').splitlines() if l.strip()]
        finally:
            tmp.unlink(missing_ok=True)
        rows = [l.replace('\\', '/') for l in rows]
    else:
        td = safe_path(tags_dir)
        rows = sorted(str(p.relative_to(td)) for p in td.rglob('*') if p.is_file())
    if group:
        rows = [r for r in rows if r.endswith('.' + safe_word(group, 'group'))]
    if contains:
        rows = [r for r in rows if contains.lower() in r.lower()]
    total = len(rows)
    limit = max(1, min(int(limit), 5000))
    return {'total': total, 'shown': min(total, limit), 'tags': rows[:limit]}


def tag_info(tags_dir, tag, key=None, list_values=False):
    """Read a tag with invader-edit: one key, or the whole field list (with values)."""
    td = safe_path(tags_dir)
    t = safe_tag(tag)
    tp = (td / t).resolve()  # a symlink inside the tags folder must not lead out of it
    if not _inside(tp, [td]) or not tp.is_file():
        raise ToolError('no such tag in %s: %s' % (td, t))
    if key:
        args = ['-G', safe_word(key, 'key')]
    else:
        args = ['-L' if list_values else '-l']
    rc, out, err = run('invader-edit', ['-t', td] + args + [t])
    if rc != 0 and key:
        raise ToolError('invader-edit: %s (keys look like triggers[0].maximum_rate_of_fire; call without key to see field names)'
                        % (err or out).strip()[:500])
    _need_ok('invader-edit', rc, out, err)
    return _clip(out.strip())


def extract_tags(map, search, out_dir, recursive=False, overwrite=False):
    m = safe_path(map)
    od = safe_path(out_dir, write=True, must_exist=False)
    od.mkdir(parents=True, exist_ok=True)
    queries = [search] if isinstance(search, str) else list(search)
    if not queries:
        raise ToolError('search needs at least one pattern, e.g. weapons/pistol/*')
    args = ['-t', od]
    for q in queries:
        args += ['-s', safe_word(q, 'search pattern')]
    if recursive:
        args.append('-r')
    if overwrite:
        args.append('-O')
    before = {p for p in od.rglob('*')}
    rc, out, err = run('invader-extract', args + [m])
    _need_ok('invader-extract', rc, out, err)
    # tag names come from the map: nothing may have been written outside the folder
    stray = [str(p) for p in od.rglob('*') if p not in before and not _inside(p.resolve(), [od])]
    if stray:
        raise ToolError('extraction wrote outside %s: %s' % (od, ', '.join(stray[:5])))
    n = sum(1 for p in od.rglob('*') if p.is_file())
    return {'output': _clip(out.strip()), 'files_in_out_dir': n}


def build_map(scenario, tags_dirs, out, engine='xbox-2276', maps_dir=None, extend_limits=False):
    """invader-build of a scenario tag (path relative to the first tags dir, no extension)."""
    if engine not in ENGINES:
        raise ToolError('engine must be one of ' + ', '.join(ENGINES))
    tds = [tags_dirs] if isinstance(tags_dirs, str) else list(tags_dirs)
    if not tds:
        raise ToolError('tags_dirs is empty')
    args = ['-g', engine, '-q']
    for t in tds:
        args += ['-t', safe_path(t)]
    o = safe_path(out, write=True, must_exist=False)
    o.parent.mkdir(parents=True, exist_ok=True)
    args += ['-o', o]
    if extend_limits:
        args.append('-E')  # beyond the engine's file size limit: the map may not load without a mod
    if maps_dir:
        args += ['-m', safe_path(maps_dir)]
    sc = safe_tag(scenario)
    if sc.endswith('.scenario'):
        sc = sc[:-len('.scenario')]
    rc, out_, err = run('invader-build', args + [sc])
    text = (out_ + '\n' + err).strip()
    if rc != 0:
        raise ToolError('invader-build failed (exit %d): %s' % (rc, _clip(text)[:4000]))
    return {'map': str(o), 'bytes': o.stat().st_size if o.exists() else None, 'log': _clip(text)}


def compare_tags(a_tags, b_tags, group=None, show='mismatched', functional=False, verbose=False):
    """invader-compare of two tags directories (same relative paths are compared)."""
    # each -I opens a new input; the -t after it belongs to that input
    args = ['-I', '-t', safe_path(a_tags), '-I', '-t', safe_path(b_tags)]
    if show not in ('all', 'matched', 'mismatched'):
        raise ToolError('show must be all, matched or mismatched')
    args += ['-s', show]
    if group:
        args += ['-c', safe_word(group, 'group')]
    if functional:
        args.append('-f')
    elif verbose:
        args.append('-v')
    rc, out, err = run('invader-compare', args)
    text = (out + '\n' + err).strip()
    if rc not in (0, 1) and not out.strip():
        raise ToolError('invader-compare failed (exit %d): %s' % (rc, _clip(text)[:4000]))
    return {'exit': rc, 'output': _clip(text)}


_layouts = None


def weapon_numbers(map, weapon):
    """Weapon / projectile / damage numbers read by our own reader (Xbox maps only)."""
    global _layouts
    from . import mapread
    m = safe_path(map)
    if _layouts is None:
        if not (SRC_DIR / 'items/weapon_definitions.h').is_file():
            raise ToolError('engine headers not found at %s (set HALO_MCP_SRC)' % SRC_DIR)
        _layouts = mapread.Layouts(str(SRC_DIR))
    try:
        cm = mapread.CacheMap(str(m), max_bytes=MAX_MAP_BYTES)
    except Exception as e:
        raise ToolError('cannot read %s as an Xbox cache file: %s' % (m, e))
    w = weapon.replace('/', '\\')
    if w.endswith('.weapon'):
        w = w[:-7]
    try:
        return mapread.weapon_numbers(cm, _layouts, w)
    except KeyError as e:
        if not str(e.args[0]).startswith('weapon tag not in map'):
            raise ToolError('layout problem (the engine headers may have changed): %s' % (e.args[0],))
        names = sorted(v[1] for v in cm.tags.values() if v[0] == 'weap')
        raise ToolError('%s. Weapons in this map: %s' % (e.args[0], ', '.join(names)))
    except (struct.error, ValueError, IndexError, UnicodeDecodeError) as e:
        raise ToolError('%s is not a readable Xbox cache file for this weapon: %s' % (m, e))
