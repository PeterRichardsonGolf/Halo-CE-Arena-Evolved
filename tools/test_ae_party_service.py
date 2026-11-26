"""The party and QUICK PLAY service (tools/ae_party_service): Ed25519 against RFC 8032, friend codes, the party calls, the QUICK PLAY
pick, seat tickets, expiry, the state file and one HTTP round trip on 127.0.0.1. Standard library only."""
import json
import sys
import threading
import urllib.request
from http.server import ThreadingHTTPServer
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent / "ae_party_service"))
import ae_party_service as svc  # noqa: E402
import ed25519  # noqa: E402


class Clock:
    def __init__(self):
        self.t = 1_000_000.0

    def __call__(self):
        return self.t


SERVERS = [
    {"address": "casual-a:1", "playlist": "casual", "name": "Casual A", "players": 6, "max_players": 16, "reachable": True},
    {"address": "casual-b:1", "playlist": "casual", "name": "Casual B", "players": 0, "max_players": 16, "reachable": True},
    {"address": "comp-a:1", "playlist": "comp", "name": "Comp A", "players": 6, "max_players": 8, "reachable": True},
    {"address": "classic-a:1", "playlist": "classic", "name": "Classic", "players": 0, "max_players": 4, "reachable": False},
]


INVITE = "0123456789abcdef" * 4


def make_service(**options):
    clock = Clock()
    return svc.Service(bytes(range(32)), [dict(s) for s in SERVERS], clock, **options), clock


class Player:
    """One PC with its own key against the in-process service."""

    def __init__(self, service, clock, n, name=None, seats=1, peer=None):
        self.service, self.clock = service, clock
        self.seed = bytes([n]) * 32
        self.pub = ed25519.public_key(self.seed)
        self.name = name or "P%d" % n
        self.seats = seats
        self.peer = peer
        self.hello()

    def hello(self, origin=None, **over):
        peer = getattr(self, "peer", None) or "peer-%s" % self.name
        nonce = over.pop("nonce", None) or self.service.challenge({}, peer)["nonce"]
        text = "ae-party-hello-2\n%s\n%s\n%s" % (origin or self.service.origin, nonce, self.pub.hex())
        sig = ed25519.sign(self.seed, text.encode())
        q = {"key": self.pub.hex(), "nonce": nonce, "sig": sig.hex(), "name": self.name, "seats": str(self.seats), "wire": str(svc.WIRE)}
        q.update(over)
        r = self.service.hello(q, peer=peer)
        self.sid, self.code = r["sid"], r["code"]
        return r

    def call(self, op, **q):
        q["sid"] = self.sid
        return getattr(self.service, op)({k: str(v) for k, v in q.items()})

    def poll(self):
        seen = self.service.sessions.get(self.sid)     # (this player is seen now, then the housekeeping thread's sweep: a poll makes none)
        if seen:
            seen.last, seen.offline_since = self.service.now(), None
        self.service.tick()
        return self.service.poll({"sid": self.sid})


def refused(fn, code):
    with pytest.raises(svc.Refusal) as info:
        fn()
    assert info.value.code == code
    return info.value


# ---- Ed25519 and codes

def test_ed25519_rfc8032_vector_1():
    seed = bytes.fromhex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60")
    pub = bytes.fromhex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a")
    sig = bytes.fromhex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b")
    assert ed25519.public_key(seed) == pub
    assert ed25519.sign(seed, b"") == sig
    assert ed25519.verify(pub, b"", sig)
    assert not ed25519.verify(pub, b"x", sig)
    bad = bytearray(sig)
    bad[40] ^= 1
    assert not ed25519.verify(pub, b"", bytes(bad))


def test_ed25519_refuses_small_order_keys_and_commitments():
    # the identity, the point of order 2, and an order-4 point as keys; each with the signature that verifies for every message
    identity = (1).to_bytes(32, "little")
    order2 = (svc.ed25519.P - 1).to_bytes(32, "little")
    for small in (identity, order2):
        for sig in (identity + bytes(32), small + bytes(32)):
            assert not ed25519.verify(small, b"any message", sig)
    seed = bytes([4]) * 32
    pub = ed25519.public_key(seed)
    good = ed25519.sign(seed, b"m")
    assert ed25519.verify(pub, b"m", good)
    assert not ed25519.verify(pub, b"m", identity + good[32:])


def test_friend_code_vectors_match_the_game_unit():
    assert svc.friend_code(bytes(32)) == "CSM7-NBFR-CAYQ"
    assert svc.friend_code(bytes(range(32))) == "CC6W-TAB6-RGSP"
    assert svc.friend_code(b"\xff" * 32) == "NYB1-6XGF-E9HN"


def test_parse_code_reads_ilo_and_refuses_the_rest():
    assert svc.parse_code("k7qm-3rxa-w9td") == "K7QM-3RXA-W9TD"
    assert svc.parse_code("IL0O-1100-ILOO") == "1100-1100-1100"
    assert svc.parse_code("K7QM3RXAW9TU") is None
    assert svc.parse_code("K7QM3RXAW9T") is None
    assert svc.parse_code("K7QM3RXAW9TD0") is None
    assert svc.parse_code(None) is None


# ---- hello

def test_hello_checks_signature_wire_and_replaces_the_old_session():
    s, clock = make_service()
    a = Player(s, clock, 1)
    assert a.code == svc.friend_code(a.pub)
    refused(lambda: a.hello(sig="00" * 64), "bad_signature")
    refused(lambda: a.hello(wire="1"), "update_required")
    refused(lambda: a.hello(wire=""), "update_required")
    refused(lambda: a.hello(wire="\u00b2"), "update_required")
    refused(lambda: a.hello(key="00" * 31), "bad_value")
    old = a.sid
    clock.t += 61
    a.hello()
    assert a.sid != old
    refused(lambda: s.poll({"sid": old}), "no_session")


def test_a_nonce_is_single_use_fresh_and_known():
    s, clock = make_service()
    a = Player(s, clock, 1)
    nonce = s.challenge({}, "x")["nonce"]
    assert len(nonce) == 32 and s.challenge({}, "x")["expires_s"] == 60
    a.hello(nonce=nonce)
    # the same (nonce, signature) again
    clock.t += 30
    refused(lambda: a.hello(nonce=nonce), "bad_nonce")
    # an unknown nonce, an empty one
    refused(lambda: a.hello(nonce="ab" * 16), "bad_nonce")
    refused(lambda: a.hello(nonce="0"), "bad_nonce")
    # 61 s old
    old = s.challenge({}, "x")["nonce"]
    clock.t += 61
    refused(lambda: a.hello(nonce=old), "bad_nonce")
    # a bad signature still spends the nonce
    spent = s.challenge({}, "x")["nonce"]
    refused(lambda: a.hello(nonce=spent, sig="00" * 64), "bad_signature")
    refused(lambda: a.hello(nonce=spent), "bad_nonce")
    # a hello signed for another service is worth nothing here (the relay case)
    clock.t += 30
    refused(lambda: a.hello(origin="https://evil.example/party"), "bad_signature")
    a.hello(origin=s.origin)


