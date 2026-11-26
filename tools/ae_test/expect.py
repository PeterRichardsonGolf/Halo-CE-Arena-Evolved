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
    log <text>                                     a line the game wrote to debug.txt holding the text ("ae needler: MCC"),
                                                   or, in a headless run, to its own log while the test was up
    option <row> <value>                           a dev option command set that GAME OPTIONS row to that value
    cmd <text> [ok|error]                          a dev command whose line holds the text answered so
    game_over                                      the game ended
    round_start [round N]                          a round mode's round began (ELIMINATION: ae_round.h)
    round_end [round N] [winner N] [red N] [blue N]
                                                   a round ended: winner a side (team 0 red, 1 blue; a player without
                                                   teams), -1 a tie; red and blue the wins after it
    down [player N]                                a player went down (a revive spot)
    out [player N]                                 a player died out of the round (no spot: the zone, revive off)
    revived [player N] [revives N]                 a downed player was revived (their revives this round)
    zone [phase N]                                 the danger zone reached a phase (0 wait, 1 closing, 2 wait, 3 the
                                                   last closing, 4 closed)
    round_wait [player N]                          a player who joined during a round waits for the next
    match_over                                     a round mode's match ended
    hurt [player N] [taken max|min N] [blocked max|min N]
                                                   the hits on a player in the window ("hurt" events): taken, the ones
                                                   that player took; blocked, the ones DEV god took for them. god: player 0
                                                   blocked min 1 taken max 0; unseen: player 0 taken max 0 (the window's
                                                   seconds said). Name the player: in co-op the AI squad are players too
    stat <group> <key> max|min N                   the campaign AI squad's counter lines ("squad <group>: key value
                                                   ..."): a count's rise from the test's first such line to its last, or
                                                   a distance's (pair, player) or a cost's (rays, ms, p99ms, grid, gridticks,
                                                   p50, p95) most (max) or least (min) over the test's lines
    member stuck|lip|gap90 max|min N               each squad member's own line ("squad member K: stuck S lip L gap90
                                                   G"): seconds stuck or at a lip, their rise over the test; the gap's
                                                   90th percentile, its last; judged on the worst member
    crossed x|y lt|gt V N within S                 the player's first position line past the plane, then at least N
                                                   squad members past it within S seconds (the squad's position lines)
    no <rule>                                     the rule is never met in the window

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
                # (a check may have several expect lines: it passes when every one does)
                current["expects"].setdefault(int(parts[0]), []).append(parts[1].strip() if len(parts) > 1 else "")
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


LOG_VERDICT = re.compile(r"test run: (?P<list>[a-z0-9-]+) (?P<id>[a-z0-9-]+) (PASS|FAIL|SKIP|NOTE)\b")
LOG_UP = re.compile(r"test run setup(?::| kept) (?P<id>[a-z0-9-]+) \(")


def log_windows(log_text, list_id):
    """Each test's lines of the game's own log (its standard error, where most of the port's lines go: the event stream
    holds the engine's debug.txt lines only): {test id: [(line number, text)]}, from the line that says the test's load
    or its going on in the running game ("test run setup: <id> (...)", "test run setup kept <id> (...)") to its
    verdict's: its game's start is part of it (what the game says at tick 0). So are the lines written while its
    gametype was chosen, before the game: "ae seed rules: ..." says the choice, "live rule: ..." the game."""
    found, current = {}, None
    for number, raw in enumerate(log_text.splitlines(), 1):
        line = raw.split("arena-evolved: ", 1)[-1].strip()
        match = LOG_UP.match(line)
        if match:
            current = match.group("id")
            found[current] = []
            continue
        match = LOG_VERDICT.match(line)
        if match and match.group("list") == list_id and match.group("id") == current:
            current = None
            continue
        if current:
            found[current].append((number, line))
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
# the round modes' events (ae_dev_events.h): judged alike, the first of the kind with the numbers given
ROUND_EVENTS = {"round_start": ("round",), "round_end": ("round", "winner", "red", "blue"), "down": ("player",),
                "out": ("player",), "revived": ("player", "revives"), "zone": ("phase",), "round_wait": ("player",),
                "match_over": ()}
OPTIONS.update({kind: names for kind, names in ROUND_EVENTS.items() if names})


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
    if kind == "stat":
        if len(words) != 5 or words[3] not in ("max", "min"):
            return "stat needs a group, a key, max|min and a number"
        try:
            float(words[4])
        except ValueError:
            return f"stat: '{words[4]}' is not a number"
        return None
    if kind == "member":
        if len(words) != 4 or words[1] not in MEMBER_KEYS or words[2] not in ("max", "min"):
            return f"member needs {'|'.join(MEMBER_KEYS)}, max|min and a number"
        try:
            float(words[3])
        except ValueError:
            return f"member: '{words[3]}' is not a number"
        return None
    if kind == "crossed":
        if len(words) != 7 or words[1] not in ("x", "y") or words[2] not in ("lt", "gt") or words[5] != "within":
            return "crossed needs x|y, lt|gt, a value, a count, within and seconds"
        try:
            float(words[3]), int(words[4]), float(words[6])
        except ValueError:
            return "crossed: its value, count and seconds are numbers"
        return None
    if kind == "cmd":
        rest = words[1:-1] if words[-1] in ("ok", "error") else words[1:]
        return None if rest else "cmd needs text"
    if kind == "hurt":
        return validate_hurt(words[1:])
    if kind == "game_over":
        return None if len(words) == 1 else "game_over takes no words"
    if kind == "match_over":
        return None if len(words) == 1 else "match_over takes no words"
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
    if kind in ROUND_EVENTS:
        wanted, _, _ = options(words[1:], OPTIONS.get(kind, ()))
        seen = [event for event in window if event.get("ev") == kind]
        for event in seen:
            if matches(event, wanted):
                shown = ", ".join(f"{key} {event.get(key)}" for key in OPTIONS.get(kind, ()) if key in event)
                return True, f"{kind}" + (f": {shown}" if shown else ""), [event["_line"]]
        return False, (f"no {kind}" if not seen else f"{len(seen)} {kind} events, none with those numbers"), \
            [event["_line"] for event in seen[:3]]
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
    if kind == "stat":
        return judge_stat(words, window)
    if kind == "member":
        return judge_member(words, window)
    if kind == "crossed":
        return judge_crossed(words, window)
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
    if kind == "hurt":
        return judge_hurt(words, window)
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


STAT_GAUGES = ("pair", "player", "p50", "p95", "rays", "ms", "grid", "gridticks", "p99ms")


def hurt_terms(words):
    """(player or None, [(field, bound, limit)]) of a hurt rule's words after its kind, or a str: why not"""
    player, terms, index = None, [], 0
    while index < len(words):
        if words[index] == "player" and index + 1 < len(words) and words[index + 1].isdigit():
            player, index = int(words[index + 1]), index + 2
        elif words[index] in ("taken", "blocked") and index + 2 < len(words) and words[index + 1] in ("max", "min") \
                and words[index + 2].isdigit():
            terms.append((words[index], words[index + 1], int(words[index + 2])))
            index += 3
        else:
            return f"hurt: '{words[index]}' (player N, taken max|min N, blocked max|min N)"
    return (player, terms) if terms else "hurt needs taken or blocked, max|min and a number"


def validate_hurt(words):
    parsed = hurt_terms(words)
    return parsed if isinstance(parsed, str) else None


def judge_hurt(words, window):
    """hurt [player N] [taken max|min N] [blocked max|min N]: the window's hits on the player, counted"""
    player, terms = hurt_terms(words[1:])
    counts, lines = {"taken": 0, "blocked": 0}, []
    for event in window:
        if event.get("ev") == "hurt" and (player is None or event.get("player") == player):
            counts["blocked" if event.get("blocked") else "taken"] += 1
            lines.append(event["_line"])
    times = [event.get("t", 0) for event in window]
    seconds = (max(times) - min(times)) / 1000 if times else 0
    failed = [f"{field} {counts[field]} (wanted {bound} {limit})" for field, bound, limit in terms
              if (counts[field] > limit if bound == "max" else counts[field] < limit)]
    said = f"taken {counts['taken']}, blocked {counts['blocked']} in {seconds:.0f} s"
    return (False, f"{'; '.join(failed)}: {said}", lines[:8]) if failed else (True, said, lines[:8])


def judge_stat(words, window):
    """stat <group> <key> max|min N over the window's "squad <group>:" lines"""
    group, key, bound, limit = words[1], words[2], words[3], float(words[4])
    head = f"squad {group}:"
    seen = []
    for event in window:
        text = event.get("text", "") if event.get("ev") == "log" else ""
        if not text.startswith(head):
            continue
        parts = text[len(head):].split()
        for index in range(len(parts) - 1):
            if parts[index] == key:
                try:
                    seen.append((float(parts[index + 1]), event["_line"]))
                except ValueError:
                    pass
                break
    if not seen:
        return False, f"no squad {group} line with {key}", []
    if key in STAT_GAUGES:
        value, line = (max if bound == "max" else min)(seen)
        what = f"{key} {'most' if bound == 'max' else 'least'} {value:g}"
    else:
        value, line = seen[-1][0] - seen[0][0], seen[-1][1]
        what = f"{key} rose {value:g}"
    passed = value <= limit if bound == "max" else value >= limit
    return passed, f"{what} (wanted {bound} {limit:g})", [line]


MEMBER_KEYS = ("stuck", "lip", "gap90")
MEMBER_LINE = re.compile(r"^squad member (\d+): stuck ([\d.]+) lip ([\d.]+) gap90 ([\d.]+)")
POSITIONS_LINE = re.compile(r"^companion: positions t\+(\d+)s human (-?[\d.]+) (-?[\d.]+) -?[\d.]+ \(")
MEMBER_AT = re.compile(r", (\d+) (-?[\d.]+) (-?[\d.]+) -?[\d.]+ \(")


def log_texts(window):
    """(text, line) of the window's log lines"""
    return [(event.get("text", ""), event["_line"]) for event in window if event.get("ev") == "log"]


def judge_member(words, window):
    """member stuck|lip|gap90 max|min N over the window's "squad member K:" lines: seconds stuck or at a lip, their rise over
    the test; the gap's 90th percentile, its last; the worst member's"""
    key, bound, limit = words[1], words[2], float(words[3])
    column = MEMBER_KEYS.index(key) + 2
    first, last = {}, {}
    for text, line in log_texts(window):
        found = MEMBER_LINE.match(text)
        if not found:
            continue
        member, value = int(found.group(1)), float(found.group(column))
        first.setdefault(member, value)
        last[member] = (value, line)
    if not last:
        return False, "no squad member line", []
    values = {member: (last[member][0] if key == "gap90" else last[member][0] - first[member]) for member in last}
    pick = max if bound == "max" else min
    worst = pick(values, key=lambda member: values[member])
    value = values[worst]
    passed = value <= limit if bound == "max" else value >= limit
    what = "last" if key == "gap90" else "rose"
    return passed, f"worst member {worst} {key} {what} {value:g} (wanted {bound} {limit:g})", [last[worst][1]]


def judge_crossed(words, window):
    """crossed x|y lt|gt V N within S: the player's first position line past the plane, then at least N members past it
    by S seconds after (the squad's positions lines)"""
    axis, side, plane, count, seconds = words[1], words[2], float(words[3]), int(words[4]), float(words[6])
    index = 0 if axis == "x" else 1

    def past(value):
        return value < plane if side == "lt" else value > plane
    rows = []
    for text, line in log_texts(window):
        found = POSITIONS_LINE.match(text)
        if found:
            members = {int(m.group(1)): (float(m.group(2)), float(m.group(3))) for m in MEMBER_AT.finditer(text)}
            rows.append((int(found.group(1)), (float(found.group(2)), float(found.group(3))), members, line))
    anchor = next((row for row in rows if past(row[1][index])), None)
    if not anchor:
        return False, f"the player never crossed {axis} {side} {plane:g}", [rows[-1][3]] if rows else []
    members = sorted({member for row in rows for member in row[2]})
    times, late = {}, []
    for member in members:
        at = next((row[0] for row in rows if member in row[2] and past(row[2][member][index])), None)
        if at is not None and at - anchor[0] <= seconds:
            times[member] = max(0, at - anchor[0])
        else:
            late.append(f"member {member} " + (f"after {at - anchor[0]} s" if at is not None else "never"))
    passed = len(times) >= count
    across = ", ".join(f"member {member} {times[member]} s" for member in sorted(times))
    reason = f"{len(times)} of {len(members)} across within {seconds:g} s of the player (wanted {count})"
    reason += f": {across}" if across else ""
    reason += f"; {', '.join(late)}" if late else ""
    return passed, reason, [anchor[3]]


def report(list_path, events_path, log_path=None):
    """Every check of the list judged: [{test, check, text, rule, verdict, reason, evidence}]. log_path: the game's own
    log of the run (game.log); its lines of each test count as log lines for `log` rules, after the event stream's."""
    text = Path(list_path).read_text(errors="replace")
    list_id = next((line.split()[1] for line in text.splitlines() if len(line.split()) > 1 and line.split()[0] == "list"), "")
    tests = parse_list(text)
    found = windows(read_events(events_path), list_id)
    logged = log_windows(Path(log_path).read_text(errors="replace"), list_id) if log_path else {}
    results = []
    for test in tests:
        window = found.get(test["id"])
        setup_failed = [(f"game log {number}", line.split("SETUP ERROR:", 1)[1].strip()[:80])
                        for number, line in logged.get(test["id"], [])
                        if line.startswith(f"test run: {list_id} {test['id']} SETUP ERROR")]
        if window is not None:
            window = window + [{"ev": "log", "text": line, "_line": f"game log {number}"}
                               for number, line in logged.get(test["id"], [])]
        for number, check in enumerate(test["checks"], 1):
            rules = [rule for rule in test["expects"].get(number, []) if rule]
            row = {"test": test["id"], "check": number, "text": check, "rule": " ; ".join(rules)}
            if not rules:
                row.update(verdict="LOOK", reason="the owner's check", evidence=[])
            elif any(validate(rule) for rule in rules):
                row.update(verdict="BAD RULE", reason=next(validate(rule) for rule in rules if validate(rule)), evidence=[])
            elif window is None:
                row.update(verdict="NOT RUN", reason="the test never started", evidence=[])
            elif setup_failed:
                # (its setup failed: the window holds no play, and a "no" rule would pass on it)
                row.update(verdict="NOT RUN", reason=f"its setup failed: {setup_failed[0][1]}", evidence=[setup_failed[0][0]])
            else:
                # (every expect line of the check is judged; the first that fails is the check's reason)
                judged = [judge(rule, window) for rule in rules]
                failed = [one for one in judged if not one[0]]
                row.update(verdict="FAIL" if failed else "PASS",
                           reason=failed[0][1] if failed else " ; ".join(one[1] for one in judged),
                           evidence=failed[0][2] if failed else [line for one in judged for line in one[2]])
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
