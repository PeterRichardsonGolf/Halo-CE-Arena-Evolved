#!/usr/bin/env python3
"""The live agent of a TEST RUN: follows DEV MODE's event stream while the owner plays, judges each check's expect line
as the events come (tools/ae_test/expect.py's rules) and puts its verdicts on the card over the telnet console
(debug.telnet_console, 127.0.0.1 only), with the "agent" dev command.

    python3 tools/ae_test/live.py <list.txt> [--events <events.jsonl or its folder>] [--port 2323]
    python3 tools/ae_test/live.py say "Try the ramp again: the jump did not land" [--port 2323]

A sent line is at most LINE (120) characters: the game's telnet console keeps 127 a line and drops a longer one
whole ("overflowed client buffer"). A verdict counts as shown only once the game's answer to it comes back ("check 2:
pass" on the console, or the line's own cmd event in the stream). An overflowed line was not run and is sent again; a
line sent with no answer yet may still run (the game was loading), so it is never sent again: it stays the one
outstanding line, the others wait behind it, until its answer comes (or OUTSTANDING_WAIT passes with none). The
console has one client: when it is taken ("the maximum number of clients are already connected") the agent says so
and exits 3.

On each test's start the card's checks show WAITING (a check with a rule) or LOOK (the owner's own). A rule met turns
its check PASS with the reason and the evidence lines; a "no ..." rule shows PASS until what it forbids happens, then
FAIL. A rule not met stays WAITING ("not seen yet"): only the owner ends a test. Read only: it sends nothing but
"agent" lines (--drive is for later, when the owner asks the agent to set a test up again).

Every verdict sent also goes to the stream as the dev command's own event, so the results file and the stream agree."""
import argparse
import json
import socket
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import expect  # noqa: E402


# (the telnet console's line is 127 characters: source/networking/telnet_console.c, TELNET_CLIENT_BUFFER_SIZE 128)
LINE = 120
OVERFLOWED = "overflowed client buffer"
SLOT_TAKEN = "maximum number of clients"
# how long a line sent with no answer is waited for before it counts as lost (a map load holds the console a while)
OUTSTANDING_WAIT = 30.0


class SlotTaken(Exception):
    """the game's one telnet client slot is someone else's"""


def card_line(head, text=""):
    """head and text as one console line of at most LINE characters: the text (a reason, a message) cut to fit, the head
    (the command and its verdict word) kept whole"""
    text = " ".join(str(text).split())
    if not text:
        return head[:LINE]
    room = LINE - len(head) - 1
    if room <= 0:
        return head[:LINE]
    if len(text) > room:
        text = text[:room - 3].rstrip() + "..."
    return f"{head} {text}"


class Console:
    """The game's telnet console: lines out, replies read (a console never read stops taking)."""

    def __init__(self, port, host="127.0.0.1", wait=1.5):
        self.address = (host, port)
        self.connection = None
        self.wait = wait

    def send(self, line, confirm=None):
        """sends a line (cut to LINE characters). "confirmed": confirm (the answer's text, "check 2: pass") came back;
        "sent": sent, no confirm asked; "unanswered": sent and no answer yet (it may still run: never send it again);
        "refused": another answer came (the game refused it); "down": not sent (no console). An overflowed line was not
        run: it is sent again. SlotTaken when the console's one client slot is someone else's"""
        line = line[:LINE]
        for _ in range(3):
            try:
                if not self.connection:
                    self.connection = socket.create_connection(self.address, timeout=3)
                    self.connection.settimeout(0.05)
                self.connection.sendall(line.encode("ascii", "replace") + b"\r\n")
            except OSError:
                self.close()
                continue
            reply = self.read(confirm)
            if confirm is None:
                return "sent"
            if confirmed(reply, confirm):
                return "confirmed"
            if OVERFLOWED in reply:
                continue
            # (an echo alone is no answer: the line may run later)
            echo = line.strip()
            rows = [row.strip() for row in reply.replace("\r", "\n").split("\n") if row.strip()]
            if any(row != echo for row in rows):
                return "refused"
            return "unanswered"
        return "down"

    def poll(self, confirm):
        """True when confirm has come back since (a late answer); never sends"""
        return bool(self.connection) and confirmed(self.read(), confirm)

    def read(self, confirm=None):
        """what the console wrote back: until confirm shows (or the wait ends); without confirm what is there now"""
        reply, end = "", time.time() + (self.wait if confirm else 0.05)
        while self.connection:
            try:
                data = self.connection.recv(65536)
            except (socket.timeout, BlockingIOError):
                if time.time() >= end or (confirm and confirmed(reply, confirm)):
                    break
                continue
            except OSError:
                self.close()
                break
            if not data:
                self.close()
                break
            reply += data.decode("ascii", "replace")
            if SLOT_TAKEN in reply:
                self.close()
                raise SlotTaken(" ".join(reply.split()))
            if confirm and (confirmed(reply, confirm) or OVERFLOWED in reply):
                break
        return reply

    def drain(self):
        self.read()

    def close(self):
        if self.connection:
            try:
                self.connection.close()
            except OSError:
                pass
        self.connection = None