def test_nonces_are_capped_and_expire():
    s, clock = make_service()
    first = s.challenge({}, "p0")["nonce"]
    for i in range(svc.NONCE_MAX):
        s.challenge({}, "p%d" % (i + 1))
    assert len(s.nonces) == svc.NONCE_MAX and first not in s.nonces
    clock.t += 61
    s.tick()
    assert len(s.nonces) == 0


def test_nonce_eviction_stays_with_the_source_that_filled_its_share():
    s, clock = make_service()
    honest = s.challenge({}, "honest")["nonce"]
    sources = [s.client_keys("2001:db8:1234:%x::1" % i) for i in range(svc.NONCES_PER_SOURCE + 1)]
    first = s.challenge({}, sources[0])["nonce"]
    for source in sources[1:]:
        s.challenge({}, source)
    assert honest in s.nonces and first not in s.nonces


def test_challenge_rate_limit_per_peer():
    s, clock = make_service()
    for _ in range(30):
        s.challenge({}, "peer")
    refused(lambda: s.challenge({}, "peer"), "rate_limited")
    s.challenge({}, "other")


def test_ipv6_48_has_its_own_challenge_budget():
    s, clock = make_service()
    sources = [s.client_keys("2001:db8:1234:%x::1" % i) for i in range(30)]
    assert len({fine for fine, coarse in sources}) == 30
    assert len({coarse for fine, coarse in sources}) == 1
    for source in sources:
        s.challenge({}, source)
    refused(lambda: s.challenge({}, s.client_keys("2001:db8:1234:ffff::1")), "rate_limited")
    s.challenge({}, s.client_keys("2001:db8:1235::1"))


def test_a_poll_makes_no_sweep_of_the_sessions_and_hellos_name_the_poll_interval():
    s, clock = make_service()
    a = Player(s, clock, 1)
    sweeps = []
    real = s.tick
    s.tick = lambda: (sweeps.append(1), real())[1]
    for _ in range(3):
        s.poll({"sid": a.sid})
    assert sweeps == []
    s.tick()
    assert sweeps == [1]
    assert a.hello()["config"]["poll_ms"] == svc.POLL_MS == 1000


def test_a_poll_with_a_crowd_online_visits_no_other_session():
    s, clock = make_service()
    a = Player(s, clock, 1)
    for i in range(4000):
        x = svc.Session("sid%d" % i, "%064x" % i, "C%d" % i, "n", "", "", 1, clock.t, "k%d" % (i // 16))
        s.sessions[x.sid], s.by_code[x.code] = x, x
    visits = []

    class Counting(dict):
        def values(self):
            visits.append(1)
            return super().values()

        def items(self):
            visits.append(1)
            return super().items()

        def __iter__(self):
            visits.append(1)
            return super().__iter__()

    s.sessions, s.by_code = Counting(s.sessions), Counting(s.by_code)
    s.poll({"sid": a.sid})
    s.poll({"sid": a.sid, "rev": "0"})
    assert visits == []


def test_hello_rate_limit_per_client_key_and_hourly():
    s, clock = make_service()
    p = Player(s, clock, 1)
    for _ in range(11):
        p.hello()
    refused(lambda: p.hello(), "rate_limited")
    Player(s, clock, 2, "other")                 # another client key is not held back
    # 12 a minute for an hour is more than 120 an hour
    total = 12
    while total < 120:
        clock.t += 61
        for _ in range(12):
            if total < 120:
                p.hello()
                total += 1
    clock.t += 61
    refused(lambda: p.hello(), "rate_limited")


def test_global_hello_limit_protects_the_cpu():
    s, clock = make_service()
    for i in range(svc.HELLO_GLOBAL_PER_S):
        Player(s, clock, i + 1, "g%d" % i)
    refused(lambda: Player(s, clock, 99, "late"), "rate_limited")
    clock.t += 1.1
    Player(s, clock, 99, "late")


def test_global_hello_refusals_do_not_spend_a_sources_budget():
    s, clock = make_service()
    for i in range(svc.HELLO_GLOBAL_PER_S):
        Player(s, clock, i + 1, "fill%d" % i)
    seed = bytes([99]) * 32
    pub = ed25519.public_key(seed)
    peer = "waiting-source"
    for _ in range(12):
        nonce = s.challenge({}, peer)["nonce"]
        sig = ed25519.sign(seed, ("ae-party-hello-2\n%s\n%s\n%s" % (s.origin, nonce, pub.hex())).encode()).hex()
        refused(lambda: s.hello({"wire": "2", "nonce": nonce, "key": pub.hex(), "sig": sig}, peer), "rate_limited")
    clock.t += 1.1
    nonce = s.challenge({}, peer)["nonce"]
    sig = ed25519.sign(seed, ("ae-party-hello-2\n%s\n%s\n%s" % (s.origin, nonce, pub.hex())).encode()).hex()
    assert s.hello({"wire": "2", "nonce": nonce, "key": pub.hex(), "sig": sig}, peer)["code"] == svc.friend_code(pub)


def test_bad_wire_and_nonce_do_not_spend_the_global_hello_budget():
    s, clock = make_service()
    for i in range(svc.HELLO_GLOBAL_PER_S * 2):
        refused(lambda: s.hello({"wire": "1", "nonce": "not-a-nonce"}, "bad-%d" % i), "update_required")
    for i in range(svc.HELLO_GLOBAL_PER_S * 2):
        refused(lambda: s.hello({"wire": "2", "nonce": "not-a-nonce"}, "nonce-%d" % i), "bad_nonce")
    for i in range(svc.HELLO_GLOBAL_PER_S * 2):
        refused(lambda: s.hello({"wire": "2", "nonce": "ab" * 16, "key": "00" * 32, "sig": "00" * 64},
                                "unknown-%d" % i), "bad_nonce")
    for i in range(svc.HELLO_GLOBAL_PER_S):
        Player(s, clock, i + 1, "valid%d" % i)
    refused(lambda: Player(s, clock, 99, "late"), "rate_limited")


def test_client_key_uses_the_proxys_last_forwarded_entry_only_from_a_trusted_peer():
    s, clock = make_service()
    a = s.client_key("127.0.0.1", "203.0.113.5")
    b = s.client_key("127.0.0.1", "203.0.113.6")
    assert a != b and a == s.client_key("::1", "10.0.0.1, 203.0.113.5") and a == s.client_key("::ffff:127.0.0.1", "203.0.113.5")
    # an address the client put in the header first is not the one the proxy added
    assert s.client_key("127.0.0.1", "198.51.100.9, 203.0.113.5") == a
    # an untrusted peer's header is ignored
    assert s.client_key("198.51.100.1", "203.0.113.5") == s.client_key("198.51.100.1", None) != a
    # IPv6: one /64 is one client
    assert s.client_key("2001:db8:1:2::1") == s.client_key("2001:db8:1:2:ffff::9") != s.client_key("2001:db8:1:3::1")
    assert s.client_key("127.0.0.1", "not an address") == s.client_key("127.0.0.1", None)
    assert len(a) == 16 and "203" not in a


def test_client_keys_group_ipv4_by_address_and_ipv6_by_64_and_48():
    s, clock = make_service()
    assert s.client_keys("203.0.113.5") == (s.client_key("203.0.113.5"), s.client_key("203.0.113.5"))
    a = s.client_keys("2001:db8:1:2::1")
    b = s.client_keys("2001:db8:1:2:ffff::9")
    c = s.client_keys("2001:db8:1:3::1")
    d = s.client_keys("2001:db8:2::1")
    assert a == b and a[0] != c[0] and a[1] == c[1] and a[1] != d[1]


def test_the_real_handler_counts_hellos_by_forwarded_client():
    service = svc.Service(bytes(range(32)), None, Clock(), "http://127.0.0.1:1")
    clock = service.now
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), svc.make_handler(service, None))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    base = "http://127.0.0.1:%d/party/v1/" % httpd.server_address[1]

    def post(op, forwarded, **q):
        from urllib.error import HTTPError
        request = urllib.request.Request(base + op, data=json.dumps(q).encode(), method="POST",
                                         headers={"Content-Type": "application/json", "X-Forwarded-For": forwarded})
        try:
            with urllib.request.urlopen(request) as r:
                return r.status, json.loads(r.read())
        except HTTPError as e:
            return e.code, json.loads(e.read())

    def hello(forwarded, n):
        seed = bytes([n]) * 32
        pub = ed25519.public_key(seed)
        nonce = post("challenge", forwarded)[1]["nonce"]
        sig = ed25519.sign(seed, ("ae-party-hello-2\n%s\n%s\n%s" % (service.origin, nonce, pub.hex())).encode()).hex()
        return post("hello", forwarded, key=pub.hex(), nonce=nonce, sig=sig, wire="2")[0]

    try:
        results = []
        for n in range(13):
            clock.t += 0.2
            results.append(hello("203.0.113.5", n + 1))
        assert results[:12] == [200] * 12 and results[12] == 429
        assert hello("203.0.113.6", 40) == 200         # a second client of the same proxy
    finally:
        httpd.shutdown()


