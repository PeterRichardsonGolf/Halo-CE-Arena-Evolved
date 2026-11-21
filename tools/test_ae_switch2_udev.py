"""The Switch 2 Pro Controller's udev rule (port/assets/udev/70-ae-switch2.rules) agrees with what uses it: the Linux
downloads carry it (tools/ci_build.py), docs/features.md's "Switch 2 Pro Controller on Linux without Steam" tells how to
install it, and the portable builds' SDL3 has the libusb support the rule is for (tools/linux_build.py,
tools/linux_sysroot.py)."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tools.linux_build import SDL_OPTIONS  # noqa: E402
from tools.linux_sysroot import PACKAGES, SDL_REQUIRED  # noqa: E402

RULE_FILE = ROOT / "port/assets/udev/70-ae-switch2.rules"
INSTALL = "sudo cp 70-ae-switch2.rules /etc/udev/rules.d/ && sudo udevadm control --reload"


def test_the_rule_opens_the_pads_usb_device():
    text = RULE_FILE.read_text()
    lines = [line for line in text.splitlines() if line and not line.startswith("#")]
    assert lines == ['SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTRS{idVendor}=="057e", ATTRS{idProduct}=="2069", '
                     'MODE="0660", TAG+="uaccess"']
    assert INSTALL in text


def test_the_linux_downloads_carry_the_rule_and_the_docs_say_how_to_install_it():
    build = (ROOT / "tools/ci_build.py").read_text()
    assert '"port/assets/udev/70-ae-switch2.rules", dist / "70-ae-switch2.rules"' in build
    features = (ROOT / "docs/features.md").read_text()
    section = features[features.index("## Switch 2 Pro Controller on Linux without Steam"):]
    section = section[:section.index("\n## ") if "\n## " in section else len(section)]
    assert INSTALL in section


def test_the_portable_sdl_has_libusb():
    assert "-DSDL_HIDAPI_LIBUSB=ON" in SDL_OPTIONS and "-DSDL_HIDAPI_LIBUSB_SHARED=ON" in SDL_OPTIONS
    assert "-DSDL_HIDAPI_LIBUSB=OFF" not in SDL_OPTIONS
    assert {"HAVE_LIBUSB", "SDL_LIBUSB_DYNAMIC"} <= set(SDL_REQUIRED)
    names = [path.rsplit("/", 1)[-1].split("_")[0] for path, _, _ in PACKAGES]
    assert "libusb-1.0-0" in names and "libusb-1.0-0-dev" in names
