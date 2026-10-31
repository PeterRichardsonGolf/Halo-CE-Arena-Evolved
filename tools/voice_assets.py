"""The built-in callout voice packs (port/assets/voices/<pack>/), which every
build copies next to the game as voices/ (as brokers.txt: network.brokers_file)."""

from pathlib import Path
from typing import List, Tuple

VOICES_DIR = Path("port/assets/voices")


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
