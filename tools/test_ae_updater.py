"""(AE, 0.4.3) The self-updater's switch and the release's list of update files, without a build:

- tools/version.py release_build: only GitHub Actions' build of the release's tag, v<VERSION>, is a release's (and
  so looks for updates); a branch's run, a build anywhere else, another tag, a nightly are not, and
  HALO_RELEASE_BUILD=0/1 decides when set;
- tools/update_sign.py manifest: an AE release's files (arena-evolved-<version>-<platform>.zip, by the release
  kit) give a list of the two desktop release builds the updater serves, linux64 and windows64, and leave out the
  release's other files; a zip of another version is refused; verify-manifest checks the signature and the files.

The updater's own end-to-end test (a download and an install) is tools/test_update_install.py; its pure parts'
unit test is port/linux/tests/ae_updater_pick_test.c (tools/test_ae_units.py)."""
import json
import sys
import zipfile
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import update_sign  # noqa: E402
import version  # noqa: E402

BASE = version.base_version()
GITHUB = ("GITHUB_ACTIONS", "GITHUB_REF_TYPE", "GITHUB_REF_NAME", "HALO_RELEASE_BUILD", "HALO_VERSION")


@pytest.fixture
def environment(monkeypatch):
    for name in GITHUB:
        monkeypatch.delenv(name, raising=False)

    def set_to(**values):
        for name, value in values.items():
            monkeypatch.setenv(name, value)
    return set_to


@pytest.mark.parametrize("values, release, channel", [
    ({}, False, "dev"),
    ({"GITHUB_ACTIONS": "true", "GITHUB_REF_TYPE": "branch", "GITHUB_REF_NAME": "ci/auto-update"}, False, "dev"),
    ({"GITHUB_ACTIONS": "true", "GITHUB_REF_TYPE": "tag", "GITHUB_REF_NAME": f"v{BASE}"}, True, "release"),
    ({"GITHUB_ACTIONS": "true", "GITHUB_REF_TYPE": "tag", "GITHUB_REF_NAME": "v0.0.1-beta"}, False, "dev"),
    # (the tag's name outside GitHub Actions: a build of ours that happens to have the variables)
    ({"GITHUB_REF_TYPE": "tag", "GITHUB_REF_NAME": f"v{BASE}"}, False, "custom"),
    ({"GITHUB_ACTIONS": "true", "GITHUB_REF_TYPE": "tag", "GITHUB_REF_NAME": f"v{BASE}",
      "HALO_VERSION": f"{BASE}-nightly.3"}, False, "nightly"),
    ({"GITHUB_ACTIONS": "true", "GITHUB_REF_TYPE": "tag", "GITHUB_REF_NAME": f"v{BASE}", "HALO_RELEASE_BUILD": "0"},
     False, "custom"),
    ({"HALO_RELEASE_BUILD": "1", "HALO_VERSION": BASE}, True, "release"),
])
def test_release_build(environment, values, release, channel):
    environment(**values)
    assert version.release_build() is release
    assert version.channel() == channel


def release_files(directory, version_text):
    """the files of a release as the kit makes them (small stand-ins)"""
    names = [f"arena-evolved-{version_text}-{platform}" for platform in
             ("linux64.zip", "linux64-arch.zip", "windows64.zip", "android-unsigned.zip", "macos.tar",
              "server-linux-x64.zip", "server-linux-arm64.zip", "snd-maps.zip")]
    paths = []
    for name in names:
        path = directory / name
        if name.endswith(".zip"):
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("halo", name)
                archive.writestr("packs/ae_battle_rifle.aepack", b"pack")
        else:
            path.write_bytes(name.encode())
        paths.append(path)
    return paths


def test_manifest_lists_the_desktop_release_builds(tmp_path):
    files = release_files(tmp_path, "0.4.3-beta")
    document = json.loads(update_sign.manifest_text("0.4.3-beta", "v0.4.3-beta", "v0.4.3-beta-debug", "", files))
    assert document["format"] == update_sign.MANIFEST_FORMAT and document["version"] == "0.4.3-beta"
    listed = [(entry["platform"], entry["configuration"], entry["kind"], entry["tag"], entry["name"])
              for entry in document["files"]]
    assert listed == [("linux64", "release", "zip", "v0.4.3-beta", "arena-evolved-0.4.3-beta-linux64.zip"),
                      ("windows64", "release", "zip", "v0.4.3-beta", "arena-evolved-0.4.3-beta-windows64.zip")]
    assert all("main" not in entry for entry in document["files"])


def test_manifest_refuses_another_versions_zip(tmp_path):
    files = release_files(tmp_path, "0.4.2-beta")
    with pytest.raises(SystemExit):
        update_sign.manifest_text("0.4.3-beta", "v0.4.3-beta", "v0.4.3-beta-debug", "", files)


def test_manifest_signed_and_verified(tmp_path, monkeypatch):
    """a test key's list (never the release key: that stays with the owner) checks, and a changed file or another
    key's signature does not"""
    try:
        import delta_table
        monkeypatch.setenv("OPENSSL", delta_table.openssl())
        key = delta_table.keygen(tmp_path / "test.pem")
        other = delta_table.keygen(tmp_path / "other.pem")
    except (SystemExit, ImportError, OSError):
        pytest.skip("needs OpenSSL 3's openssl")
    release = tmp_path / "out"
    release.mkdir()
    files = release_files(release, "0.4.3-beta")
    update_sign.manifest(tmp_path / "test.pem", "0.4.3-beta", "v0.4.3-beta", "v0.4.3-beta-debug", "", files)
    manifest = release / "updates.json"
    assert update_sign.verify_manifest(manifest, [key]) == 0
    assert update_sign.verify_manifest(manifest, [other]) == 1
    # (the release key the game is built with did not sign a test's list)
    assert update_sign.verify_manifest(manifest) == 1
    (release / "arena-evolved-0.4.3-beta-windows64.zip").write_bytes(b"changed")
    assert update_sign.verify_manifest(manifest, [key]) == 1
