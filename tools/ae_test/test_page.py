#!/usr/bin/env python3
"""One page per list run for the tester: what the automation measured beside what you saw.

    python3 tools/ae_test/test_page.py [<install>] <list id> --preflight [host:]ROOT [--out <pages folder>]

It reads the list and your results from the install (test-runs/<list>.txt, .results.jsonl, your shots), and the
preflight's run from ROOT (preflight.py's --root, here or on another machine): <list>.preflight.json, <list>/report.json,
<list>/summary.json and the scene pictures in frames/. It writes <out>/<list>-<build>.html with its pictures in
<out>/<list>-<build>/, and prints the page's path. Local files only; you decide whether to publish a page.

On the page, a test a row: its checks, each MEASURED (an expect line judged it: its rule, PASS or FAIL and why), PROXY
(an expect line that stands in for the check: report_words.py) or LOOK (your eyes only); your verdict and note; the
preflight's picture of the scene beside your shot. Above: the list, its revision, the build, the way it was played and
the counts (STAGED · MEASURED · PROXY · LOOK). Below: what the preflight found before your round, and how well the
automation called your verdicts on this build (testenv.py join).
"""
import argparse
import html
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import report_words  # noqa: E402
import testenv  # noqa: E402

FONTS = HERE.parent.parent / "port" / "assets" / "fonts"

# the AE menus' own colours (port/linux/game/ae_style.h)
STYLE = """
@font-face { font-family: "Overpass"; font-weight: 750; src: url("fonts/Overpass-750.ttf"); }
@font-face { font-family: "Overpass"; font-weight: 900; src: url("fonts/Overpass-900.ttf"); }
:root {
  --page: #0c1218; --panel: #181f26; --row: #202830; --rule: rgba(200, 214, 224, .20);
  --text: #cdd6dd; --title: #eef3f6; --muted: #84939f; --accent: #5fc8ec; --selection: #e8eef2;
  --selection-text: #0f171e; --destructive: #ec8074; --warning: #2894fe;
  --gap: 24px; --tight: 8px;
}
* { box-sizing: border-box; }
html { background: var(--page); color: var(--text); font: 750 16px/1.45 "Overpass", system-ui, sans-serif;
  font-variant-numeric: tabular-nums; scrollbar-color: #b9c7d2 var(--page); }
::selection { background: rgba(95, 200, 236, .43); color: var(--title); }
body { margin: 0 auto; padding: 40px 48px 64px; max-width: 1840px; }
a { color: var(--accent); text-underline-offset: 3px; }
a:focus-visible, button:focus-visible { outline: 2px solid var(--accent); outline-offset: 3px; }
h1 { font-weight: 900; font-size: 2.25rem; line-height: 1.1; letter-spacing: -0.01em; color: var(--title); margin: 0 0 6px; }
h2 { font-weight: 900; font-size: 1.05rem; letter-spacing: .02em; color: var(--muted); margin: 40px 0 12px; }
.facts { color: var(--muted); margin: 0; }
.facts b { color: var(--text); font-weight: 750; }
.counts { display: flex; flex-wrap: wrap; gap: 6px 28px; margin: 18px 0 0; padding: 14px 18px; background: var(--panel); }
.counts span { color: var(--muted); }
.counts b { color: var(--title); font-weight: 900; font-size: 1.15rem; margin-left: 6px; }
.found { margin: 0; padding: 14px 18px; background: var(--panel); list-style: none; }
.found li { padding: 4px 0; }
.found li + li { border-top: 1px solid var(--rule); }
.test { display: grid; grid-template-columns: minmax(0, 1.25fr) minmax(0, 1fr); gap: var(--gap); padding: 20px;
  background: var(--row); margin-bottom: 10px; }
.test h3 { font-weight: 900; font-size: 1.2rem; color: var(--title); margin: 0; }
.test .id { color: var(--muted); font-size: .9rem; }
.tester { margin: 10px 0 14px; display: flex; gap: 10px; align-items: baseline; flex-wrap: wrap; }
.note { color: var(--title); }
.checks { width: 100%; border-collapse: collapse; }
.checks td { padding: 8px 10px 8px 0; vertical-align: top; border-top: 1px solid var(--rule); }
.checks td.n { color: var(--muted); width: 1.5em; }
.checks td.kind { width: 8.5em; }
.rule { color: var(--muted); font-size: .9rem; margin-top: 2px; overflow-wrap: anywhere; }
.chip { display: inline-block; padding: 1px 8px 0; font-weight: 900; font-size: .78rem; letter-spacing: .06em;
  border: 1.5px solid currentColor; white-space: nowrap; }
.chip.filled { background: var(--selection); color: var(--selection-text); border-color: var(--selection); }
.chip.fail { color: var(--destructive); }
.chip.measured { color: var(--accent); }
.chip.proxy { color: var(--warning); }
.chip.look, .chip.none { color: var(--muted); }
.pictures { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; align-content: start; }
.pictures figure { margin: 0; }
.pictures img { width: 100%; aspect-ratio: 16 / 9; object-fit: cover; display: block; background: var(--page); }
.pictures figcaption { color: var(--muted); font-size: .85rem; margin-top: 4px; }
.pictures.one { grid-template-columns: 1fr; }
.pictures .none { color: var(--muted); font-size: .85rem; }
.first { margin: 0; padding: 14px 18px; background: var(--panel); list-style: none; columns: 2 420px; }
.first li { padding: 3px 0; break-inside: avoid; }
.legend { color: var(--muted); font-size: .9rem; margin: 8px 0 0; }
.legend b { font-weight: 750; color: var(--text); }
.pictures .empty { aspect-ratio: 16 / 9; display: grid; place-items: center; color: var(--muted); background: var(--page);
  font-size: .9rem; padding: 12px; text-align: center; }
.calibration { padding: 14px 18px; background: var(--panel); margin: 0; }
footer { color: var(--muted); margin-top: 40px; font-size: .9rem; }
@media (max-width: 1200px) {
  body { padding: 24px 16px 48px; }
  .test { grid-template-columns: 1fr; }
}
"""


