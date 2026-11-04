"""Judges a TEST RUN list's expect lines against DEV MODE's event stream (port/linux/game/ae_dev_events.h).

    python3 tools/ae_test/expect.py <list.txt> <events.jsonl> [--json]

Each test's window is the stream from its "test ... start" event to its "verdict" (or the next test's start, or the
end of the stream). Each expect line of the test is judged in its window: PASS or FAIL, the reason, and the stream's
line numbers it rests on (the evidence). A check with no expect line is the owner's ("LOOK").

Rules (words; a <tag> matches a tag path that holds it, "_" and " " alike, any case; N is a whole number):

    weapon <tag> [loaded N] [total N] [player N]   the weapon in hand became <tag> (a pickup, a give), with those rounds
    ammo <tag> [loaded N] [total N] [player N]     the rounds of <tag> in hand went up to those
    spawn [player N]                               a player spawned
    death [player N]                               a player died
    kill [victim N] [killer N]                     a kill
    score [player N] [individual N] [team N]       a score reached those
    object <vehicle|item> <tag>                    one was made (placed, spawned, respawned)
    gone <vehicle|item> <tag>                      one went (taken, destroyed and removed)
    respawn <vehicle|item> <tag> <seconds> [+-S]   one went and one of its tag was made again that long after (+-1 s
                                                   unless given)
    hud <text>                                     a HUD message holding the text
    log <text>                                     a line the game wrote to debug.txt holding the text ("ae needler: MCC")
    option <row> <value>                           a dev option command set that GAME OPTIONS row to that value
    cmd <text> [ok|error]                          a dev command whose line holds the text answered so
    game_over                                      the game ended
    no <rule>                                      the rule is never met in the window

Player numbers are slots: 0 is the first local player. A rule that cannot be judged (unknown words, a key with no
number) is BAD RULE, a failure, "no" before it or not. A test's window opens at its "start" event: after the reset
and the settling, just before its setup lines go."""
import json
import re
import sys
from pathlib import Path

TICKS_PER_SECOND = 30


def norm(text):
    return re.sub(r"[_\s]+", " ", str(text)).strip().lower()


def short(path):
    """a tag path's last name, as the owner says it ("weapons\\sniper rifle\\sniper rifle": "sniper rifle")"""
    return str(path).replace("/", "\\").split("\\")[-1].replace("_", " ")


def tag_has(path, part):
    return norm(part) in norm(path)


def parse_list(text):
    """The tests of a list (the parser's rules for groups, checks and expects, kept small): [{id, checks, expects}]"""
    tests, current = [], None
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        key, value = parts[0], parts[1].strip() if len(parts) > 1 else ""
        if key == "group":
            current = None
        elif key == "test":
            current = {"id": value, "checks": [], "expects": {}}
            tests.append(current)
        elif current and key == "check":
            current["checks"].append(value)
        elif current and key == "expect":
            parts = value.split(None, 1)
            if parts and parts[0].isdigit():
                current["expects"][int(parts[0])] = parts[1].strip() if len(parts) > 1 else ""
    return tests


def read_events(path):
    events = []
    for number, raw in enumerate(Path(path).read_text(errors="replace").splitlines(), 1):
        try:
            event = json.loads(raw)
        except ValueError:
            continue
        event["_line"] = number
        events.append(event)
    return events


def windows(events, list_id):
    """Each test's events: {test id: [events]} (the last run of a test wins)."""
    found, current = {}, None
    for event in events:
        if event.get("ev") == "test" and event.get("list") == list_id:
            if event.get("state") == "start":
                current = event.get("test")
                found[current] = [event]
                continue
            if current and event.get("state") == "verdict" and event.get("test") == current:
                found[current].append(event)
                current = None
                continue
        if current:
            found[current].append(event)
    return found


def options(words, names):
    """key N pairs out of words: ({key: int}, the words left, a key with no number after it or None)"""
    picked, rest, index, bad = {}, [], 0, None
    while index < len(words):
        if words[index] in names:
            if index + 1 < len(words) and re.fullmatch(r"-?\d+", words[index + 1]):
                picked[words[index]] = int(words[index + 1])
                index += 2
                continue
            bad = bad or words[index]
        rest.append(words[index])
        index += 1
    return picked, rest, bad


OPTIONS = {"weapon": ("loaded", "total", "player"), "ammo": ("loaded", "total", "player"), "spawn": ("player",),
           "death": ("player",), "kill": ("victim", "killer"), "score": ("player", "individual", "team")}


