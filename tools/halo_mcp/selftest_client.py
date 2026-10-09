"""Stdio client that calls every tool once; prints the calls and outputs. Run from the tools/ folder:
python halo_mcp/selftest_client.py"""
import asyncio
import json
import sys
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

H = '/home/builder/halo-test'
MAP = '/home/builder/halo-data/maps/bloodgulch.map'
CALLS = [
    ('map_info', {'map': MAP}),
    ('list_tags', {'map': MAP, 'group': 'weapon'}),
    ('weapon_numbers', {'map': MAP, 'weapon': 'weapons/pistol/pistol'}),
    ('extract_tags', {'map': MAP, 'search': ['weapons\\pistol\\*'], 'out_dir': H + '/mcp-spike/extract'}),
    ('list_tags', {'tags_dir': H + '/mcp-spike/extract', 'limit': 5}),
    ('tag_info', {'tags_dir': H + '/mcp-spike/extract', 'tag': 'weapons/pistol/pistol.weapon',
                  'key': 'triggers[0].maximum_rate_of_fire'}),
    ('tag_info', {'tags_dir': H + '/mcp-spike/extract', 'tag': 'weapons/pistol/pistol.weapon'}),
    ('compare_tags', {'a_tags': H + '/mcp-spike/extract', 'b_tags': H + '/wp2/stock-tags', 'group': 'weapon'}),
    ('compare_tags', {'a_tags': H + '/wp2/fix2', 'b_tags': H + '/wp2/cmt-tags', 'group': 'shader_model', 'show': 'all'}),
    ('build_map', {'scenario': 'levels/test/bloodgulch/bloodgulch', 'tags_dirs': [H + '/wp2/stock-tags'],
                   'out': H + '/mcp-spike/bloodgulch_rebuilt.map'}),
    ('build_map', {'scenario': 'ae_pack/donor/donor', 'tags_dirs': [H + '/wp2/fix2', H + '/wp2/cmt-tags', H + '/wp2/stock-tags'],
                   'out': H + '/mcp-spike/donor_test.map', 'extend_limits': True}),
    ('map_info', {'map': H + '/mcp-spike/donor_test.map'}),
    # policy: these must be refused
    ('list_tags', {'tags_dir': '/etc'}),
    ('extract_tags', {'map': MAP, 'search': ['*'], 'out_dir': '/home/builder/halo-data/x'}),
    ('tag_info', {'tags_dir': H + '/mcp-spike/extract', 'tag': '../../../etc/passwd'}),
    ('weapon_numbers', {'map': MAP, 'weapon': 'weapons/nope/nope'}),
]


async def main():
    params = StdioServerParameters(command=sys.executable, args=['-m', 'halo_mcp.server'])
    async with stdio_client(params) as (r, w):
        async with ClientSession(r, w) as s:
            await s.initialize()
            tools = await s.list_tools()
            print('TOOLS:', [t.name for t in tools.tools])
            for name, args in CALLS:
                res = await s.call_tool(name, args)
                text = '\n'.join(c.text for c in res.content if hasattr(c, 'text'))
                print('\n### %s %s\n(isError=%s)\n%s' % (name, json.dumps(args), res.isError, text[:1800]))

asyncio.run(main())