def confirmed(reply, confirm):
    """the answer's own line is in the reply (not the echo of the line typed: "check 2 pass" has no colon)"""
    return any(row.strip() == confirm or row.strip().startswith(confirm + " ") for row in reply.replace("\r", "\n").split("\n"))


def newest_stream(where):
    path = Path(where).expanduser()
    if path.is_file():
        return path
    streams = sorted(path.glob("events-*.jsonl"), key=lambda p: p.stat().st_mtime)
    return streams[-1] if streams else None


class Agent:
    def __init__(self, list_path, console):
        text = Path(list_path).read_text(errors="replace")
        self.list_id = next((line.split()[1] for line in text.splitlines()
                             if len(line.split()) > 1 and line.split()[0] == "list"), "")
        self.tests = {test["id"]: test for test in expect.parse_list(text)}
        self.console = console
        self.test = None
        self.window = []
        self.shown = {}
        # the lines waiting to go (by check, or "say"), and the one sent with no answer yet
        self.wanted = {}
        self.outstanding = None
        self.serial = 0
        self.greet = ""
        self.shots = 0.0
        self.last_shot = time.time()

    def tick(self):
        """a late answer looked for, and (headless checks) a screenshot every so many seconds, through the one console the
        game reads (not while a line waits for its answer: the shot's read would take it)"""
        self.flush()
        if self.shots and not self.outstanding and time.time() - self.last_shot >= self.shots:
            self.last_shot = time.time()
            self.console.send("dev shot")

    def show(self, check, verdict, reason=""):
        if self.shown.get(check) == (verdict, reason):
            self.wanted.pop(check, None)
            return
        # (the same verdict already sent, its answer awaited: not again)
        item = self.outstanding
        if item and item["key"] == check and item["serial"] == self.serial and item["value"] == (verdict, reason):
            self.wanted.pop(check, None)
            return
        self.wanted[check] = (card_line(f"agent check {check} {verdict}", reason), f"check {check}: {verdict}",
                              (verdict, reason))
        self.flush()

    def answered(self, item):
        if item["serial"] == self.serial and item["key"] != "say":
            self.shown[item["key"]] = item["value"]

    def flush(self):
        """sends the waiting lines one at a time; none while a line sent earlier has no answer yet"""
        if self.outstanding:
            item = self.outstanding
            if self.console.poll(item["confirm"]):
                self.outstanding = None
                self.answered(item)
            elif time.time() - item["at"] < OUTSTANDING_WAIT:
                return
            else:
                # (never answered: taken as lost; what is still wanted goes now)
                self.outstanding = None
        while self.wanted and not self.outstanding:
            key = "say" if "say" in self.wanted else min(self.wanted)
            line, confirm, value = self.wanted.pop(key)
            if key != "say" and self.shown.get(key) == value:
                continue
            item = {"key": key, "line": line, "confirm": confirm, "value": value, "serial": self.serial,
                    "at": time.time()}
            result = self.console.send(line, confirm=confirm)
            if result == "confirmed":
                self.answered(item)
            elif result == "unanswered":
                self.outstanding = item
            elif result == "down":
                self.wanted.setdefault(key, (line, confirm, value))
                return

    def start(self, test_id):
        self.test = self.tests.get(test_id)
        self.window = []
        self.shown = {}
        self.wanted = {}
        self.serial += 1
        if not self.test:
            return
        if self.greet:
            self.wanted["say"] = (card_line("agent say", self.greet), "said", None)
        for number in range(1, len(self.test["checks"]) + 1):
            if self.test["expects"].get(number):
                self.show(number, "waiting", "not seen yet")
            else:
                self.show(number, "look")

    def judge(self):
        if not self.test:
            return
        for number, rule in self.test["expects"].items():
            if number > len(self.test["checks"]) or expect.validate(rule):
                continue
            passed, reason, lines = expect.judge(rule, self.window)
            evidence = f" (events {', '.join(map(str, lines[:3]))})" if lines else ""
            if passed:
                self.show(number, "pass", reason + evidence)
            elif rule.split()[0] == "no":
                self.show(number, "fail", reason + evidence)
            else:
                self.show(number, "waiting", "not seen yet")

    def take(self, event):
        if event.get("ev") == "test" and event.get("list") == self.list_id:
            if event.get("state") == "start":
                self.start(event.get("test"))
                return
            if event.get("state") == "verdict":
                self.test = None
                return
        # (the agent's own line run: its answer, however late)
        if event.get("ev") == "cmd" and self.outstanding and event.get("ok") and \
                str(event.get("line", "")).strip() == self.outstanding["line"].strip():
            item, self.outstanding = self.outstanding, None
            self.answered(item)
        if self.test:
            self.window.append(event)
            # (the agent's own lines are events too: no judging on them)
            if not (event.get("ev") == "cmd" and str(event.get("line", "")).startswith("agent ")):
                self.judge()
        self.flush()


