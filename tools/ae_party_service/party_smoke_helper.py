#!/usr/bin/env python3
"""A stand-in party service and a second player for one game of the harness (tools/test_ae_party_online.py): the service on this
namespace's loopback, and "Rook", who leads a party, invites the game, sends it on a QUICK PLAY and removes it. Usage:
party_smoke_helper.py <port> <steps file>. Every step is printed (helper.log) and written to the steps file as one line:
"<name> ok" or "<name> FAIL <why>"."""
import json
import os
import sys
import threading
import time
from http.server import ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ae_party_service as svc  # noqa: E402
import ed25519  # noqa: E402

ROOK_SEED = bytes([0x52]) * 32
SERVERS = [
    {"address": "127.0.0.1:10660", "playlist": "casual", "name": "Casual A", "players": 5, "max_players": 16, "reachable": True},
    {"address": "127.0.0.1:10661", "playlist": "casual", "name": "Casual B", "players": 0, "max_players": 16, "reachable": True},
]


def rook_code():
    return svc.friend_code(ed25519.public_key(ROOK_SEED))


def main():
    port, steps_path = int(sys.argv[1]), sys.argv[2]
    service = svc.Service(None, SERVERS, origin="http://127.0.0.1:%d/party" % port)
    service.log = lambda text: print("service: " + text, flush=True)
    httpd = ThreadingHTTPServer(("127.0.0.1", port), svc.make_handler(service, None))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    steps = open(steps_path, "w")

    def step(name, ok, why=""):
        line = "%s %s" % (name, "ok" if ok else "FAIL " + why)
        print("step: " + line, flush=True)
        steps.write(line + "\n")
        steps.flush()
        return ok

    def waiting(name, condition, seconds):
        end = time.time() + seconds
        while time.time() < end:
            service.tick()
            if condition():
                return step(name, True)
            time.sleep(0.25)
        return step(name, False, "timed out after %d s" % seconds)

    pub = ed25519.public_key(ROOK_SEED)
    nonce = service.challenge({}, "rook")["nonce"]
    sig = ed25519.sign(ROOK_SEED, ("ae-party-hello-2\n%s\n%s\n%s" % (service.origin, nonce, pub.hex())).encode()).hex()
    rook = service.hello({"key": pub.hex(), "nonce": nonce, "sig": sig, "name": "Rook", "seats": "1", "wire": str(svc.WIRE)}, "rook")
    sid = rook["sid"]

    def call(op, **q):
        q["sid"] = sid
        try:
            return getattr(service, op)({k: str(v) for k, v in q.items()})
        except svc.Refusal as r:
            return {"error": r.code}

    # the game says hello; Rook invites it; it joins by itself (autojoin)
    if not waiting("game_hello", lambda: len(service.sessions) >= 2, 90):
        return finish(steps)
    game = [s for s in service.by_code.values() if s.code != rook["code"]][0]
    answer = call("party_invite", to=game.code)
    step("invite", "error" not in answer, str(answer))
    waiting("game_joined", lambda: service.parties and len(next(iter(service.parties.values())).members) == 2, 20)
    # QUICK PLAY for the party of two, then follow, then cancel
    answer = call("quick_play", playlist="casual", pings="127.0.0.1:10660=30,127.0.0.1:10661=40")
    step("quick_play", answer.get("search", {}).get("state") == "found", str(answer))
    time.sleep(16)            # (the game tries the found server for up to 10 s, then is free to follow)
    call("party_follow", kind="hosted", invite="0123456789abcdef" * 4)
    time.sleep(8)
    call("quick_cancel")
    time.sleep(3)
    # the game leaves with a kick; the game's own log shows "no party"
    answer = call("party_kick", code=game.code)
    step("kick", "error" not in answer, str(answer))
    waiting("game_out", lambda: all(game.code not in p.members for p in service.parties.values()), 10)
    time.sleep(5)
    finish(steps)


def finish(steps):
    steps.write("end\n")
    steps.flush()
    print("step: end", flush=True)
    while True:
        time.sleep(60)


if __name__ == "__main__":
    main()
