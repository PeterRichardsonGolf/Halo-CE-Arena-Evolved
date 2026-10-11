#!/bin/sh
# (AE) Checks the portable 64-bit Linux download (the CI job
# linux64-portable-check), unpacked in <folder>, on the Ubuntu it is
# started on: its libraries are all found (SDL3 is beside halo), nothing
# in it asks for a glibc newer than 2.35 (Ubuntu 22.04's, the oldest
# supported system), halo and SDL3 name only libraries every desktop has,
# and halo starts and loads everything it links to (LD_BIND_NOW=1 resolves
# every symbol at start-up): it needs no game data and no display for that.
#
#     sh tools/ci_check_linux_portable.sh <folder>
set -eu
cd "$1"
maximum=2.35
fail() { echo "FAIL: $*" >&2; exit 1; }

test -x ./halo || fail "no halo"
test -f ./libSDL3.so.0 || fail "no libSDL3.so.0 beside halo"
test -f ./SDL3-LICENSE.txt || fail "no SDL3 license"

echo "== ldd"
ldd ./halo
ldd ./libSDL3.so.0
if ldd ./halo ./libSDL3.so.0 | grep -q "not found"; then fail "a library is not found"; fi
# (SDL3 must be the one beside halo, not a system's)
# (ldd prints $ORIGIN's path as written, e.g. ".../portable/./libSDL3.so.0": compare real paths)
sdl_path=$(ldd ./halo | sed -n 's/.*libSDL3\.so\.0 => \([^ ]*\).*/\1/p')
[ -n "$sdl_path" ] && [ "$(readlink -f "$sdl_path")" = "$(readlink -f ./libSDL3.so.0)" ] || fail "halo does not load the SDL3 beside it"

echo "== libraries halo links to"
readelf -d ./halo | grep -E "NEEDED|RPATH|RUNPATH"
readelf -d ./halo | grep -q "RPATH.*\\\$ORIGIN" || fail "halo has no rpath of \$ORIGIN"
for library in $(readelf -d ./halo | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
	case "$library" in
		libSDL3.so.0|libc.so.6|libm.so.6|libdl.so.2|libpthread.so.0|librt.so.1|ld-linux-x86-64.so.2) ;;
		*) fail "halo links to $library, which not every desktop has" ;;
	esac
done

echo "== glibc symbol versions (at most $maximum)"
for file in ./halo ./libSDL3.so.0; do
	versions=$(objdump -T "$file" | grep -o "GLIBC_[0-9][0-9.]*" | sort -u -V)
	echo "$file: $(echo $versions)"
	for version in $versions; do
		number=${version#GLIBC_}
		newest=$(printf '%s\n%s\n' "$number" "$maximum" | sort -V | tail -n 1)
		if [ "$newest" != "$maximum" ]; then fail "$file needs $version"; fi
	done
done

echo "== start"
work=$(mktemp -d)
status=0
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LD_BIND_NOW=1 HALO_DATA_ROOT="$work/data" HALO_SAVE_ROOT="$work/saves" \
	HALO_NET_ONLINE=false HALO_NET_LIST_GAMES=false HALO_EXIT_AFTER=2 \
	timeout 60 ./halo > "$work/out.txt" 2>&1 || status=$?
cat "$work/out.txt"
echo "status $status"
# (without the maps it prints its version and stops with status 1)
grep -q "Halo CE: Arena Evolved" "$work/out.txt" || fail "halo printed no start-up line"
# (127: the loader could not start it; 124: it did not stop; 128 and up: a
# signal. Any other status, a refusal for lack of data or a display
# included, is a program that started.)
if [ "$status" = 127 ] || [ "$status" = 124 ] || [ "$status" -ge 128 ]; then fail "halo did not start (status $status)"; fi
if grep -qE "error while loading shared libraries|symbol lookup error|version .GLIBC" "$work/out.txt"; then fail "the loader refused halo"; fi
rm -rf "$work" ./config.toml
echo "OK"