def tolerance_of(words):
    """(seconds, tolerance, the tag's words) of a respawn rule's words after its kind, or a str: why not"""
    tolerance = 1.0
    if words and words[-1].startswith("+-"):
        try:
            tolerance = float(words[-1][2:])
        except ValueError:
            return "respawn's +- is not a number"
        if not (0 <= tolerance < 1e6):
            return "respawn's +- is out of range"
        words = words[:-1]
    if len(words) < 2:
        return "respawn needs a tag and seconds"
    try:
        seconds = float(words[-1])
    except ValueError:
        return "respawn's seconds are not a number"
    if not (0 <= seconds < 1e6):
        return "respawn's seconds are out of range"
    return seconds, tolerance, words[:-1]


def validate(rule):
    """None when the rule can be judged, else why not (a bad rule fails, whatever "no" before it)"""
    words = rule.split()
    if not words:
        return "empty rule"
    kind = words[0]
    if kind == "no":
        return validate(" ".join(words[1:])) if len(words) > 1 else "no needs a rule"
    if kind in OPTIONS:
        _, rest, bad = options(words[1:], OPTIONS[kind])
        if bad:
            return f"{kind}: '{bad}' needs a number"
        if kind in ("weapon", "ammo"):
            return None if rest else f"{kind} needs a tag"
        return f"{kind}: unknown words {' '.join(rest)}" if rest else None
    if kind in ("object", "gone"):
        return None if len(words) >= 3 and words[1] in ("vehicle", "item") else f"{kind} needs vehicle|item and a tag"
    if kind == "respawn":
        if len(words) < 4 or words[1] not in ("vehicle", "item"):
            return "respawn needs vehicle|item, a tag and seconds"
        parsed = tolerance_of(words[2:])
        return parsed if isinstance(parsed, str) else None
    if kind in ("hud", "log"):
        return None if len(words) > 1 else f"{kind} needs text"
    if kind == "option":
        return None if len(words) == 3 else "option needs a row and a value"
    if kind == "cmd":
        rest = words[1:-1] if words[-1] in ("ok", "error") else words[1:]
        return None if rest else "cmd needs text"
    if kind == "game_over":
        return None if len(words) == 1 else "game_over takes no words"
    return f"unknown rule {kind}"


def matches(event, wanted):
    return all(event.get(key) == value for key, value in wanted.items())


