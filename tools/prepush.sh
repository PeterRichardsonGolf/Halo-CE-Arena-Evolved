#!/bin/sh
# Local checks before a push: undefined symbols, merge damage, then the units and hooks. No network, no git push.
# Usage: tools/prepush.sh [build-dir]   (default build/linux64 of this tree; build it first: ninja linux64)
cd "$(dirname "$0")/.." || exit 2
BUILD=${1:-build/linux64}
BUILD=$(cd "$BUILD" 2>/dev/null && pwd) || { echo "prepush: no build dir '${1:-build/linux64}' (build it first)"; exit 2; }
FAILED=0

run() {
    name=$1
    shift
    start=$(date +%s)
    "$@" >"$LOG" 2>&1
    code=$?
    secs=$(( $(date +%s) - start ))
    if [ "$code" -eq 0 ]; then
        echo "RAN $name: PASS (${secs}s)"
    else
        echo "RAN $name: FAIL exit $code (${secs}s)"
        tail -n 25 "$LOG" | sed 's/^/    /'
        FAILED=1
    fi
}

LOG=$(mktemp) || exit 2
trap 'rm -f "$LOG"' EXIT

run "undefined symbols" python3 tools/ae_undefined_symbols.py "$BUILD"
run "merge damage" python3 tools/ae_merge_damage.py
run "units + hooks" env AE_MENUS_BUILD="$BUILD" python3 -m pytest -q -p no:cacheprovider tools/test_ae_units.py tools/test_ae_hooks.py

if [ "$FAILED" -ne 0 ]; then
    echo "prepush: FAILED"
    exit 1
fi
echo "prepush: all passed"
