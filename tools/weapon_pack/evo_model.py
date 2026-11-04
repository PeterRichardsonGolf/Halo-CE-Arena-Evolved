"""The DMR Evo rifle's model from CMT's DMR (our own work, from CMT's gbxmodel): the scope taken off (the welded
pieces of the body part that form it), the barrel and the under tube 0.030 wu shorter (23 % of the barrel in front of
the receiver; the flash and smoke markers moved back with it), and on the top rail a holo sight: CuiGames' H02 Holo
(h02_sight.py: read from the kit's glb, made smaller, drawn with the rifle's own texels) or, with no glb given, eight
plain boxes; and a glass quad in the sight's window, whose shader is ours (evo_holo). The node skeleton is untouched,
so CMT's first-person animations still drive it. Reads the tag(s), writes the tag.

Needs numpy and Reclaimer (its mod2 definition): the art stage runs it (make_pack.py --derive-python).
Usage: evo_model.py <CMT's dmr.gbxmodel> [<the H02 Holo glb>] <out gbxmodel>"""
import os
import sys

import numpy as np
from reclaimer.hek.defs.mod2 import mod2_def

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # (run isolated: h02_sight beside it)
SRC, OUT = sys.argv[1], sys.argv[-1]
SIGHT = sys.argv[2] if len(sys.argv) > 3 else None
D = 0.030                         # how much shorter the barrel is, wu
A, B = 0.2145, 0.2595             # the barrel's span along x, wu
# welded-vertex components of part 0 that make the scope (found by connectivity, see components())
SCOPE = {9, 47, 43, 7, 11, 14, 30, 32, 42, 26, 6}
tag = mod2_def.build(filepath=SRC)
d = tag.data.tagdata
g = d.geometries.STEPTREE[0]
parts = g.parts.STEPTREE


def part_geometry(p):
    verts = [[float(x) for x in v[0:18]] for v in p.uncompressed_vertices.STEPTREE]
    flat = []
    for e in p.triangles.STEPTREE:
        flat += [e[0], e[1], e[2]]
    tris = []
    for i in range(len(flat) - 2):
        a, b, c = flat[i:i + 3]
        if -1 in (a, b, c) or a == b or b == c or a == c:
            continue
        tris.append([a, b, c] if i % 2 == 0 else [b, a, c])
    return verts, tris


def components(V, T):
    """the vertices' connected components (vertices at the same place, to 5 decimals, are one), numbered by first sight"""
    key = {}
    wid = np.zeros(len(V), int)
    for i, v in enumerate(V):
        wid[i] = key.setdefault(tuple(np.round(v[0:3], 5)), len(key))
    par = list(range(len(key)))

    def f(a):
        while par[a] != a:
            par[a] = par[par[a]]
            a = par[a]
        return a
    for a, b, c in T:
        for x, y in ((a, b), (b, c)):
            ra, rb = f(wid[x]), f(wid[y])
            if ra != rb:
                par[ra] = rb
    comp = np.array([f(wid[i]) for i in range(len(V))])
    ids = {}
    for c in comp:
        ids.setdefault(c, len(ids))
    return np.array([ids[c] for c in comp])


verts0, tris0 = part_geometry(parts[0])
V = np.array(verts0)
T = np.array(tris0)
comp = components(V, T)

def tri_orient(verts,tris):
    s=0
    for a,b,c in tris[:400]:
        A_,B_,C_=verts[a],verts[b],verts[c]
        u=[B_[i]-A_[i] for i in range(3)]; v=[C_[i]-A_[i] for i in range(3)]
        cr=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
        s+= 1 if sum(cr[i]*A_[3+i] for i in range(3))>0 else -1
    return s
def strip(tris):
    seq=[]
    for a,b,c in tris:
        if not seq: seq=[a,b,c]; continue
        L=seq[-1]
        cand=[L,a,a,b,c]
        if (len(seq)+2)%2: cand=[L,L,a,a,b,c]
        seq+=cand
    return seq
