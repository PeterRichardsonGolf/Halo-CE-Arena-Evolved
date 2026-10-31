"""The DualSense's settings, files and documents agree (port/linux/src/ae_ds5.c; the rules are
port/linux/tests/ae_ds5_rules_test.c's, tools/test_ae_units.py):

- the udev rule the game prints is the line of port/assets/udev/70-ae-dualsense.rules, which the Linux downloads carry
  (tools/ci_build.py) and the README's "DualSense on Linux without Steam" tells how to install;
- Settings > Gamepads has ADAPTIVE TRIGGERS, TRIGGER STRENGTH and LIGHTBAR for input.adaptive_triggers,
  input.trigger_strength and input.lightbar, desktop only, whose values the C side reads, and their rows in
  port_config.c (on, normal, player colour by default) and port/linux/README.md."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import port_settings  # noqa: E402

RULE_FILE = ROOT / "port/assets/udev/70-ae-dualsense.rules"
SETTINGS = {"input.adaptive_triggers": ("true", ["true", "false"]),
            "input.trigger_strength": ('"normal"', ["light", "normal", "strong"]),
            "input.lightbar": ('"player_color"', ["off", "player_color", "shield"])}


def _rule_line():
    lines = [line for line in RULE_FILE.read_text().splitlines() if line and not line.startswith("#")]
    assert len(lines) == 1
    return lines[0]


def test_the_printed_rule_is_the_file_s():
    source = (ROOT / "port/linux/src/ae_ds5.c").read_text()
    match = re.search(r'#define AE_DS5_UDEV_RULE \\\n\s*"((?:[^"\\]|\\.)*)"', source)
    assert match
    assert match.group(1).encode().decode("unicode_escape") == _rule_line()
    assert 'KERNELS=="*:054C:0CE6.*|*:054C:0DF2.*"' in _rule_line() and 'TAG+="uaccess"' in _rule_line()


def test_the_linux_downloads_carry_the_rule():
    build = (ROOT / "tools/ci_build.py").read_text()
    assert '"port/assets/udev/70-ae-dualsense.rules", dist / "70-ae-dualsense.rules"' in build
    # (the section moved from README.md to docs/features.md with 0.3.4's short README)
    features = (ROOT / "docs/features.md").read_text()
    section = features[features.index("## DualSense on Linux without Steam"):]
    section = section[:section.index("\n## ") if "\n## " in section else len(section)]
    assert "sudo cp 70-ae-dualsense.rules /etc/udev/rules.d/ && sudo udevadm control --reload" in section
    assert "(#dualsense-on-linux-without-steam)" in features


def test_the_gamepads_screen_rows():
    rows = {row[1]: row for row in port_settings.SCREENS["gamepad_setup"]["rows"]}
    for key, (_, values) in SETTINGS.items():
        assert key in rows, key
        assert [value for _, value in rows[key][2]] == values, key
        assert rows[key][4] == "desktop", key
    assert [row[0] for row in port_settings.SCREENS["gamepad_setup"]["rows"]][-3:] == \
        ["ADAPTIVE TRIGGERS:", "TRIGGER STRENGTH:", "LIGHTBAR:"]
    # (the generated file is the generator's)
    name = "main_menu.settings_select.player_setup.player_profile_edit.gamepad_setup.xml"
    assert (ROOT / "port/assets/menus/ce" / name).read_text() == "\n".join(port_settings.settings_files()[name])


def test_config_rows_and_their_documents():
    config = (ROOT / "port/linux/src/port_config.c").read_text()
    readme = (ROOT / "port/linux/README.md").read_text()
    for key, (default, _) in SETTINGS.items():
        assert re.search(r'\{ "' + re.escape(key) + r'", _config_\w+, "' + re.escape(default.replace('"', '\\"')) +
                         r'"', config), key
        assert any(line.startswith(f"| `{key}` |") for line in readme.splitlines()), key
    assert '{ "debug.ds5_log", _config_boolean, "false"' in config
    assert any(line.startswith("| `debug.ds5_log` |") for line in readme.splitlines())
    # (the values the C side tells apart)
    platform = (ROOT / "port/linux/src/ae_ds5.c").read_text()
    rules = (ROOT / "port/linux/src/ae_ds5_rules.c").read_text()
    assert 'strcmp(lightbar, "off")' in platform and 'strcmp(lightbar, "shield")' in platform
    assert '"light"' in rules and '"strong"' in rules
