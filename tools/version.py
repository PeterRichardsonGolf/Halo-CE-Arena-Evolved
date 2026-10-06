"""Arena Evolved's version, for the builds (tools/linux_build.py,
windows_build.py, macos_build.py; port/android/app/build.gradle reads the
same):

  - VERSION, in the repository's root, is the version being made:
    0.1.0-beta. It is Arena Evolved's own, not the upstream's; what it is
    built on is UPSTREAM_BASE (below), which the game logs at start-up.
  - A release is built by GitHub Actions from its tag, v<VERSION>: such a
    build is VERSION's own (version() reads the tag; HALO_VERSION, when
    set, wins). Arena Evolved's self-updater is off in every build.
  - Other builds of the workflow are nightlies, <VERSION>-nightly.<run>; a
    build anywhere else is <VERSION>-dev.
"""

import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# the upstream releases this version is built on, as the game logs them at
# start-up ("Halo CE: Arena Evolved 0.1.0-beta (ChupathingyCE 0.6.7b, OpenCE
# build-128)"); kept in step with port/linux/include/halo_product.h's
# HALO_UPSTREAM_BASE when an upstream release is merged
UPSTREAM_BASE = "ChupathingyCE 0.6.8b, OpenCE build-132"


def base_version() -> str:
    """VERSION's"""
    return (ROOT / "VERSION").read_text(encoding="utf-8").strip()


def version() -> str:
    """this build's: HALO_VERSION if set; VERSION's own when GitHub Actions
    builds the release's tag, v<VERSION>; else VERSION's -dev"""
    if os.environ.get("HALO_VERSION"):
        return os.environ["HALO_VERSION"]
    base = base_version()
    if (os.environ.get("GITHUB_REF_TYPE") == "tag" and
            os.environ.get("GITHUB_REF_NAME") == f"v{base}"):
        return base
    return f"{base}-dev"


def release_build() -> bool:
    """whether this build is a release's (and so looks for newer ones)"""
    return os.environ.get("HALO_RELEASE_BUILD") == "1"
