"""The Steam client in the desktop game (the Steam sign-in: Settings > MCC).

The client is the Android app's host's (port/android/host/steam_*.c, from Halo Content Evolved, AGPL-3.0-or-later),
built here into the desktop game's targets (``ninja linux64``, ``ninja macos``, ``ninja windows64``) and not into the
dedicated server, the web build or the 32-bit builds. Its units have the host's own ABI, as the posix_*.c ones, and
reach the game only through port/linux/src/steam_screen.c's calls, which HALO_STEAM_CLIENT turns from stubs into
steam_downloads.c's own. Its seal (steam_seal.c) uses the system's secret store (port/linux/src/ae_steam_secret.c)
instead of the Android Keystore.
"""

from pathlib import Path
from typing import List

from .linux_build import MBEDTLS_DIR, ZLIB_DIR

STEAM_HOST_DIR = Path("port/android/host")
# the Android guest's ABI header, which host.h includes (plain types: the client takes nothing else of the guest)
STEAM_ABI_INCLUDE = Path("port/android/include")
LINUX_SRC = Path("port/linux/src")
# the Windows build's shims for the POSIX headers the client includes (strings.h, dirent.h)
STEAM_WINDOWS_INCLUDE = Path("port/windows/include/steam")

# the client's own units (the Android host's, unchanged but for AE hooks)
STEAM_CLIENT_UPSTREAM = ("steam_cm.c", "steam_crypto.c", "steam_depot.c", "steam_downloads.c", "steam_net.c",
                         "steam_seal.c", "steam_unpack.c", "host_mcc_hash.c")
# the desktop's own: the log, the seal's key store, and STEAM ACCOUNT's door to the client (ae_steam_client.c). Left out of the platform units' list of every build, and
# added here to the ones that have the client
STEAM_CLIENT_AE = ("ae_steam_host.c", "ae_steam_secret.c", "ae_steam_client.c", "ae_steam_ws.c", "ae_steam_ws_rules.c",
                   "ae_steam_preview.c", "ae_steam_preview_rules.c")

# HOST_MCC_HASH_PORTABLE: host_mcc_hash.c's portable C SHA-256 and CRC32, not the ARM instructions the Android
# phones are asked about (getauxval)
STEAM_CLIENT_DEFINES = ["-DHALO_STEAM_CLIENT", "-DHOST_MCC_HASH_PORTABLE"]


def steam_client_sources() -> List[Path]:
    return [*(STEAM_HOST_DIR / name for name in STEAM_CLIENT_UPSTREAM), *(LINUX_SRC / name for name in STEAM_CLIENT_AE)]


def steam_client_flags(source: Path, mbedtls_flags: str) -> str:
    """the flags a client unit adds to the host-ABI units' (Mbed TLS with its own locks, as the updater's)"""
    flags = [*STEAM_CLIENT_DEFINES, f"-I{STEAM_HOST_DIR}", f"-I{STEAM_ABI_INCLUDE}", f"-I{ZLIB_DIR}", mbedtls_flags]
    if source.name == "steam_unpack.c":
        # (its third-party decoders: LZMA's and Zstandard's, which build with their own warnings)
        flags.append("-w")
    return " ".join(flags)


def steam_client_in_platform_list(source: Path) -> bool:
    """whether a platform unit of port/linux/src goes in the usual list: not the desktop client's own"""
    return source.name not in STEAM_CLIENT_AE
