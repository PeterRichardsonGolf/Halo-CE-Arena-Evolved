#!/usr/bin/env python3
"""record: an MP4 clip of a headless game (the capture feature, queue item 50).

The game records when it supports HALO_RECORD_SECONDS / HALO_RECORD_DIR and ffmpeg is on the
machine. Until both hold, recording is a clean no-op: the game runs as asked and its result says
"record: skipped (<why>)". run.py takes --record SECONDS; this tool is run.py with --record set:

    python3 tools/ae_test/record.py --build <rev> --map bloodgulch --seconds 20 [--box]
"""
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

RECORD_VARIABLE = b"HALO_RECORD_SECONDS"


def game_supports_record(binary):
    """the binary names HALO_RECORD_SECONDS (read in 1 MiB pieces)"""
    try:
        with open(binary, "rb") as f:
            tail = b""
            while True:
                piece = f.read(1 << 20)
                if not piece:
                    return False
                if RECORD_VARIABLE in tail + piece:
                    return True
                tail = piece[-len(RECORD_VARIABLE):]
    except OSError:
        return False


def record_env(binary, seconds, video_dir, which=shutil.which):
    """{'status', 'env'}: the variables that ask the game for a clip, or a skip with the reason"""
    if not seconds:
        return {"status": "off", "env": {}}
    if not game_supports_record(binary):
        return {"status": "skipped (the game has no HALO_RECORD_SECONDS yet)", "env": {}}
    if not which("ffmpeg"):
        return {"status": "skipped (no ffmpeg on this machine)", "env": {}}
    Path(video_dir).mkdir(parents=True, exist_ok=True)
    return {"status": f"recording {seconds} s", "env": {"HALO_RECORD_SECONDS": str(seconds),
                                                         "HALO_RECORD_DIR": str(video_dir)}}


def main(argv):
    import run
    if "--seconds" in argv:
        i = argv.index("--seconds")
        argv = argv[:i] + ["--record", argv[i + 1]] + argv[i + 2:]
    elif not any(a.startswith("--record") for a in argv):
        argv = argv + ["--record", "20"]
    return run.main(argv, tool="record")


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
