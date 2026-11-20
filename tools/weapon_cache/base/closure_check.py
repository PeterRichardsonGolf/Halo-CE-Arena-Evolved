"""base layer closure check (local maps, invader-extract): which names.tsv entries exist in which maps. usage: closure_check.py names.tsv map...; paths: the environment AE_INVADER_EXTRACT (invader-extract) and AE_MAPS_DIR (the maps)"""
import os, subprocess, sys, tempfile, shutil
EXT={"antr":"model_animations","bitm":"bitmap","coll":"model_collision_geometry","cont":"contrail","deca":"decal","effe":"effect","eqip":"equipment","foot":"material_effects","jpt!":"damage_effect","ligh":"light","mgs2":"light_volume","part":"particle","pphy":"point_physics","proj":"projectile","schi":"shader_transparent_chicago","snd!":"sound","soso":"shader_model","wphi":"weapon_hud_interface"}
I=os.environ["AE_INVADER_EXTRACT"]
M=os.environ["AE_MAPS_DIR"]
names=[l.rstrip("\n").split("\t") for l in open(sys.argv[1]) if not l.startswith("#")]
maps=sys.argv[2:]
res={}
for m in maps:
    d=tempfile.mkdtemp(dir=os.getcwd())
    for g,n,c in names:
        p=n.replace("\\","/")+"."+EXT[g]
        out=subprocess.run([I,"-G","-m",M,"-t",d,"-s",p,f"{M}/{m}.map"],capture_output=True,text=True).stdout
        res[(m,g,n)]= "Extracted "+p in out
    shutil.rmtree(d)
    print(m, sum(1 for k,v in res.items() if k[0]==m and v), "of", len(names), flush=True)
with open("closure.tsv","w") as f:
    f.write("group\tname\t"+"\t".join(maps)+"\n")
    for g,n,c in names:
        f.write(f"{g}\t{n}\t"+"\t".join("1" if res[(m,g,n)] else "0" for m in maps)+"\n")
