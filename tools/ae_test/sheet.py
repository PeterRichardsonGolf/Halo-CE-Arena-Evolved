#!/usr/bin/env python3
"""sheet: split-screen screenshots, one labelled full-size PNG per case, before vs after.

    python3 tools/ae_test/sheet.py --after <rev> [--before <rev>] [--cases 1p,2p,3p,4p,4p-720] [--reticle [--flat]]
        [--bots 7] [--env HALO_MATCH_CLOCK=both ...] --box

Each case is one Blood Gulch game (MATCH CLOCK BOTH, TIMERS on, PC menus, windowed) with 1-4 local players at
1920x1080 (the "-720" cases at 1280x720), a screenshot every 100 frames; the picked frame is the last one that is
not black. Written to the output folder:

    <case>-after.png, <case>-before.png    each build's frame, full size, a label bar on top
    <case>.png                             before above after (only with --before)
    sheet.json                             the picked frames, per-view reticle offsets (--reticle)

--reticle measures, per view, the reticle's offset from the view's centre (the biggest near-white blob
near the middle). Use --flat (HALO_GPU_DEBUG_FLAT=1: the world in flat colour) for a clean measure; on a
normal render the sky or a light can be taken for the reticle. Labelling needs Pillow (on the machine
that composes: with --box the games run on the box and the images are composed here); without Pillow
the frames are copied unlabelled.
"""
import argparse
import json
import shutil
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import harness  # noqa: E402

CASES = {
    "1p": (1, "1920x1080"), "2p": (2, "1920x1080"), "3p": (3, "1920x1080"), "4p": (4, "1920x1080"),
    "1p-720": (1, "1280x720"), "2p-720": (2, "1280x720"), "3p-720": (3, "1280x720"), "4p-720": (4, "1280x720"),
}
DEFAULT_CASES = "1p,2p,3p,4p,4p-720"
BASE_ENV = {"HALO_MATCH_CLOCK": "both", "HALO_PERFORMANCE": "minimal"}


def case_spec(case, build, bots=0, env=None, flat=False, exit_after=40, every=100):
    players, window = CASES[case]
    e = dict(BASE_ENV)
    if flat:
        e["HALO_GPU_DEBUG_FLAT"] = "1"
    e.update(env or {})
    return harness.parse_spec({"name": case, "build": build, "map": "bloodgulch", "start": 25, "flags": 25100321,
                               "local_players": players, "menus": "pc", "window": window, "exit_after": exit_after,
                               "screenshots": every, "bots": bots, "env": e, "expect": {"ticks": 0}})


def play_all(cfg, a, out):
    builds = [("after", a.after)] + ([("before", a.before)] if a.before else [])
    jobs = [(case, label, rev) for case in a.cases.split(",") for label, rev in builds]
    for case, _, _ in jobs:
        if case not in CASES:
            raise SystemExit(f"ae_test sheet: unknown case {case} (known: {', '.join(CASES)})")
    for _, rev in builds:
        harness.resolve_build(cfg, rev)
    parallel = harness.resolve_parallel(cfg, a.parallel)
    slots = harness.GameSlots(cfg, parallel)
    env = dict(kv.partition("=")[::2] for kv in (a.env or []))
    meta = {"after": a.after, "before": a.before, "cases": {}}

    def one(job):
        case, label, rev = job
        spec = case_spec(case, rev, a.bots, env, a.flat)
        r = harness.play(cfg, spec, out / "runs" / f"{case}-{label}", slots, a.keep_work, run_id=f"{out.name}-{label}")
        pngs = [out / "runs" / f"{case}-{label}" / s for s in r.get("shots", [])]
        frame = harness.pick_frame(pngs) if pngs else None
        entry = {"status": r.get("status"), "why": r.get("why"), "build_label": r.get("build_label"),
                 "frame": str(frame.relative_to(out)) if frame else None}
        if frame and a.reticle:
            w, h, rgb = harness.read_png(frame)
            entry["reticle"] = harness.reticle_offsets(w, h, rgb, CASES[case][0])
        harness.log(f"  {case} {label}: {r.get('status')} frame {entry['frame']}")
        return case, label, entry

    with ThreadPoolExecutor(max_workers=parallel) as ex:
        for case, label, entry in ex.map(one, jobs):
            meta["cases"].setdefault(case, {})[label] = entry
    harness.write_json(out / "sheet.json", meta)
    return meta


