"""The MCC HUD layout fingerprint (port/linux/game/mcc_hud_layout.inl) walks each HUD's bitmap sequences and sprites
within one budget a map (MCC_HUD_MAX_VISITS, 4,000,000): many sequences sharing one large sprite list are refused
as an unknown layout (the MCC frame kept) instead of walked without end; an ordinary bitmap is fingerprinted."""
from pathlib import Path
import re
import shutil
import subprocess

import pytest
from harness import function

ROOT = Path(__file__).resolve().parents[1]
HOOKS = (r"    if \(\(mcc_hud_layout_work\+=count\)>4000000u\) return 0; /\* AE hook \*/\n",
         r"        if \(\(mcc_hud_layout_work\+=sprites\)>4000000u\) return 0; /\* AE hook \*/\n")

MAIN = r'''
static void word(unsigned at,uint32_t value) {memcpy(arena+at,&value,4);}
int main(int argc,char **argv) {
    struct mcc_runtime r={arena,{2},sizeof(arena),arena};
    uint32_t entry[8]={'wphi',0,0,0,0,256,0,0};
    unsigned p=256,e=1024,b=2048,n,sequences,sprites,at;
    uint64_t hash;
    if(argc!=4)return 1;
    sequences=(unsigned)atoi(argv[2]);sprites=(unsigned)atoi(argv[3]);
    /* one weapon HUD element naming one bitmap, as tools/test_mcc_hud_layout.py's */
    word(p+0x60,1);word(p+0x64,e);
    word(e+0x24+0x30,0xe0000001);
    word(32,'bitm');word(32+12,0xe0000001);word(32+16,4000);word(32+20,b);
    memcpy(arena+4000,"test\\meter",11);
    word(b+0x60,1);word(b+0x64,3000);
    /* its sequences, every one naming the same sprites */
    at=1<<16;
    word(b+0x54,sequences);word(b+0x58,at);
    for(n=0;n<sequences;n++){word(at+n*0x40+0x34,sprites);word(at+n*0x40+0x38,8192);}
    mcc_hud_layout_work=0;
    hash=mcc_hud_layout_visit(&r,entry);
    printf("%s %llu\n",hash ? "hashed" : "unknown",(unsigned long long)mcc_hud_layout_work);
    return 0;
}
'''


def tool(tmp_path, source_text):
    compiler = shutil.which("clang") or pytest.skip("clang is needed")
    source = (ROOT / "port/linux/game/mcc_hud.c").read_text()
    code = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NUMBEROF(x) (sizeof(x)/sizeof((x)[0]))
struct bitmap_group {short type,format,usage;unsigned short flags;char rest[100];};
struct mcc_runtime {unsigned char *tag_index;struct {uint32_t tag_count;} report;unsigned used;unsigned char *tags;};
static unsigned char arena[1<<21];
static void *mcc_runtime_pointer(struct mcc_runtime *r,uint32_t p,uint32_t bytes) {
    (void)r;return p<=sizeof(arena)&&bytes<=sizeof(arena)-p ? arena+p : NULL;
}
''' + function(source, "mcc_hud_word") + '\n' + function(source, "mcc_hud_block") + '\n' + source_text + MAIN
    path = tmp_path / "budget.c"
    path.write_text(code)
    binary = tmp_path / "budget"
    result = subprocess.run([compiler, "-m32", "-O1", "-std=gnu99", "-Wall", "-Werror", "-Wno-multichar",
                             "-Wno-unused-function", str(path), "-o", str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    return binary


def visit(binary, sequences, sprites):
    out = subprocess.run([str(binary), "x", str(sequences), str(sprites)], capture_output=True, text=True, timeout=60)
    assert out.returncode == 0, out.stderr
    kind, work = out.stdout.split()
    return kind, int(work)


def layout_text():
    return (ROOT / "port/linux/game/mcc_hud_layout.inl").read_text()


def test_ordinary_bitmap_hashed(tmp_path):
    assert visit(tool(tmp_path, layout_text()), 4, 3)[0] == "hashed"


def test_shared_sprites_stop_at_the_budget(tmp_path):
    kind, work = visit(tool(tmp_path, layout_text()), 20000, 300)
    assert kind == "unknown" and work <= 4000000 + 300, (kind, work)


def test_negative_control_without_the_budget(tmp_path):
    text = layout_text()
    for hook in HOOKS:
        text, count = re.subn(hook, "", text)
        assert count
    kind, work = visit(tool(tmp_path, text), 20000, 300)
    assert kind == "hashed" and work == 0, (kind, work)
