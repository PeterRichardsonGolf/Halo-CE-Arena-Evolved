"""A derived file that is one art input as it is: copy_input.py <input> <output>. For a recipe whose layer takes a tag of
an extra zip (make_pack.py's extra_sources) unchanged, pinned by its bytes (the recipe's derived SHA-256)."""
import shutil
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: copy_input.py <input> <output>")
shutil.copyfile(sys.argv[1], sys.argv[2])
