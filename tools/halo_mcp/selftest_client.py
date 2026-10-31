"""Stdio client that calls the tools once and checks the results (needs the `mcp` package, Invader and a map).

Run from the tools/ folder:
    HALO_MCP_TEST_MAP=/path/to/a/xbox/bloodgulch.map python halo_mcp/selftest_client.py

HALO_MCP_TEST_MAP   an Xbox-format cache file to read (required; the script exits 0 with a note when it is not set)
HALO_MCP_TEST_WORK  a folder under HALO_MCP_WRITE_ROOTS for the extraction (default ~/halo-test/mcp-out/selftest)
The offline checks of the path policy and argument checks are in tools/test_halo_mcp.py (no map or Invader needed).
"""
import asyncio
import os
import sys
from pathlib import Path

MAP = os.environ.get('HALO_MCP_TEST_MAP', '')
WORK = os.environ.get('HALO_MCP_TEST_WORK', str(Path.home() / 'halo-test' / 'mcp-out' / 'selftest'))
EXTRACT = WORK + '/extract'
PISTOL = 'weapons/pistol/pistol'


def calls():
    """(tool, arguments, expect_error)"""
    return [
        ('map_info', {'map': MAP}, False),
        ('list_tags', {'map': MAP, 'group': 'weapon'}, False),
        ('weapon_numbers', {'map': MAP, 'weapon': PISTOL}, False),
        ('extract_tags', {'map': MAP, 'search': ['weapons\\pistol\\*'], 'out_dir': EXTRACT}, False),
        ('list_tags', {'tags_dir': EXTRACT, 'limit': 5}, False),
        ('tag_info', {'tags_dir': EXTRACT, 'tag': PISTOL + '.weapon', 'key': 'triggers[0].maximum_rate_of_fire'}, False),
        ('tag_info', {'tags_dir': EXTRACT, 'tag': PISTOL + '.weapon'}, False),
        ('compare_tags', {'a_tags': EXTRACT, 'b_tags': EXTRACT, 'group': 'weapon'}, False),
        # policy: these must be refused
        ('list_tags', {'tags_dir': '/etc'}, True),
        ('extract_tags', {'map': MAP, 'search': ['*'], 'out_dir': '/etc/halo-mcp-x'}, True),
        ('tag_info', {'tags_dir': EXTRACT, 'tag': '../../../etc/passwd'}, True),
        ('weapon_numbers', {'map': MAP, 'weapon': 'weapons/nope/nope'}, True),
    ]


async def main():
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    params = StdioServerParameters(command=sys.executable, args=['-m', 'halo_mcp.server'])
    failures = []
    async with stdio_client(params) as (r, w):
        async with ClientSession(r, w) as s:
            await s.initialize()
            tools = await s.list_tools()
            print('TOOLS:', [t.name for t in tools.tools])
            for name, args, expect_error in calls():
                res = await s.call_tool(name, args)
                text = '\n'.join(c.text for c in res.content if hasattr(c, 'text'))
                ok = bool(res.isError) == expect_error
                print('%s %s %s (isError=%s)\n%s\n' % ('ok  ' if ok else 'FAIL', name, args, res.isError, text[:600]))
                if not ok:
                    failures.append(name)
    if failures:
        print('FAILED:', failures)
        return 1
    print('all calls behaved as expected')
    return 0


if __name__ == '__main__':
    if not MAP:
        print('set HALO_MCP_TEST_MAP to an Xbox cache file; nothing run')
        sys.exit(0)
    sys.exit(asyncio.run(main()))
