#!/usr/bin/env python3
"""bot_scorecard: count bot behaviour in Arena Evolved debug logs and compare it with a known-good baseline.

    python3 tools/ae_test/bot_scorecard.py debug.txt [more debug.txt ...] --save good.json
    python3 tools/ae_test/bot_scorecard.py debug.txt --baseline good.json [--all] [--per-game]

Only ``arena-evolved:`` bot, objective, seat, ride and fireteam lines are read.  Counts from several logs are
added, unless --per-game asks for one table per objective game.  A stopped behaviour is a failure, a rate below
half its baseline is a warning, and a newly active behaviour is informational.  Failures exit 1; bad input exits 2.
"""
import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

from bot_report import LINE, WORD

PREFIX = "arena-evolved: "
NOISE = {"pos", "ps", "snap"}
KEY_METRICS = ["ride.answered", "ride.orders", "seat.driver.scorpion", "ev.tank_offer", "ev.horn_get_in",
               "ev.vantage_go", "ev.vantage_hold", "ev.lane_wp", "item.sniper_rifle", "item.rocket_launcher",
               "seat.any", "ev.kill"]
SEAT = re.compile(r"bots: bot (\d+) took the (\w+) seat of an? (.+?)\s*$")
RIDE = re.compile(r"bots: ride: (?:t=(-?\d+) )?bot (\d+) gets kind (\S+), vehicle (\S+)"
                  r"(?:, caller (\S+))?\s*$")
FIRETEAM = re.compile(r"bots: fireteams: team \d+: (.*)$")
FIRETEAM_SIZE = re.compile(r"(\d+)\s*$")
ANSWER_TICKS = 30 * 30
# The summary lines carry counters that grow from 0 at each map's start (a new map starts them over): a game's figure is
# the last value of each stretch between starts over, added up. (Not last minus first: a minute line comes a minute in,
# and what happened before it would be lost; the first gate score failed 18 holds a game as 0.)
TICK_LINE = re.compile(r"bots: tick (\d+), (\d+) bots \(.*?\): .*?stuck (\d+) \(goals given up (\d+)\), kills/deaths(.*)$")
KILLS_DEATHS = re.compile(r"\d+:(\d+)/(\d+)")
FEAR_LINE = re.compile(r"bots: fear tick (\d+): squads formed (\d+) \(with a player (\d+)\), flanks (\d+) \(reached (\d+), "
                       r"no place (\d+)\), holds (\d+), suppressing ([\d.]+) s")
FALLS_LINE = re.compile(r"bots: falls tick (\d+): steps turned (\d+), held (\d+), goals given up at an edge (\d+), jumps held (\d+)")
TRAVEL_LINE = re.compile(r"bots: travel tick (\d+): teleports (\d+) \((\d+) planned, (\d+) pads missed\), jumps (\d+) \((\d+) missed\), "
                         r"walks (\d+) \((\d+) missed\)")
FIGHTS_LINE = re.compile(r"bots: fights since the map's start: grenades (\d+) .*?blows (\d+) .*?head aims (\d+), retreats (\d+), "
                         r"charges (\d+), jumps (\d+), halts (\d+)")
# Counters where more is worse: a rate that more than doubles is a regression, not a change.
BAD = {"ev.stuck", "sum.stuck", "sum.giveups", "falls.goals_given_up", "travel.jumps_missed", "drive.wedges",
       "drive.reverses", "drive.stuck_s", "drive.bumps", "ev.drive_crash", "ev.drive_wedge", "fear.flanks_no_place",
       "travel.pads_missed", "travel.walks_missed", "ev.climb_drop", "ev.objective_lost"}