def test_idle_windows_are_dropped_by_tick():
    s, clock = make_service()
    p = Player(s, clock, 1)
    assert s.client_limits
    clock.t += 3601
    s.tick()
    assert s.client_limits == {}


def test_a_full_service_evicts_the_session_idle_longest():
    s, clock = make_service()
    # (sessions are made directly: 4096 hellos would only test the hello limits)
    for i in range(svc.SESSIONS_MAX):
        x = svc.Session("sid%d" % i, "%064x" % i, "C%d" % i, "n", "", "", 1, clock.t, "k%d" % (i // 16))
        s.sessions[x.sid], s.by_code[x.code] = x, x
    p = Player.__new__(Player)
    p.service, p.clock, p.seed, p.name, p.seats = s, clock, bytes([7]) * 32, "newcomer", 1
    p.pub = ed25519.public_key(p.seed)
    # every session is live: busy
    refused(lambda: p.hello(), "busy")
    clock.t += 31
    s.sessions["sid5"].last = clock.t - 5            # live again; the rest are 31 s idle, "sid0" the first of them
    p.hello()
    assert "sid0" not in s.sessions and "sid5" in s.sessions and len(s.sessions) == svc.SESSIONS_MAX


def test_live_sessions_per_client_key_are_capped():
    s, clock = make_service()
    for i in range(svc.SESSIONS_PER_CLIENT):
        x = svc.Session("sid%d" % i, "%064x" % i, "C%d" % i, "n", "", "", 1, clock.t, "peer-new")
        s.sessions[x.sid], s.by_code[x.code] = x, x
    refused(lambda: Player(s, clock, 1, "new"), "rate_limited")
    clock.t += 31
    Player(s, clock, 1, "new")


def test_no_raw_address_is_kept_anywhere_in_the_service():
    s, clock = make_service()
    p = Player(s, clock, 1)
    s.client_key("203.0.113.77", None)
    seen, stack = set(), [s]
    found = []
    while stack:
        o = stack.pop()
        if id(o) in seen:
            continue
        seen.add(id(o))
        if isinstance(o, (str, bytes)):
            text = o if isinstance(o, str) else o.decode("latin1")
            if "203.0.113" in text or "127.0.0.1" in text.replace("http://127.0.0.1:8780", ""):
                found.append(text)
        elif isinstance(o, dict):
            stack += list(o.keys()) + list(o.values())
        elif isinstance(o, (list, tuple, set, frozenset)):
            stack += list(o)
        elif hasattr(o, "__dict__") and not callable(o):
            stack += list(vars(o).values())
    assert found == []


# ---- party

def test_invite_answer_and_party_view():
    s, clock = make_service()
    a, b = Player(s, clock, 1, "Chief"), Player(s, clock, 2, "Rook", seats=2)
    a.call("party_invite", to=b.code.lower().replace("-", ""))
    v = b.poll()
    assert v["invites"][0]["from"]["name"] == "Chief" and v["invites"][0]["expires_s"] == svc.INVITE_S
    b.call("party_answer", party=v["invites"][0]["party"], join=1)
    va, vb = a.poll(), b.poll()
    assert va["party"]["leader"] == a.code and va["party"]["seats"] == 3 and len(va["party"]["members"]) == 2
    assert va["party"]["link"] and vb["party"]["link"] is None
    assert vb["invites"] == []
    # poll with the current rev is "same"
    assert a.service.poll({"sid": a.sid, "rev": str(va["rev"])}) == {"rev": va["rev"], "same": True}


def test_invite_errors():
    s, clock = make_service()
    a, b, c = Player(s, clock, 1), Player(s, clock, 2), Player(s, clock, 3)
    refused(lambda: a.call("party_invite", to="NOPE"), "unknown_code")
    refused(lambda: a.call("party_invite", to=a.code), "self")
    a.call("party_invite", to=b.code)
    b.call("party_answer", party=b.poll()["invites"][0]["party"], join=1)
    # member invites need FRIENDS OF THE PARTY
    refused(lambda: b.call("party_invite", to=c.code), "not_leader")
    a.call("party_privacy", level="friends")
    b.call("party_invite", to=c.code)
    a.call("party_privacy", level="closed")
    refused(lambda: b.call("party_invite", to=c.code), "party_closed")
    refused(lambda: b.call("party_privacy", level="invite"), "not_leader")


def test_an_invite_answers_the_same_whoever_is_online():
    s, clock = make_service()
    sender, online, away, in_party, member = (Player(s, clock, n, "P%d" % n) for n in range(1, 6))
    member.call("party_invite", to=in_party.code)
    in_party.call("party_answer", party=in_party.poll()["invites"][0]["party"], join=1)
    for _ in range(3):                               # ("away" makes no poll for 75 s: offline)
        clock.t += 25
        for p in (sender, online, in_party, member):
            p.poll()
    away_code = away.code
    answers = [json.dumps(sender.call("party_invite", to=code), sort_keys=True) for code in
               (online.code, away_code, in_party.code, "K7QM-3RXA-W9TD", online.code.lower())]
    assert len(set(answers)) == 1 and answers[0] == '{"ok": true}'
    # delivered only to the player who is online and free
    assert len(online.poll()["invites"]) == 1 and in_party.poll()["invites"] == []
    clock.t += 1
    assert away.service.by_code[away_code].invites == {}


def test_a_stranger_flood_leaves_three_pending_and_a_friend_comes_first():
    s, clock = make_service()
    target = Player(s, clock, 1, "Target")
    strangers = [Player(s, clock, n, "S%d" % n) for n in range(2, 8)]
    for p in strangers[:5]:
        clock.t += 1
        p.call("party_invite", to=target.code)
    assert len(s.by_code[target.code].invites) == 3
    # a mutual friend's invite is delivered past the stranger cap, and shown first although the strangers' are older
    friend = strangers[5]
    friend.call("presence", codes=target.code)
    target.call("presence", codes=friend.code)
    clock.t += 1
    friend.call("party_invite", to=target.code)
    invites = target.poll()["invites"]
    assert len(invites) == 4 and invites[0]["from"]["code"] == friend.code
    # ten pending in all, friends or not
    assert len(s.by_code[target.code].invites) == 4
    for n in range(8, 8 + 8):
        p = Player(s, clock, n, "F%d" % n)
        p.call("presence", codes=target.code)
        target.call("presence", codes=",".join([friend.code, p.code]))
        p.call("party_invite", to=target.code)
    assert len(s.by_code[target.code].invites) == svc.INVITES_MAX


def test_a_blocked_code_reaches_nobody_and_sees_nothing():
    s, clock = make_service()
    a, b = Player(s, clock, 1, "A"), Player(s, clock, 2, "B")
    b.call("presence", codes=a.code, blocks=a.code)
    a.call("party_invite", to=b.code)
    assert b.poll()["invites"] == []
    # presence shows nothing either way between them, though each added the other
    a.call("presence", codes=b.code)
    assert a.call("presence", codes=b.code) == {"presence": []} and b.call("presence", codes=a.code, blocks=a.code) == {"presence": []}
    # the block list is session memory, replaced by the next presence call that carries one
    b.call("presence", codes=a.code, blocks="")
    assert b.call("presence", codes=a.code)["presence"][0]["code"] == a.code
    a.call("party_leave")
    clock.t += 11
    a.call("party_invite", to=b.code)
    assert len(b.poll()["invites"]) == 1


def test_party_full_counts_seats_and_reports_room():
    s, clock = make_service()
    a = Player(s, clock, 1, seats=4)
    b = Player(s, clock, 2, seats=3)
    c = Player(s, clock, 3, seats=2)
    a.call("party_invite", to=b.code)
    b.call("party_answer", party=b.poll()["invites"][0]["party"], join=1)
    a.call("party_invite", to=c.code)
    e = refused(lambda: c.call("party_answer", party=c.poll()["invites"][0]["party"], join=1), "party_full")
    assert e.extra["room"] == 1
    # a guest added after joining is checked too
    d = Player(s, clock, 4, seats=1)
    a.call("party_invite", to=d.code)
    d.call("party_answer", party=d.poll()["invites"][0]["party"], join=1)
    refused(lambda: d.call("party_seats", seats=2), "party_full")
    refused(lambda: d.call("party_seats", seats=9), "bad_value")


def test_leader_leaves_lead_passes_to_longest_and_last_out_ends_it():
    s, clock = make_service()
    a, b, c = Player(s, clock, 1, "A"), Player(s, clock, 2, "B"), Player(s, clock, 3, "C")
    for p in (b, c):
        a.call("party_invite", to=p.code)
        p.call("party_answer", party=p.poll()["invites"][0]["party"], join=1)
    a.call("party_leave")
    assert b.poll()["party"]["leader"] == b.code and "A" not in [m["name"] for m in b.poll()["party"]["members"]]
    assert "B leads now" in json.dumps(c.poll()["notice"])
    b.call("party_leave")
    c.call("party_leave")
    assert s.parties == {}
    refused(lambda: c.call("party_leave"), "not_in_party")


def test_kick_promote_and_leader_only():
    s, clock = make_service()
    a, b, c = Player(s, clock, 1), Player(s, clock, 2), Player(s, clock, 3)
    for p in (b, c):
        a.call("party_invite", to=p.code)
        p.call("party_answer", party=p.poll()["invites"][0]["party"], join=1)
    refused(lambda: b.call("party_kick", code=c.code), "not_leader")
    refused(lambda: a.call("party_kick", code=a.code), "self")
    a.call("party_kick", code=c.code)
    assert c.poll()["party"] is None and len(a.poll()["party"]["members"]) == 2
    a.call("party_promote", code=b.code)
    assert a.poll()["party"]["leader"] == b.code
    refused(lambda: a.call("party_promote", code=b.code), "not_leader")


def test_link_join_rotates_and_locks_after_failures():
    s, clock = make_service()
    a, b, c = Player(s, clock, 1), Player(s, clock, 2), Player(s, clock, 3)
    a.call("party_invite", to=Player(s, clock, 9).code)
    link = a.poll()["party"]["link"]
    assert len(link) == 24
    b.call("party_link", link=link.lower())
    assert len(a.poll()["party"]["members"]) == 2
    a.call("party_new_link")
    refused(lambda: c.call("party_link", link=link), "link_expired")
    for _ in range(4):
        refused(lambda: c.call("party_link", link=link), "link_expired")
    refused(lambda: c.call("party_link", link=a.poll()["party"]["link"]), "rate_limited")
    for _ in range(2):
        clock.t += 301
        for p in (a, b, c):
            p.poll()
    c.call("party_link", link=a.poll()["party"]["link"])


def test_offline_members_go_after_two_minutes_and_expired_invites_vanish():
    s, clock = make_service()
    a, b = Player(s, clock, 1), Player(s, clock, 2)
    a.call("party_invite", to=b.code)
    b.call("party_answer", party=b.poll()["invites"][0]["party"], join=1)
    clock.t += 40
    a.poll()
    s.tick()
    assert a.poll()["party"]["members"][1]["state"] == "offline"
    clock.t += 100
    a.poll()
    s.tick()
    assert len(a.poll()["party"]["members"]) == 1
    c = Player(s, clock, 3)
    a.call("party_invite", to=c.code)
    clock.t += svc.INVITE_S + 1
    a.poll()
    assert c.poll()["invites"] == []


def test_presence_is_mutual_only():
    s, clock = make_service()
    a, b = Player(s, clock, 1), Player(s, clock, 2)
    assert a.call("presence", codes=b.code) == {"presence": []}
    assert b.call("presence", codes=a.code)["presence"][0]["state"] == "menus"
    assert a.call("presence", codes=b.code)["presence"][0] == {"code": b.code, "state": "menus"}


def test_follow_reaches_members_without_an_address_of_a_pc():
    s, clock = make_service()
    a, b = Player(s, clock, 1), Player(s, clock, 2)
    a.call("party_invite", to=b.code)
    b.call("party_answer", party=b.poll()["invites"][0]["party"], join=1)
    refused(lambda: b.call("party_follow", kind="server", address="x:1"), "not_leader")
    a.call("party_follow", kind="hosted", invite=INVITE)
    f = b.poll()["follow"]
    assert f["kind"] == "hosted" and f["invite"] == INVITE
    assert "127.0.0.1" not in json.dumps(b.poll())


def test_a_follow_is_for_the_members_of_that_moment_and_ends_with_any_change():
    s, clock = make_service()
    a, b, c, d = Player(s, clock, 1), Player(s, clock, 2), Player(s, clock, 3), Player(s, clock, 4)
    for p in (b, c):
        a.call("party_invite", to=p.code)
        p.call("party_answer", party=p.poll()["invites"][0]["party"], join=1)
    a.call("party_follow", kind="hosted", invite=INVITE)
    assert b.poll()["follow"]["invite"] == INVITE and c.poll()["follow"]["invite"] == INVITE
    # D joins after the follow: sees none, and the join ends the follow for everyone
    a.call("party_invite", to=d.code)
    d.call("party_answer", party=d.poll()["invites"][0]["party"], join=1)
    assert d.poll()["follow"] is None and b.poll()["follow"] is None
    # C is kicked: the follow made before is gone for the rest (and C never sees a later one)
    clock.t += 11
    a.call("party_follow", kind="hosted", invite=INVITE)
    assert b.poll()["follow"] is not None
    a.call("party_kick", code=c.code)
    assert b.poll()["follow"] is None and c.poll()["follow"] is None
    # the leader changes
    clock.t += 11
    a.call("party_follow", kind="hosted", invite=INVITE)
    a.call("party_promote", code=b.code)
    assert d.poll()["follow"] is None
    # nobody who left keeps the invite in a view
    clock.t += 11
    b.call("party_follow", kind="hosted", invite=INVITE)
    d.call("party_leave")
    assert INVITE not in json.dumps(d.poll())


def test_a_follow_lasts_ten_minutes():
    s, clock = make_service()
    a, b = Player(s, clock, 1), Player(s, clock, 2)
    a.call("party_invite", to=b.code)
    b.call("party_answer", party=b.poll()["invites"][0]["party"], join=1)
    a.call("party_follow", kind="hosted", invite=INVITE)
    for _ in range(29):                                   # (both stay online: 29 x 20 s)
        clock.t += 20
        a.poll()
        b.poll()
    clock.t += 19
    assert b.poll()["follow"] is not None
    clock.t += 2
    assert b.poll()["follow"] is None
    s.tick()
    assert s.parties[next(iter(s.parties))].follow is None


def test_follow_names_only_a_listed_server_or_a_real_invite_and_not_often():
    s, clock = make_service()
    a, b = Player(s, clock, 1), Player(s, clock, 2)
    a.call("party_invite", to=b.code)
    b.call("party_answer", party=b.poll()["invites"][0]["party"], join=1)
    # a server that is not in the list, a LAN follow, an unknown kind
    refused(lambda: a.call("party_follow", kind="server", address="10.0.0.5:10660"), "bad_value")
    refused(lambda: a.call("party_follow", kind="server", address="casual-a:1 "), "bad_value")
    refused(lambda: a.call("party_follow", kind="lan", address="192.168.1.2:10660"), "bad_value")
    refused(lambda: a.call("party_follow", kind="x"), "bad_value")
    # an invite of 63 or 65 digits, upper case, not hex
    for bad in (INVITE[:-1], INVITE + "0", INVITE.upper().replace("0", "A"), "g" * 64, INVITE + "\n", ""):
        refused(lambda: a.call("party_follow", kind="hosted", invite=bad), "bad_value")
    assert b.poll()["follow"] is None
    # a listed server, once; a second one inside 10 s waits
    a.call("party_follow", kind="server", address="casual-a:1")
    assert b.poll()["follow"]["address"] == "casual-a:1"
    refused(lambda: a.call("party_follow", kind="server", address="casual-b:1"), "rate_limited")
    clock.t += 11
    a.call("party_follow", kind="server", address="casual-b:1")
    assert b.poll()["follow"]["address"] == "casual-b:1"


def test_follow_is_limited_to_sixty_an_hour():
    s, clock = make_service()
    a = Player(s, clock, 1)
    a.call("party_invite", to=Player(s, clock, 2).code)
    for i in range(60):
        clock.t += 11
        a.poll()                                            # (a live session)
        a.call("party_follow", kind="hosted", invite=INVITE)
    clock.t += 11
    refused(lambda: a.call("party_follow", kind="hosted", invite=INVITE), "rate_limited")


# ---- quick play

def test_pick_prefers_populated_then_ping_and_keeps_prefer():
    servers = [dict(s) for s in SERVERS]
    r = svc.pick_server(servers, "casual", 2, {}, {})
    assert r["server"]["address"] == "casual-a:1"            # populated beats empty with no pings
    r = svc.pick_server(servers, "casual", 2, {"casual-a:1": 90, "casual-b:1": 20}, {})
    assert r["server"]["address"] == "casual-b:1"            # a lower ping beats the 25 point empty penalty
    r = svc.pick_server(servers, "casual", 2, {"casual-a:1": 40, "casual-b:1": 20}, {})
    assert r["server"]["address"] == "casual-a:1"            # 40 against 20 + 25
    r = svc.pick_server(servers, "casual", 2, {"casual-a:1": 40, "casual-b:1": 10}, {})
    assert r["server"]["address"] == "casual-b:1"
    r = svc.pick_server(servers, "casual", 2, {"casual-a:1": 40, "casual-b:1": 10}, {}, prefer="casual-a:1")
    assert r["server"]["address"] == "casual-a:1"            # the server just played wins within 40 points
    r = svc.pick_server(servers, "casual", 2, {"casual-a:1": 40, "casual-b:1": 20}, {}, failed={"casual-b:1"})
    assert r["server"]["address"] == "casual-a:1"


def test_pick_counts_held_seats_and_room():
    servers = [dict(s) for s in SERVERS]
    assert svc.pick_server(servers, "comp", 2, {}, {"comp-a:1": 0})["server"]["address"] == "comp-a:1"
    r = svc.pick_server(servers, "comp", 3, {}, {"comp-a:1": 0})
    assert r["server"] is None and r["partial"]["room"] == 2
    r = svc.pick_server(servers, "comp", 2, {}, {"comp-a:1": 1})
    assert r["server"] is None
    assert svc.pick_server(servers, "classic", 1, {}, {})["server"] is None       # unreachable
    assert svc.pick_server(servers, "btb", 1, {}, {})["server"] is None           # no server at all


def test_quick_play_found_ticket_hold_and_cancel():
    s, clock = make_service()
    a, b = Player(s, clock, 1, "Chief"), Player(s, clock, 2, "Rook")
    a.call("party_invite", to=b.code)
    b.call("party_answer", party=b.poll()["invites"][0]["party"], join=1)
    r = a.call("quick_play", playlist="casual", pings="casual-a:1=30,casual-b:1=35")["search"]
    assert r["state"] == "found" and r["server"]["address"] == "casual-a:1" and r["seats"] == 2
    assert b.poll()["search"]["ticket"] == r["ticket"]
    # the ticket is the service's, for that server and those members only
    assert svc.check_ticket(s.public, r["ticket"], "casual-a:1", a.code, clock()) == 2
    assert svc.check_ticket(s.public, r["ticket"], "casual-a:1", b.code, clock()) == 2
    assert svc.check_ticket(s.public, r["ticket"], "casual-b:1", a.code, clock()) == 0
    assert svc.check_ticket(s.public, r["ticket"], "casual-a:1", "XXXX-XXXX-XXXX", clock()) == 0
    assert svc.check_ticket(s.public, r["ticket"], "casual-a:1", a.code, clock() + svc.HOLD_S + 1) == 0
    assert svc.check_ticket(s.public, r["ticket"][:-4], "casual-a:1", a.code, clock()) == 0
    other = svc.Service(bytes([7]) * 32)
    assert svc.check_ticket(other.public, r["ticket"], "casual-a:1", a.code, clock()) == 0
    # the hold counts as players for another party, then goes on cancel
    assert s._hold_seats("casual-a:1") == 2
    refused(lambda: b.call("quick_cancel"), "not_leader")
    refused(lambda: b.call("quick_play", playlist="casual"), "not_leader")
    a.call("quick_cancel")
    assert s._hold_seats("casual-a:1") == 0 and b.poll()["search"]["state"] == "none"


def test_quick_play_holds_expire_and_failed_server_is_skipped_for_a_while():
    s, clock = make_service()
    a = Player(s, clock, 1)
    a.call("quick_play", playlist="casual", pings="casual-a:1=30")
    assert s._hold_seats("casual-a:1") == 1
    clock.t += svc.HOLD_S + 1
    assert s._hold_seats("casual-a:1") == 0
    r = a.call("quick_play", playlist="casual", pings="casual-a:1=30,casual-b:1=200", again=1, failed="casual-a:1")["search"]
    assert r["server"]["address"] == "casual-b:1"
    clock.t += svc.FAILED_SERVER_S + 1
    s.tick()
    assert a.call("quick_play", playlist="casual", pings="casual-a:1=30,casual-b:1=200")["search"]["server"]["address"] == "casual-a:1"


def test_quick_play_holds_only_live_members_seats():
    s, clock = make_service()
    a, b = Player(s, clock, 1), Player(s, clock, 2)
    a.call("party_invite", to=b.code)
    b.call("party_answer", party=b.poll()["invites"][0]["party"], join=1)
    clock.t += svc.SESSION_OFFLINE_S + 1
    a.poll()
    a.call("quick_play", playlist="casual")
    assert s._hold_seats("casual-a:1") == 1


def test_quick_play_caps_a_servers_holds_at_half_its_open_seats():
    s, clock = make_service()
    s.servers = [{"address": "only:1", "playlist": "casual", "name": "Only", "players": 0,
                  "max_players": 16, "reachable": True}]
    players = [Player(s, clock, i + 1, seats=4) for i in range(4)]
    for player in players:
        player.call("quick_play", playlist="casual")
    assert s._hold_seats("only:1") == 8


def test_quick_play_reclamps_existing_holds_when_open_seats_shrink():
    s, clock = make_service()
    s.servers = [{"address": "only:1", "playlist": "casual", "name": "Only", "players": 0,
                  "max_players": 16, "reachable": True}]
    for i in range(2):
        Player(s, clock, i + 1, seats=4).call("quick_play", playlist="casual")
    s.servers[0]["players"] = 8
    assert s._hold_seats("only:1") == 4


def test_quick_play_allows_only_two_holding_parties_per_source():
    s, clock = make_service()
    players = [Player(s, clock, i + 1) for i in range(3)]
    for player in players:
        s.sessions[player.sid].client = "shared-source"
        player.call("quick_play", playlist="casual")
    assert s._hold_seats("casual-a:1") == 2


def test_quick_play_groups_ipv6_64_sources_by_48_for_hold_limits():
    s, clock = make_service()
    players = [Player(s, clock, i + 1, peer=s.client_keys("2001:db8:1234:%x::1" % i)) for i in range(3)]
    for player in players:
        player.call("quick_play", playlist="casual")
    assert s._hold_seats("casual-a:1") == 2


def test_expired_holds_on_removed_servers_free_a_sources_slots():
    s, clock = make_service()
    source = "shared-source"
    for player, playlist in zip([Player(s, clock, 1), Player(s, clock, 2)], ("casual", "comp")):
        s.sessions[player.sid].client = source
        s.sessions[player.sid].client_coarse = source
        player.call("quick_play", playlist=playlist)
    clock.t += svc.HOLD_S + 1
    s.servers = [{"address": "new:1", "playlist": "community", "name": "New", "players": 0,
                  "max_players": 16, "reachable": True}]
    third = Player(s, clock, 3)
    s.sessions[third.sid].client = source
    s.sessions[third.sid].client_coarse = source
    third.call("quick_play", playlist="community")
    assert s._hold_seats("new:1") == 1


def test_a_failed_report_bars_a_server_for_that_party_only():
    s, clock = make_service()
    a, b = Player(s, clock, 1), Player(s, clock, 2)
    pings = "casual-a:1=30,casual-b:1=200"
    assert a.call("quick_play", playlist="casual", pings=pings)["search"]["server"]["address"] == "casual-a:1"
    a.call("quick_play", playlist="casual", pings=pings, again=1, failed="casual-a:1")
    # party B's next pick is still casual-a
    assert b.call("quick_play", playlist="casual", pings=pings)["search"]["server"]["address"] == "casual-a:1"
    assert b.call("quick_play", playlist="casual", pings=pings, again=1)["search"]["server"]["address"] == "casual-a:1"


def test_a_failed_report_for_a_server_the_party_was_not_sent_to_changes_nothing():
    s, clock = make_service()
    a = Player(s, clock, 1)
    pings = "casual-a:1=30,casual-b:1=200"
    # no search yet, then a report for the other server
    a.call("quick_play", playlist="casual", pings=pings, again=1, failed="casual-b:1")
    assert s.parties[s.by_code[a.code].party].failed == {}
    a.call("quick_play", playlist="casual", pings=pings, again=1, failed="never-listed:9")
    assert s.parties[s.by_code[a.code].party].failed == {}


def test_a_failed_report_after_two_minutes_is_ignored_and_the_ninth_evicts_the_oldest():
    s, clock = make_service()
    a = Player(s, clock, 1)
    party_of = lambda: s.parties[s.by_code[a.code].party]
    a.call("quick_play", playlist="casual", pings="casual-a:1=30")
    clock.t += svc.FAILED_REPORT_S + 1
    a.call("quick_play", playlist="casual", pings="casual-a:1=30", again=1, failed="casual-a:1")
    assert party_of().failed == {}
    # eight bars kept, a ninth evicts the oldest (the party is set up with its last_found each time)
    party = party_of()
    for i in range(svc.FAILED_MAX):
        party.failed["old-%d" % i] = clock.t + 100 + i
    party.last_found = ("casual-a:1", clock.t)
    a.call("quick_play", playlist="casual", pings="casual-a:1=30", again=1, failed="casual-a:1")
    assert len(party.failed) == svc.FAILED_MAX and "old-0" not in party.failed and "casual-a:1" in party.failed


def test_quick_play_never_empty_bots_offer_and_limits():
    s, clock = make_service()
    a = Player(s, clock, 1, seats=4)
    r = a.call("quick_play", playlist="btb")["search"]
    assert r["state"] == "bots" and r["ask"] is False and r["room_for"] == 0
    r = a.call("quick_play", playlist="comp", pings="")["search"]
    assert r["state"] == "bots" and r["ask"] is True and r["reason"] == "no_room" and r["room_for"] == 2
    refused(lambda: a.call("quick_play", playlist="classic"), "party_too_big")
    refused(lambda: a.call("quick_play", playlist="nope"), "bad_playlist")
    for _ in range(6):
        a.call("quick_play", playlist="btb")
    refused(lambda: a.call("quick_play", playlist="btb"), "rate_limited")


def test_leader_leaving_cancels_the_search_and_frees_the_hold():
    s, clock = make_service()
    a, b = Player(s, clock, 1), Player(s, clock, 2)
    a.call("party_invite", to=b.code)
    b.call("party_answer", party=b.poll()["invites"][0]["party"], join=1)
    a.call("quick_play", playlist="casual")
    a.call("party_leave")
    assert b.poll()["search"]["state"] == "none" and s._hold_seats("casual-a:1") == 0


# ---- state file and HTTP

def test_state_file_restores_a_party_when_members_say_hello_again():
    s, clock = make_service()
    a, b = Player(s, clock, 1), Player(s, clock, 2)
    a.call("party_invite", to=b.code)
    b.call("party_answer", party=b.poll()["invites"][0]["party"], join=1)
    data = json.loads(json.dumps(s.snapshot()))
    s2 = svc.Service(bytes(range(32)), [], clock)
    clock.t += 20
    assert s2.restore(data) == 1
    a2 = Player(s2, clock, 1)
    assert a2.poll()["party"]["leader"] == a.code and len(a2.poll()["party"]["members"]) == 2
    old = svc.Service(bytes(range(32)), [], clock)
    clock.t += 400
    assert old.restore(data) == 0


def test_http_round_trip_on_localhost(tmp_path):
    servers = tmp_path / "servers.json"
    servers.write_text(json.dumps(SERVERS))
    service = svc.Service(bytes(range(32)))
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), svc.make_handler(service, str(servers)))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    base = "http://127.0.0.1:%d/party/v1/" % httpd.server_address[1]

    def get(op, **q):
        from urllib.error import HTTPError
        body = json.dumps({k: str(v) for k, v in q.items()}).encode()
        request = urllib.request.Request(base + op, data=body, headers={"Content-Type": "application/json"}, method="POST")
        try:
            with urllib.request.urlopen(request) as r:
                return r.status, json.loads(r.read())
        except HTTPError as e:
            return e.code, json.loads(e.read())

    try:
        seed = bytes([5]) * 32
        pub = ed25519.public_key(seed)
        status, body = get("challenge")
        assert status == 200
        nonce = body["nonce"]
        sig = ed25519.sign(seed, ("ae-party-hello-2\n%s\n%s\n%s" % (service.origin, nonce, pub.hex())).encode()).hex()
        status, body = get("hello", key=pub.hex(), nonce=nonce, sig=sig, name="Net Player", seats=1, wire=svc.WIRE)
        assert status == 200 and body["code"] == svc.friend_code(pub)
        sid = body["sid"]
        status, body = get("quick_play", sid=sid, playlist="casual", pings="casual-a:1=20")
        assert status == 200 and body["search"]["state"] == "found"
        status, body = get("poll", sid=sid)
        assert status == 200 and body["party"]["members"][0]["name"] == "Net Player"
        status, body = get("party_leave", sid="bad")
        assert status == 401 and body["error"] == "no_session"
        status, body = get("nothing")
        assert status == 404
        status, body = get("health")
        assert status == 200 and body["sessions"] == 1 and body["servers"] == 4
        # wire 2 has no GET but the health check: a call with a query is refused, nothing in it is read
        from urllib.error import HTTPError
        with pytest.raises(HTTPError) as info:
            urllib.request.urlopen(base + "poll?sid=" + sid)
        assert info.value.code == 405
        with urllib.request.urlopen(base + "health") as r:
            assert r.status == 200
    finally:
        httpd.shutdown()