def judge(rule, window):
    """(passed, reason, evidence line numbers) for one rule in one window"""
    words = rule.split()
    invalid = validate(rule)
    if invalid:
        return False, f"bad rule: {invalid}", []
    if words[0] == "no":
        passed, reason, lines = judge(" ".join(words[1:]), window)
        return (not passed, ("never: " if not passed else "it happened: ") + reason, lines)
    kind = words[0]
    if kind in ("weapon", "ammo"):
        wanted, rest, _ = options(words[1:], OPTIONS[kind])
        if not rest:
            return False, f"{kind} needs a tag", []
        part = " ".join(rest)
        kinds = ("weapon", "ammo") if kind == "ammo" else ("weapon",)
        seen = []
        for event in window:
            if event.get("ev") in kinds and tag_has(event.get("weapon", ""), part):
                seen.append(event)
                if matches(event, wanted):
                    return True, f"{short(event['weapon'])}: {event['loaded']} loaded, {event['total']} more", \
                        [event["_line"]]
        if seen:
            last = seen[-1]
            return False, f"{short(last['weapon'])}: {last['loaded']} loaded, {last['total']} more", [e["_line"] for e in seen]
        return False, f"no {kind} event for {part}", []
    if kind in ("spawn", "death"):
        wanted, _, _ = options(words[1:], OPTIONS[kind])
        for event in window:
            if event.get("ev") == kind and matches(event, wanted):
                return True, f"{kind} of player {event.get('player')}", [event["_line"]]
        return False, f"no {kind}", []
    if kind == "kill":
        wanted, _, _ = options(words[1:], OPTIONS[kind])
        for event in window:
            if event.get("ev") == "kill" and matches(event, wanted):
                return True, f"kill: victim {event['victim']} killer {event['killer']}", [event["_line"]]
        return False, "no such kill", []
    if kind == "score":
        wanted, _, _ = options(words[1:], OPTIONS[kind])
        last = None
        for event in window:
            if event.get("ev") == "score" and ("player" not in wanted or event.get("player") == wanted["player"]):
                last = event
                if matches(event, wanted):
                    return True, f"score: player {event['player']} {event['individual']}/{event['team']}", \
                        [event["_line"]]
        return False, ("no score change" if not last else
                       f"score reached {last['individual']}/{last['team']}"), [last["_line"]] if last else []
    if kind in ("object", "gone"):
        if len(words) < 3:
            return False, f"{kind} needs vehicle|item and a tag", []
        ev = "object_new" if kind == "object" else "object_gone"
        part = " ".join(words[2:])
        for event in window:
            if event.get("ev") == ev and event.get("kind") == words[1] and tag_has(event.get("tag", ""), part):
                return True, f"{short(event['tag'])} {'made' if ev == 'object_new' else 'gone'}", [event["_line"]]
        return False, f"no {ev} of a {words[1]} {part}", []
    if kind == "respawn":
        seconds, tolerance, tag_words = tolerance_of(words[2:])
        part = " ".join(tag_words)
        gone = [e for e in window if e.get("ev") in ("object_gone", "vehicle_dead") and
                e.get("kind", words[1]) == words[1] and tag_has(e.get("tag", ""), part)]
        made = [e for e in window if e.get("ev") == "object_new" and e.get("kind") == words[1] and
                tag_has(e.get("tag", ""), part)]
        # (every gone with the next one made after it: one within the time passes, else the nearest is the reason)
        nearest = None
        for left in gone:
            back = next((e for e in made if e["tick"] > left["tick"] >= 0), None)
            if back is None:
                continue
            took = (back["tick"] - left["tick"]) / TICKS_PER_SECOND
            pair = (abs(took - seconds), took, [left["_line"], back["_line"]])
            if pair[0] <= tolerance:
                return True, f"back after {took:.1f} s (wanted {seconds:g} +-{tolerance:g})", pair[2]
            nearest = min(nearest, pair) if nearest else pair
        if nearest:
            return False, f"back after {nearest[1]:.1f} s at nearest (wanted {seconds:g} +-{tolerance:g})", nearest[2]
        return False, f"no {part} went and came back", [e["_line"] for e in gone[:3] + made[:3]]
    if kind == "log":
        part = norm(" ".join(words[1:]))
        for event in window:
            if event.get("ev") == "log" and part in norm(event.get("text", "")):
                return True, f"log: {event['text'][:80]}", [event["_line"]]
        return False, f"no log line with {part}", []
    if kind == "option":
        row, value = words[1].replace("_", "").lower(), words[2].replace("_", "").lower()
        for event in window:
            if event.get("ev") != "cmd" or not event.get("ok"):
                continue
            said = event.get("line", "").lower().split()
            if len(said) >= 3 and said[-3] == "option" and said[-2].replace("_", "") == row and \
                    said[-1].replace("_", "") == value:
                return True, f"option: {event['text']}", [event["_line"]]
        return False, f"no option {words[1]} {words[2]} answered ok", []
    if kind == "hud":
        part = norm(" ".join(words[1:]))
        for event in window:
            if event.get("ev") == "hud" and part in norm(event.get("text", "")):
                return True, f"hud: {event['text']}", [event["_line"]]
        return False, f"no HUD message with {part}", []
    if kind == "cmd":
        want_ok = None
        rest = words[1:]
        if rest and rest[-1] in ("ok", "error"):
            want_ok = rest[-1] == "ok"
            rest = rest[:-1]
        part = " ".join(rest).lower()
        for event in window:
            # (the live agent's own lines are not the game's commands)
            if event.get("ev") == "cmd" and not event.get("line", "").startswith("agent ") and \
                    part in event.get("line", "").lower() and \
                    (want_ok is None or event.get("ok") == want_ok):
                return True, f"cmd {event['line']}: {event['text']}", [event["_line"]]
        return False, f"no dev command answer for {part}", []
    if kind == "game_over":
        for event in window:
            if event.get("ev") == "game_over":
                return True, "game over", [event["_line"]]
        return False, "the game did not end", []
    return False, f"unknown rule {kind}", []


def report(list_path, events_path):
    text = Path(list_path).read_text(errors="replace")
    list_id = next((line.split()[1] for line in text.splitlines() if len(line.split()) > 1 and line.split()[0] == "list"), "")
    tests = parse_list(text)
    found = windows(read_events(events_path), list_id)
    results = []
    for test in tests:
        window = found.get(test["id"])
        for number, check in enumerate(test["checks"], 1):
            rule = test["expects"].get(number)
            row = {"test": test["id"], "check": number, "text": check, "rule": rule or ""}
            if not rule:
                row.update(verdict="LOOK", reason="the owner's check", evidence=[])
            elif validate(rule):
                row.update(verdict="BAD RULE", reason=validate(rule), evidence=[])
            elif window is None:
                row.update(verdict="NOT RUN", reason="the test never started", evidence=[])
            else:
                passed, reason, lines = judge(rule, window)
                row.update(verdict="PASS" if passed else "FAIL", reason=reason, evidence=lines)
            results.append(row)
    return results


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    results = report(argv[1], argv[2])
    if "--json" in argv:
        print(json.dumps(results, indent=1))
    else:
        for row in results:
            evidence = f" (events {', '.join(map(str, row['evidence']))})" if row["evidence"] else ""
            print(f"{row['verdict']:7} {row['test']} #{row['check']}: {row['text']}"
                  + (f"\n        {row['rule']}: {row['reason']}{evidence}" if row["rule"] else ""))
    return 1 if any(row["verdict"] in ("FAIL", "NOT RUN", "BAD RULE") for row in results) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
