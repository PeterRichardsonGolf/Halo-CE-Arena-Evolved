"""What every StageKit scenario does (README.md): its map needs no correction, and played, the game raises no
exception, the player is in it, and played again it plays the same."""

import subprocess

import pytest
import runner
from runner import SCENARIOS


def test_scenarios_list(stagekit):
    listed = subprocess.run([str(stagekit), "--list"], capture_output=True, text=True, check=True).stdout
    assert sorted(line.split()[0] for line in listed.splitlines()) == SCENARIOS


# StageKit writes a Custom Edition map. Arena Evolved checks those in the game itself (ae_tag_validate_custom_edition_map,
# the AE hook in cache_files.c: ascetic@ce "checked" in a game), while map_validate here runs the Xbox-format check only
# and refuses a Custom Edition tag header ("has a damaged tag header"), 0.7.5 and 0.7.6 alike. Restore when map_validate
# links the Custom Edition check.
@pytest.mark.skip(reason="map_validate checks Xbox-format maps only; Arena Evolved checks Custom Edition maps in the game")
@pytest.mark.parametrize("scenario", SCENARIOS)
def test_map_needs_no_correction(stagekit, scenario, tmp_path):
    assert runner.validate(runner.generate(stagekit, scenario, tmp_path)) == 0


@pytest.mark.parametrize("scenario", SCENARIOS)
def test_no_engine_exception(play, scenario):
    assert "EXCEPTION" not in play(scenario).debug


@pytest.mark.parametrize("scenario", SCENARIOS)
def test_player_plays(play, scenario):
    session = play(scenario)
    assert len(session.places()) == len(session.seconds) > 0


@pytest.mark.parametrize("scenario", SCENARIOS)
def test_plays_the_same_every_time(play, scenario):
    """the game's whole log of each second of the slice, every player's every field, the same"""
    assert play(scenario, 1).logged() == play(scenario).logged()