def follow(path, agent, where=None, poll=0.25):
    """reads the stream as it grows, line by line (a torn last line waits for its end); when a newer stream appears in
    where (the game started again), it follows that one from its start"""
    while True:
        number, pending, checked = 0, "", time.time()
        with open(path, "r", errors="replace") as stream:
            while True:
                chunk = stream.readline()
                if not chunk:
                    agent.tick()
                    if where and time.time() - checked > 2:
                        checked = time.time()
                        newest = newest_stream(where)
                        if newest and newest != path and newest.stat().st_mtime > path.stat().st_mtime:
                            print(f"live agent: {newest}", flush=True)
                            path = newest
                            break
                    time.sleep(poll)
                    continue
                pending += chunk
                if not pending.endswith("\n"):
                    continue
                number += 1
                try:
                    event = json.loads(pending)
                except ValueError:
                    pending = ""
                    continue
                pending = ""
                event["_line"] = number
                agent.take(event)


def main(argv):
    if len(argv) > 1 and argv[1] == "say":
        parser = argparse.ArgumentParser()
        parser.add_argument("say")
        parser.add_argument("text")
        parser.add_argument("--port", type=int, default=2323)
        args = parser.parse_args(argv[1:])
        try:
            return 0 if Console(args.port).send(card_line("agent say", args.text), confirm="said") == "confirmed" else 1
        except SlotTaken as taken:
            print(f"live agent: the game's telnet console is taken: {taken}", file=sys.stderr, flush=True)
            return 3
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("list")
    parser.add_argument("--events", default="~/src/halo-test/ae-gametypes-preview/dev")
    parser.add_argument("--port", type=int, default=2323)
    # (headless checks: a message at each test's start, a screenshot every so many seconds)
    parser.add_argument("--greet", default="")
    parser.add_argument("--shots", type=float, default=0.0)
    args = parser.parse_args(argv[1:])
    path = None
    while path is None:
        path = newest_stream(args.events)
        if path is None:
            time.sleep(1)
    print(f"live agent: {path}", flush=True)
    agent = Agent(args.list, Console(args.port))
    agent.greet = args.greet
    agent.shots = args.shots
    try:
        follow(path, agent, args.events if Path(args.events).expanduser().is_dir() else None)
    except SlotTaken as taken:
        # (no verdict can reach the card: said, not followed forever in silence)
        print(f"live agent: the game's telnet console is taken, no verdict can reach the card: {taken}",
              file=sys.stderr, flush=True)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
