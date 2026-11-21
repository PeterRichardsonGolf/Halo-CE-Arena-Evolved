#!/bin/bash
# usage: convert.sh <source.map> <outname>
# Converts a version-13 multiplayer map to an Xbox NTSC map.
#
# Environment (all required unless defaulted):
#   INVADER_DIR   directory holding invader-extract, -convert, -refactor, -bludgeon, -build, -edit and libinvader.so
#   EXTRACT_BIN   invader-extract that names tags sharing a path (default $INVADER_DIR/invader-extract; see
#                 invader-protected-names.patch, build_invader.sh). The run stops when tags would be lost.
#   STOCK_TAGS    stock Xbox tag tree (sounds and the tags the map does not carry)
#   RESOURCE_DIR  directory holding bitmaps.map, loc.map, sounds.map of the source format
#   WORK_ROOT     working root; this run uses $WORK_ROOT/<outname>
#   SCRIPT_DIR    this directory (default: where this file is)
#
# Rules:
#  * Every tag the map carries is built from the map's own data. The stock tag of that path is used only when the
#    map's tag cannot be built; each such tag is listed in keep-stock.txt.
#  * Tags with no path of their own are extracted under protected_<n>; nothing resolves to another weapon.
#  * The field-diff gate runs on the result; the script fails when a gameplay tag differs and is not in keep-stock.txt.
#  * A placed instance that cannot be built is erased from the scenario; each one is listed under ERASED and fails
#    the run, as the scenario then differs from the source.
#  * fixes/<outname>.sh may set USE_HSC (a script source file) for a map whose scenario lost its source.
set -e
src=$1; f=$2
case "$f" in ''|*/*|.*) echo "usage: convert.sh <source.map> <outname> (outname: no '/', no leading '.')" >&2; exit 64;; esac
[ -f "$src" ] || { echo "source map not found: $src" >&2; exit 64; }
src=$(readlink -f "$src")
: "${INVADER_DIR:?}" "${STOCK_TAGS:?}" "${RESOURCE_DIR:?}" "${WORK_ROOT:?}"
SCRIPT_DIR=${SCRIPT_DIR:-$(cd "$(dirname "$0")" && pwd)}
EXTRACT_BIN=${EXTRACT_BIN:-$INVADER_DIR/invader-extract}
I=$INVADER_DIR; S=$SCRIPT_DIR; X=$STOCK_TAGS; export WORK_ROOT I
export LD_LIBRARY_PATH="$I${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
W=$WORK_ROOT/$f; mkdir -p "$W/maps" "$W/tags2"; cd "$W"
export CONVERT_TMP=$W
fail() { echo "FAILED at step '$1'; see $W/$2" >&2; exit "${3:-1}"; }
for r in bitmaps loc sounds; do ln -sf "$RESOURCE_DIR/$r.map" "maps/$r.map"; done
# a re-run starts from the source map again
rm -rf tags2 data "maps/$f.map"; mkdir -p tags2
cp "$src" "maps/$f.map"
nice "$EXTRACT_BIN" -O -m maps -t tags2 "maps/$f.map" > extract.log 2>&1 || fail extract extract.log
# every tag of the map must reach a file of its own
grep -a -q '^Extracted [0-9]* out of [0-9]* matched tags' extract.log || fail "extract (no count line)" extract.log
listed=$(grep -a -c '^Extracted [^0-9]' extract.log || true)
files=$(find tags2 -type f | wc -l)
if [ "$listed" -gt "$files" ]; then
  echo "EXTRACT LOST TAGS: $listed extracted, $files files (tags sharing a path overwrite each other)." >&2
  echo "Use an invader-extract built with invader-protected-names.patch (EXTRACT_BIN)." >&2
  exit 2
fi
rm -rf tags_src; cp -r tags2 tags_src
sc=$(find tags2 -name "*.scenario" | grep levels/ | head -1); rel=${sc#tags2/}; dir=$(dirname "$rel")
echo "scenario $rel"
rm -f usedE fixes.log
USE_HSC=; if [ -f "$S/fixes/$f.sh" ]; then . "$S/fixes/$f.sh"; fi
python3 -I "$S/getsrc.py" "$sc" extracted.hsc || fail getsrc ""
if [ -n "$USE_HSC" ]; then
  [ -f "$USE_HSC" ] || { echo "$f needs $USE_HSC (convert the map it names first)" >&2; exit 65; }
  cp "$USE_HSC" extracted.hsc
fi
# compiled scripts need their source to build
scripts=$("$I/invader-edit" -t tags2 -L "$rel" 2>/dev/null | grep -a -c -E '^(scripts|globals)\[[1-9][0-9]*\]' || true)
if [ "$scripts" -gt 0 ] && [ ! -s extracted.hsc ]; then
  echo "scenario has compiled scripts but no source was found; set USE_HSC in fixes/$f.sh" >&2; exit 66
fi
nice "$I/invader-convert" -t tags2 -O -g gbxmodel model -b "*" > convert.log 2>&1 || fail convert convert.log
nice "$I/invader-convert" -t tags2 -O -g shader_transparent_chicago_extended shader_transparent_chicago -b "*" >> convert.log 2>&1 || fail convert convert.log
nice "$I/invader-refactor" -t tags2 -M no-move -g gbxmodel model -g shader_transparent_chicago_extended shader_transparent_chicago > refactor.log 2>&1 || fail refactor refactor.log
rm -rf tags3; cp -r tags2 tags3
# the stock sound tag replaces the map's wherever a stock tag of that path exists; the map's globals are kept
( cd "$X"; find sound -type f ) | while IFS= read -r s; do [ -f "tags3/$s" ] && cp "$X/$s" "tags3/$s"; done || true
( cd tags3; find sound -type f -name "*.sound" ) | while IFS= read -r s; do [ -f "$X/$s" ] || echo "$s"; done > sounds_not_stock.txt
python3 -I "$S/ogg2pcm.py" tags3 > ogg2pcm.log 2>&1 || fail ogg2pcm ogg2pcm.log
nice "$I/invader-bludgeon" -t tags3 -T everything -b "*" > bludgeon.log 2>&1 || fail bludgeon bludgeon.log
if [ -s extracted.hsc ]; then mkdir -p "data/$dir/scripts"; grep -a -v player_effect_set_max_vibrate extracted.hsc > "data/$dir/scripts/extracted.hsc"; fi
: > keep-stock.txt
mkdir -p out data
B=(-g xbox-ntsc -t tags3 -t "$X" -d data -S data -N "$f")
SC="${rel%.scenario}"
for try in $(seq 1 120); do
  nice "$I/invader-build" -E "${B[@]}" -o "out/$f.map" "$SC" > build.log 2>&1 && break
  # a bitmap marked swizzled that the target cannot take: deswizzle it (the stock tag or a stop follows when that is not possible)
  if grep -a -q "is marked as swizzled" build.log; then
    sw=0
    while IFS= read -r cand; do
      case "$cand" in
        *.bitmap)
          if python3 -I "$S/unswizzle.py" "tags3/$cand" >> fixes.log; then sw=1; fi;;
      esac
    done < <(grep -a "^Failed to compile tag " build.log | sed 's/^Failed to compile tag //')
    [ "$sw" = 1 ] && continue
  fi
  # a map tag that cannot be built on the target: use the stock tag of that path instead
  t=
  while IFS= read -r cand; do
    if [ -f "tags3/$cand" ] && [ -f "$X/$cand" ] && ! grep -qxF "$cand" keep-stock.txt; then t=$cand; break; fi
  done < <(grep -a "^Failed to compile tag " build.log | sed 's/^Failed to compile tag //')
  if [ -n "$t" ]; then
    cp "$X/$t" "tags3/$t"; echo "$t" >> keep-stock.txt; echo "stock $t" >> fixes.log; continue
  fi
  # an unbuildable placed instance (bad refs): erase it from the scenario
  l=$(grep -a -o "Failed to compile Scenario::[a-z_]* #[0-9]*" build.log | head -1)
  if [[ "$l" =~ Scenario::([a-z_]+)\ \#([0-9]+) ]]; then
    arr=${BASH_REMATCH[1]}; idx=${BASH_REMATCH[2]}
    case $arr in *palette) echo "palette entry $arr[$idx] cannot auto-erase" >> fixes.log; break;; esac
    echo "erased $arr[$idx]: $(grep -a -B12 'Failed to compile Scenario' build.log | grep -a -v '^\.\.\.in' | grep -a -m2 -e FATAL -e 'Failed to compile tag' | tr '\n' ' ')" >> fixes.log
    "$I/invader-edit" -t tags3 -E "$arr[$idx]" "$rel" >> fixes.log 2>&1
  else break; fi
done
# over the 128 MiB cache cap: drop mip 0 of big 32-bit 2D bitmaps (the map's own folders only), rebuild
for minw in 1024 512; do
  u=$(grep -a "Uncompressed size" build.log | sed 's/.*size: *\([0-9.]*\) .*/\1/')
  awk -v u="${u:-0}" 'BEGIN{exit !(u>126)}' || break
  echo "uncompressed ${u} MiB over the cap: halving 32-bit bitmaps >= $minw" >> fixes.log
  python3 -I "$S/halve.py" tags3 $minw ui sound >> fixes.log 2>&1
  nice "$I/invader-build" -E "${B[@]}" -o "out/$f.map" "$SC" > build.log 2>&1 || true