# ---- the servers exporter

def test_exporter_checks_the_list_and_builds_candidates(tmp_path, monkeypatch):
    import base64
    import time
    import export_servers as ex
    seed = bytes([9]) * 32
    key = ed25519.public_key(seed).hex()
    body = json.dumps({"protocol": 1, "master": {"name": "m", "key_id": "x", "time": int(time.time())}, "servers": [
        {"id": "a", "name": "Casual A", "players": 4, "max_players": 16, "open": True, "password": False, "reachable": True,
         "address": "play.example:10660", "ae": {"bots": 2}},
        {"id": "b", "name": "Locked", "players": 0, "max_players": 8, "open": True, "password": True, "reachable": True,
         "address": "comp.example:10683"}]}).encode()
    sig = "ed25519=" + base64.b64encode(ed25519.sign(seed, body)).decode()
    data = ex.check_list(body, sig, key, int(time.time()))
    addresses = {"play.example": "203.0.113.10", "comp.example": "203.0.113.11", "btb.example": "203.0.113.12",
                 "btb2.example": "203.0.113.13"}
    monkeypatch.setattr(ex.socket, "gethostbyname", addresses.__getitem__)
    playlists = ex.read_playlists("# c\ncasual|CASUAL|d|play.example:10660\ncomp|COMP|d|comp.example:10683\nbad|x\nbtb|BTB|d|btb.example:1 btb2.example:2\n")
    servers = ex.build(playlists, data)
    assert servers[0] == {"address": "203.0.113.10:10660", "playlist": "casual", "name": "Casual A", "players": 6,
                          "max_players": 16, "reachable": True}
    assert servers[1]["reachable"] is False                    # a password keeps a party out
    assert [s["address"] for s in servers[2:]] == ["203.0.113.12:1", "203.0.113.13:2"] and servers[2]["players"] == -1
    # refusals: wrong key, a changed body, a stale list, no signature
    with pytest.raises(ValueError):
        ex.check_list(body, sig, ed25519.public_key(bytes([8]) * 32).hex(), int(time.time()))
    with pytest.raises(ValueError):
        ex.check_list(body + b" ", sig, key, int(time.time()))
    with pytest.raises(ValueError):
        ex.check_list(body, sig, key, int(time.time()) + 1000)
    with pytest.raises(ValueError):
        ex.check_list(body, "", key, int(time.time()))
    # the whole program, from files: writes the file; a bad list leaves the old one
    (tmp_path / "list.json").write_bytes(body)
    (tmp_path / "mm.txt").write_text("casual|CASUAL|d|play.example:10660\n")
    out = tmp_path / "servers.json"
    args = ["--from-file", str(tmp_path / "list.json"), "--signature", sig, "--key", key, "--playlists", str(tmp_path / "mm.txt"), "--out", str(out)]
    assert ex.main(args) == 0 and json.loads(out.read_text())[0]["players"] == 6
    out.write_text("[]")
    assert ex.main(args[:-6] + ["--key", "00" * 32] + args[-4:]) == 1 and out.read_text() == "[]"
    # the service takes the exporter's unknown-population candidates: a party still gets a server
    r = svc.pick_server(servers, "btb", 2, {}, {})
    assert r["server"]["address"] == "203.0.113.12:1"