def esc(text):
    return html.escape(str(text), quote=True)


def fetch_text(source, relative):
    return testenv.read_remote(source, relative)


def fetch_frames(source, list_id, target):
    """The preflight's scene pictures of the list into target: {test id: file name}."""
    target.mkdir(parents=True, exist_ok=True)
    if re.match(r"^[A-Za-z0-9_.-]+:", source) and not Path(source).exists():
        host, folder = source.split(":", 1)
        subprocess.run(["rsync", "-rq", "-e", "ssh -o ConnectTimeout=8", f"{host}:{folder.rstrip('/')}/frames/",
                        str(target) + "/", "--include", f"{list_id}-*.png", "--exclude", "*"], capture_output=True)
    else:
        for picture in (Path(source).expanduser() / "frames").glob(f"{list_id}-*.png"):
            shutil.copy2(picture, target / picture.name)
    return {path.stem[len(list_id) + 1:]: path.name for path in target.glob(f"{list_id}-*.png")}


def tester_records(results_text, list_id, build):
    """The tester's last PASS, FAIL or SKIP per test, on this build when there is one: {test: record}."""
    same, other = {}, {}
    for line in (results_text or "").splitlines():
        try:
            record = json.loads(line)
        except ValueError:
            continue
        if record.get("list") != list_id or record.get("verdict") not in ("PASS", "FAIL", "SKIP"):
            continue
        (same if build and str(record.get("build", ""))[:8] == build[:8] else other)[record.get("test")] = record
    return {test: same.get(test) or other.get(test) for test in set(same) | set(other)}


def chip(word, kind):
    return f'<span class="chip {kind}">{esc(word)}</span>'


def check_rows(test, rows, proxies):
    out = []
    for row in rows:
        why = proxies.get((test, row.get("check")))
        if not row.get("rule"):
            kind = chip("LOOK", "look")
            detail = '<div class="rule">your eyes only</div>'
        else:
            kind = chip("PROXY", "proxy") if why else chip("MEASURED", "measured")
            verdict = chip(row.get("verdict", ""), "filled" if row.get("verdict") == "PASS" else "fail")
            detail = (f'<div class="rule">{verdict} expect {esc(row["rule"])}&nbsp;·&nbsp; found {esc(row.get("reason", ""))}'
                      + (f"&nbsp;·&nbsp; stands in for the check: {esc(why)}" if why else "") + "</div>")
        out.append(f'<tr><td class="n">{esc(row.get("check", ""))}</td><td class="kind">{kind}</td>'
                   f'<td>{esc(row.get("text", ""))}{detail}</td></tr>')
    return "\n".join(out)


