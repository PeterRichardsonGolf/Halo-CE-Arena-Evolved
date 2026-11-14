"""shared parts and materials for the bomb mockups"""
import os, sys; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine import *
TS = os.environ['AE_BOMB_TSCE_TAGS'].rstrip('/') + '/'    # CMT's tags (the build's input)
P_BATT = 'levels/b40/scenery/b40_battery/b40_battery'
P_BIGB = 'levels/b40/scenery/b40_bigbattery/b40_bigbattery'
P_HP = 'powerups/healthpack/healthpack'
P_AMMO = 'powerups/shotgun ammo/shotgun ammo'
P_CUBE = 'item collections/powerups/powerups'
P_FRAG = 'weapons/frag grenade/frag grenade'
P_PLAS = 'weapons/plasma grenade/plasma grenade'
P_BEAC = 'scenery/small beacon/small beacon'
P_SIG = 'levels/a10/devices/signal light/signal light'
P_CYB = 'characters/cyborg/cyborg'
P_TIMER = 'digsite/weapons/_shared/00_e3/bitmaps/h grenade timer'
P_DECAL = 'digsite/weapons/_shared/00_e3/bitmaps/h grenade decal'

def zmin(path, s=1.0):
    parts, _ = load_gbx(path)
    return min(p[1][:, 2].min() for p in parts) * s

def bbox(path):
    parts, _ = load_gbx(path)
    P = np.vstack([p[1] for p in parts])
    return P.min(0), P.max(0)
