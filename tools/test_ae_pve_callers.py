"""Content Evolved's campaign score, campaign medals, service rank and Firefight stats (campaign_score_*, campaign_medals_*,
service_rank_*, firefight_stats_*) are called only by the files listed here. None of them is an engine file: they write
records, medals and scores, and each must stay out of multiplayer games (ae_pve_rules.h). A new call site, a damage, player,
network or HUD hook for instance, fails here, and needs a multiplayer-isolation review before it is added to the list."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCANNED = ("source", "port", "server")
SUFFIXES = (".c", ".h", ".inc", ".cpp", ".m", ".mm")
NAME = re.compile(r"\b(?:campaign_score_|campaign_medals_|firefight_stats_|service_rank_)[a-z_0-9]*")

ALLOWED = {
    # the systems themselves
    "port/linux/game/campaign_medals.c", "port/linux/game/campaign_medals.h",
    "port/linux/game/campaign_score.c", "port/linux/game/campaign_score.h",
    "port/linux/game/campaign_score_values.c", "port/linux/game/campaign_score_values.h",
    # the daily challenge's (a challenge's score is told to the rank; the call is made only for a campaign level won)
    "port/linux/game/community.c",
    "port/linux/game/firefight_stats.c", "port/linux/game/service_rank.c", "port/linux/game/service_rank.h",
    # Firefight (DEV MODE; behind firefight_active) and the skulls (a campaign or Firefight game)
    "port/linux/game/firefight.c", "port/linux/game/firefight_generators.c", "port/linux/game/firefight_hud.c",
    "port/linux/game/firefight_objectives.c", "port/linux/game/skulls.c",
    "port/linux/include/halo_firefight.h", "port/linux/game/ae_firefight_glue.h",
    # Forge (port/forge): at the main menu it asks whether the stats' loopback socket (open only with the community services'
    # opt-in, ae_pve_rules_stats_socket) was asked for a link code, and says it; it writes no record, medal, score or rank
    "port/forge/src/forge_hooks.c",
    # Arena Evolved's own campaign score and medals rules, and the settings table's names
    "port/linux/game/ae_campaign_medals_rules.c", "port/linux/game/ae_campaign_medals_rules.h",
    "port/linux/game/ae_campaign_score.c", "port/linux/game/ae_campaign_score.h",
    "port/linux/game/ae_campaign_score_rules.c", "port/linux/game/ae_campaign_score_rules.h",
    "port/linux/src/port_config.c",
    # the PVE menus and the end-of-mission card: they read a level's record and the score's values to draw them, and name the
    # score's settings as strings; none writes a record, medal, score or rank
    "port/linux/game/ae_glue_pve_menus.c", "port/linux/game/ae_pve_menus_rules.c",
    "port/linux/game/ae_pve_score_menus.h", "port/linux/game/ae_pve_score_menus.inc",
    "port/linux/game/ae_screen_end_card.h", "port/linux/game/ae_screen_scoring.c",
    "port/linux/game/ae_screen_scoring.h", "port/linux/game/ae_settings_rows.c",
}


def test_only_listed_files_name_the_pve_systems():
    found = set()
    for top in SCANNED:
        for path in (ROOT / top).rglob("*"):
            if path.suffix not in SUFFIXES or not path.is_file() or "/tests/" in path.as_posix():
                continue
            if NAME.search(path.read_text(errors="ignore")):
                found.add(path.relative_to(ROOT).as_posix())
    extra = sorted(found - ALLOWED)
    assert not extra, (
        "these files call the campaign score, medals, service rank or Firefight stats; each needs a multiplayer-isolation "
        "review (ae_pve_rules.h), then a line in ALLOWED: " + ", ".join(extra))
