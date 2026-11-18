#!/bin/sh
# Arena Evolved server: copy the Halo MCC files a rotation needs from a PC that has Steam to a server that has not.
#
#   sh sync-mcc.sh --from <steam folder or MCC folder> --to <user@host:/folder | /folder> --rotation rotation.txt [--dry-run]
#
# What it copies (nothing else, and it only READS the source):
#   halo1/maps/ui.map, the 10 campaign maps and the 19 multiplayer maps, bitmaps.map, and
#   halo1/maps/custom_edition/{bitmaps,sounds,loc}.map          (always, about 1.4 GB: the server starts only when MCC's Halo CE
#                                                                content is whole, as the game checks it; it never sends them)
#   halo1/maps/<name>.map                                   any other  <name>@mcc  entry of the rotation
#   steamapps/workshop/content/976730/<item id>/            for every  <name>@ws<item id>  entry
# It makes this shape in the target folder, which is the shape of a Steam library:
#   <target>/steamapps/common/Halo The Master Chief Collection/halo1/maps/...
#   <target>/steamapps/workshop/content/976730/<item id>/...
# On the server set  HALO_MCC_PATH="<target>/steamapps/common/Halo The Master Chief Collection"  and restart it.
# Run it again after you subscribe to a Workshop item, or Steam updates one: it only sends what changed (with rsync).
# Needs: sh, tar, and ssh for a remote target. rsync is used when it is there (resumes, sends only changes).
# Works from Linux, macOS and Git Bash on Windows. Entries that are not @mcc or @ws (Xbox pack maps, @ce maps) are not
# MCC files: copy those folders yourself (GUIDE.md, "Community map packs").
# MCC and its Workshop items belong to their owners and to you through your own Steam account: copy them only to a server
# you run for yourself, and do not pass them on.
set -eu

usage() {
	sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
	exit "${1:-2}"
}

from=""; to=""; rotation_file=""; rotation_text=""; dry=0
while [ $# -gt 0 ]; do
	case "$1" in
		--from) from=${2:?}; shift 2 ;;
		--to) to=${2:?}; shift 2 ;;
		--rotation) rotation_file=${2:?}; shift 2 ;;
		--rotation-text) rotation_text=${2:?}; shift 2 ;;
		--dry-run) dry=1; shift ;;
		-h|--help) usage 0 ;;
		*) echo "unknown argument: $1" >&2; usage 2 ;;
	esac
done
[ -n "$from" ] && [ -n "$to" ] || usage 2
if [ -n "$rotation_file" ]; then
	[ -r "$rotation_file" ] || { echo "cannot read $rotation_file" >&2; exit 2; }
	rotation_text=$(grep -v '^[[:space:]]*#' "$rotation_file" | tr '\n' ',')
fi
[ -n "$rotation_text" ] || { echo "give --rotation <file> or --rotation-text '<entries>'" >&2; exit 2; }

# the MCC folder (holds halo1/) and the steamapps folder, from whatever --from names
from=${from%/}
mcc=""
for candidate in "$from" "$from/steamapps/common/Halo The Master Chief Collection" \
	"$from/common/Halo The Master Chief Collection"; do
	if [ -d "$candidate/halo1/maps" ]; then mcc=$candidate; break; fi
done
[ -n "$mcc" ] || { echo "no halo1/maps under $from: name the Steam folder or the MCC folder" >&2; exit 1; }
steamapps=$(dirname "$(dirname "$mcc")")
workshop="$steamapps/workshop/content/976730"

list=$(mktemp)
trap 'rm -f "$list" "$list.ws" "$list.entries"' EXIT INT TERM
: > "$list"; : > "$list.ws"
missing=0
add() { # source-root relative-path list-file
	if [ -e "$1/$2" ]; then
		printf '%s\n' "$2" >> "$3"
		return 0
	fi
	echo "missing at the source: $1/$2" >&2
	missing=$((missing + 1))
	return 1
}