def parse_summary(line):
    """One summary line (tick, fear, falls, travel, fights, drive_stats) as {"type": "summary", "src", "tick", "v"}."""
    match = TICK_LINE.fullmatch(line)
    if match:
        pairs = KILLS_DEATHS.findall(match.group(5))
        return {"type": "summary", "src": "tick", "tick": int(match.group(1)), "v": {
            "stuck": int(match.group(3)), "giveups": int(match.group(4)),
            "kills": sum(int(k) for k, _ in pairs), "deaths": sum(int(d) for _, d in pairs)}}
    for src, pattern, names in (
            ("fear", FEAR_LINE, ("squads", "person_squads", "flanks", "flanks_reached", "flanks_no_place", "holds",
                                 "suppress_s")),
            ("falls", FALLS_LINE, ("steps_turned", "steps_held", "goals_given_up", "jumps_held")),
            ("travel", TRAVEL_LINE, ("teleports", "teleports_planned", "pads_missed", "jumps", "jumps_missed", "walks",
                                     "walks_missed")),
            ("fights", FIGHTS_LINE, ("grenades", "blows", "head_aims", "retreats", "charges", "jumps", "halts"))):
        match = pattern.match(line)
        if match:
            return {"type": "summary", "src": src, "tick": int(match.group(1)) if src != "fights" else None,
                    "v": {name: float(value) for name, value in zip(names, match.groups()[1 if src != "fights" else 0:])}}
    if line.startswith("bots: ev=drive_stats "):
        words = dict(WORD.findall(line[len("bots: ev=drive_stats "):]))
        values = {}
        for key, value in words.items():
            try:
                values[key] = float(value)
            except ValueError:
                pass
        if values:
            return {"type": "summary", "src": "drive", "tick": None, "mode": words.get("mode", ""), "v": values}
    return None


def summary_deltas(records):
    """Metrics from the summary lines of one game: for each family, the last value of each stretch, added up."""
    series = {}
    for record in records:
        if record["type"] != "summary":
            continue
        key = (record["src"], record.get("mode", "")) if record["src"] == "drive" else (record["src"], "")
        series.setdefault(key, []).append(record["v"])
    metrics = {}
    drive = [(key, lines) for key, lines in series.items() if key[0] == "drive"]
    if drive:
        # (the mode that drove longest, when a run logs both)
        series = {k: v for k, v in series.items() if k[0] != "drive"}
        key, lines = max(drive, key=lambda item: item[1][-1].get("drive_s", 0))
        series[key] = lines
    prefix = {"tick": "sum", "fear": "fear", "falls": "falls", "travel": "travel", "fights": "fight", "drive": "drive"}
    for (src, _), lines in series.items():
        for name in lines[-1]:
            total, previous = 0.0, None
            for line in lines:
                value = line.get(name)
                if value is None:
                    continue
                # (a value under the one before: a new map started the counter over; the stretch before it is done)
                if previous is not None and value < previous:
                    total += previous
                previous = value
            metrics[f"{prefix[src]}.{name}"] = total + (previous or 0.0)
    return metrics


def parse(path):
    """Relevant records from one log, retaining their file order and inferred ticks."""
    records = []
    last_bot_tick = None
    for whole in Path(path).read_text(errors="replace").splitlines():
        if not whole.startswith(PREFIX):
            continue
        line = whole[len(PREFIX):]
        match = LINE.fullmatch(line)
        if match:
            tick = int(match.group(1))
            last_bot_tick = tick
            extra = dict(WORD.findall(match.group(7)))
            records.append({"type": "event", "t": tick, "bot": int(match.group(2)),
                            "ev": match.group(4), **extra})
            continue
        summary = parse_summary(line)
        if summary:
            summary["t"] = last_bot_tick
            records.append(summary)
            continue
        if line.startswith("bots: ev="):
            words = dict(WORD.findall(line[6:]))
            if "ev" in words and "bot" in words:
                tick = int(words["t"]) if "t" in words else None
                records.append({**words, "type": "event", "t": tick, "bot": int(words["bot"])})
            continue
        if line.startswith("ae obj: "):
            words = dict(WORD.findall(line[8:]))
            if "ev" in words:
                records.append({**words, "type": "event", "t": int(words["t"]) if "t" in words else None})
            continue
        match = SEAT.fullmatch(line)
        if match:
            records.append({"type": "seat", "t": last_bot_tick, "bot": int(match.group(1)),
                            "seat": match.group(2), "class": "_".join(match.group(3).lower().split())})
            continue
        match = RIDE.fullmatch(line)
        if match:
            records.append({"type": "ride", "t": int(match.group(1)) if match.group(1) else last_bot_tick,
                            "bot": int(match.group(2))})
            continue
        match = FIRETEAM.fullmatch(line)
        if match:
            sizes = [int(size.group(1)) for member in match.group(1).split(",")
                     if (size := FIRETEAM_SIZE.search(member))]
            records.append({"type": "fireteam", "t": last_bot_tick,
                            "sizes": sizes})
    return records


