# ogg2pcm.py <tagsdir>: rewrite Ogg Vorbis .sound tags (non-stock) to 16-bit PCM big-endian so Xbox builds take them
import struct,sys,os,subprocess,glob
MAXS=4194304
td_tmp=os.environ.get('CONVERT_TMP','.')
td_root='.'
def conv(path):
    b=open(path,'rb').read()
    H=64
    fmt=struct.unpack_from('>H',b,H+110)[0]
    if fmt!=3: return None
    nch=struct.unpack_from('>H',b,H+108)[0]+1
    npr=struct.unpack_from('>I',b,H+152)[0]
    pr_off=H+164
    prs=[]
    for i in range(npr):
        nperm=struct.unpack_from('>I',b,pr_off+i*72+60)[0]
        prs.append(nperm)
    perm_off=pr_off+72*npr
    nperms=sum(prs)
    d_off=perm_off+124*nperms
    # data blobs
    pos=d_off; perms=[]
    for i in range(nperms):
        po=perm_off+124*i
        sizes=[struct.unpack_from('>I',b,po+o)[0] for o in (64,84,104)]
        perms.append((po,sizes))
    blobs=[]
    for po,sz in perms:
        bl=[]
        for s in sz:
            bl.append(b[pos:pos+s]); pos+=s
        blobs.append(bl)
    if pos!=len(b): raise Exception('layout mismatch %d vs %d'%(pos,len(b)))
    ba=bytearray(b[:d_off]); out=b''
    srate=struct.unpack_from('>H',b,H+6)[0]  # 0=22050 1=44100
    rate=44100 if srate else 22050
    tmp=os.path.join(td_tmp,'ogg_%d.ogg'%os.getpid())
    def dec(ogg,r):
        with open(tmp,'wb') as f: f.write(ogg)
        p=subprocess.run(['ffmpeg','-v','error','-i',tmp,'-f','s16be','-acodec','pcm_s16be','-ar',str(r),'-ac',str(nch),'-'],capture_output=True)
        if p.returncode!=0 or not p.stdout:
            raise Exception('ffmpeg failed: '+p.stderr.decode('latin1').strip()[:200])
        return p.stdout
    # one sample rate for every permutation: all at the tag's rate, or all at 22050 when any is over the size cap
    try:
        while True:
            newblobs=[dec(blobs[k][0],rate) for k in range(len(perms))]
            if rate==44100 and any(len(x)>MAXS for x in newblobs):
                rate=22050; continue
            break
    finally:
        if os.path.exists(tmp): os.remove(tmp)
    for k in range(len(newblobs)):
        if len(newblobs[k])>MAXS:
            print('CUT',os.path.relpath(path,td_root),'permutation',k,'from',len(newblobs[k]),'to',MAXS-MAXS%(2*nch),'bytes')
            newblobs[k]=newblobs[k][:MAXS-MAXS%(2*nch)]
    for k,(po,sz) in enumerate(perms):
        struct.pack_into('>H',ba,po+40,0)
        struct.pack_into('>I',ba,po+64,len(newblobs[k]))
        struct.pack_into('>I',ba,po+56,len(newblobs[k]))
    struct.pack_into('>H',ba,H+110,0)
    struct.pack_into('>H',ba,H+6,1 if rate==44100 else 0)
    for k in range(nperms): out+=newblobs[k]+blobs[k][1]+blobs[k][2]
    open(path,'wb').write(bytes(ba)+out)
    return len(b),len(ba)+len(out)
if __name__=='__main__':
    td=sys.argv[1]
    td_root=td
    failed=0
    for p in glob.glob(td+'/**/*.sound',recursive=True):
        try:
            r=conv(p)
            if r: print('converted',os.path.relpath(p,td),r)
        except Exception as e:
            print('FAIL',p,e); failed+=1
    sys.exit(1 if failed else 0)