def decode(seq):
    out=[]
    for i in range(len(seq)-2):
        a,b,c=seq[i:i+3]
        if a==b or b==c or a==c: continue
        out.append((a,b,c) if i%2==0 else (b,a,c))
    return out
def canon(t3):
    k=t3.index(min(t3)); return tuple(t3[k:]+t3[:k])
def fill(p,verts,tris,node=0):
    seq=strip(tris)
    assert sorted(map(canon,[list(x) for x in decode(seq)]))==sorted(map(canon,[list(x) for x in tris]))
    if len(seq)%3: seq+=[-1]*(3-len(seq)%3)
    del p.compressed_vertices.STEPTREE[:]
    cx=[sum(v[i] for v in verts)/len(verts) for i in range(3)]
    p.centroid_translation[:]=cx
    del p.uncompressed_vertices.STEPTREE[:]; del p.triangles.STEPTREE[:]
    p.uncompressed_vertices.STEPTREE.extend(len(verts))
    for vv,v in zip(p.uncompressed_vertices.STEPTREE,verts):
        vv[0:3]=v[0:3]; vv[3:6]=v[3:6]; vv[6:9]=v[6:9]; vv[9:12]=v[9:12]
        vv[12]=v[12]; vv[13]=v[13]; vv[14]=node; vv[15]=-1; vv[16]=1.0; vv[17]=0.0
    p.triangles.STEPTREE.extend(len(seq)//3)
    for k,e in enumerate(p.triangles.STEPTREE): e[0],e[1],e[2]=seq[3*k:3*k+3]

# ---- part 0: scope out, barrel shorter
keep=np.array([comp[t[0]] not in SCOPE for t in T]); T0=T[keep]
used=sorted(set(T0.flatten().tolist())); remap={o:i for i,o in enumerate(used)}
V0=V[used].copy(); T0=np.array([[remap[x] for x in tri] for tri in T0])
s=(B-A-D)/(B-A)
def fx(x):
    return np.where(x<=A,x,np.where(x<B,A+(x-A)*s,x-D))
V0[:,0]=fx(V0[:,0])
print('part0 verts',len(V),'->',len(V0),'tris',len(T),'->',len(T0),'xmax',V0[:,0].max())
# UV of a plain dark metal patch: the barrel's (component 4) uv centroid
UVP=(0.1697,0.0690)
verts0=[list(map(float,v)) for v in V0]
fill(parts[0],verts0,[tuple(map(int,x)) for x in T0])
# node of every vertex in part 0 kept: fill() sets node 0, restore from V0
for vv,v in zip(parts[0].uncompressed_vertices.STEPTREE,V0):
    vv[14]=int(v[14]); vv[15]=int(v[15]); vv[16]=float(v[16]); vv[17]=float(v[17])
# markers back
for m in d.regions.STEPTREE[0].permutations.STEPTREE[0].local_markers.STEPTREE:
    if m.name in ('primary trigger','smoke'): m.translation[0]=float(fx(np.array(m.translation[0])))
    print('marker',m.name,[round(x,4) for x in m.translation])

# ---- boxes
def box(x0,x1,y0,y1,z0,z1,uv):
    P=lambda x,y,z:(x,y,z)
    faces=[((x1,y0,z0),(x1,y1,z0),(x1,y1,z1),(x1,y0,z1),(1,0,0)),   # +x
           ((x0,y1,z0),(x0,y0,z0),(x0,y0,z1),(x0,y1,z1),(-1,0,0)),
           ((x1,y1,z0),(x0,y1,z0),(x0,y1,z1),(x1,y1,z1),(0,1,0)),
           ((x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1),(0,-1,0)),
           ((x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1),(0,0,1)),
           ((x0,y1,z0),(x1,y1,z0),(x1,y0,z0),(x0,y0,z0),(0,0,-1))]
    vs=[];ts=[]
    for a,b,c,d_,n in faces:
        i=len(vs); tg=np.array(b)-np.array(a); tg=tg/np.linalg.norm(tg); bn=np.cross(n,tg)
        for p in (a,b,c,d_): vs.append([p[0],p[1],p[2],n[0],n[1],n[2],bn[0],bn[1],bn[2],tg[0],tg[1],tg[2],uv[0],uv[1],0])
        ts+= [(i,i+1,i+2),(i,i+2,i+3)]
    return vs,ts
BOXES=[(0.030,0.052,-0.0066,0.0066,0.0495,0.0535),   # mount
       (0.033,0.050,-0.0055,0.0055,0.0535,0.0565),   # pedestal
       (0.0285,0.0505,-0.0066,0.0066,0.0565,0.0585), # window sill
       (0.0285,0.0505,0.0048,0.0066,0.0585,0.0695),  # left wall
       (0.0285,0.0505,-0.0066,-0.0048,0.0585,0.0695),# right wall
       (0.0285,0.0505,-0.0066,0.0066,0.0675,0.0695), # roof
       (0.0355,0.0435,-0.0030,0.0030,0.0695,0.0710), # cap
       (0.0485,0.0505,-0.0048,0.0048,0.0585,0.0605)] # front sill lip
allv=[];allt=[]
if SIGHT:
    import h02_sight
    allv,allt,(GX,y0,y1,z0,z1)=h02_sight.sight(SIGHT)
    GX-=0.0002                       # (just behind the kit's pane)
else:
    for b in BOXES:
        v,tt=box(*b,UVP); o=len(allv); allv+=v; allt+=[(a+o,b_+o,c+o) for a,b_,c in tt]
    GX=0.0290; y0,y1,z0,z1=-0.0048,0.0048,0.0585,0.0675
# the dot's picture square in the H02's window: v spans the window's height at the width's scale (the boxes' window is
# nearly square: the whole picture)
VS=(z1-z0)/(y1-y0) if SIGHT else 1.0; GV0,GV1=0.5-VS/2,0.5+VS/2
# orientation as CMT's (compare with part 0)
ref=tri_orient([list(map(float,v)) for v in V],[tuple(map(int,x)) for x in T[:400]])
mine=tri_orient(allv,allt)
if mine*ref<0: allt=[(a,c,b) for a,b,c in allt]
print('orient',ref,mine)
# ---- glass (part 2): a quad in the sight's window, both sides
gv=[];gt=[]
for n,xo in ((-1,0.0),(1,0.00005)):
    i=len(gv)
    corners=[(GX+xo,y1,z0,0,GV1),(GX+xo,y0,z0,1,GV1),(GX+xo,y0,z1,1,GV0),(GX+xo,y1,z1,0,GV0)] if n<0 else [(GX+xo,y0,z0,1,GV1),(GX+xo,y1,z0,0,GV1),(GX+xo,y1,z1,0,GV0),(GX+xo,y0,z1,1,GV0)]
    for c in corners: gv.append([c[0],c[1],c[2],float(n),0,0,0,float(n)*1,0,0,0,1,c[3],c[4],0])
    gt+= [(i,i+1,i+2),(i,i+2,i+3)]
for k in (0,1):
    ft=gt[2*k:2*k+2]
    if tri_orient(gv,ft)*ref<0: gt[2*k:2*k+2]=[(a,c,b) for a,b,c in ft]
fill(parts[2],[v for v in gv],gt)
parts[2].shader_index=2
# sight body = new part with the body shader
parts.append(); pb=parts[-1]; pb.shader_index=0
fill(pb,allv,allt)
# shader 2 = the holo glass (a chicago shader like the display ones)
sh=d.shaders.STEPTREE
sh[2].shader.filepath='cmt\\weapons\\_shared\\dmr\\shaders\\evo_holo'
sh[2].shader.tag_class=sh[4].shader.tag_class
print('shaders',[x.shader.filepath for x in sh])
print('parts',[(p.shader_index,len(p.uncompressed_vertices.STEPTREE)) for p in parts])
os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True)
tag.filepath=OUT
tag.serialize(temp=False,backup=False,calc_pointers=True,int_test=False)
print('written',OUT)