def test_exporter_resolves_hostnames_once_and_skips_unresolved_names(capsys):
    import export_servers as ex
    calls = []

    def resolve(host):
        calls.append(host)
        if host == "play.example":
            return "203.0.113.9"
        raise OSError("not found")

    playlists = ex.read_playlists("casual|CASUAL|d|play.example:10660 play.example:10661 missing.example:1\n")
    data = {"servers": [{"address": "203.0.113.9:10660", "name": "Resolved", "players": 3,
                         "max_players": 16, "reachable": True, "open": True, "password": False}]}
    servers = ex.build(playlists, data, resolve=resolve)
    assert [s["address"] for s in servers] == ["203.0.113.9:10660", "203.0.113.9:10661"]
    assert servers[0]["name"] == "Resolved" and servers[0]["players"] == 3
    assert calls == ["play.example", "missing.example"]
    assert "missing.example" in capsys.readouterr().err


# ---- Steam friends (opt-in, hashes, a match both ways)

def steam_hash(steam_id):
    import hashlib
    return hashlib.sha256(("ae-steam-1" + str(steam_id)).encode()).hexdigest()[:12]


def test_steam_friends_match_only_both_ways_and_only_when_opted_in():
    s, clock = make_service(steam_dev=True)
    a, b, c = Player(s, clock, 1, "Ann"), Player(s, clock, 2, "Bob"), Player(s, clock, 3, "Cy")
    ha, hb, hc = steam_hash(76561190000000001), steam_hash(76561190000000002), steam_hash(76561190000000003)
    # not registered: nothing, and a lookup is refused
    assert a.poll()["steam"] == []
    refused(lambda: a.call("steam_lookup", hashes=hb), "not_registered")
    a.call("steam_register", hash=ha)
    b.call("steam_register", hash=hb)
    c.call("steam_register", hash=hc)
    a.call("steam_lookup", hashes=",".join([hb, hc]), reset=1)
    assert a.poll()["steam"] == []                                     # Bob and Cy have not asked about Ann
    b.call("steam_lookup", hashes=ha, reset=1)
    found = a.poll()["steam"]
    assert [f["code"] for f in found] == [b.code] and found[0]["name"] == "Bob" and found[0]["hash"] == hb and found[0]["verified"] is False
    assert [f["code"] for f in b.poll()["steam"]] == [a.code]
    assert c.poll()["steam"] == []
    # the update reaches a poll that already saw its rev
    rev = a.poll()["rev"]
    c.call("steam_lookup", hashes=ha, reset=1)
    assert a.service.poll({"sid": a.sid, "rev": str(rev)}).get("same") is not True
    assert sorted(f["code"] for f in a.poll()["steam"]) == sorted([b.code, c.code])
    # withdrawing: the registration and the watch go, nobody sees Ann
    a.call("steam_register", hash="")
    assert a.poll()["steam"] == [] and b.poll()["steam"] == []
    # bad values and the offline rule
    refused(lambda: a.call("steam_register", hash="xyz"), "bad_value")
    a.call("steam_register", hash=ha)
    a.call("steam_lookup", hashes="short,%s,NOTHEXNOTHEX" % hb, reset=1)
    assert a.service.sessions[a.sid].steam_watch == {hb}
    clock.t += 40
    a.poll()
    s.tick()
    assert a.poll()["steam"] == []