done
if ! { [ -s "out/$f.map" ] && grep -a -q "Uncompressed size" build.log; }; then
  echo BUILD FAIL; grep -a -v WARNING build.log | tail -n 15; fail build build.log
fi
u=$(grep -a "Uncompressed size" build.log | sed 's/.*size: *\([0-9.]*\) .*/\1/'); if awk -v u="$u" 'BEGIN{exit !(u>47)}'; then touch usedE; fi
ls -la "out/$f.map"
# gate: the map's gameplay tags against the converted map's
rm -rf gate; mkdir -p gate/conv/maps gate/conv/tags
for r in bitmaps loc sounds; do ln -sf "$RESOURCE_DIR/$r.map" "gate/conv/maps/$r.map"; done
cp "out/$f.map" gate/conv/maps/c.map
( cd gate/conv && nice "$I/invader-extract" -O -m maps -t tags maps/c.map > extract.log 2>&1 ) || fail "gate extract" gate/conv/extract.log
# tags whose out-of-range indices the build step repaired
grep -a 'Fixed invalid-indices' bludgeon.log | sed 's/: Fixed invalid-indices.*//' > fixed-indices.txt || true
set +e
python3 -I "$S/mcc_fielddiff.py" --invader-edit "$I/invader-edit" --allow keep-stock.txt --fixed fixed-indices.txt --json gate/result.json tags_src gate/conv/tags | tee gate/summary.txt
gate=${PIPESTATUS[0]}
set -e
# fixes a person should look at in the result
{
  grep -a '^unswizzled ' fixes.log | sed 's/^/REVIEW FIXED: /'
  grep -a '^unswizzle failed ' fixes.log | sed 's/^/REVIEW: /'
  grep -a '^mirrored:' fixes.log | sed 's/^/REVIEW /'
  grep -a -E '^(halved|layout not understood)' fixes.log | sed 's/^/REVIEW NOTE: /'
  grep -a -E '^uncompressed .* MiB over the cap' fixes.log | sed 's/^/REVIEW NOTE: /'
  grep -a -E '^(CUT|FAIL) ' ogg2pcm.log | sed 's/^/REVIEW NOTE: /'
} >> gate/summary.txt || true
erased=$(grep -a '^erased ' fixes.log || true)
if [ -n "$erased" ]; then
  echo "ERASED instances (the converted scenario differs from the source):"; echo "$erased"
  { echo "ERASED:"; echo "$erased"; } >> gate/summary.txt
  exit 3
fi
exit "$gate"
