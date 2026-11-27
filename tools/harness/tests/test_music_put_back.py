"""MY PLAYLIST's BACK puts a held song back, and ends when the list no longer reaches its place."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

PROGRESS = ("\t\tif (pick.order.list.focus == before)\n\t\t\tbreak;\n", "\t\t(void)before;\n")


def generated(fault=None):
    code = function(read("port/linux/game/ae_screen_music.c"), "order_put_back") + "\n"
    if fault:
        code = mutated(code, *fault)
    return (("under_test.inc", code),)


def test_back():
    status, output = run(build("music_put_back", generated()), "back")
    assert status == 0, output


def test_negative_control():
    status, output = run(build("music_put_back", generated(PROGRESS)), "back")
    assert status == CHECK_FAILED, output