def games(records):
    """Split records at objective game_start/game_end lines, or return the whole log when it has no boundaries."""
    if not any(r.get("ev") == "game_start" for r in records):
        return [records]
    result, current = [], None
    for record in records:
        if record.get("ev") == "game_start":
            if current:
                result.append(current)
            current = []
        if current is not None:
            current.append(record)
            if record.get("ev") == "game_end":
                result.append(current)
                current = None
    if current:
        result.append(current)
    return result


def score(records, keep_noise=False):
    """Return the minutes, metric counts and rates for a sequence of records."""
    metrics = Counter({"ride.orders": 0, "ride.answered": 0, "seat.any": 0, "fireteam.largest": 0})
    orders, seats, ticks = [], [], []
    for record in records:
        tick = record.get("t")
        if tick is not None:
            ticks.append(int(tick))
        if record["type"] == "event":
            kind = record["ev"]
            if keep_noise or kind not in NOISE:
                metrics["ev." + kind] += 1
            if kind == "item_take" and "item" in record:
                metrics["item." + record["item"]] += 1
            if kind == "seat" and "bot" in record:
                seats.append((int(record["bot"]), tick))
        elif record["type"] == "seat":
            metrics["seat." + record["seat"] + "." + record["class"]] += 1
            metrics["seat." + record["seat"]] += 1
            metrics["seat.any"] += 1
            seats.append((record["bot"], tick))
        elif record["type"] == "ride":
            metrics["ride.orders"] += 1
            orders.append((record["bot"], tick))
        elif record["type"] == "fireteam" and record["sizes"]:
            metrics["fireteam.largest"] = max(metrics["fireteam.largest"], max(record["sizes"]))
    metrics.update(summary_deltas(records))
    for bot, ordered_at in orders:
        if ordered_at is not None and any(other == bot and seated_at is not None and
                                          0 <= seated_at - ordered_at <= ANSWER_TICKS
                                          for other, seated_at in seats):
            metrics["ride.answered"] += 1
    minutes = max(1.0, (max(ticks) - min(ticks)) / 1800.0) if ticks else 1.0
    metrics["game.minutes"] = minutes
    return {"minutes": minutes, "metrics": dict(metrics), "rates": rates_of(metrics, minutes)}


def rates_of(metrics, minutes):
    """Rates a minute: of the game, except the drive counters (drive.*, not the drive time itself), which are a minute of
    driving (drive.drive_s / 60): a mode whose bots drive longer meets more trouble a game for that alone."""
    drive_minutes = metrics.get("drive.drive_s", 0) / 60.0
    rates = {}
    for name, value in metrics.items():
        per = drive_minutes if name.startswith("drive.") and name != "drive.drive_s" and drive_minutes > 0 else minutes
        rates[name] = value / per
    return rates


def combine(cards):
    """Add scorecards, except that the largest fireteam remains a maximum."""
    minutes = sum(card["minutes"] for card in cards)
    metrics = Counter()
    for card in cards:
        largest = card["metrics"].get("fireteam.largest", 0)
        metrics.update({k: v for k, v in card["metrics"].items() if k not in ("fireteam.largest", "game.minutes")})
        metrics["fireteam.largest"] = max(metrics["fireteam.largest"], largest)
    metrics["game.minutes"] = minutes
    return {"minutes": minutes, "metrics": dict(metrics), "rates": rates_of(metrics, minutes)}


# a behaviour "stopped" (FAIL) only from this many in the baseline: fewer is too few to tell from chance (11-12: the CTF
# scene's pistols picked up, 4 in two games of 0.8.1, 0 in the new build's two, then 1 and 1 in three games each; a count of
# 4 is 0 by chance about once in 55 samples, and a table has dozens of counters)
STOPPED_MINIMUM = 8