def render(info, record, report, summary, verdicts, frames, shots, joined, folder_name):
    list_id = info["id"]
    rows_by_test = {}
    for row in report:
        rows_by_test.setdefault(row.get("test"), []).append(row)
    # (one classifier for the chips and the counts: report_words.proxy_reason over each expect line and its check)
    proxies = {(row.get("test"), row.get("check")): report_words.proxy_reason(row.get("rule", ""), row.get("text", ""))
               for row in report if row.get("rule")}
    proxies = {key: why for key, why in proxies.items() if why}
    build = record.get("build") or "unknown"
    way = "the way you play it (pad presses, one game)" if record.get("mode") == "owner" else "automatically"
    counts = dict(summary or record.get("counts") or {})
    if report:
        # (the counts from the report's rows, with the classifier the chips use: a summary written by an older
        # run_list.py counted otherwise; STAGED and FRAME stay the run's own)
        again = report_words.count(report, [test["id"] for test in info["tests"]], [])
        counts.update({key: again[key] for key in ("measured_passed", "measured", "proxy", "look")})
        counts.setdefault("staged", again["staged"])
        counts.setdefault("tests", again["tests"])
    when = str(record.get("time", "?")).replace("T", " ")[:16]
    played = [mine for mine in verdicts.values() if mine]
    unnamed = played and all(str(mine.get("build", ""))[:8] != build[:8] for mine in played)
    count_line = "".join(f"<span>{label}<b>{value}</b></span>" for label, value in (
        ("STAGED", f"{counts.get('staged', '?')}/{counts.get('tests', '?')}"),
        ("MEASURED", f"{counts.get('measured_passed', '?')}/{counts.get('measured', '?')}"),
        ("PROXY", counts.get("proxy", "?")), ("LOOK", counts.get("look", "?")))
        + ((("RETIRED", counts["retired"]),) if counts.get("retired") else ())
        + ((("FRAME", counts["frame"]),) if counts.get("frame") else ()))
    tests_html, first_items = [], []
    for test in info["tests"]:
        test_id = test["id"]
        mine = verdicts.get(test_id)
        if mine:
            kind = "filled" if mine["verdict"] == "PASS" else "fail" if mine["verdict"] == "FAIL" else "none"
            other = str(mine.get("build", ""))[:8] != build[:8] and not unnamed
            on_build = f' (on build {esc(mine.get("build") or "unknown")})' if other else ""
            verdict_line = (f'{chip(mine["verdict"], kind)}<span class="note">{esc(mine.get("note") or "")}</span>'
                            f'<span class="id">{on_build}</span>')
        else:
            verdict_line = f'{chip("NOT PLAYED", "none")}'
        frame = frames.get(test_id)
        shot = shots.get(test_id)
        frame_html = (f'<a href="{esc(folder_name)}/{esc(frame)}"><img src="{esc(folder_name)}/{esc(frame)}" '
                      f'alt="Preflight picture of {esc(test_id)}" loading="lazy"></a>' if frame else
                      '<div class="empty">No preflight picture</div>')
        shot_html = (f'<a href="{esc(folder_name)}/{esc(shot)}"><img src="{esc(folder_name)}/{esc(shot)}" '
                     f'alt="Your shot of {esc(test_id)}" loading="lazy"></a>' if shot else "")
        rows_here = rows_by_test.get(test_id, [])
        auto_fail = any(row.get("rule") and row.get("verdict") != "PASS" for row in rows_here)
        measured_pass = [row for row in rows_here if row.get("rule") and row.get("verdict") == "PASS"
                         and (test_id, row.get("check")) not in proxies]
        proxy_pass = [row for row in rows_here if row.get("verdict") == "PASS" and (test_id, row.get("check")) in proxies]
        if (mine and mine["verdict"] == "FAIL") or auto_fail:
            words = []
            if mine and mine["verdict"] == "FAIL":
                words.append("you failed it" + (f": {mine.get('note')}" if mine.get("note") else ""))
            if auto_fail:
                words.append("an expect line failed")
            elif mine and mine["verdict"] == "FAIL" and measured_pass:
                words.append("the automation passed it")
            if mine and mine["verdict"] == "FAIL" and proxy_pass:
                words.append("only a proxy passed: " + ", ".join(f"#{row['check']}" for row in proxy_pass))
            first_items.append(f'<li><a href="#{esc(test_id)}">{esc(test.get("title") or test_id)}</a>: '
                               f'{esc("; ".join(words))}</li>')
        tests_html.append(f"""<section class="test" id="{esc(test_id)}">
<div>
<h3>{esc(test.get("title") or test_id)}</h3><span class="id">{esc(test_id)}</span>
<div class="tester"><span class="id">You:</span>{verdict_line}</div>
<table class="checks">{check_rows(test_id, rows_by_test.get(test_id, []), proxies)}</table>
</div>
<div class="pictures{"" if shot else " one"}">
<figure>{frame_html}<figcaption>Preflight, before you played</figcaption></figure>
{f'<figure>{shot_html}<figcaption>Your shot</figcaption></figure>' if shot else ""}
</div>
</section>""")
    found = summary.get("found") or []
    found_html = ("<ul class=\"found\">" + "".join(f"<li>{esc(line)}</li>" for line in found) + "</ul>") if found else \
        '<p class="calibration">The preflight found nothing to fix before your round.</p>'
    calibration = "<br>".join(esc(line.strip()) for line in testenv.join_lines(list_id, joined, joined.get("retired", []))[-2:])
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{esc(info.get("title") or list_id)}: test run</title>
<style>{STYLE}</style>
</head>
<body>
<header>
<h1>{esc(info.get("title") or list_id)}</h1>
<p class="facts"><b>{esc(list_id)}</b>&nbsp;·&nbsp; revision <b>{esc(info.get("revision", 0))}</b>&nbsp;·&nbsp; build <b title="{esc(build)}">{esc(build[:8])}</b>{("&nbsp;·&nbsp; program <b>" + esc(record.get("program")) + "</b>") if record.get("program") else ""}&nbsp;·&nbsp; the preflight played it {esc(way)} at {esc(when)}</p>
{'<p class="facts">Your verdicts below were played on builds that did not name themselves, so they are shown, not scored.</p>' if unnamed else ""}
<div class="counts">{count_line}</div>
<p class="legend"><b>MEASURED</b>: an expect line judged the check. <b>PROXY</b>: it judged something that stands in for
the check (a command's answer, a screen opening). <b>LOOK</b>: only your eyes judged it.</p>
</header>
<main>
<h2>Look here first</h2>
{('<ul class="first">' + "".join(first_items) + "</ul>") if first_items else
 '<p class="calibration">No test failed for you or for the automation.</p>'}
<h2>Found by the preflight before you</h2>
{found_html}
<h2>The tests</h2>
{"".join(tests_html)}
<h2>How well the automation called your verdicts on this build</h2>
<p class="calibration">{calibration}</p>
</main>
<footer>Made by tools/ae_test/test_page.py from the list, your results and the preflight's run.</footer>
</body>
</html>
"""


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("install", type=Path)
    parser.add_argument("list")
    parser.add_argument("--preflight", required=True)
    parser.add_argument("--out", type=Path, default=os.environ.get("AE_TEST_PAGES"),
                        help="the pages folder (default: $AE_TEST_PAGES)")
    words = list(argv) if argv is not None else sys.argv[1:]
    install_from_env = os.environ.get("AE_TEST_INSTALL")
    positional = []
    skip = False
    for word in words:
        if skip:
            skip = False
        elif word in ("--preflight", "--out"):
            skip = True
        elif not word.startswith("-"):
            positional.append(word)
    if install_from_env and len(positional) == 1:
        words.insert(0, install_from_env)
    args = parser.parse_args(words)
    if args.out is None:
        parser.error("--out is required when AE_TEST_PAGES is unset")
    install, out = args.install.expanduser(), args.out.expanduser()
    list_path = install / "test-runs" / f"{args.list}.txt"
    if not list_path.exists():
        print(f"{args.list}: not in {install / 'test-runs'}")
        return 1
    list_text = list_path.read_text(encoding="utf-8", errors="replace")
    info = testenv.parse_list(list_text)
    info["title"] = ""
    titles, current = {}, None
    for line in list_text.splitlines():
        words = line.strip().split(None, 1)
        if words[:1] == ["test"] and len(words) > 1:
            current = words[1]
        elif words[:1] == ["title"] and len(words) > 1:
            if current:
                titles.setdefault(current, words[1])
            elif not info["title"]:
                info["title"] = words[1]
    for test in info["tests"]:
        test["title"] = titles.get(test["id"], "")
    record = testenv.read_preflight(args.preflight, args.list) or {}
    try:
        report = json.loads(fetch_text(args.preflight, f"{args.list}/report.json") or "[]")
        summary = json.loads(fetch_text(args.preflight, f"{args.list}/summary.json") or "{}")
    except ValueError:
        report, summary = [], {}
    build = record.get("build") or "unknown"
    name = f"{args.list}-{re.sub(r'[^A-Za-z0-9_.-]', '_', build)}"
    folder = out / name
    (out / "fonts").mkdir(parents=True, exist_ok=True)
    for font in ("Overpass-750.ttf", "Overpass-900.ttf"):
        if not (out / "fonts" / font).exists() and (FONTS / font).exists():
            shutil.copy2(FONTS / font, out / "fonts" / font)
    frames = fetch_frames(args.preflight, args.list, folder)
    results = install / "test-runs" / f"{args.list}.results.jsonl"
    results_text = results.read_text(errors="replace") if results.exists() else ""
    verdicts = tester_records(results_text, args.list, build)
    shots = {}
    for test, mine in verdicts.items():
        shot = mine.get("shot") if mine else ""
        if shot:
            source = (install / shot) if not Path(shot).is_absolute() else Path(shot)
            if not source.exists():
                found = list((install / "test-runs" / "shots").glob(Path(shot).name))
                source = found[0] if found else source
            if source.exists():
                shutil.copy2(source, folder / f"shot-{test}{source.suffix}")
                shots[test] = f"shot-{test}{source.suffix}"
    joined = testenv.join(list_text, results_text, report, record.get("build", ""))
    joined["retired"] = testenv.retired_lines(list_text)
    page = out / f"{name}.html"
    page.write_text(render(info, record, report, summary, verdicts, frames, shots, joined, name), encoding="utf-8")
    print(page)
    return 0


if __name__ == "__main__":
    sys.exit(main())