def reticle_text(entry):
    parts = []
    for v in entry.get("reticle") or []:
        parts.append(f"v{v['view']} " + (f"{v['offset'][0]:+.0f},{v['offset'][1]:+.0f}" if v.get("found") else "-"))
    return ("  reticle " + " ".join(parts)) if parts else ""


def compose(out):
    """the labelled PNGs from sheet.json (needs Pillow; else the frames are copied as they are)"""
    meta = json.loads((out / "sheet.json").read_text())
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        Image = None
    written = []
    for case, by in meta["cases"].items():
        singles = []
        for label in ("before", "after"):
            e = by.get(label)
            if not e or not e.get("frame"):
                continue
            src = out / e["frame"]
            dst = out / f"{case}-{label}.png"
            text = f"{case} {CASES[case][1]}  {label}: {e.get('build_label') or meta.get(label)}{reticle_text(e)}"
            if Image is None:
                shutil.copy2(src, dst)
            else:
                im = Image.open(src).convert("RGB")
                bar = 36 if im.width >= 1600 else 28
                canvas = Image.new("RGB", (im.width, im.height + bar), (0, 0, 0))
                canvas.paste(im, (0, bar))
                draw = ImageDraw.Draw(canvas)
                try:
                    font = ImageFont.truetype("DejaVuSans.ttf", bar - 12)
                except OSError:
                    font = ImageFont.load_default()
                draw.text((10, 5), text, fill=(255, 255, 255), font=font)
                canvas.save(dst)
                singles.append(canvas)
            written.append(dst)
        if Image is not None and len(singles) == 2:
            w = max(s.width for s in singles)
            both = Image.new("RGB", (w, sum(s.height for s in singles) + 8), (40, 40, 40))
            both.paste(singles[0], (0, 0))
            both.paste(singles[1], (0, singles[0].height + 8))
            both.save(out / f"{case}.png")
            written.append(out / f"{case}.png")
    if Image is None:
        harness.log("ae_test sheet: no Pillow here: frames copied without labels")
    return written


def summary(meta):
    lines = []
    for case, by in meta["cases"].items():
        for label, e in by.items():
            lines.append(f"{case:7} {label:6} {e.get('status')}  {e.get('frame')}{reticle_text(e)}")
    return "\n".join(lines)


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    harness.add_common_args(p)
    p.add_argument("--after", help="the build under test")
    p.add_argument("--before", help="the build to compare with")
    p.add_argument("--cases", default=DEFAULT_CASES, help=f"comma-separated (default {DEFAULT_CASES}; "
                                                          f"known {', '.join(CASES)})")
    p.add_argument("--bots", type=int, default=0)
    p.add_argument("--env", action="append", help="NAME=value for every game (repeat)")
    p.add_argument("--reticle", action="store_true", help="measure the reticle's offset per view")
    p.add_argument("--flat", action="store_true", help="flat-colour world (HALO_GPU_DEBUG_FLAT=1)")
    p.add_argument("--parallel", default=None)
    p.add_argument("--no-compose", action="store_true", help="play only (sheet.json); compose elsewhere")
    p.add_argument("--compose", help="compose the labelled PNGs of an earlier run's folder")
    a = p.parse_args(argv)
    cfg = harness.load_config(a.config)
    if a.compose:
        for f in compose(Path(a.compose)):
            print(f)
        return 0
    if a.box:
        rc, local = harness.remote(cfg, "sheet", argv + ["--no-compose"])
        if local and (local / "sheet.json").exists():
            for f in compose(local):
                print(f)
        return rc
    if not a.after:
        raise SystemExit("ae_test sheet: --after is needed")
    harness.apply_load_policy(cfg)
    out = harness.new_out_dir(cfg, "sheet", None, a.out_name)
    meta = play_all(cfg, a, out)
    (out / "summary.txt").write_text(summary(meta) + "\n")
    print(summary(meta))
    if not a.no_compose:
        for f in compose(out):
            print(f)
    print(f"ae_test: results in {out}")
    bad = [c for c, by in meta["cases"].items() for e in by.values() if not e.get("frame")]
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
