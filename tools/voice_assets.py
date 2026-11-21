"""The built-in callout voice packs (port/assets/voices/<pack>/) and announcer packs
(port/assets/announcers/<pack>/), which every build copies next to the game as voices/ and
announcers/ (as brokers.txt: network.brokers_file)."""

from pathlib import Path
from typing import List, Tuple

VOICES_DIR = Path("port/assets/voices")
ANNOUNCERS_DIR = Path("port/assets/announcers")


def voice_files() -> List[Tuple[Path, Path]]:
    """(source, path under voices/) of each file of each pack"""
    return [(p, p.relative_to(VOICES_DIR)) for p in sorted(VOICES_DIR.rglob("*")) if p.is_file()]


def voices_build(n, rule: str, build_dir: Path) -> List[Path]:
    """the ninja copy steps of the packs into build_dir/voices; the outputs"""
    outputs = []
    for source, relative in voice_files():
        out = build_dir / "voices" / relative
        n.build(outputs=out, rule=rule, inputs=source)
        outputs.append(out)
    return outputs


def announcer_files() -> List[Tuple[Path, Path]]:
    """(source, path under announcers/) of each file of each announcer pack"""
    return [(p, p.relative_to(ANNOUNCERS_DIR)) for p in sorted(ANNOUNCERS_DIR.rglob("*")) if p.is_file()]


def announcers_build(n, rule: str, build_dir: Path) -> List[Path]:
    """the ninja copy steps of the announcer packs into build_dir/announcers; the outputs"""
    outputs = []
    for source, relative in announcer_files():
        out = build_dir / "announcers" / relative
        n.build(outputs=out, rule=rule, inputs=source)
        outputs.append(out)
    return outputs
