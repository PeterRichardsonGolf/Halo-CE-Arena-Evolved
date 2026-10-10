"""AE's MCC mouse style (port/linux/game/ae_mouse.c, ae_glue_mouse.c; docs/testing.md "Test input: the mouse").

- the generator: the Mouse screen has the style row, the 100-value MCC list and the two 20-value scales, and
  every other generated screen is byte-identical to its file on disk (a regeneration changes only this screen);
- the config rows exist, with their defaults;
- (a game, only when $AE_MOUSE_BUILD names one, as $AE_MENUS_BUILD does in test_ae_menus.py) 100 counts turn the view
  12.60507 degrees in classic 1.0, the same within 1% in MCC 5.67, and 1.4 / 45 degrees a count in MCC 1.4 (with the
  vertical sensitivity ignored); the unset MCC value is seeded to 5.7 from classic 1.0.
"""
import os
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import port_settings  # noqa: E402
import test_ae_menus  # noqa: E402  (its harness loader)

CE = ROOT / "port" / "assets" / "menus" / "ce"
MOUSE_SCREEN = "main_menu.settings_select.player_setup.player_profile_edit.mouse_settings.xml"


def generated():
    return {**port_settings.settings_files(), **port_settings.multiplayer_files()}


def test_untouched_screens_are_byte_identical():
    """a regeneration changes no file but the Mouse screen's (and that one matches its file)"""
    for name, lines in generated().items():
        assert "\n".join(lines) == (CE / name).read_text(), name


def test_mouse_screen_rows():
    spinner = {row[1]: row[2] for row in port_settings.SCREENS["mouse_settings"]["rows"]}
    assert [v for _, v in spinner["input.mouse_style"]] == ["classic", "mcc"]
    sens = [v for _, v in spinner["input.mouse_mcc_sensitivity"]]
    assert len(sens) == 100 and sens[0] == "0.1" and sens[-1] == "10.0" and "5.7" in sens and "1.6" in sens
    for key in ("input.mouse_zoom_scale", "input.mouse_vehicle_scale"):
        scale = [v for _, v in spinner[key]]
        assert len(scale) == 20 and scale[0] == "0.1" and scale[-1] == "2.0" and "1.0" in scale
    text = (CE / MOUSE_SCREEN).read_text()
    for setting in spinner:
        assert f'setting="{setting}"' in text
    # (the classic rows still offer today's 14 values)
    assert [v for _, v in spinner["input.mouse_sensitivity"]][:3] == ["0.1", "0.15", "0.25"]


def test_config_rows():
    text = (ROOT / "port" / "linux" / "src" / "port_config.c").read_text()
    for key, kind, default, env in (("input.mouse_style", "_config_string", r'"\\"classic\\""', "HALO_MOUSE_STYLE"),
                                    ("input.mouse_mcc_sensitivity", "_config_real", '"0.0"', "HALO_MOUSE_MCC_SENSITIVITY"),
                                    ("input.mouse_zoom_scale", "_config_real", '"1.0"', "HALO_MOUSE_ZOOM_SCALE"),
                                    ("input.mouse_vehicle_scale", "_config_real", '"1.0"', "HALO_MOUSE_VEHICLE_SCALE")):
        assert re.search(rf'\{{ "{re.escape(key)}", {kind}, {default}, "{env}", _environment_value, /\* AE hook \*/',
                         text), key


TURN = re.compile(r"mouse test: counts (\S+),(\S+) turn yaw (\S+) pitch (\S+) degrees")


def game_turns(case, env, counts="100,40"):
    """one game: (its debug.txt's first 'mouse:' lines, the yaw and pitch of its first test turn)"""
    cfg = test_ae_menus.config()
    if cfg is None:
        pytest.skip("no build or data ($AE_MOUSE_BUILD, the harness's data_dir)")
    harness = test_ae_menus.harness
    out = harness.expand(cfg["out_dir"]) / "ae_mouse" / f"{harness.stamp()}-{case}"
    out.mkdir(parents=True, exist_ok=True)
    spec = {"name": case, "build": os.environ["AE_MOUSE_BUILD"], "map": "bloodgulch", "local_players": 1,
            "exit_after": 30, "test_input": f"mouse:{counts}", "env": env}
    harness.play(cfg, spec, out / case)
    text = (out / case / "debug.txt").read_text(errors="replace")
    turn = TURN.search(text)
    assert turn, "no mouse test line in debug.txt"
    return text, float(turn.group(3)), float(turn.group(4))


@pytest.mark.skipif(not os.environ.get("AE_MOUSE_BUILD"), reason="$AE_MOUSE_BUILD names no game")
def test_game_angles():
    # (AE_MOUSE_BUILD is read by test_ae_menus.build() too)
    os.environ.setdefault("AE_MENUS_BUILD", os.environ["AE_MOUSE_BUILD"])
    text, yaw, pitch = game_turns("classic", {"HALO_MOUSE_STYLE": "classic", "HALO_MOUSE_SENSITIVITY": "1.0"})
    assert "mouse: style classic, sensitivity 1, zoom scale 1, vehicle scale 1" in text
    assert yaw == pytest.approx(-100 * 0.0022 * 180 / 3.14159265358979, abs=1e-4)
    assert pitch == pytest.approx(-40 * 0.0022 * 180 / 3.14159265358979, abs=1e-4)
    classic_yaw = yaw
    text, yaw, pitch = game_turns("mcc567", {"HALO_MOUSE_STYLE": "mcc", "HALO_MOUSE_MCC_SENSITIVITY": "5.67"})
    assert "mouse: style mcc, sensitivity 5.67," in text
    assert yaw == pytest.approx(classic_yaw, rel=0.01)
    # (1.4 / 45 degrees a count; the vertical key, 3, is ignored: pitch follows yaw)
    text, yaw, pitch = game_turns("mcc14", {"HALO_MOUSE_STYLE": "mcc", "HALO_MOUSE_MCC_SENSITIVITY": "1.4",
                                            "HALO_MOUSE_VERTICAL_SENSITIVITY": "3"})
    assert yaw == pytest.approx(-100 * 1.4 / 45, abs=1e-3)
    assert pitch == pytest.approx(-40 * 1.4 / 45, abs=1e-3)
    # (unset, style mcc: seeded from classic 1.0)
    text, yaw, pitch = game_turns("mccseed", {"HALO_MOUSE_STYLE": "mcc", "HALO_MOUSE_SENSITIVITY": "1.0"})
    assert "mouse: MCC sensitivity 5.7 from the classic one" in text
    assert yaw == pytest.approx(-100 * 5.7 / 45, abs=1e-3)