# (what the game's own check of MCC's Halo CE content asks for: ui.map and the campaign maps, the 19 stock multiplayer maps)
for f in ui bitmaps sounds loc a10 a30 a50 b30 b40 c10 c20 c40 d20 d40 \
	beavercreek bloodgulch boardingaction carousel chillout damnation hangemhigh longest prisoner putput ratrace \
	sidewinder wizard dangercanyon deathisland gephyrophobia icefields infinity timberland; do
	if [ -e "$mcc/halo1/maps/$f.map" ]; then
		printf '%s\n' "halo1/maps/$f.map" >> "$list"
	elif [ "$f" != sounds ] && [ "$f" != loc ]; then
		echo "missing at the source: halo1/maps/$f.map (the server will not start without MCC's whole Halo CE content)" >&2
		missing=$((missing + 1))
	fi
done
for f in bitmaps sounds loc; do
	if [ -e "$mcc/halo1/maps/custom_edition/$f.map" ]; then printf '%s\n' "halo1/maps/custom_edition/$f.map" >> "$list"; fi
done

# the rotation: entries by comma, each  map[:gametype[+rule=value...]]; only the map part matters here
printf '%s' "$rotation_text" | tr ',' '\n' | while IFS= read -r entry; do
	entry=$(printf '%s' "$entry" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//; s/:.*$//')
	printf '%s\n' "$entry"
done | sort -u > "$list.entries"

mcc_maps=0; ws_items=0; skipped=0
while IFS= read -r entry; do
	[ -n "$entry" ] || continue
	case "$entry" in
		*@mcc)
			name=${entry%@mcc}
			case "$name" in *[!A-Za-z0-9_.-]*|"") echo "skipped (odd name): $entry" >&2; continue ;; esac
			if add "$mcc" "halo1/maps/$name.map" "$list"; then mcc_maps=$((mcc_maps + 1)); fi ;;
		*@ws[0-9]*)
			id=${entry##*@ws}
			case "$id" in *[!0-9]*|"") echo "skipped (odd item id): $entry" >&2; continue ;; esac
			if [ -d "$workshop/$id" ]; then
				echo "$id" >> "$list.ws"; ws_items=$((ws_items + 1))
			else
				echo "missing at the source: Workshop item $id (subscribe to it in Steam first)" >&2; missing=$((missing + 1))
			fi ;;
		*) skipped=$((skipped + 1)); echo "not an MCC file, not copied: $entry" >&2 ;;
	esac
done < "$list.entries"
sort -u "$list.ws" -o "$list.ws"
sort -u "$list" -o "$list"

remote=0; host=""; dest=$to
case "$to" in
	[A-Za-z]:[\\/]*) ;;                       # a Windows drive path, local
	*:*) remote=1; host=${to%%:*}; dest=${to#*:} ;;
esac
target_mcc="$dest/steamapps/common/Halo The Master Chief Collection"
target_ws="$dest/steamapps/workshop/content/976730"
echo "source MCC:  $mcc"
echo "source ws:   $workshop"
echo "target:      ${host:+$host:}$dest   (MCC: the whole Halo CE set; Workshop items: $ws_items)"
[ "$missing" -eq 0 ] || echo "WARNING: $missing file(s) or item(s) missing at the source (listed above); they are not copied" >&2

send() { # source-root target-root file-list (relative paths), a directory list for the Workshop
	root=$1; target=$2; files=$3
	[ -s "$files" ] || return 0
	if command -v rsync >/dev/null 2>&1; then
		flags="-rt --no-owner --no-group --files-from=$files"
		[ "$dry" -eq 1 ] && flags="$flags --dry-run -v"
		if [ "$dry" -eq 0 ] && [ -t 1 ]; then flags="$flags --info=progress2"; fi
		if [ "$remote" -eq 1 ]; then
			ssh "$host" "mkdir -p '$target'"
			# shellcheck disable=SC2086
			rsync $flags "$root/" "$host:$target/"
		else
			mkdir -p "$target"
			# shellcheck disable=SC2086
			rsync $flags "$root/" "$target/"
		fi
	else
		if [ "$dry" -eq 1 ]; then sed "s|^|would copy $root/|" "$files"; return 0; fi
		if [ "$remote" -eq 1 ]; then
			tar -C "$root" -cf - -T "$files" | ssh "$host" "mkdir -p '$target' && tar -C '$target' -xf -"
		else
			mkdir -p "$target"
			tar -C "$root" -cf - -T "$files" | tar -C "$target" -xf -
		fi
	fi
}

send "$mcc" "$target_mcc" "$list"
send "$workshop" "$target_ws" "$list.ws"
echo "done. On the server:  export HALO_MCC_PATH=\"$target_mcc\"   and restart the server."
