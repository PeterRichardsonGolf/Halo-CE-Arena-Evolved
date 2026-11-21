"""menu_tags.c reserves PAUSE_PATCH_TAGS tag slots for the pause patches before it knows how many it makes: the
reserve has to cover the most any game makes, or the last tags are written past the grown tag table.

Counted from the source: a multiplayer game patches one list (shared by its pause screens) with CHANGE TEAMS,
SETTINGS and END GAME; network co-op on a campaign map patches every list of solo_pause_screens (solo's and split
screen's, each its own) with SETTINGS and FRIENDLY FIRE. Each button is 2 tags: the widget and its text."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "port/linux/game/menu_tags.c"
TAGS_PER_BUTTON = 2  # pause_list_patch's tags[] next_tag + pause_button's text_tag next_tag


def reserve_covers(source):
    reserved = int(re.search(r"^#define PAUSE_PATCH_TAGS (\d+)", source, re.M).group(1))
    screens = re.search(r"solo_pause_screens\[\] =\n\{\n(.*?)\n\};", source, re.S).group(1)
    campaign_lists = screens.count('"')  // 2
    patch = source[source.index("static long pause_list_patch("):]
    patch = patch[:patch.index("\n}\n")]
    # (the buttons one list adds: CHANGE TEAMS (first), SETTINGS, and END GAME or FRIENDLY FIRE)
    assert "added = first + (host || build.coop ? 2 : 1)" in patch
    assert patch.count("next_tag()") == 3
    assert "pause_button(" in patch and "long text_tag = next_tag()" in source
    multiplayer = 3 * TAGS_PER_BUTTON
    coop_campaign = campaign_lists * 2 * TAGS_PER_BUTTON
    return reserved >= max(multiplayer, coop_campaign), reserved, multiplayer, coop_campaign


def test_reserve_covers_every_pause_patch():
    covers, reserved, multiplayer, coop_campaign = reserve_covers(SOURCE.read_text())
    assert covers, f"PAUSE_PATCH_TAGS {reserved}: multiplayer makes {multiplayer}, network co-op {coop_campaign}"


def test_negative_control_six_is_short_for_coop():
    source = re.sub(r"^#define PAUSE_PATCH_TAGS \d+", "#define PAUSE_PATCH_TAGS 6", SOURCE.read_text(), flags=re.M)
    assert not reserve_covers(source)[0]
