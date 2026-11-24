"""Halo CE tag and map tools as an MCP server (stdio).

Run:  python -m halo_mcp.server   (run from the tools/ folder; needs the `mcp` package)
"""
import json
from mcp.server.fastmcp import FastMCP

from . import core

mcp = FastMCP('halo-ce-tools')


def _wrap(fn, *a, **k):
    try:
        r = fn(*a, **k)
    except core.ToolError as e:
        raise ValueError(str(e))
    return r if isinstance(r, str) else json.dumps(r, indent=1)


@mcp.tool()
def map_info(map: str) -> str:
    """Header summary of a cache file (build, engine, tag count, protection, CRC...). map = absolute path."""
    return _wrap(core.map_info, map)


@mcp.tool()
def list_tags(map: str = '', tags_dir: str = '', group: str = '', contains: str = '', limit: int = 500) -> str:
    """List tags of a cache file (map) OR of a tags directory (tags_dir); give one. Filter by group (file
    extension such as weapon, projectile, scenario) and/or a case-insensitive substring."""
    return _wrap(core.list_tags, map or None, tags_dir or None, group or None, contains or None, limit)


@mcp.tool()
def tag_info(tags_dir: str, tag: str, key: str = '', list_values: bool = False) -> str:
    """Read a tag's fields (invader-edit). tag = path relative to tags_dir with extension, e.g.
    weapons/pistol/pistol.weapon. With key: that field's value (e.g. triggers[0].projectile); otherwise the
    field list, with values if list_values."""
    return _wrap(core.tag_info, tags_dir, tag, key or None, list_values)


@mcp.tool()
def extract_tags(map: str, search: list[str], out_dir: str, recursive: bool = False, overwrite: bool = False) -> str:
    """Extract tags from a cache file into out_dir (must be under the write root). search = wildcard patterns
    such as weapons/pistol/*. Extraction from Xbox maps can be lossy for some groups."""
    return _wrap(core.extract_tags, map, search, out_dir, recursive, overwrite)


@mcp.tool()
def build_map(scenario: str, tags_dirs: list[str], out: str, engine: str = 'xbox-2276', maps_dir: str = '',
              extend_limits: bool = False) -> str:
    """Build a cache file from a scenario tag (path relative to the tags dirs, extension optional) with
    invader-build. Earlier dirs win over later ones. out must be under the write root. extend_limits lifts the
    engine's map size limit (the result may not load in the stock engine)."""
    return _wrap(core.build_map, scenario, tags_dirs, out, engine, maps_dir or None, extend_limits)


@mcp.tool()
def compare_tags(a_tags: str, b_tags: str, group: str = '', show: str = 'mismatched',
                 functional: bool = False, verbose: bool = False) -> str:
    """Compare two tags directories tag by tag (invader-compare). show = all | matched | mismatched."""
    return _wrap(core.compare_tags, a_tags, b_tags, group or None, show, functional, verbose)


@mcp.tool()
def weapon_numbers(map: str, weapon: str) -> str:
    """Weapon numbers read straight from an Xbox cache file by our own reader: magazines, triggers (rate of
    fire, error, projectiles per shot), the projectile and its impact damage effect (damage bounds, material
    modifiers). weapon = tag path such as weapons/pistol/pistol."""
    return _wrap(core.weapon_numbers, map, weapon)


if __name__ == '__main__':
    mcp.run()
