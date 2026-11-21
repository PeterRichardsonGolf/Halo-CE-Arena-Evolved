"""The game's zlib (source/memory/zlib), as the replay recorder uses it (deflate in 4 KB pieces, each synced, then
inflate): 1 MB goes through and comes back whole on the 64-bit builds' rewrite (tools/lp64_rewrite.py) and on the
32-bit build, at the recorder's level and the default. The old _tr_init found the second and third static tree
descriptions by 20-byte steps, where they are only on the 32-bit builds: the negative control puts that back and the
64-bit deflate fails."""
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
ZLIB = ROOT / "source" / "memory" / "zlib"
UNITS = ("adler32", "compress", "crc32", "deflate", "infblock", "infcodes", "inffast", "inflate", "inftrees", "infutil",
         "trees", "uncompr", "zutil")
FIXED = ("    s->d_desc.stat_desc = &data_00308b54[1]; /* AE hook */",
         "    s->bl_desc.stat_desc = &data_00308b54[2]; /* AE hook */")
OLD = ("    s->d_desc.stat_desc = (static_tree_desc *)((char *)data_00308b54 + 0x14);",
       "    s->bl_desc.stat_desc = (static_tree_desc *)((char *)data_00308b54 + 0x28);")

# (the few game functions the game's zlib calls)
CSERIES = r'''#include <stdio.h>
#include <stdlib.h>
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
static void display_assert(const char *m, const char *f, int l, int h) { fprintf(stderr, "%s %s:%d\n", m, f, l); if (h) abort(); }
static void *debug_malloc(unsigned long n, int z, const char *f, int l) { (void)f; (void)l; return z ? calloc(1, n) : malloc(n); }
static void debug_free(void *p, const char *f, int l) { (void)f; (void)l; free(p); }
'''

CHECK = r'''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"

int main(int argc, char **argv)
{
	int level = argc > 1 ? atoi(argv[1]) : 1;
	unsigned size = 1u << 20, i, seed = 12345;
	unsigned char *in = malloc(size), *out = malloc(size * 2), *back = malloc(size);
	z_stream d, f;
	unsigned long packed;

	/* replay-like: runs of small numbers, repeats, some noise */
	for (i = 0; i < size; i++) {
		seed = seed * 1103515245u + 12345u;
		in[i] = (i % 64 < 40) ? (unsigned char)(i / 64 % 7) : (unsigned char)(seed >> 24);
	}
	memset(&d, 0, sizeof(d));
	if (deflateInit(&d, level) != Z_OK) { printf("deflateInit failed\n"); return 2; }
	d.next_out = out; d.avail_out = size * 2;
	for (i = 0; i < size; i += 4096) {
		d.next_in = in + i; d.avail_in = 4096;
		if (deflate(&d, Z_SYNC_FLUSH) != Z_OK) { printf("deflate failed at %u\n", i); return 3; }
	}
	deflate(&d, Z_FINISH);
	packed = d.total_out;
	deflateEnd(&d);
	memset(&f, 0, sizeof(f));
	if (inflateInit(&f) != Z_OK) { printf("inflateInit failed\n"); return 4; }
	f.next_in = out; f.avail_in = (unsigned)packed; f.next_out = back; f.avail_out = size;
	{ int r = inflate(&f, Z_FINISH); printf("level %d: %u -> %lu bytes, inflate %d, %lu back, %s\n", level, size, packed, r,
		f.total_out, f.total_out == size && !memcmp(in, back, size) ? "WHOLE" : "DIFFERENT"); 
	  return f.total_out == size && !memcmp(in, back, size) ? 0 : 1; }
}
'''


def build(directory, bits, old=False):
    """The check linked with the game's zlib for 64 or 32 bits; the 64-bit one from its LP64 rewrite."""
    cc = shutil.which("clang")
    if not cc:
        pytest.skip("no clang")
    sources = directory / f"zlib{bits}"
    sources.mkdir()
    (sources / "cseries.h").write_text(CSERIES)
    (sources / "zcheck.c").write_text(CHECK)
    for path in sorted(ZLIB.glob("*.[ch]")):
        text = path.read_text(errors="replace")
        if path.name == "trees.c":
            for fixed, before in zip(FIXED, OLD):
                assert text.count(fixed) == 1, fixed
                if old:
                    text = text.replace(fixed, before)
        (sources / path.name).write_text(text)
    if bits == 64:
        for path in sorted(ZLIB.glob("*.[ch]")):
            subprocess.run([sys.executable, str(ROOT / "tools" / "lp64_rewrite.py"), "--output", str(sources / path.name),
                            str(sources / path.name)], cwd=ROOT, check=True)
    target = ["--target=x86_64-linux-gnu"] if bits == 64 else ["-m32"]
    binary = sources / "zcheck"
    run = subprocess.run([cc, *target, "-std=gnu89", "-O2", "-w", "-fno-strict-aliasing", "-fwrapv", "-I", str(sources),
                          str(sources / "zcheck.c"), *(str(sources / f"{unit}.c") for unit in UNITS), "-o", str(binary)],
                         capture_output=True, text=True)
    if run.returncode and bits == 32 and "cannot find" in run.stderr:
        pytest.skip("no 32-bit C library")
    assert run.returncode == 0, run.stderr
    return binary


@pytest.mark.parametrize("bits", [64, 32])
@pytest.mark.parametrize("level", [1, 6])
def test_round_trip(tmp_path, bits, level):
    run = subprocess.run([str(build(tmp_path, bits)), str(level)], capture_output=True, text=True, timeout=60)
    assert run.returncode == 0 and "WHOLE" in run.stdout, (run.returncode, run.stdout, run.stderr)


def test_negative_control_old_steps(tmp_path):
    run = subprocess.run([str(build(tmp_path, 64, old=True)), "1"], capture_output=True, text=True, timeout=60)
    assert run.returncode != 0, run.stdout