def test_a_public_service_refuses_the_steam_calls():
    s, clock = make_service()
    a = Player(s, clock, 1)
    e = refused(lambda: a.call("steam_register", hash=steam_hash(1)), "unknown_call")
    assert e.status == 404
    refused(lambda: a.call("steam_lookup", hashes=steam_hash(2), reset=1), "unknown_call")
    assert a.poll()["steam"] == [] and s.sessions[a.sid].steam_hash is None
    # through the real handler: 404
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), svc.make_handler(s, None))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    try:
        from urllib.error import HTTPError
        request = urllib.request.Request("http://127.0.0.1:%d/v1/steam_register" % httpd.server_address[1],
                                         data=json.dumps({"sid": a.sid, "hash": steam_hash(1)}).encode(), method="POST")
        with pytest.raises(HTTPError) as info:
            urllib.request.urlopen(request)
        assert info.value.code == 404
    finally:
        httpd.shutdown()


def test_steam_hashes_never_reach_an_address_or_other_players_view():
    s, clock = make_service(steam_dev=True)
    a, b = Player(s, clock, 1), Player(s, clock, 2)
    a.call("steam_register", hash=steam_hash(1))
    assert "steam_hash" not in json.dumps(b.poll()) and steam_hash(1) not in json.dumps(b.poll())


