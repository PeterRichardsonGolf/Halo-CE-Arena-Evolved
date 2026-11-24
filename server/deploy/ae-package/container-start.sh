#!/bin/sh
# Arena Evolved dedicated server inside the container: sets up the volume, then runs ./halo --server.
# Settings come from the environment (compose.yaml); the rotation from /etc/ae-server/rotation.txt when that file is mounted
# (compose.yaml does), else the package's own example. HALO_SERVER_ROTATION set in the environment wins over both.
set -eu
data=${HALO_DATA_ROOT:-/var/lib/ae-server/data}
save=${HALO_SAVE_ROOT:-/var/lib/ae-server/save}
mkdir -p "$data/maps" "$save"
# the weapon data belongs to the image, not to the volume: always point at the image's copy
mkdir -p "$data/packs"
ln -sfn /opt/ae-server/packs/ae_content.aecache "$data/packs/ae_content.aecache"
if [ -z "${HALO_SERVER_ROTATION:-}" ]; then
	rotation=/etc/ae-server/rotation.txt
	[ -r "$rotation" ] || rotation=/opt/ae-server/rotation.default.txt
	HALO_SERVER_ROTATION=$(grep -v '^[[:space:]]*#' "$rotation" | tr '\n' ',' | sed 's/,\{2,\}/,/g; s/^,//; s/,$//')
	export HALO_SERVER_ROTATION
fi
# gametypes saved in the game (file=<name> entries) go in the volume: data/../gametypes is the package's; mount your own
# folder at /opt/ae-server/gametypes to replace them
cd "$data"
exec /opt/ae-server/halo --server "$@"