def verdict(name, base_count, current_count, base_rate, current_rate, minimum, drop, in_baseline):
    if name in BAD:
        # (more is worse: a rate that more than doubles, or a new one, is WORSE; less is never a failure)
        if in_baseline and base_rate > 0 and current_rate > base_rate * 2 and current_count >= minimum:
            return "WORSE doubled"
        # (a counter the baseline never logged, as drive_stats in older builds, cannot be judged: information only)
        if in_baseline and base_count == 0 and current_count >= minimum:
            return "WORSE new"
        return "OK"
    if name == "fireteam.largest" and in_baseline:
        return "FAIL exceed" if current_count > base_count + 1 else "OK"
    # (one item's pickups swing with a game's flow, never a failure alone: the CTF scene's shotguns 17, 7 and 0 in three
    # rounds of nearly the same build, 11-12; ev.item_take, all of them, still fails)
    if in_baseline and base_count >= max(minimum, STOPPED_MINIMUM) and current_count == 0 and not name.startswith("item."):
        return "FAIL stopped"
    if in_baseline and base_rate > 0 and current_rate < base_rate * drop:
        return "WARN fell"
    if (not in_baseline or base_count == 0) and current_count >= minimum:
        return "WARN new"
    return "OK"


def table(card, baseline=None, minimum=3, drop=0.5, show_all=False):
    """A Markdown comparison table and whether it contains a failure."""
    baseline = baseline or {"metrics": {}, "rates": {}}
    base_metrics, base_rates = baseline.get("metrics", {}), baseline.get("rates", {})
    names = set(card["metrics"]) | set(base_metrics) | set(KEY_METRICS)
    ordered = KEY_METRICS + sorted(names - set(KEY_METRICS))
    rows, failed = [], False
    for name in ordered:
        before, now = base_metrics.get(name, 0), card["metrics"].get(name, 0)
        before_rate, now_rate = base_rates.get(name, 0), card["rates"].get(name, 0)
        result = verdict(name, before, now, before_rate, now_rate, minimum, drop, name in base_metrics)
        failed |= result.startswith("FAIL")
        if name in KEY_METRICS or show_all or result.startswith(("FAIL", "WARN", "WORSE")):
            rows.append(f"| {name} | {before:g} | {now:g} | {before_rate:.2f} | {now_rate:.2f} | {result} |")
    head = ["| metric | baseline count | current count | baseline /min | current /min | verdict |",
            "|---|---:|---:|---:|---:|---|"]
    note = ["", "(/min: a minute of the game; drive.* except drive.drive_s: a minute of driving)"]
    return "\n".join(head + rows + note), failed


def load_baseline(path):
    data = json.loads(Path(path).read_text())
    if data.get("version") != 1 or not isinstance(data.get("metrics"), dict) or not isinstance(data.get("rates"), dict):
        raise ValueError("baseline is not a version 1 bot scorecard")
    # (rates again from the counts: a baseline saved before the drive counters went per drive minute)
    if data["metrics"].get("game.minutes"):
        data["rates"] = rates_of(data["metrics"], data["metrics"]["game.minutes"])
    return data


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("logs", nargs="+")
    parser.add_argument("--save")
    parser.add_argument("--baseline")
    parser.add_argument("--min", type=int, default=3, dest="minimum")
    parser.add_argument("--drop", type=float, default=0.5)
    parser.add_argument("--all", action="store_true", dest="show_all")
    parser.add_argument("--per-game", action="store_true")
    parser.add_argument("--keep-noise", action="store_true")
    args = parser.parse_args(argv)
    try:
        records = [parse(path) for path in args.logs]
        file_cards = [score(log, args.keep_noise) for log in records]
        game_cards = [score(game, args.keep_noise) for log in records for game in games(log)]
        aggregate = combine(game_cards if args.per_game else file_cards)
        baseline = load_baseline(args.baseline) if args.baseline else None
        if args.save:
            saved = {"version": 1, "source": args.logs[0] if len(args.logs) == 1 else ", ".join(args.logs), **aggregate}
            Path(args.save).write_text(json.dumps(saved, indent=2, sort_keys=True) + "\n")
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as error:
        print(f"bot_scorecard: {error}", file=sys.stderr)
        return 2
    shown = game_cards if args.per_game else [aggregate]
    failed = False
    for number, card in enumerate(shown, 1):
        if args.per_game:
            print(f"## Game {number}\n")
        text, this_failed = table(card, baseline, args.minimum, args.drop, args.show_all)
        print(text)
        if number != len(shown):
            print()
        failed |= this_failed
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