def test_bad_length_and_bad_numbers_are_400_and_the_state_file_is_owner_only(tmp_path):
    import http.client
    import os
    servers = tmp_path / "servers.json"
    servers.write_text(json.dumps(SERVERS))
    service = svc.Service(bytes(range(32)))
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), svc.make_handler(service, str(servers)))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    port = httpd.server_address[1]
    try:
        for length in ("-1", "x", None):
            conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
            conn.putrequest("POST", "/party/v1/hello")
            if length is not None:
                conn.putheader("Content-Length", length)
            conn.endheaders()
            assert conn.getresponse().status == 400
            conn.close()
        seed = bytes([5]) * 32
        pub = ed25519.public_key(seed)
        sig = ed25519.sign(seed, b"x").hex()
        for payload, status in ((b"not json", 400), (b"[1]", 400), (b"x" * 9000, 413),
                                (json.dumps({"key": pub.hex(), "nonce": "0", "sig": sig, "wire": "\u00b2"}).encode(), 426)):
            conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
            conn.request("POST", "/party/v1/hello", body=payload, headers={"Content-Type": "application/json"})
            assert conn.getresponse().status == status
            conn.close()
    finally:
        httpd.shutdown()
    state = tmp_path / "party-state.json"
    svc.write_state(str(state), {"parties": []})
    assert (os.stat(state).st_mode & 0o777) == 0o600
