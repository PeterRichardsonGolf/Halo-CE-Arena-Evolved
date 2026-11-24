"""(0.9.3) AE TOURNAMENT's games choose their START for the run only: arena_gametype_starts.txt keeps the player's own, so a crash
mid-game leaves nothing of the tournament's choice behind. Static checks of the two sides (the game itself is the owner's check)."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAMETYPES = (ROOT / "source" / "saved games" / "arena_gametypes.c").read_text()
GLUE = (ROOT / "port" / "linux" / "game" / "ae_glue_social_game.c").read_text()


def body(text, start, end):
    i = text.index(start)
    return text[i:text.index(end, i)]


def test_choose_writes_the_file_only_when_not_run_only():
    choose = body(GAMETYPES, "boolean arena_gametype_start_choose(", "\n}\n")
    assert re.search(r"if \(!arena_gametype_starts_run_only\)\s*arena_gametype_starts_write_file\(\);", choose), choose


def test_the_writer_is_called_nowhere_else_unguarded():
    calls = [m.start() for m in re.finditer(r"arena_gametype_starts_write_file\(\);", GAMETYPES)]
    assert len(calls) == 1, calls


def test_the_tournament_turns_run_only_on_at_its_start_and_off_when_it_puts_settings_back():
    start = body(GLUE, "int ae_social_plan_start(", "weapons_force(plan);")
    assert "arena_gametype_starts_set_run_only(TRUE);" in start
    restore = body(GLUE, "static void settings_restore(void)", "\n}\n")
    assert restore.index("arena_gametype_start_choose(wide);") < restore.index("arena_gametype_starts_set_run_only(FALSE);")
