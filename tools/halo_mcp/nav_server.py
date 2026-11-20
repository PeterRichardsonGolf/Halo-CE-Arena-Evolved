"""Bot navigation tools as an MCP server (stdio): place vehicle routes and keepouts from a map's geometry dump.

Run:  python -m halo_mcp.nav_server   (run from the tools/ folder; needs the `mcp` package)
Environment: HALO_NAV_GEO (folder of <file>.json dumps), HALO_NAV_OUT (folder route files are written to).
"""
import json
from mcp.server.fastmcp import FastMCP

from . import core, nav

mcp = FastMCP('halo-nav')


def _wrap(fn, *a, **k):
    try:
        r = fn(*a, **k)
    except core.ToolError as e:
        raise ValueError(str(e))
    return core._clip(r if isinstance(r, str) else json.dumps(r, indent=1))


@mcp.tool()
def nav_maps() -> str:
    """The geometry dumps found in HALO_NAV_GEO: file, level, hash, surface count."""
    return _wrap(nav.nav_maps)


@mcp.tool()
def nav_points(file: str) -> str:
    """Vehicles, flags, items, spawns and bounds of a map's dump. file = map file name without .map."""
    return _wrap(nav.nav_points, file)


@mcp.tool()
def nav_ground(file: str, x: float, y: float) -> str:
    """Every floor at x y: z, surface, walk flag, piece, slope in degrees, material. Highest first."""
    return _wrap(nav.nav_ground, file, x, y)


@mcp.tool()
def nav_clear(file: str, a: list[float], b: list[float], height: float = 1.0, radius: float = 0.0) -> str:
    """First solid between points a and b ([x, y, z] each) for a body: the segment is raised by height; radius
    widens it (three parallel segments). Returns the hit point and surface or object, or clear."""
    return _wrap(nav.nav_clear, file, a, b, height, radius)


@mcp.tool()
def nav_corridor(file: str, a: list[float], b: list[float], vehicle: str = 'warthog') -> str:
    """Search a drivable path from a to b for a vehicle (warthog ghost scorpion any). Returns simplified route
    points with speed and turn values, and ready-to-paste point lines."""
    return _wrap(nav.nav_corridor, file, a, b, vehicle)


@mcp.tool()
def nav_keepouts(file: str, vehicle: str = 'warthog', near: float = 3.0) -> str:
    """Keepout circles for objects and steep surfaces within `near` units of drivable ground."""
    return _wrap(nav.nav_keepouts, file, vehicle, near)


@mcp.tool()
def nav_validate(file: str, text: str) -> str:
    """Check a route file text against the map's dump. Returns ok, errors and warnings with line numbers."""
    return _wrap(nav.nav_validate, file, text)


@mcp.tool()
def nav_write(file: str, text: str) -> str:
    """Validate, then write <HALO_NAV_OUT>/<file>.txt. Refused when the text has errors."""
    return _wrap(nav.nav_write, file, text)


if __name__ == '__main__':
    mcp.run()
