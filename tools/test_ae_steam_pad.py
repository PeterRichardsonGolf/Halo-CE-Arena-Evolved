"""The Steam Controller's (2026) files agree: the udev rule's ids are the pad's (port/linux/src/ae_steam_pad_rules.h), the Linux
downloads carry the rule, and docs/features.md tells how to install it."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RULE_FILE = ROOT / "port/assets/udev/70-ae-steam-controller.rules"


def _rule_line():
    lines = [line for line in RULE_FILE.read_text().splitlines() if line and not line.startswith("#")]
    assert len(lines) == 1
    return lines[0]


def test_the_rule_names_the_four_products():
    header = (ROOT / "port/linux/src/ae_steam_pad_rules.h").read_text()
    products = re.findall(r"#define AE_STEAM_PAD_PRODUCT_(?:WIRED|BLUETOOTH|RECEIVER|RECEIVER_2) 0x([0-9a-f]{4})", header)
    assert sorted(products) == ["1302", "1303", "1304", "1305"]
    vendor = re.search(r"#define AE_STEAM_PAD_VENDOR 0x([0-9a-f]{4})", header).group(1)
    line = _rule_line()
    assert 'KERNELS=="' + "|".join(f"*:{vendor.upper()}:{product}.*" for product in sorted(products)) + '"' in line
    assert 'TAG+="uaccess"' in line and 'MODE="0660"' in line


def test_the_linux_downloads_carry_the_rule_and_the_guide_tells_how():
    build = (ROOT / "tools/ci_build.py").read_text()
    assert '"port/assets/udev/70-ae-steam-controller.rules", dist / "70-ae-steam-controller.rules"' in build
    features = (ROOT / "docs/features.md").read_text()
    section = features[features.index("## Steam Controller\n"):]
    section = section[:section.index("\n## ", 5)]
    assert "sudo cp 70-ae-steam-controller.rules /etc/udev/rules.d/ && sudo udevadm control --reload" in section
    assert "EXTRA BUTTONS" in section and "Disable Steam Input" in section and "Backpack mode" in section
